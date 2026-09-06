# SkullVision

Camera-driven input for the Captain's Cuffs puzzle in Mermaid's Tale, replacing
the hall sensors that came out with the physical handcuffs.

Watches the Reolink feed, detects people, publishes the debounced head-count
(`system/People`) and per-skull occupancy over MQTT. **Solve rule since
firmware v1.5.0 (2026-09-06):** the Mega releases when the number of skull
touch sensors held at the same moment equals `system/People`. Which skulls
does not matter; more touches than players does not solve. `dry_run` stays
true — the legacy "all skulls held" solve command must never be armed.

---

## Read this before you install anything

This is the highest-risk option on the table for a room that opens in days.
Three things will decide whether it works, and none of them are the code:

1. **Light.** Escape rooms are dark. A person detector needs to see a person.
   If the Reolink flips to IR night mode you get monochrome infrared, and
   detection accuracy drops. Test at *actual game lighting*, not with the work
   lights on. If it fails there, that is the end of this approach — no amount
   of tuning fixes an unlit subject.
2. **Camera angle.** The zones are floor patches. A camera looking down the
   room from a low angle makes two guests standing in a line overlap into one
   detection. High and looking down is what you want.
3. **Occlusion.** Five people milling around in a small set will block each
   other. The hysteresis in the config buys you some tolerance; it cannot
   invent a person the camera cannot see.

Run this in `dry_run` mode alongside whatever else you build. Do not make it
the only way out of the room until you have watched it work through a full
group at game lighting.

**The touch sensors are still the safer primary.** The TTP223 boards already
work, they are already on the Mega's pins, and moving them into the skulls with
a brass contact plate is a wiring job, not a computer vision project. This
gives you something the touch sensors cannot: knowing *how many* guests are in
the room, so the puzzle scales to group size on its own. That is a real
upgrade — after you are open.

---

## Install

On the room PC (Windows is fine):

```
pip install ultralytics opencv-python paho-mqtt numpy
```

First run downloads the YOLO model file. **Do that on a day with internet, not
on game day.**

```
copy config.example.json config.json
```

Fill in:
- `camera.rtsp_url` — your Reolink address, user and password. Sub stream:
  `rtsp://user:pass@10.1.10.X:554/h264Preview_01_sub`
- `mqtt.host` — `10.1.10.115` (matches what the ESP is already pointed at)
- `puzzle.enabled_skulls` — one id per physical skull, numbered from 0

`config.json` holds the camera password. It is in `.gitignore`. Keep it there.

---

## Set up

**1. Check the logic works, before touching hardware:**

```
python skullvision.py --selftest
```

No camera, no broker. Feeds fake detections through the real solve rules and
asserts they behave.

**2. Draw the zones:**

```
python calibrate_zones.py
```

Grabs a live frame, you click a polygon around the floor patch in front of each
skull, in order. `ENTER` between zones, `s` to save. Draw where guests **stand**,
not where the skull is — the detector tracks feet.

Re-run this if you move the camera or change the lighting.

**3. Watch it with the safety on:**

```
python skullvision.py --dry-run --preview
```

Green zone = occupied, red = empty, red dot = the foot point being tested.
Walk the room. Watch whether zones latch cleanly or flicker.

**4. Arm it** by setting `dry_run: false` in `config.json`.

---

## Tuning

| Symptom | Change |
|---|---|
| Zones flicker on and off | raise `occupy_frames` and `release_frames` |
| Feels sluggish to respond | lower `occupy_frames` |
| A guest drops out mid-solve | raise `release_frames` (this is the important one) |
| Solves too easily on a fluke | raise `hold_frames` |
| Misses people in the dark | try `yolov8s.pt`, then fix the lighting |
| Running slow / high CPU | lower `target_fps` or `imgsz` to 480 |

`release_frames` should always be well above `occupy_frames`. Latch on fast,
let go slow — that way one dropped frame of detection does not kick a guest out
of a puzzle they are actively solving.

---

## MQTT topics

Base: `MermaidsTale/CaptainsCuffs`

**Published:**

| Topic | Payload | Retained |
|---|---|---|
| `system/People` | `0`..`9` debounced head-count = **the solve target** (Mega v1.5.0: touched skulls == this number, any skulls) | yes, re-sent every `republish_s` |
| `system/Skull<N>` | `Occupied` / `Empty` (status only since v1.5.0) | yes |
| `vision/status` | `ONLINE` / `OFFLINE` | yes (OFFLINE is the LWT) |
| `vision/summary` | JSON: people count, occupied list, solved | no |
| `command` | `SKULLS_SOLVED` | no |

The retained `Occupied`/`Empty` topics follow the same convention your ESP
already uses for `system/TouchSensor<N>` and `system/Cuff<N>`, so they should
drop straight into the GM dashboard.

`vision/status` is a last-will topic. If this process dies or the PC drops off
the network, the broker publishes `OFFLINE` on its own. **Put that on the GM
screen.** A vision node that quietly died looks exactly like a puzzle nobody
has solved yet, and your GM needs to be able to tell those apart.

**Not subscribed to anything.** This process only reports. The Mega still owns
every relay. That is deliberate — a camera should not be the thing that can
open a door on its own.

---

## Firmware

See `FIRMWARE_PATCH.md`. **This script does nothing until those changes are
made** — right now no MQTT command reaches the Mega at all, and the Mega hangs
at boot with the hall sensors removed. That boot hang is a game-day problem
whether or not you use this script.

---

## Files

| File | What it is |
|---|---|
| `skullvision.py` | the service; run this |
| `calibrate_zones.py` | click-to-draw the skull zones |
| `zone_logic.py` | pure puzzle logic, no hardware — where the solve rules live |
| `config_loader.py` | config validation |
| `config.example.json` | copy to `config.json` and fill in |
| `FIRMWARE_PATCH.md` | required ESP + Mega changes |
