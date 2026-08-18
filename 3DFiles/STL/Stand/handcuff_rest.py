"""
Alchemy Escape Rooms - Captain's Cuffs handcuff rest ("upside-down V" / A-frame)
Parametric generator. Edit the PARAMS block, re-run, get a fresh STL.

    python3 handcuff_rest.py

Mounts flat against a wall (the back face is a true flat plane) or free-stands
on the flat feet. All units are millimetres.
"""

import numpy as np
import trimesh
from shapely.geometry import LineString, box as shapely_box

# ----------------------------------------------------------------------
# PARAMS  -- assumed values, NOT measured against the built cuffs.
# ----------------------------------------------------------------------
HEIGHT        = 120.0   # apex height off the table / bottom of the bracket
BASE_WIDTH    = 180.0   # foot-to-foot span -- wider stance, shallower V
DEPTH         = 80.0    # how far the frame stands off the wall
LEG_THICKNESS = 24.0    # how chunky each leg is

GROOVE        = True    # saddle notch across the apex to keep the chain centred
GROOVE_RADIUS = 7.0
GROOVE_DEPTH  = 3.5

# --- retaining lip -----------------------------------------------------
LIP           = True    # raised rim on the FRONT face so cuffs can't slide off
LIP_PROUD     = 8.0     # how far the rim stands above the leg/apex surface
LIP_THICKNESS = 5.0     # how thick the rim is, front-to-back

# --- wall mounting -----------------------------------------------------
SCREW_HOLES   = True
SCREW_CLEAR_D = 4.5     # through-hole: clearance for a #8 / M4 screw shank
SCREW_HEAD_D  = 9.0     # counterbore so the head sits recessed, not proud
BACK_WALL     = 8.0     # solid material left against the wall face
# Heights (above the base) at which each leg gets a screw -> 4 screws total.
# The high pair matters most: hanging weight tries to lever the top off the wall.
SCREW_HEIGHTS = [22.0, 75.0]

OUT_STL       = "handcuff_rest.stl"

# ----------------------------------------------------------------------


def _cyl_along_y(radius, length, centre, sections=64):
    """Cylinder whose axis runs front-to-back (the Y axis)."""
    c = trimesh.creation.cylinder(radius=radius, height=length, sections=sections)
    c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
    c.apply_translation(centre)
    return c


def build():
    half_w = BASE_WIDTH / 2.0
    t = LEG_THICKNESS / 2.0

    # Centreline of the A: base-left -> apex -> base-right. Pull the apex up
    # and the feet in so the buffered outer envelope lands on HEIGHT/BASE_WIDTH.
    leg_angle = np.arctan2(half_w, HEIGHT)          # from vertical
    apex_lift = t / np.cos(leg_angle)
    foot_out = t * np.tan(leg_angle)

    # The true centreline the profile is built from. The foot end overshoots
    # below z=0 so the feet can be trimmed flat; screw positions MUST be
    # interpolated along this exact line or the bores drift off-centre and
    # break out through the side of the leg.
    foot_l = np.array([-half_w + foot_out, -t * 2])
    apex = np.array([0.0, HEIGHT - apex_lift])

    centre_line = LineString([
        (foot_l[0], foot_l[1]),
        (apex[0], apex[1]),
        (-foot_l[0], foot_l[1]),
    ])

    # Round joins give a smooth apex crown for free -- nothing for the cuff
    # chain to bite into.
    profile = centre_line.buffer(t, join_style=1, cap_style=1, resolution=48)
    profile = profile.intersection(
        shapely_box(-half_w - 10, 0.0, half_w + 10, HEIGHT + 10)
    )

    solid = trimesh.creation.extrude_polygon(profile, height=DEPTH)

    if LIP:
        # A flange that follows the whole A silhouette, standing proud of the
        # top and outer faces. Sits on the front face; the wall is the backstop.
        lip_profile = profile.buffer(LIP_PROUD, join_style=1, resolution=48)
        lip_profile = lip_profile.intersection(
            shapely_box(-half_w - 40, 0.0, half_w + 40, HEIGHT + 40)
        )
        # NOTE: the X-rotation below maps low z -> the front face, so the lip
        # sits at z = 0 here, not z = DEPTH.
        lip = trimesh.creation.extrude_polygon(lip_profile, height=LIP_THICKNESS)
        solid = trimesh.boolean.union([solid, lip])

    # Profile drawn in XY with Y = up; stand it upright so Y -> Z.
    solid.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))

    # Normalise: centred on X and Y, sitting on z = 0.
    # Wall face is y = -DEPTH/2, front face is y = +DEPTH/2.
    solid.apply_translation([0, DEPTH / 2.0, -solid.bounds[0][2]])
    solid.apply_translation([
        -(solid.bounds[0][0] + solid.bounds[1][0]) / 2.0,
        -(solid.bounds[0][1] + solid.bounds[1][1]) / 2.0,
        0,
    ])

    apex_z = solid.bounds[1][2]
    y_wall, y_front = -DEPTH / 2.0, DEPTH / 2.0

    cutters = []

    if GROOVE:
        # Runs left-to-right across the ridge, the way the chain crosses it.
        g = trimesh.creation.cylinder(radius=GROOVE_RADIUS, height=BASE_WIDTH + 60,
                                      sections=96)
        g.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0]))
        g.apply_translation([0, 0, apex_z + GROOVE_RADIUS - GROOVE_DEPTH])
        cutters.append(g)

    if SCREW_HOLES:
        cb_top = y_front + 5.0                       # overshoot past the front face
        cb_bottom = y_wall + BACK_WALL               # leave BACK_WALL solid at the wall
        cb_len = cb_top - cb_bottom
        cb_mid = (cb_top + cb_bottom) / 2.0
        for z in SCREW_HEIGHTS:
            # Interpolate x along the real leg centreline at this height.
            frac = (z - foot_l[1]) / (apex[1] - foot_l[1])
            x_mag = abs(foot_l[0] + (apex[0] - foot_l[0]) * frac)
            for sx in (-1.0, 1.0):
                x = sx * x_mag
                # through-hole for the shank, full depth
                cutters.append(_cyl_along_y(SCREW_CLEAR_D / 2.0, DEPTH + 20, [x, 0, z]))
                # counterbore from the front face, stopping BACK_WALL short
                cutters.append(_cyl_along_y(
                    SCREW_HEAD_D / 2.0, cb_len, [x, cb_mid, z],
                ))

    if cutters:
        solid = trimesh.boolean.difference([solid] + cutters)

    solid.process(validate=True)
    return solid


if __name__ == "__main__":
    mesh = build()
    mesh.export(OUT_STL)
    lo, hi = mesh.bounds
    print(f"wrote {OUT_STL}")
    print(f"  size (W,D,H) : {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm")
    print(f"  watertight   : {mesh.is_watertight}")
    print(f"  winding ok   : {mesh.is_winding_consistent}")
    print(f"  volume       : {mesh.volume/1000.0:.1f} cm^3")
    print(f"  triangles    : {len(mesh.faces)}")
