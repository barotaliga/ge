# The minigun's builder (held weapon 4, the nailgun's slot): a six-barrel rotary gun carried in
# both hands. Six barrels round a central axle, held by a middle and a front clamp, with a flash
# hider; a finned motor housing behind them, all ahead of the rear grip so the eye looks past them; a carry handle over the top; a rear pistol grip and its
# trigger for the right hand and a vertical foregrip for the left; an olive ammunition box on the
# left side (the side the player sees) feeding a belt of brass rounds through a chute into the
# receiver.
#
# THE CLIP `fire`: the barrel cluster turns one barrel on (60 deg) a shot, in the 0.1 s between
# shots, so the clip ends where it began and a held trigger spins it without a seam.
#
# ge:summary The minigun's Blender builder: six turning barrels, the motor, the belt, two grips
# ge:keywords minigun rotary six-barrel builder blender belt ammo-box foregrip spin clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "minigun")
kit.stance = 30.0  # carried bladed at the hip, the left hand forward on its foregrip

gunmetal = wk.surface("gunmetal", (0.11, 0.112, 0.118), mottle=0.3, mottle_scale=30, streak=0.12,
                      streak_axis="Y", wear=(0.40, 0.40, 0.42), wear_amount=0.45, chip_scale=90,
                      cavity=0.65)
barrel_steel = wk.surface("barrel_steel", (0.085, 0.085, 0.088), mottle=0.2, streak=0.25,
                          streak_axis="Y", streak_scale=600, wear=(0.50, 0.50, 0.52),
                          wear_amount=0.4, cavity=0.55)
heat = wk.surface("heat_blued", (0.10, 0.075, 0.11), mottle=0.4, mottle_scale=40, streak=0.2,
                  streak_axis="Y", wear=(0.35, 0.30, 0.38), wear_amount=0.3, cavity=0.5)
polymer = wk.surface("polymer", (0.045, 0.046, 0.045), mottle=0.15, mottle_scale=50,
                     pattern=("diamond", 1500.0, 0.25), wear=(0.10, 0.10, 0.10), wear_amount=0.4,
                     cavity=0.6)
olive = wk.surface("olive_drab", (0.085, 0.090, 0.040), mottle=0.3, mottle_scale=25,
                   wear=(0.28, 0.27, 0.22), wear_amount=0.5, chip_scale=70, cavity=0.65)
brass = wk.surface("brass", (0.55, 0.38, 0.12), mottle=0.18, wear=(0.85, 0.70, 0.38),
                   wear_amount=0.5, cavity=0.45)
copper = wk.surface("copper", (0.50, 0.22, 0.10), mottle=0.2, wear=(0.80, 0.50, 0.30),
                    wear_amount=0.4, cavity=0.4)

AXIS_Z = 0.058  # the barrel cluster's axle above the grip hand
FWD = 0.095     # the motor and the barrels this far ahead of the rear grip: the eye looks past them
RING = 0.026    # each barrel's axis from the axle
BARRELS = 6

# The housing: the motor drum with its cooling fins, the receiver box under it, the rear cap.
kit.cyl("motor", 0.048, 0.150, (0, -0.010 + FWD, AXIS_Z), gunmetal, sides=32, bevel=0.003)
for i in range(7):
    y = -0.070 + FWD + i * 0.018
    kit.tube("fin%d" % i, 0.054, 0.046, 0.006, (0, y, AXIS_Z), gunmetal, sides=32, bevel=0.001)
kit.lathe("rear_cap", [(0, 0), (0.044, 0), (0.048, 0.010), (0.048, 0.018), (0, 0.018)],
          (0, -0.103 + FWD, AXIS_Z), polymer, sides=32, bevel=0.0)
kit.box("receiver", (0.070, 0.200, 0.050), (0, 0.055, 0.020), gunmetal, bevel=0.005)
kit.lathe("buffer", [(0, 0), (0.020, 0), (0.023, 0.006), (0.023, 0.018), (0.018, 0.026), (0, 0.028)],
          (0, -0.045, 0.020), polymer, sides=24, bevel=0.0, rot=(0, 0, math.pi))
kit.box("feed_port", (0.010, 0.050, 0.030), (-0.038, 0.020 + FWD, 0.034), gunmetal, bevel=0.002)
for i, (y, z) in enumerate(((-0.050, 0.010), (0.050, 0.010), (0.000, 0.048))):
    kit.screw("bolt_l%d" % i, (-0.0355, y, z), gunmetal, r=0.004, axis="-X", depth=0.002)
    kit.screw("bolt_r%d" % i, (0.0355, y, z), gunmetal, r=0.004, axis="X", depth=0.002)

# The carry handle over the top.
kit.box("handle_post_a", (0.016, 0.016, 0.050), (0, -0.040 + FWD, 0.125), polymer, bevel=0.003)
kit.box("handle_post_b", (0.016, 0.016, 0.050), (0, 0.090 + FWD, 0.125), polymer, bevel=0.003)
kit.loft("handle_bar", [(y + FWD, 0, 0.155, 0.012, 0.010, 2.6) for y in (-0.052, 0.102)], polymer,
         sides=16, bevel=0.0)

# The rear pistol grip and its trigger, the vertical foregrip.
kit.loft("grip", [(-0.040, 0, 0.002, 0.0170, 0.022, 2.4), (-0.050, 0, -0.040, 0.0175, 0.022, 2.4),
                  (-0.062, 0, -0.080, 0.0172, 0.021, 2.4), (-0.070, 0, -0.106, 0.0160, 0.018, 2.4),
                  (-0.073, 0, -0.112, 0.0140, 0.014, 2.4)], polymer, sides=20)
kit.prism("guard", [(-0.030, 0.000), (0.035, 0.000), (0.033, -0.008), (0.024, -0.032),
                    (0.006, -0.042), (-0.016, -0.040), (-0.030, -0.026)], 0.014, gunmetal,
          bevel=0.0014)
kit.prism("trigger", [(0.002, 0.000), (0.010, 0.000), (0.006, -0.016), (-0.002, -0.026),
                      (-0.006, -0.024), (0.000, -0.012)], 0.006, gunmetal, bevel=0.0008)
kit.loft("foregrip", [(z, 0, 0.140 + FWD, 0.0175, 0.0200, 2.3) for z in (-0.002, -0.112)], polymer,
         sides=20, rot=(math.pi / 2, 0, 0))
kit.box("foregrip_base", (0.030, 0.040, 0.012), (0, 0.140 + FWD, -0.004), gunmetal, bevel=0.002)

# The ammunition box on the left and the belt feeding up into the receiver.
kit.box("ammo_box", (0.060, 0.130, 0.090), (-0.085, 0.010 + FWD, -0.040), olive, bevel=0.004)
kit.box("ammo_lid", (0.064, 0.134, 0.012), (-0.085, 0.010 + FWD, 0.010), olive, bevel=0.003)
kit.box("ammo_latch", (0.006, 0.020, 0.020), (-0.117, 0.010 + FWD, -0.010), gunmetal, bevel=0.001)
for i in range(7):
    t = i / 6
    y = 0.020 + FWD
    x = -0.085 + 0.040 * t
    z = 0.010 + 0.028 * math.sin(t * math.pi / 2)
    kit.box("link%d" % i, (0.012, 0.030, 0.006), (x, y, z), gunmetal, bevel=0.001,
            rot=(0, -0.9 * t, 0))
    kit.cyl("case%d" % i, 0.0058, 0.034, (x, y - 0.002, z + 0.006), brass, axis="Y", sides=12,
            bevel=0.0005, rot=None)
    kit.lathe("tip%d" % i, [(0, 0), (0.0055, 0), (0.0050, 0.008), (0.0025, 0.016), (0, 0.018)],
              (x, y + 0.015, z + 0.006), copper, sides=12, bevel=0.0)

# The barrel cluster: six barrels on their axle, the clamps and the flash hider, all turning.
with kit.group("barrels", pivot=(0, FWD, AXIS_Z)):
    kit.cyl("axle", 0.010, 0.520, (0, 0.330 + FWD, AXIS_Z), gunmetal, sides=16)
    for b in range(BARRELS):
        a = 2 * math.pi * b / BARRELS
        x, z = RING * math.cos(a), AXIS_Z + RING * math.sin(a)
        kit.lathe("barrel%d" % b, [(0.0045, 0.075), (0.0085, 0.075), (0.0085, 0.590),
                                   (0.0090, 0.596), (0.0090, 0.604), (0.0045, 0.604)],
                  (x, FWD, z), barrel_steel if b % 2 else heat, sides=16)
    for name, y, length in (("clamp_mid", 0.340, 0.020), ("clamp_front", 0.560, 0.024)):
        kit.tube(name, RING + 0.0135, 0.008, length, (0, y + FWD, AXIS_Z), gunmetal, sides=36,
                 bevel=0.0015)
    kit.tube("hub", RING + 0.012, 0.010, 0.040, (0, 0.090 + FWD, AXIS_Z), gunmetal, sides=36,
             bevel=0.002)
    kit.tube("flash_hider", RING + 0.0145, RING + 0.0105, 0.030, (0, 0.612 + FWD, AXIS_Z), gunmetal,
             sides=36, bevel=0.0012)

kit.hand("r", centre=(0, -0.058, -0.060), axis=(0, 0.30, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.140 + FWD, -0.060), axis=(0, 0, 1), knuckles=(-1, 0.3, 0), grip_r=0.019,
         index_curl=1.0)

turn = 2 * math.pi / BARRELS
kit.clip("fire", {"barrels": [(0.0, (0, 0, 0), (0, 0, 0)), (0.1, (0, 0, 0), (0, turn, 0))]},
         ease="linear")

kit.finish("minigun", out)
