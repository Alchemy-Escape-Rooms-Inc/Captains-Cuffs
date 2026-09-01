"""Shared config loading. Strips _comment keys and validates the parts that
will otherwise fail at 3am with a confusing traceback."""

import json
import sys
from pathlib import Path

REQUIRED = {
    "camera": ["rtsp_url"],
    "mqtt": ["host", "port", "base_topic"],
    "detector": ["model", "confidence"],
    "puzzle": ["enabled_skulls", "occupy_frames", "release_frames", "hold_frames"],
    "zones": ["file"],
}


def _strip_comments(obj):
    if isinstance(obj, dict):
        return {k: _strip_comments(v) for k, v in obj.items() if not k.startswith("_comment")}
    if isinstance(obj, list):
        return [_strip_comments(v) for v in obj]
    return obj


def load(path="config.json"):
    p = Path(path)
    if not p.exists():
        sys.exit(
            f"{p} not found.\n"
            "Copy config.example.json to config.json and fill in the camera URL and MQTT host."
        )
    try:
        cfg = _strip_comments(json.loads(p.read_text()))
    except json.JSONDecodeError as e:
        sys.exit(f"{p} is not valid JSON: {e}")

    for section, keys in REQUIRED.items():
        if section not in cfg:
            sys.exit(f"config.json is missing the '{section}' section.")
        for k in keys:
            if k not in cfg[section]:
                sys.exit(f"config.json: '{section}' is missing '{k}'.")

    if "PASSWORD" in cfg["camera"]["rtsp_url"] or "XXX" in cfg["camera"]["rtsp_url"]:
        sys.exit("config.json still has the placeholder RTSP URL. Fill in the real camera address.")

    cfg.setdefault("runtime", {})
    cfg["runtime"].setdefault("dry_run", True)
    cfg["runtime"].setdefault("preview", False)
    cfg["runtime"].setdefault("log_file", "skullvision.log")
    cfg["puzzle"].setdefault("min_people", 1)
    cfg["puzzle"].setdefault("require_distinct_person_per_skull", True)
    cfg["puzzle"].setdefault("solve_cooldown_s", 300.0)
    cfg["detector"].setdefault("device", "cpu")
    cfg["detector"].setdefault("imgsz", 640)
    cfg["detector"].setdefault("target_fps", 8)
    cfg["camera"].setdefault("reconnect_delay_s", 3.0)
    cfg["mqtt"].setdefault("client_id", "SkullVision")
    cfg["mqtt"].setdefault("username", None)
    cfg["mqtt"].setdefault("password", None)
    return cfg


def load_zones(path):
    p = Path(path)
    if not p.exists():
        sys.exit(f"{p} not found. Run calibrate_zones.py first.")
    data = json.loads(p.read_text())
    if not data.get("zones"):
        sys.exit(f"{p} has no zones in it.")
    return data
