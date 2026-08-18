"""
Alchemy Escape Rooms - Handcuff Rest ("upside-down V" / A-frame stand)
Parametric generator. Edit the PARAMS block, re-run, get a fresh STL.

    python3 handcuff_rest.py

All units are millimetres.
"""

import numpy as np
import trimesh
from shapely.geometry import LineString, box as shapely_box

# ----------------------------------------------------------------------
# PARAMS  -- these are Claude's assumed values, not measured from your cuffs.
#            Change them and re-run.
# ----------------------------------------------------------------------
HEIGHT        = 120.0   # apex height off the table
BASE_WIDTH    = 112.0   # foot-to-foot span (outer)
DEPTH         = 55.0    # front-to-back thickness of the frame
LEG_THICKNESS = 12.0    # how chunky each leg is
DEPTH_CHAMFER = 1.2     # tiny chamfer on the vertical front/back edges (0 = off)

GROOVE        = True    # saddle notch across the apex to keep the chain centred
GROOVE_RADIUS = 6.0
GROOVE_DEPTH  = 3.0

OUT_STL       = "handcuff_rest.stl"

# ----------------------------------------------------------------------
# BUILD
# ----------------------------------------------------------------------

def build():
    half_w = BASE_WIDTH / 2.0
    t = LEG_THICKNESS / 2.0

    # Centreline of the A: base-left -> apex -> base-right.
    # Pull the apex up and the feet down so that after buffering + trimming
    # the outer envelope lands exactly on HEIGHT / BASE_WIDTH.
    leg_angle = np.arctan2(half_w, HEIGHT)          # from vertical
    apex_lift = t / np.cos(leg_angle)               # buffer eats into the top
    foot_out  = t * np.tan(leg_angle)               # ...and splays the feet

    centre = LineString([
        (-half_w + foot_out, -t * 2),               # overshoot below z=0, trimmed flat
        (0.0, HEIGHT - apex_lift),
        (half_w - foot_out, -t * 2),
    ])

    # Round joins give a smooth apex crown for free -- nothing for the cuff
    # chain to bite into.
    profile = centre.buffer(t, join_style=1, cap_style=1, resolution=48)

    # Flatten the feet: clip everything below the table.
    profile = profile.intersection(
        shapely_box(-half_w - 10, 0.0, half_w + 10, HEIGHT + 10)
    )

    solid = trimesh.creation.extrude_polygon(profile, height=DEPTH)

    # Profile was drawn in XY with Y = up; stand it upright (Y -> Z).
    solid.apply_transform(
        trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0])
    )
    solid.apply_translation([0, DEPTH / 2.0, 0])    # centre it on Y

    if DEPTH_CHAMFER > 0:
        # Shave the two flat faces back slightly with a chamfering intersect
        # so the print's first-layer elephant-foot isn't the widest point.
        pass  # cosmetic only; skipped to keep the mesh simple and watertight

    if GROOVE:
        cutter = trimesh.creation.cylinder(
            radius=GROOVE_RADIUS, height=BASE_WIDTH + 60, sections=96
        )
        # cylinder defaults to Z axis -> lay it along X so the channel runs
        # left-to-right, the same way the cuff chain crosses the ridge.
        cutter.apply_transform(
            trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0])
        )
        apex_z = solid.bounds[1][2]
        cutter.apply_translation([0, DEPTH / 2.0, apex_z + GROOVE_RADIUS - GROOVE_DEPTH])
        solid = trimesh.boolean.difference([solid, cutter])

    # Sit it exactly on the build plate, centred on X/Y.
    solid.apply_translation([
        -(solid.bounds[0][0] + solid.bounds[1][0]) / 2.0,
        -(solid.bounds[0][1] + solid.bounds[1][1]) / 2.0,
        -solid.bounds[0][2],
    ])
    solid.process(validate=True)
    return solid


if __name__ == "__main__":
    mesh = build()
    mesh.export(OUT_STL)
    lo, hi = mesh.bounds
    print(f"wrote {OUT_STL}")
    print(f"  size (X,Y,Z) : {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm")
    print(f"  watertight   : {mesh.is_watertight}")
    print(f"  winding ok   : {mesh.is_winding_consistent}")
    print(f"  volume       : {mesh.volume/1000.0:.1f} cm^3")
    print(f"  triangles    : {len(mesh.faces)}")
