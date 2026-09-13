"""
Pure puzzle logic - no camera, no MQTT, no OpenCV.

Kept separate on purpose so it can be tested on a laptop with fake detections
before it ever runs on the room PC. See test_logic.py.
"""

import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

Point = Tuple[float, float]


def point_in_polygon(pt: Point, polygon: Sequence[Point]) -> bool:
    """Ray casting. Points exactly on an edge are not guaranteed either way -
    that is fine, zones are drawn with margin."""
    x, y = pt
    inside = False
    n = len(polygon)
    j = n - 1
    for i in range(n):
        xi, yi = polygon[i]
        xj, yj = polygon[j]
        if (yi > y) != (yj > y):
            x_cross = (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi
            if x < x_cross:
                inside = not inside
        j = i
    return inside


def foot_point(box: Tuple[float, float, float, float]) -> Point:
    """Bottom-centre of a person box. Where they are STANDING, which is what
    the floor zones are drawn around. Using the box centre instead makes a
    tall person register in the wrong zone."""
    x1, y1, x2, y2 = box
    return ((x1 + x2) / 2.0, y2)


@dataclass
class Zone:
    id: int
    name: str
    polygon: List[Point]


@dataclass
class ZoneTracker:
    """Hysteresis for one zone. Asymmetric on purpose: latching ON is quick so
    the puzzle feels responsive, latching OFF is slow so one dropped frame of
    detection does not knock a guest out mid-solve."""
    occupy_frames: int
    release_frames: int
    occupied: bool = False
    _on_streak: int = 0
    _off_streak: int = 0

    def update(self, raw: bool) -> bool:
        """Feed one frame's raw observation, get the debounced state back."""
        if raw:
            self._on_streak += 1
            self._off_streak = 0
            if not self.occupied and self._on_streak >= self.occupy_frames:
                self.occupied = True
        else:
            self._off_streak += 1
            self._on_streak = 0
            if self.occupied and self._off_streak >= self.release_frames:
                self.occupied = False
        return self.occupied


@dataclass
class PeopleCounter:
    """Robust head-count for the Mega's solve target (touched skulls == people).

    The overhead fisheye is a poor view of a person leaning over a skull at
    the frame edge: with ONE person present the raw YOLO count flips
    0/1/2 several times a second (09-13 wire log: ~45% of frames "0", ~50%
    "1", ~5% "2"). A symmetric "N identical frames in a row" debounce never
    latched, so the head-count sat at 0 and nothing could solve.

    Rule: a count of v is CONFIRMED when the camera saw >= v people for
    `up_frames` consecutive frames; it stays believed for `hold_s` seconds
    after the last such run. The published count is the largest confirmed
    value still inside its hold window. So a dropped frame (or a whole
    second of dropped frames) cannot pull the count down, and a phantom
    that lasts 1-2 frames cannot push it up. `floor` (number of debounced
    occupied skull zones) is a hard minimum: an occupied zone IS a person.
    """
    up_frames: int = 4
    hold_s: float = 10.0
    max_people: int = 9
    count: int = 0
    _streak: List[int] = field(default_factory=list)
    _confirmed_at: List[Optional[float]] = field(default_factory=list)

    def __post_init__(self):
        self._streak = [0] * (self.max_people + 1)
        self._confirmed_at = [None] * (self.max_people + 1)

    def update(self, raw: int, floor: int = 0, now: Optional[float] = None) -> int:
        """Feed one frame's raw person count (plus the occupied-zone floor),
        get the debounced count back."""
        if now is None:
            now = time.monotonic()
        raw = max(0, min(int(raw), self.max_people))
        for v in range(1, self.max_people + 1):
            if raw >= v:
                self._streak[v] += 1
                if self._streak[v] >= self.up_frames:
                    self._confirmed_at[v] = now
            else:
                self._streak[v] = 0
        confirmed = 0
        for v in range(1, self.max_people + 1):
            t = self._confirmed_at[v]
            if t is not None and (now - t) <= self.hold_s:
                confirmed = v
        self.count = max(confirmed, max(0, min(int(floor), self.max_people)))
        return self.count

    @property
    def camera_count(self) -> int:
        """Largest camera-confirmed value still inside its hold window
        (ignores the zone floor) - for log lines."""
        now = time.monotonic()
        best = 0
        for v in range(1, self.max_people + 1):
            t = self._confirmed_at[v]
            if t is not None and (now - t) <= self.hold_s:
                best = v
        return best

    def reset(self):
        self.count = 0
        self._streak = [0] * (self.max_people + 1)
        self._confirmed_at = [None] * (self.max_people + 1)


@dataclass
class PuzzleResult:
    occupancy: Dict[int, bool]
    changed: Dict[int, bool]
    people_count: int
    all_occupied: bool
    solved_now: bool
    hold_progress: int


@dataclass
class SkullPuzzle:
    zones: List[Zone]
    enabled: List[int]
    occupy_frames: int
    release_frames: int
    hold_frames: int
    min_people: int = 1
    solved: bool = False
    _trackers: Dict[int, ZoneTracker] = field(default_factory=dict)
    _hold: int = 0

    def __post_init__(self):
        for z in self.zones:
            self._trackers[z.id] = ZoneTracker(self.occupy_frames, self.release_frames)

    def reset(self):
        self.solved = False
        self._hold = 0
        for t in self._trackers.values():
            t.occupied = False
            t._on_streak = 0
            t._off_streak = 0

    def assign(self, boxes: Sequence[Tuple[float, float, float, float]]) -> Dict[int, bool]:
        """Map detected people onto zones. Each person claims at most ONE zone,
        so a guest standing on a zone boundary cannot satisfy two skulls."""
        raw = {z.id: False for z in self.zones}
        claimed = set()
        for box in boxes:
            fp = foot_point(box)
            for z in self.zones:
                if z.id in claimed:
                    continue
                if point_in_polygon(fp, z.polygon):
                    raw[z.id] = True
                    claimed.add(z.id)
                    break
        return raw

    def update(self, boxes: Sequence[Tuple[float, float, float, float]]) -> PuzzleResult:
        raw = self.assign(boxes)

        occupancy: Dict[int, bool] = {}
        changed: Dict[int, bool] = {}
        for z in self.zones:
            t = self._trackers[z.id]
            before = t.occupied
            after = t.update(raw[z.id])
            occupancy[z.id] = after
            if after != before:
                changed[z.id] = after

        all_occupied = bool(self.enabled) and all(occupancy.get(i, False) for i in self.enabled)
        enough_people = len(boxes) >= self.min_people

        solved_now = False
        if all_occupied and enough_people:
            self._hold += 1
            if not self.solved and self._hold >= self.hold_frames:
                self.solved = True
                solved_now = True
        else:
            self._hold = 0

        return PuzzleResult(
            occupancy=occupancy,
            changed=changed,
            people_count=len(boxes),
            all_occupied=all_occupied,
            solved_now=solved_now,
            hold_progress=self._hold,
        )


def zones_from_json(data: dict, scale_to: Optional[Tuple[int, int]] = None) -> List[Zone]:
    """Build Zone objects, rescaling if the live stream resolution differs from
    what the zones were calibrated at (sub stream vs main stream mix-up)."""
    zones: List[Zone] = []
    sx = sy = 1.0
    if scale_to:
        cw = data.get("frame_width")
        ch = data.get("frame_height")
        if cw and ch:
            sx = scale_to[0] / float(cw)
            sy = scale_to[1] / float(ch)
    for z in data["zones"]:
        poly = [(float(x) * sx, float(y) * sy) for x, y in z["polygon"]]
        zones.append(Zone(id=int(z["id"]), name=z.get("name", f"Skull{z['id']}"), polygon=poly))
    return zones
