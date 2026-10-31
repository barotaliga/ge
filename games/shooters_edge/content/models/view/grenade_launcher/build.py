# The grenade launcher's builder (held weapon 6): a six-shot revolver launcher. A big fluted drum
# turning on its axle beneath a short wide-bore barrel; the barrel's ribbed handguard and the
# flip-up ladder sight on the top strap; the frame round the drum; a pistol grip for the right hand
# and a vertical foregrip for the left; a folding tube stock; and a pouch of spare grenades on the
# left side (the side the player sees).
#
# THE CLIP `fire`: the kick, then the drum turns one chamber on (60 deg) in the 0.6 s between
# shots.
#
# ge:summary The grenade launcher's Blender builder: the revolving drum, barrel, sight, grips, clip
# ge:keywords grenade-launcher revolver drum six-shot ladder-sight foregrip builder blender clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from mathutils import Matrix  # noqa: E402

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "grenade_launcher")
kit.stance = 35.0

polymer = wk.surface("polymer", (0.050, 0.052, 0.048), mottle=0.15, mottle_scale=50,
                     pattern=("diamond", 1500.0, 0.25), wear=(0.14, 0.14, 0.13), wear_amount=0.4,
                     cavity=0.6)
olive = wk.surface("olive_drab", (0.090, 0.095, 0.045), mottle=0.3, mottle_scale=25,
                   wear=(0.30, 0.29, 0.24), wear_amount=0.5, chip_scale=70, cavity=0.65)
gunmetal = wk.surface("gunmetal", (0.11, 0.112, 0.118), mottle=0.3, mottle_scale=30, streak=0.12,
                      streak_axis="Y", wear=(0.45, 0.45, 0.47), wear_amount=0.5, chip_scale=90,
                      cavity=0.65)
dark = wk.surface("dark_steel", (0.040, 0.041, 0.045), mottle=0.2, wear=(0.32, 0.32, 0.34),
                  wear_amount=0.35, cavity=0.55)
canvas = wk.surface("canvas", (0.13, 0.12, 0.07), mottle=0.25, pattern=("diamond", 1800.0, 0.2),
                    wear=(0.22, 0.20, 0.13), wear_amount=0.4, cavity=0.7)
shell = wk.surface("grenade_shell", (0.11, 0.13, 0.06), mottle=0.2, wear=(0.30, 0.30, 0.20),
                   wear_amount=0.35, cavity=0.5)
band = wk.surface("grenade_band", (0.70, 0.55, 0.06), mottle=0.15, cavity=0.4)

BARREL_Z = 0.083
DRUM_Z = BARREL_Z - 0.036  # the drum's axle: its top chamber lines up with the barrel
DRUM_R = 0.066

# The barrel, its handguard ribs and muzzle; the top strap with the ladder sight.
kit.lathe("barrel", [(0.020, 0.135), (0.026, 0.135), (0.026, 0.430), (0.029, 0.434),
                     (0.029, 0.452), (0.020, 0.452)], (0, 0, BARREL_Z), gunmetal, sides=32)
for i in range(6):
    y = 0.180 + i * 0.038
    kit.tube("guard_rib%d" % i, 0.034, 0.026, 0.026, (0, y, BARREL_Z), polymer, sides=28,
             bevel=0.002)
kit.box("top_strap", (0.026, 0.260, 0.012), (0, 0.040, BARREL_Z + 0.034), gunmetal, bevel=0.002)
kit.box("sight_base", (0.030, 0.030, 0.010), (0, 0.120, BARREL_Z + 0.045), dark, bevel=0.0015)
kit.prism("sight_frame", [(0.105, BARREL_Z + 0.049), (0.115, BARREL_Z + 0.049),
                          (0.128, BARREL_Z + 0.098), (0.118, BARREL_Z + 0.098)], 0.026, dark,
          bevel=0.001)
kit.box("sight_aperture", (0.010, 0.006, 0.040), (0, 0.120, BARREL_Z + 0.075), polymer,
        bevel=0.0008, rot=(-0.25, 0, 0))

# The frame round the drum: the rear plate, the lower arm to the axle, the front plate.
kit.box("rear_frame", (0.060, 0.030, 0.110), (0, -0.030, 0.035), gunmetal, bevel=0.004)
kit.box("front_frame", (0.050, 0.020, 0.080), (0, 0.142, DRUM_Z + 0.010), gunmetal, bevel=0.003)
kit.box("lower_arm", (0.030, 0.190, 0.016), (0, 0.055, DRUM_Z - DRUM_R - 0.004), gunmetal,
        bevel=0.002)

# The grip and trigger, the foregrip under the barrel, the folding tube stock.
kit.loft("grip", [(-0.040, 0, -0.012, 0.0170, 0.022, 2.4), (-0.050, 0, -0.050, 0.0175, 0.022, 2.4),
                  (-0.062, 0, -0.090, 0.0172, 0.021, 2.4), (-0.070, 0, -0.114, 0.0160, 0.018, 2.4),
                  (-0.073, 0, -0.120, 0.0140, 0.014, 2.4)], polymer, sides=20)
kit.prism("guard", [(-0.030, -0.012), (0.035, -0.012), (0.033, -0.020), (0.024, -0.044),
                    (0.006, -0.054), (-0.016, -0.052), (-0.030, -0.038)], 0.014, dark,
          bevel=0.0014)
kit.prism("trigger", [(0.002, -0.012), (0.010, -0.012), (0.006, -0.028), (-0.002, -0.038),
                      (-0.006, -0.036), (0.000, -0.024)], 0.006, gunmetal, bevel=0.0008)
kit.box("grip_mount", (0.040, 0.050, 0.030), (0, -0.040, -0.004), gunmetal, bevel=0.003)
kit.loft("foregrip", [(z, 0, 0.250, 0.0175, 0.0200, 2.3) for z in (-0.050, -0.160)], polymer,
         sides=20, rot=(math.pi / 2, 0, 0))
kit.box("foregrip_clamp", (0.030, 0.040, 0.028), (0, 0.250, BARREL_Z - 0.040), dark, bevel=0.002)
for sx in (1, -1):
    kit.cyl("stock_tube_%d" % (sx + 1), 0.0075, 0.190, (sx * 0.020, -0.140, 0.036), dark,
            sides=12, bevel=0.0008)
kit.loft("stock_pad", [(y, 0, -0.004, 0.026, 0.056, 3.0) for y in (-0.235, -0.255)], polymer,
         sides=24, bevel=0.0)
kit.box("stock_hinge", (0.052, 0.020, 0.024), (0, -0.050, 0.040), dark, bevel=0.002)

# The pouch of spare grenades on the left.
kit.box("pouch", (0.022, 0.140, 0.050), (-0.044, -0.010, -0.010), canvas, bevel=0.004)
for i in range(4):
    y = -0.060 + i * 0.033
    kit.lathe("spare%d" % i, [(0, 0), (0.016, 0), (0.017, 0.006), (0.017, 0.026), (0.012, 0.040),
                              (0, 0.044)], (-0.044, y, 0.014), shell, axis="Z", sides=16,
              bevel=0.0)
    kit.tube("spare_band%d" % i, 0.0178, 0.0165, 0.004, (-0.044, y, 0.034), band, axis="Z",
             sides=16, bevel=0.0)

# The drum: six chambers round its axle, fluted between them, turning on each shot.
with kit.group("drum", pivot=(0, 0.065, DRUM_Z)):
    kit.cyl("drum", DRUM_R, 0.140, (0, 0.065, DRUM_Z), olive, sides=48, bevel=0.003)
    for c in range(6):
        a = 2 * math.pi * c / 6 + math.pi / 6
        x, z = (DRUM_R + 0.002) * math.cos(a), DRUM_Z + (DRUM_R + 0.002) * math.sin(a)
        kit.box("flute%d" % c, (0.014, 0.110, 0.014), (x, 0.065, z), dark, bevel=0.002,
                rot=(0, -a, 0))
        a2 = a - math.pi / 6
        kit.tube("chamber%d" % c, 0.024, 0.020, 0.008,
                 (0.036 * math.cos(a2), 0.137, DRUM_Z + 0.036 * math.sin(a2)), dark, sides=20,
                 bevel=0.0)
    kit.cyl("axle", 0.012, 0.170, (0, 0.065, DRUM_Z), dark, sides=16)

kit.hand("r", centre=(0, -0.058, -0.072), axis=(0, 0.30, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.250, BARREL_Z - 0.110), axis=(0, 0, 1), knuckles=(-1, 0.3, 0),
         grip_r=0.019, index_curl=1.0)

kit.clip("fire", {
    "body": [(0.00, (0, 0, 0), (0, 0, 0)), (0.05, (0, -0.014, 0.006), (0.07, 0, 0)),
             (0.20, (0, 0, 0), (0, 0, 0)), (0.58, (0, 0, 0), (0, 0, 0))],
    "drum": [(0.00, (0, 0, 0), (0, 0, 0)), (0.18, (0, 0, 0), (0, 0, 0)),
             (0.45, (0, 0, 0), (0, math.pi / 3, 0)), (0.58, (0, 0, 0), (0, math.pi / 3, 0))],
})

# Turned in a little, so the drum and the barrel's side show.
kit.finish("grenade_launcher", out, turn=Matrix.Rotation(0.07, 4, "Z"))
