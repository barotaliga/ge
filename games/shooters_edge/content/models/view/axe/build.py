# The axe's builder (held weapon 1): a bearded war axe. A curved ash haft swelling to a knob at
# the butt and wrapped in leather where the hand holds it; a forged head through its eye, the
# beard dropping below, the edge ground thin and polished, a square poll behind; steel langets
# riveted down the haft under the head, and the haft's wedged end grain on top. Built upright
# (+Z up, the edge toward -Y), then turned to how a hand holds it ready to chop: spun a little so
# the blade's face shows, leant forward and in toward the middle of the view, and turned round
# into the kit's frame (the edge toward +Y, away from the eye).
#
# THE CLIP `fire` is the chop, led by the arm: the axe and the arm turn together about the right
# shoulder, raised and drawn out to the right, then brought down and across to strike low in front
# of the view, and back to rest by the time the next blow may fall (refire 0.5 s); the body turns
# into the blow and carries them. The wrist stays as it holds; the elbow bends by IK.
#
# ge:summary The axe's Blender builder: haft, leather wrap, bearded head, langets, the chop clip
# ge:keywords shooters-edge axe melee builder blender bearded-axe haft leather langet chop clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from mathutils import Matrix  # noqa: E402

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "axe")
SIZE = 0.74  # drawn as a 0.75 m war axe, built as a 0.55 m hatchet
kit.size = SIZE

steel = wk.surface("forged_steel", (0.13, 0.13, 0.14), mottle=0.3, mottle_scale=25,
                   streak=0.18, streak_axis="Y", streak_scale=500, wear=(0.42, 0.42, 0.44),
                   wear_amount=0.55, chip_scale=80, cavity=0.6,
                   zone=("Y", -0.118, -0.142, (0.62, 0.62, 0.64)))
poll_steel = wk.surface("poll_steel", (0.15, 0.145, 0.145), mottle=0.35, mottle_scale=30,
                        wear=(0.45, 0.44, 0.44), wear_amount=0.7, chip_scale=70, cavity=0.6)
ash = wk.surface("ash", (0.42, 0.27, 0.13), grain=((0.15, 0.075, 0.028), 140.0, "Z"),
                 mottle=0.14, wear=(0.55, 0.40, 0.24), wear_amount=0.35, cavity=0.5)
end_grain = wk.surface("end_grain", (0.30, 0.18, 0.08), grain=((0.15, 0.08, 0.03), 140.0, "Z"),
                       mottle=0.2, cavity=0.4)
leather = wk.surface("leather", (0.085, 0.045, 0.025), mottle=0.25, mottle_scale=40,
                     streak=0.12, streak_axis="Z", wear=(0.19, 0.11, 0.06), wear_amount=0.45,
                     chip_scale=120, cavity=0.75)
brass = wk.surface("brass", (0.50, 0.33, 0.10), mottle=0.2, wear=(0.85, 0.68, 0.35),
                   wear_amount=0.6, cavity=0.5)

UP = (math.pi / 2, 0, 0)  # a loft along Y stood up along Z

# The haft: oval (deeper toward the edge), curved toward the blade at the butt, a knob below.
haft = []
for z, rx, ry, bend in ((0.000, 0.016, 0.020, 0.010), (0.006, 0.0185, 0.0235, 0.010),
                        (0.018, 0.0175, 0.022, 0.009), (0.035, 0.0150, 0.0195, 0.008),
                        (0.120, 0.0140, 0.0185, 0.005), (0.260, 0.0132, 0.0176, 0.002),
                        (0.420, 0.0130, 0.0172, 0.000), (0.560, 0.0136, 0.0180, 0.000),
                        (0.640, 0.0146, 0.0192, 0.000), (0.716, 0.0146, 0.0192, 0.000)):
    haft.append((z, 0.0, bend, rx, ry))
kit.loft("haft", haft, ash, sides=28, rot=UP)
kit.loft("end_grain", [(0.716, 0, 0, 0.0146, 0.0192), (0.719, 0, 0, 0.0128, 0.0174)], end_grain,
         sides=28, rot=UP)
kit.box("wedge", (0.003, 0.030, 0.006), (0, 0, 0.718), poll_steel, bevel=0.0005)

# The leather wrap where the hand holds: a ridged sleeve, a ridge a turn of the thong.
wrap = []
z, turns = 0.040, 22
for i in range(turns * 2 + 1):
    zz = 0.040 + (0.205 - 0.040) * i / (turns * 2)
    t = (zz - 0.035) / (0.120 - 0.035)
    rx = 0.0150 + (0.0140 - 0.0150) * min(1.0, t) if zz < 0.12 else 0.0140 - 0.00006 * (zz - 0.12) / 0.01
    ry = rx * 1.32
    grow = 0.0024 if i % 2 else 0.0010
    bend = 0.008 - 0.003 * min(1.0, (zz - 0.035) / 0.085)
    wrap.append((zz, 0.0, max(bend, 0.004), rx + grow, ry + grow))
kit.loft("wrap", wrap, leather, sides=28, rot=UP)
for zz in (0.034, 0.211):
    kit.loft("wrap_end_%d" % int(zz * 1000),
             [(zz - 0.004, 0, 0.006, 0.0172, 0.0222), (zz + 0.004, 0, 0.006, 0.0172, 0.0222)],
             leather, sides=28, bevel=0.0012, rot=UP)

# The head: a bearded blade seen from its side (Y forward-back, Z up), thinning to the edge.
head = [(0.046, 0.632), (0.052, 0.640), (0.052, 0.702), (0.046, 0.710), (0.020, 0.713),
        (-0.020, 0.713), (-0.060, 0.717), (-0.100, 0.727), (-0.132, 0.741), (-0.150, 0.745),
        (-0.158, 0.728), (-0.162, 0.690), (-0.162, 0.640), (-0.157, 0.592), (-0.146, 0.548),
        (-0.130, 0.514), (-0.114, 0.520), (-0.090, 0.546), (-0.064, 0.582), (-0.042, 0.612),
        (-0.026, 0.627), (0.020, 0.629)]


def head_width(y, z):
    if y > -0.028:
        return 0.040 if y < 0.030 else 0.034  # the eye's cheeks, then the poll
    k = min(1.0, (-0.028 - y) / (0.162 - 0.028))
    return 0.040 + (0.004 - 0.040) * k ** 0.8


kit.prism("head", head, 0.040, steel, bevel=0.0012, width_at=head_width)
kit.box("poll_face", (0.034, 0.004, 0.068), (0, 0.053, 0.671), poll_steel, bevel=0.0012)

# Langets down the haft front and back, riveted through.
for side, y in (("front", -0.0205), ("back", 0.0205)):
    kit.box("langet_" + side, (0.011, 0.003, 0.095), (0, y, 0.585), poll_steel, bevel=0.0007,
            taper=None)
    for zz in (0.556, 0.600):
        kit.screw("rivet_%s_%d" % (side, int(zz * 1000)), (0, y + (-0.0015 if y < 0 else 0.0015), zz),
                  brass, r=0.0032, axis="-Y" if y < 0 else "Y", depth=0.0018)
# The maker's rivet through the cheek, both sides.
for sx in (1, -1):
    kit.screw("cheek_rivet_%d" % (sx + 1), (sx * 0.0200, 0.000, 0.671), brass, r=0.0042,
              axis="X" if sx > 0 else "-X", depth=0.0016)

# Held ready: spun so the blade's face shows, leant forward and in toward the view's middle.
HOLD = (Matrix.Rotation(math.pi, 4, "Z") @ Matrix.Rotation(0.36, 4, "Y") @
        Matrix.Rotation(0.22, 4, "X") @ Matrix.Rotation(0.55, 4, "Z"))

# The right hand round the wrap, the knuckles out to the right and forward (the built frame: right
# is -X, away -Y, before HOLD turns it round); its elbow out to the right and down.
kit.hand("r", centre=(0.0, 0.006 * SIZE, 0.115 * SIZE), axis=(0, -0.08, 1),
         knuckles=(-1, -0.55, 0), grip_r=0.019 * SIZE, pole=(1.0, -0.3, -1.0))

# The chop, about the hand on the wrap.
# The axe swings with the arm about the right shoulder, and the body (the torso) carries both.
kit.groups["body"]["pivot"] = "shoulder_r"
kit.groups["body"]["parent"] = "torso"
kit.clip("fire", {
    # About the shoulder (x right, y away, z up): raised and drawn out to the right, then brought
    # down and across to the left to strike low in front, the elbow reaching through it, and back.
    "body": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.09, (0.00, -0.02, 0.01), (0.10, 0.04, -0.06)),
        (0.18, (0.00, 0.04, -0.01), (-0.17, -0.08, 0.22)),
        (0.25, (0.00, 0.04, -0.015), (-0.21, -0.10, 0.25)),
        (0.35, (0.00, 0.02, -0.01), (-0.09, -0.04, 0.11)),
        (0.48, (0, 0, 0), (0, 0, 0)),
    ],
    # The body turns into the blow: back to the right on the draw, round to the left and down
    # through the strike.
    "torso": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.09, (0.01, -0.02, 0.01), (0.02, 0, -0.10)),
        (0.20, (-0.02, 0.03, -0.02), (-0.06, 0, 0.14)),
        (0.32, (-0.01, 0.02, -0.01), (-0.03, 0, 0.07)),
        (0.48, (0, 0, 0), (0, 0, 0)),
    ],
})

kit.finish("axe", out, turn=HOLD)
