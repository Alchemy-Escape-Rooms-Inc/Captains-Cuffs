#!/usr/bin/env python3
"""
calibrate_zones.py - define the floor zone in front of each skull.

Run this ONCE per camera position, in the room, with the game lighting on.
If you bump the camera or change the lighting, run it again.

Usage:
    python calibrate_zones.py                 # pull a live frame from the camera
    python calibrate_zones.py --image f.jpg   # or calibrate from a saved still

Controls:
    left click      add a point to the current zone
    ENTER / n       finish this zone, start the next one
    u               undo last point
    r               restart the current zone
    b               go back and redo the previous zone
    s               save and quit
    ESC / q         quit without saving

Draw the zone as the patch of FLOOR a guest stands on to reach the skull,
not the skull itself. The detector tracks where feet are, not where hands are.
"""

import argparse
import json
import sys
from pathlib import Path

try:
    import cv2
except ImportError:
    sys.exit("opencv-python is not installed. Run: pip install opencv-python")

import config_loader

WINDOW = "Skull zone calibration"
COLORS = [
    (80, 200, 255), (120, 255, 140), (255, 160, 90),
    (200, 140, 255), (140, 220, 255), (255, 220, 120),
    (170, 255, 200), (255, 170, 200),
]


def grab_frame(rtsp_url: str):
    """Pull one frame off the RTSP stream. Retries a few times - first frame
    after connect is often garbage or missing."""
    cap = cv2.VideoCapture(rtsp_url, cv2.CAP_FFMPEG)
    if not cap.isOpened():
        sys.exit(
            "Could not open the RTSP stream.\n"
            "  - Check the URL, username and password in config.json\n"
            "  - Confirm the camera is reachable: ping its IP\n"
            "  - Try the sub stream path: /h264Preview_01_sub"
        )
    frame = None
    for _ in range(30):
        ok, f = cap.read()
        if ok and f is not None:
            frame = f
    cap.release()
    if frame is None:
        sys.exit("Connected to the camera but got no usable frame.")
    return frame


def draw(base, zones, current, cursor, skull_count):
    img = base.copy()

    for idx, poly in enumerate(zones):
        color = COLORS[idx % len(COLORS)]
        pts = [(int(x), int(y)) for x, y in poly]
        overlay = img.copy()
        cv2.fillPoly(overlay, [_np_pts(pts)], color)
        cv2.addWeighted(overlay, 0.25, img, 0.75, 0, img)
        cv2.polylines(img, [_np_pts(pts)], True, color, 2)
        cx = sum(p[0] for p in pts) // len(pts)
        cy = sum(p[1] for p in pts) // len(pts)
        _label(img, f"SKULL {idx}", (cx - 40, cy), color)

    color = COLORS[len(zones) % len(COLORS)]
    if current:
        pts = [(int(x), int(y)) for x, y in current]
        for p in pts:
            cv2.circle(img, p, 4, color, -1)
        if len(pts) > 1:
            cv2.polylines(img, [_np_pts(pts)], False, color, 2)
        if cursor and len(pts) >= 1:
            cv2.line(img, pts[-1], cursor, color, 1)

    header = f"Drawing SKULL {len(zones)}  |  {len(zones)} of {skull_count} placed"
    _label(img, header, (12, 26), (255, 255, 255))
    _label(img, "click=point  ENTER=next skull  u=undo  r=restart  b=back  s=save  q=quit",
           (12, 50), (200, 200, 200))
    return img


def _np_pts(pts):
    import numpy as np
    return np.array(pts, dtype=np.int32)


def _label(img, text, org, color):
    cv2.putText(img, text, org, cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(img, text, org, cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 1, cv2.LINE_AA)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="config.json")
    ap.add_argument("--image", help="calibrate from a saved image instead of the live camera")
    ap.add_argument("--out", default=None, help="output path (default: zones file from config)")
    args = ap.parse_args()

    cfg = config_loader.load(args.config)
    out_path = Path(args.out or cfg["zones"]["file"])
    skull_count = len(cfg["puzzle"]["enabled_skulls"])

    if args.image:
        frame = cv2.imread(args.image)
        if frame is None:
            sys.exit(f"Could not read image: {args.image}")
    else:
        print("Grabbing a frame from the camera...")
        frame = grab_frame(cfg["camera"]["rtsp_url"])

    h, w = frame.shape[:2]
    print(f"Frame is {w}x{h}. Draw {skull_count} zones, one per skull, in order 0..{skull_count - 1}.")

    zones = []
    current = []
    state = {"cursor": None}

    def on_mouse(event, x, y, flags, param):
        if event == cv2.EVENT_MOUSEMOVE:
            state["cursor"] = (x, y)
        elif event == cv2.EVENT_LBUTTONDOWN:
            current.append((x, y))

    cv2.namedWindow(WINDOW, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(WINDOW, min(w, 1280), min(h, 720))
    cv2.setMouseCallback(WINDOW, on_mouse)

    while True:
        cv2.imshow(WINDOW, draw(frame, zones, current, state["cursor"], skull_count))
        key = cv2.waitKey(20) & 0xFF

        if key in (13, ord("n")):                      # ENTER or n
            if len(current) < 3:
                print("A zone needs at least 3 points.")
                continue
            zones.append(list(current))
            current = []
            print(f"Skull {len(zones) - 1} placed.")
            if len(zones) == skull_count:
                print("All zones placed. Press 's' to save.")
        elif key == ord("u"):
            if current:
                current.pop()
        elif key == ord("r"):
            current = []
        elif key == ord("b"):
            if current:
                current = []
            elif zones:
                current = zones.pop()
                print(f"Editing skull {len(zones)} again.")
        elif key == ord("s"):
            if len(current) >= 3:
                zones.append(list(current))
                current = []
            if not zones:
                print("Nothing to save.")
                continue
            payload = {
                "frame_width": w,
                "frame_height": h,
                "zones": [
                    {"id": i, "name": f"Skull{i}", "polygon": [[int(x), int(y)] for x, y in poly]}
                    for i, poly in enumerate(zones)
                ],
            }
            out_path.write_text(json.dumps(payload, indent=2))
            print(f"Saved {len(zones)} zones to {out_path.resolve()}")
            break
        elif key in (27, ord("q")):
            print("Quit without saving.")
            break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
