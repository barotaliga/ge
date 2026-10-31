# The lightning gun's builder (held weapon 8, the beam gun's slot): a tesla gun. A housing ringed
# with ribbed white ceramic insulators ending in a glowing blue coil; three prongs reaching forward
# round a spinning rotor whose nodes glow; a glass capacitor tank with a glowing core on the left
# (the side the player sees); a battery under the stock feeding the coil through cables; a pistol
# grip for the right hand and a front grip for the left.
#
# THE CLIP `fire`: the rotor turns a third of a turn a shot, in the 0.1 s between shots, so it
# ends looking as it began and a held trigger spins it steadily.
#
# ge:summary The lightning gun's Blender builder: insulators, glowing coil, prongs, rotor, tank
# ge:keywords lightning-gun tesla coil insulator ceramic prongs rotor capacitor glow builder
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from mathutils import Matrix  # noqa: E402

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "lightning_gun")
kit.stance = 35.0

brass = wk.surface("brass", (0.50, 0.34, 0.11), mottle=0.25, streak=0.15, streak_axis="Y",
                   wear=(0.85, 0.68, 0.36), wear_amount=0.55, chip_scale=80, cavity=0.65)
ceramic = wk.surface("ceramic", (0.62, 0.60, 0.55), mottle=0.15, mottle_scale=40,
                     wear=(0.30, 0.28, 0.24), wear_amount=0.25, cavity=0.7)
dark = wk.surface("dark_alloy", (0.050, 0.052, 0.058), mottle=0.25, streak=0.15,
                  streak_axis="Y", wear=(0.38, 0.38, 0.40), wear_amount=0.45, cavity=0.6)
copper = wk.surface("copper", (0.70, 0.33, 0.14), mottle=0.25, streak=0.3, streak_axis="X",
                    streak_scale=900, wear=(0.85, 0.55, 0.32), wear_amount=0.4, cavity=0.6)
polymer = wk.surface("polymer", (0.045, 0.046, 0.045), mottle=0.15, mottle_scale=50,
                     pattern=("diamond", 1500.0, 0.25), wear=(0.12, 0.12, 0.12), wear_amount=0.4,
                     cavity=0.6)
rubber = wk.surface("rubber", (0.030, 0.029, 0.028), mottle=0.2, cavity=0.5)
glass = wk.surface("tank_glass", (0.05, 0.10, 0.14), mottle=0.1, cavity=0.3)
glow = wk.surface("arc_glow", (0.55, 0.80, 1.00), mottle=0.0, cavity=0.0, glow=True)

AXIS_Z = 0.058
R = 0.030

# The housing and its insulators, the coil at its front.
kit.cyl("housing", R, 0.300, (0, 0.110, AXIS_Z), dark, sides=32, bevel=0.002)
for i in range(6):
    y = 0.060 + i * 0.028
    kit.lathe("insulator%d" % i, [(R, -0.010), (R + 0.010, -0.009), (R + 0.016, -0.004),
                                  (R + 0.016, 0.004), (R + 0.010, 0.009), (R, 0.010)],
              (0, y, AXIS_Z), ceramic, sides=32, bevel=0.0)
for i in range(7):
    kit.tube("coil%d" % i, R + 0.008, R - 0.002, 0.005, (0, 0.238 + i * 0.008, AXIS_Z),
             glow if i % 2 == 0 else copper, sides=32, bevel=0.0)
kit.tube("coil_cap", R + 0.012, R - 0.004, 0.008, (0, 0.296, AXIS_Z), brass, sides=32,
         bevel=0.0015)
kit.box("rear_block", (0.046, 0.070, 0.050), (0, -0.030, 0.024), dark, bevel=0.005)

# Three prongs reaching forward from the coil cap, bending in round the rotor, glowing at the tips.
for k in range(3):
    a = 2 * math.pi * k / 3 + math.pi / 2
    pts = []
    for t in (0.0, 0.25, 0.5, 0.75, 1.0):
        rr = R + 0.006 + 0.010 * math.sin(t * math.pi) - 0.012 * t
        pts.append((rr * math.cos(a), 0.300 + 0.110 * t, AXIS_Z + rr * math.sin(a)))
    kit.cable("prong%d" % k, pts, 0.0045, brass, sides=10)
    tip = pts[-1]
    kit.lathe("prong_tip%d" % k, [(0, -0.008), (0.007, -0.004), (0.008, 0.0), (0.007, 0.004),
                                  (0, 0.008)], tip, glow, sides=12, bevel=0.0)

# The capacitor tank on the left: glass round a glowing core, capped in brass.
kit.cyl("tank_core", 0.010, 0.120, (-0.046, 0.090, AXIS_Z + 0.004), glow, sides=16, bevel=0.0)
kit.tube("tank_glass", 0.018, 0.0165, 0.120, (-0.046, 0.090, AXIS_Z + 0.004), glass, sides=24,
         bevel=0.0)
for y in (0.026, 0.154):
    kit.cyl("tank_cap%d" % int(y * 1000), 0.020, 0.012, (-0.046, y, AXIS_Z + 0.004), brass,
            sides=24, bevel=0.0015)
kit.box("tank_mount", (0.020, 0.020, 0.020), (-0.034, 0.090, AXIS_Z + 0.004), dark, bevel=0.002)

# The grip and trigger, the front grip, the stock with its battery and the cables to the coil.
kit.loft("grip", [(-0.040, 0, -0.012, 0.0170, 0.022, 2.4), (-0.050, 0, -0.050, 0.0175, 0.022, 2.4),
                  (-0.062, 0, -0.090, 0.0172, 0.021, 2.4), (-0.070, 0, -0.114, 0.0160, 0.018, 2.4),
                  (-0.073, 0, -0.120, 0.0140, 0.014, 2.4)], polymer, sides=20)
kit.prism("guard", [(-0.030, -0.006), (0.035, -0.006), (0.033, -0.014), (0.024, -0.040),
                    (0.006, -0.050), (-0.016, -0.048), (-0.030, -0.032)], 0.014, dark,
          bevel=0.0014)
kit.prism("trigger", [(0.002, -0.006), (0.010, -0.006), (0.006, -0.024), (-0.002, -0.034),
                      (-0.006, -0.032), (0.000, -0.020)], 0.006, brass, bevel=0.0008)
kit.box("grip_mount", (0.040, 0.060, 0.012), (0, -0.040, -0.002), dark, bevel=0.002)
kit.loft("front_grip", [(z, 0, 0.170, 0.0175, 0.0200, 2.3) for z in (AXIS_Z - R, -0.075)],
         polymer, sides=20, rot=(math.pi / 2, 0, 0))
kit.box("front_clamp", (0.030, 0.034, 0.016), (0, 0.170, AXIS_Z - R - 0.004), dark, bevel=0.002)
kit.box("stock", (0.022, 0.200, 0.020), (0, -0.170, 0.000), polymer, bevel=0.003)
kit.box("battery", (0.050, 0.090, 0.050), (0, -0.170, -0.034), dark, bevel=0.004)
kit.box("battery_glow", (0.052, 0.050, 0.006), (0, -0.170, -0.014), glow, bevel=0.0)
kit.loft("pad", [(y, 0, -0.020, 0.024, 0.050, 3.0) for y in (-0.270, -0.290)], rubber, sides=24,
         bevel=0.0)
for k, x in enumerate((0.030, 0.038)):
    kit.cable("cable%d" % k, [(x, -0.130, -0.030), (x + 0.012, -0.060, -0.020),
                              (x + 0.010, 0.040, 0.020), (x, 0.150, AXIS_Z - 0.010),
                              (0.020, 0.235, AXIS_Z - 0.010)], 0.0045, rubber)

# The rotor between the prongs: a ring of three glowing nodes, spinning.
with kit.group("rotor", pivot=(0, 0.370, AXIS_Z)):
    kit.tube("rotor_ring", 0.020, 0.016, 0.006, (0, 0.370, AXIS_Z), brass, sides=24, bevel=0.0)
    for k in range(3):
        a = 2 * math.pi * k / 3
        kit.lathe("node%d" % k, [(0, -0.006), (0.005, -0.003), (0.006, 0.0), (0.005, 0.003),
                                 (0, 0.006)],
                  (0.018 * math.cos(a), 0.370, AXIS_Z + 0.018 * math.sin(a)), glow, sides=10,
                  bevel=0.0)
    kit.cyl("rotor_hub", 0.006, 0.040, (0, 0.360, AXIS_Z), brass, sides=12, bevel=0.0)

kit.hand("r", centre=(0, -0.058, -0.072), axis=(0, 0.30, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.170, -0.040), axis=(0, 0, 1), knuckles=(-1, 0.3, 0), grip_r=0.019,
         index_curl=1.0)

kit.clip("fire", {"rotor": [(0.0, (0, 0, 0), (0, 0, 0)), (0.1, (0, 0, 0), (0, 2 * math.pi / 3, 0))]},
         ease="linear")

kit.finish("lightning_gun", out, turn=Matrix.Rotation(0.06, 4, "Z"))
