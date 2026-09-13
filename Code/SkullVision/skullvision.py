#!/usr/bin/env python3
"""
skullvision.py - Alchemy Escape Rooms / Mermaid's Tale
Camera-driven replacement for the Captain's Cuffs hall-sensor input.

Watches the Reolink feed, counts people, works out which skull each one is
standing at, and reports the head-count + occupancy over MQTT. Since firmware
v1.5.0 the Mega solves when the number of touched skulls equals the head-count
published on system/People (any skulls). The legacy "all skulls held" solve
command below stays behind dry_run and should never be armed.

    python skullvision.py                 # normal run (honours dry_run in config)
    python skullvision.py --dry-run       # detect and log, never publish SOLVE
    python skullvision.py --preview       # draw a debug window
    python skullvision.py --selftest      # no camera, no MQTT - logic check only

Published topics (base = MermaidsTale/CaptainsCuffs):
    <base>/system/People     "0".."9" debounced head-count   retained  <- solve target
                             (camera-confirmed count held people_hold_s, never below the
                              number of occupied skull zones - see zone_logic.PeopleCounter)
    <base>/system/Skull<N>   "Occupied" | "Empty"      retained
    <base>/vision/status     "ONLINE" | "OFFLINE"      retained, OFFLINE is the LWT
    <base>/vision/summary    JSON snapshot every state change
    <base>/command           "SKULLS_SOLVED"           only on solve

Safety note: this publishes a solve. It has no authority to unlock a door on
its own - the Mega still owns the relays. Keep the GM manual override working.
"""

import argparse
import json
import logging
import os
import signal
import sys
import time
from typing import List, Optional, Tuple

# Force RTSP over TCP. Over UDP, corrupt packets from the Reolink can crash
# ffmpeg inside OpenCV and take the whole process down with it (exit 255, no
# traceback) - seen twice in production, both right after a stream stall.
# Must be set before cv2 is first imported anywhere.
os.environ.setdefault("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp")

import config_loader
import zone_logic

log = logging.getLogger("skullvision")

_running = True


def _stop(signum, frame):
    global _running
    _running = False
    log.info("Shutdown signal received.")


class MqttPublisher:
    """Thin wrapper. If the broker is down the vision loop keeps running and
    keeps logging - it just cannot publish. Losing MQTT should never crash the
    process mid-game."""

    def __init__(self, cfg, dry_run: bool):
        self.base = cfg["base_topic"].rstrip("/")
        self.dry_run = dry_run
        self.client = None
        self.connected = False
        self._cfg = cfg

    def connect(self):
        try:
            import paho.mqtt.client as mqtt
        except ImportError:
            sys.exit("paho-mqtt is not installed. Run: pip install paho-mqtt")

        try:
            self.client = mqtt.Client(client_id=self._cfg["client_id"], clean_session=True)
        except TypeError:  # paho 2.x
            self.client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION1, client_id=self._cfg["client_id"]
            )

        if self._cfg.get("username"):
            self.client.username_pw_set(self._cfg["username"], self._cfg.get("password"))

        status_topic = f"{self.base}/vision/status"
        self.client.will_set(status_topic, "OFFLINE", qos=1, retain=True)

        def on_connect(client, userdata, flags, rc, *a):
            self.connected = (rc == 0)
            if self.connected:
                log.info("MQTT connected to %s:%s", self._cfg["host"], self._cfg["port"])
                client.publish(status_topic, "ONLINE", qos=1, retain=True)
            else:
                log.error("MQTT connect failed rc=%s", rc)

        def on_disconnect(client, userdata, rc, *a):
            self.connected = False
            log.warning("MQTT disconnected rc=%s - will retry", rc)

        self.client.on_connect = on_connect
        self.client.on_disconnect = on_disconnect

        try:
            self.client.connect_async(self._cfg["host"], int(self._cfg["port"]), keepalive=30)
            self.client.loop_start()
        except Exception as e:
            log.error("MQTT setup failed: %s (continuing without MQTT)", e)

    def publish(self, topic: str, payload: str, retain: bool = False, qos: int = 0):
        full = f"{self.base}/{topic.lstrip('/')}"
        if not self.client or not self.connected:
            log.debug("MQTT down, dropped: %s = %s", full, payload)
            return
        self.client.publish(full, payload, qos=qos, retain=retain)

    def publish_solve(self):
        if self.dry_run:
            log.warning("DRY RUN - solve detected but NOT published. "
                        "Set dry_run false in config.json to arm.")
            return
        log.info("PUBLISHING SOLVE -> %s/command = SKULLS_SOLVED", self.base)
        self.publish("command", "SKULLS_SOLVED", qos=1)

    def close(self):
        if self.client:
            try:
                self.publish("vision/status", "OFFLINE", retain=True, qos=1)
                time.sleep(0.2)
                self.client.loop_stop()
                self.client.disconnect()
            except Exception:
                pass


class Detector:
    """Ultralytics YOLO, person class only."""

    PERSON_CLASS = 0

    def __init__(self, cfg):
        try:
            from ultralytics import YOLO
        except ImportError:
            sys.exit(
                "ultralytics is not installed. Run: pip install ultralytics\n"
                "First run downloads the model file - do that before game day, it needs internet."
            )
        log.info("Loading model %s on %s", cfg["model"], cfg["device"])
        self.model = YOLO(cfg["model"])
        self.device = cfg["device"]
        self.conf = float(cfg["confidence"])
        self.imgsz = int(cfg["imgsz"])

    def people(self, frame) -> List[Tuple[float, float, float, float]]:
        results = self.model.predict(
            frame,
            classes=[self.PERSON_CLASS],
            conf=self.conf,
            imgsz=self.imgsz,
            device=self.device,
            verbose=False,
        )
        boxes = []
        for r in results:
            if r.boxes is None:
                continue
            for b in r.boxes.xyxy.tolist():
                boxes.append((float(b[0]), float(b[1]), float(b[2]), float(b[3])))
        return boxes


class Stream:
    """RTSP reader that reconnects on its own. Reolink streams drop; the room
    should not need a human to restart this between groups.

    Frames are pulled on a background thread that always keeps only the
    newest one. The camera pushes ~15fps but the detect loop runs at
    target_fps; reading inline lets the ffmpeg buffer grow without bound,
    so occupancy lags further behind live the longer the process runs.
    """

    def __init__(self, cfg):
        import cv2
        import threading
        self.cv2 = cv2
        self.url = cfg["rtsp_url"]
        self.reconnect_delay = float(cfg.get("reconnect_delay_s", 3.0))
        self.cap = None
        self.fail_count = 0
        self._lock = threading.Lock()
        self._latest = None
        self._latest_seq = 0
        self._returned_seq = 0
        self._stop = False
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        self._thread.start()

    def open(self) -> bool:
        if self.cap is not None:
            self.cap.release()
        self.cap = self.cv2.VideoCapture(self.url, self.cv2.CAP_FFMPEG)
        try:
            self.cap.set(self.cv2.CAP_PROP_BUFFERSIZE, 1)
        except Exception:
            pass
        ok = self.cap.isOpened()
        log.info("RTSP open: %s", "ok" if ok else "FAILED")
        return ok

    def _pump(self):
        while not self._stop:
            if self.cap is None or not self.cap.isOpened():
                if not self.open():
                    time.sleep(self.reconnect_delay)
                    continue
            ok, frame = self.cap.read()
            if not ok or frame is None:
                self.fail_count += 1
                if self.fail_count >= 15:
                    log.warning("Stream stalled, reconnecting.")
                    self.fail_count = 0
                    self.open()
                    time.sleep(self.reconnect_delay)
                continue
            self.fail_count = 0
            with self._lock:
                self._latest = frame
                self._latest_seq += 1

    def read(self):
        """Newest frame if one arrived since the last call, else None."""
        with self._lock:
            if self._latest is None or self._latest_seq == self._returned_seq:
                return None
            self._returned_seq = self._latest_seq
            return self._latest

    def close(self):
        self._stop = True
        if self._thread.is_alive():
            self._thread.join(timeout=2.0)
        if self.cap is not None:
            self.cap.release()


def draw_preview(cv2, frame, zones, occupancy, boxes, result):
    import numpy as np
    img = frame.copy()
    for z in zones:
        pts = np.array([[int(x), int(y)] for x, y in z.polygon], dtype=np.int32)
        on = occupancy.get(z.id, False)
        color = (90, 230, 120) if on else (90, 90, 220)
        overlay = img.copy()
        cv2.fillPoly(overlay, [pts], color)
        cv2.addWeighted(overlay, 0.25, img, 0.75, 0, img)
        cv2.polylines(img, [pts], True, color, 2)
        cx = int(sum(p[0] for p in z.polygon) / len(z.polygon))
        cy = int(sum(p[1] for p in z.polygon) / len(z.polygon))
        cv2.putText(img, f"{z.name}: {'ON' if on else 'off'}", (cx - 50, cy),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2, cv2.LINE_AA)

    for (x1, y1, x2, y2) in boxes:
        cv2.rectangle(img, (int(x1), int(y1)), (int(x2), int(y2)), (255, 220, 120), 2)
        fx, fy = zone_logic.foot_point((x1, y1, x2, y2))
        cv2.circle(img, (int(fx), int(fy)), 5, (0, 0, 255), -1)

    hud = f"people={result.people_count}  all={result.all_occupied}  hold={result.hold_progress}"
    cv2.putText(img, hud, (12, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(img, hud, (12, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1, cv2.LINE_AA)
    return img


def selftest():
    """No camera, no broker. Feeds synthetic detections through the real logic
    so you can confirm the solve rules before wiring anything up."""
    zones = [
        zone_logic.Zone(0, "Skull0", [(0, 0), (100, 0), (100, 100), (0, 100)]),
        zone_logic.Zone(1, "Skull1", [(200, 0), (300, 0), (300, 100), (200, 100)]),
    ]
    p = zone_logic.SkullPuzzle(zones=zones, enabled=[0, 1], occupy_frames=3,
                               release_frames=5, hold_frames=4, min_people=2)

    at0 = (40, 20, 60, 90)     # foot point (50, 90) -> inside zone 0
    at1 = (240, 20, 260, 90)   # foot point (250, 90) -> inside zone 1

    print("empty room:")
    for _ in range(5):
        r = p.update([])
    print(f"  occupancy={r.occupancy} solved={r.solved_now}")
    assert not r.all_occupied

    print("one guest at skull 0 only:")
    for _ in range(10):
        r = p.update([at0])
    print(f"  occupancy={r.occupancy} solved={r.solved_now}")
    assert r.occupancy[0] and not r.occupancy[1]
    assert not r.solved_now, "must not solve with one skull"

    print("both guests in place:")
    fired = []
    for i in range(12):
        r = p.update([at0, at1])
        if r.solved_now:
            fired.append(i)
    print(f"  occupancy={r.occupancy} solve fired on frame(s) {fired}")
    assert len(fired) == 1, "solve must fire exactly once, not every frame"

    print("dropout tolerance:")
    p.reset()
    for _ in range(3):
        p.update([at0, at1])
    r = p.update([at0])              # skull 1 misses one frame
    assert r.occupancy[1], "a single dropped frame must not release a zone"
    print("  single dropped frame did not release the zone")

    print("head-count (solve target):")
    FPS = 8.0
    pc = zone_logic.PeopleCounter(up_frames=4, hold_s=10.0)
    t = [0.0]

    def feed(raw, frames, floor=0):
        c = None
        for _ in range(frames):
            t[0] += 1.0 / FPS
            c = pc.update(raw, floor=floor, now=t[0])
        return c

    assert feed(2, 3) == 0, "3 frames of 2 (phantom) must not latch"
    assert feed(2, 1) == 2, "4th consecutive frame of 2 latches"
    assert feed(1, 8) == 2, "one person missed for a second must not drop the count"
    assert feed(0, int(9 * FPS)) == 2, "count holds for hold_s after the last confirmed run"
    assert feed(0, int(2 * FPS)) == 0, "count falls once hold_s has passed with nobody seen"
    print("  phantom rejected, real 2 latched, held 10 s through dropouts, then released")

    # Replay of the 2026-09-13 19:32:48-19:33:10 wire log: ONE person at Skull0,
    # raw count flipping 0/1/2. (value, seconds) runs. The old symmetric
    # 12-frame debounce never latched and People sat at 0 for the whole test.
    replay = [(1, .5), (0, .25), (1, .65), (0, .25), (1, .9), (2, .12), (1, 1.4), (0, .5),
              (1, .27), (0, .12), (1, .12), (0, .4), (1, .5), (0, .12), (1, .38), (0, .12),
              (1, .6), (0, .12), (1, .15), (2, .15), (0, 1.7), (1, .65), (2, .27), (1, .5),
              (2, .13), (1, 1.5), (0, .84), (2, .25), (1, .43), (0, 1.1), (1, .27), (0, 1.0),
              (1, .55), (0, .14), (1, .33), (0, .42), (1, .12), (0, .9), (1, .13), (0, .37)]
    pc.reset(); t[0] = 0.0
    seen = []
    for val, secs in replay:
        for _ in range(max(1, int(round(secs * FPS)))):
            t[0] += 1.0 / FPS
            seen.append(pc.update(val, now=t[0]))
    first_one = seen.index(1)
    assert first_one <= 8, f"one person must latch within a second, took {first_one} frames"
    assert set(seen[first_one:]) == {1}, f"after latching, count must stay 1: {sorted(set(seen[first_one:]))}"
    print(f"  09-13 flicker replay: People latched 1 at frame {first_one} and never left it")

    print("zone floor:")
    pc.reset(); t[0] = 0.0
    assert feed(0, 4, floor=1) == 1, "an occupied skull zone is a person even if the count says 0"
    assert feed(0, 4, floor=0) == 0, "floor is not sticky on its own"
    print("  occupied zone raises the count; released zone lets it fall")

    print("\nAll logic checks passed.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="config.json")
    ap.add_argument("--dry-run", action="store_true", help="never publish SOLVE")
    ap.add_argument("--preview", action="store_true", help="show a debug window")
    ap.add_argument("--selftest", action="store_true", help="logic check, no hardware")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.selftest:
        selftest()
        return

    cfg = config_loader.load(args.config)
    zdata = config_loader.load_zones(cfg["zones"]["file"])

    log_file = cfg["runtime"].get("log_file")
    if log_file:
        fh = logging.FileHandler(log_file)
        fh.setFormatter(logging.Formatter("%(asctime)s %(levelname)-7s %(message)s"))
        logging.getLogger().addHandler(fh)

    dry_run = args.dry_run or bool(cfg["runtime"].get("dry_run"))
    preview = args.preview or bool(cfg["runtime"].get("preview"))
    if dry_run:
        log.warning("DRY RUN - occupancy will publish, SOLVE will not.")

    signal.signal(signal.SIGINT, _stop)
    signal.signal(signal.SIGTERM, _stop)

    stream = Stream(cfg["camera"])
    stream.start()
    cv2 = stream.cv2
    detector = Detector(cfg["detector"])
    mqtt_pub = MqttPublisher(cfg["mqtt"], dry_run)
    mqtt_pub.connect()

    first = None
    while _running and first is None:
        first = stream.read()
        if first is None:
            time.sleep(0.2)
    if first is None:
        stream.close()
        mqtt_pub.close()
        return

    fh_, fw_ = first.shape[:2]
    log.info("Live frame is %dx%d", fw_, fh_)
    zones = zone_logic.zones_from_json(zdata, scale_to=(fw_, fh_))
    if (zdata.get("frame_width"), zdata.get("frame_height")) != (fw_, fh_):
        log.warning("Zones were calibrated at %sx%s, stream is %sx%s - rescaled. "
                    "Recalibrate if detection looks off.",
                    zdata.get("frame_width"), zdata.get("frame_height"), fw_, fh_)

    pz = cfg["puzzle"]
    puzzle = zone_logic.SkullPuzzle(
        zones=zones,
        enabled=list(pz["enabled_skulls"]),
        occupy_frames=int(pz["occupy_frames"]),
        release_frames=int(pz["release_frames"]),
        hold_frames=int(pz["hold_frames"]),
        min_people=int(pz.get("min_people", 1)),
    )

    people = zone_logic.PeopleCounter(up_frames=int(pz.get("people_up_frames", 4)),
                                      hold_s=float(pz.get("people_hold_s", 10.0)))
    republish_s = float(pz.get("republish_s", 60.0))
    puzzle_occupancy = {}

    def publish_retained_state():
        # Everything the Mega needs to solve, re-sent so a wiped broker
        # (retained sweeper / broker reboot) or a rebooted ESP catches up.
        mqtt_pub.publish("system/People", str(people.count), retain=True)
        for z in zones:
            mqtt_pub.publish(f"system/{z.name}",
                             "Occupied" if puzzle_occupancy.get(z.id) else "Empty",
                             retain=True)

    publish_retained_state()
    last_republish = time.time()

    target_fps = float(cfg["detector"].get("target_fps", 8)) or 8.0
    frame_budget = 1.0 / target_fps
    cooldown = float(pz.get("solve_cooldown_s", 300.0))
    last_solve_at: Optional[float] = None
    last_summary = None

    log.info("Watching %d zones, enabled: %s; head-count confirms after %d frames, holds %.0fs, "
             "floor = occupied skulls", len(zones), puzzle.enabled, people.up_frames, people.hold_s)
    raw_tally = {}
    last_tally_log = time.time()

    while _running:
        loop_start = time.time()

        frame = stream.read()
        if frame is None:
            time.sleep(0.02)  # no new frame yet; don't spin the CPU
            continue

        try:
            boxes = detector.people(frame)
        except Exception as e:
            log.error("Detector error: %s", e)
            time.sleep(0.5)
            continue

        result = puzzle.update(boxes)
        puzzle_occupancy = result.occupancy

        for zid, now_on in result.changed.items():
            name = next(z.name for z in zones if z.id == zid)
            log.info("%s -> %s", name, "OCCUPIED" if now_on else "EMPTY")
            mqtt_pub.publish(f"system/{name}", "Occupied" if now_on else "Empty", retain=True)

        occupied_n = sum(1 for zid in puzzle.enabled if result.occupancy.get(zid))
        before = people.count
        if people.update(result.people_count, floor=occupied_n) != before:
            log.info("People -> %d (camera %d, occupied skulls %d; solve target: %d skull%s touched)",
                     people.count, people.camera_count, occupied_n,
                     people.count, "" if people.count == 1 else "s")
            mqtt_pub.publish("system/People", str(people.count), retain=True)

        # Raw-count tally every 5 s while anything is going on, so the next
        # "why didn't it solve" can be answered from this log alone.
        raw_tally[result.people_count] = raw_tally.get(result.people_count, 0) + 1
        if (time.time() - last_tally_log) >= 5.0:
            if people.count or occupied_n or any(raw_tally):
                log.info("raw people last 5s: %s -> People %d (camera %d, occupied %d)",
                         " ".join(f"{k}x{raw_tally[k]}" for k in sorted(raw_tally)),
                         people.count, people.camera_count, occupied_n)
            raw_tally = {}
            last_tally_log = time.time()

        if (time.time() - last_republish) >= republish_s:
            publish_retained_state()
            last_republish = time.time()

        summary = {
            "people": result.people_count,
            "players": people.count,
            "occupied": [z.name for z in zones if result.occupancy.get(z.id)],
            "all": result.all_occupied,
            "solved": puzzle.solved,
        }
        if summary != last_summary:
            mqtt_pub.publish("vision/summary", json.dumps(summary))
            last_summary = summary

        if result.solved_now:
            log.info("SOLVE CONDITION MET - %d people, all skulls held for %d frames",
                     result.people_count, result.hold_progress)
            mqtt_pub.publish_solve()
            last_solve_at = time.time()

        if last_solve_at and (time.time() - last_solve_at) >= cooldown:
            log.info("Cooldown elapsed - resetting for the next group.")
            puzzle.reset()
            last_solve_at = None
            puzzle_occupancy = {}
            publish_retained_state()

        if preview:
            cv2.imshow("SkullVision", draw_preview(cv2, frame, zones,
                                                   result.occupancy, boxes, result))
            if (cv2.waitKey(1) & 0xFF) in (27, ord("q")):
                break

        elapsed = time.time() - loop_start
        if elapsed < frame_budget:
            time.sleep(frame_budget - elapsed)

    log.info("Stopping.")
    stream.close()
    mqtt_pub.close()
    if preview:
        try:
            cv2.destroyAllWindows()
        except Exception:
            pass


if __name__ == "__main__":
    main()
