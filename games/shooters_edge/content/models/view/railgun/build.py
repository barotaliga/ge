# The railgun's builder (held weapon 5, the heavy nailgun's slot): twin rails with a glowing cyan
# channel between them, wrapped by copper coil bands and ending in a squared muzzle; an angular
# receiver with a scope on top; a capacitor bank on the left (the side the player sees) under a
# glowing charge strip; a skeleton stock with its pad; a pistol grip for the right hand and a
# fore-end under the rails for the left.
#
# THE CLIP `fire`: the charging block behind the rails slams back with the shot and returns, in
# the 0.1 s between shots.
#
# ge:summary The railgun's Blender builder: twin rails, glowing channel, coils, capacitors, clip
# ge:keywords railgun rails coil capacitor glow emissive scope skeleton-stock builder blender
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from mathutils import Matrix  # noqa: E402

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "railgun")
kit.stance = 40.0  # held bladed: the left shoulder forward to the fore-end

armour = wk.surface("armour_grey", (0.16, 0.17, 0.18), mottle=0.2, mottle_scale=20,
                    wear=(0.50, 0.50, 0.52), wear_amount=0.5, chip_scale=80, cavity=0.7)
dark = wk.surface("dark_alloy", (0.045, 0.048, 0.055), mottle=0.25, streak=0.15,
                  streak_axis="Y", wear=(0.35, 0.36, 0.38), wear_amount=0.45, cavity=0.6)
rail = wk.surface("rail_steel", (0.30, 0.30, 0.31), mottle=0.15, streak=0.35, streak_axis="Y",
                  streak_scale=700, wear=(0.65, 0.65, 0.67), wear_amount=0.5, cavity=0.5)
copper = wk.surface("coil_copper", (0.78, 0.38, 0.16), mottle=0.25, streak=0.3, streak_axis="X",
                    streak_scale=900, wear=(0.85, 0.55, 0.32), wear_amount=0.4, cavity=0.6)
polymer = wk.surface("polymer", (0.040, 0.041, 0.040), mottle=0.15, mottle_scale=50,
                     pattern=("diamond", 1500.0, 0.25), wear=(0.12, 0.12, 0.12), wear_amount=0.4,
                     cavity=0.6)
glow = wk.surface("charge_glow", (0.30, 0.85, 1.00), mottle=0.0, cavity=0.0, glow=True)
glass = wk.surface("lens", (0.02, 0.05, 0.08), mottle=0.0, cavity=0.2)

RAIL_TOP, RAIL_BOTTOM = 0.058, 0.014
CHANNEL = (RAIL_TOP + RAIL_BOTTOM) / 2

# The rails: two flat bars, the glowing channel between them, the coil bands round both, the muzzle.
for name, z in (("rail_top", RAIL_TOP), ("rail_bottom", RAIL_BOTTOM)):
    kit.box(name, (0.030, 0.560, 0.012), (0, 0.350, z), rail, bevel=0.0015)
kit.box("channel", (0.010, 0.540, 0.030), (0, 0.345, CHANNEL), glow, bevel=0.0)
kit.box("spine_l", (0.004, 0.540, 0.040), (-0.012, 0.345, CHANNEL), dark, bevel=0.0008)
kit.box("spine_r", (0.004, 0.540, 0.040), (0.012, 0.345, CHANNEL), dark, bevel=0.0008)
for i in range(8):
    y = 0.150 + i * 0.058
    kit.box("coil%d" % i, (0.062, 0.024, 0.092), (0, y, CHANNEL), copper, bevel=0.004)
    kit.box("coil_cap%d" % i, (0.066, 0.006, 0.096), (0, y - 0.015, CHANNEL), dark, bevel=0.0015)
kit.box("muzzle", (0.066, 0.044, 0.100), (0, 0.640, CHANNEL), armour, bevel=0.005,
        taper=(0.85, 0.85))
kit.box("muzzle_slot", (0.012, 0.010, 0.034), (0, 0.661, CHANNEL), glow, bevel=0.0)

# The receiver: an angular block, its top rail and the scope, the capacitors and the charge strip.
kit.box("receiver", (0.056, 0.190, 0.066), (0, 0.010, 0.022), armour, bevel=0.006,
        taper=(0.9, 0.85))
kit.box("receiver_top", (0.040, 0.170, 0.010), (0, 0.010, 0.060), dark, bevel=0.002)
for i in range(9):
    kit.box("picatinny%d" % i, (0.024, 0.008, 0.005), (0, -0.060 + i * 0.018, 0.0675), dark,
            bevel=0.0006)
kit.cyl("scope", 0.016, 0.130, (0, 0.010, 0.092), dark, sides=24, bevel=0.002)
kit.lathe("scope_front", [(0.0, 0.0), (0.020, 0.0), (0.020, 0.024), (0.0, 0.024)],
          (0, 0.075, 0.092), dark, sides=24, bevel=0.0015)
kit.lathe("scope_glass", [(0, 0), (0.017, 0), (0.017, 0.002), (0, 0.004)], (0, 0.099, 0.092),
          glass, sides=24, bevel=0.0)
for y in (-0.025, 0.045):
    kit.box("scope_ring%d" % int(y * 1000), (0.026, 0.012, 0.030), (0, y, 0.077), dark,
            bevel=0.002)
for i in range(3):
    z = -0.002 + i * 0.026
    kit.lathe("capacitor%d" % i, [(0, 0), (0.0115, 0), (0.0125, 0.004), (0.0125, 0.150),
                                  (0.0115, 0.154), (0, 0.154)],
              (-0.040, -0.040, z), dark, sides=20, bevel=0.0)
    for j in range(3):
        kit.tube("cap_band%d_%d" % (i, j), 0.0135, 0.0110, 0.006,
                 (-0.040, -0.010 + j * 0.045, z), copper, sides=20, bevel=0.0006)
kit.box("charge_strip", (0.004, 0.120, 0.008), (-0.0545, 0.020, 0.048), glow, bevel=0.0)
kit.box("strip_frame", (0.006, 0.128, 0.014), (-0.052, 0.020, 0.048), dark, bevel=0.001)

# The fore-end under the rails, the pistol grip and its guard and trigger.
kit.loft("fore_end", [(y, 0, -0.010, h, 0.018, 2.8) for y, h in
                      ((0.150, 0.020), (0.165, 0.025), (0.300, 0.025), (0.315, 0.020))],
         polymer, sides=24)
kit.loft("grip", [(-0.040, 0, -0.012, 0.0170, 0.022, 2.4), (-0.050, 0, -0.050, 0.0175, 0.022, 2.4),
                  (-0.062, 0, -0.090, 0.0172, 0.021, 2.4), (-0.070, 0, -0.114, 0.0160, 0.018, 2.4),
                  (-0.073, 0, -0.120, 0.0140, 0.014, 2.4)], polymer, sides=20)
kit.prism("guard", [(-0.030, -0.012), (0.035, -0.012), (0.033, -0.020), (0.024, -0.044),
                    (0.006, -0.054), (-0.016, -0.052), (-0.030, -0.038)], 0.014, dark,
          bevel=0.0014)
kit.prism("trigger", [(0.002, -0.012), (0.010, -0.012), (0.006, -0.028), (-0.002, -0.038),
                      (-0.006, -0.036), (0.000, -0.024)], 0.006, rail, bevel=0.0008)

# The skeleton stock: a top bar, a bottom strut, the pad.
kit.box("stock_bar", (0.022, 0.220, 0.018), (0, -0.195, 0.000), polymer, bevel=0.003)
kit.prism("stock_strut", [(-0.090, -0.030), (-0.080, -0.018), (-0.350, -0.070), (-0.355, -0.086)],
          0.018, polymer, bevel=0.003)
kit.loft("pad", [(y, 0, -0.040, 0.022, 0.056, 3.2) for y in (-0.352, -0.376)], polymer,
         sides=24, bevel=0.0)

# The charging block behind the rails: its own part, slammed back by the shot.
with kit.group("block", pivot=(0, 0.110, CHANNEL)):
    kit.box("block", (0.044, 0.030, 0.056), (0, 0.110, CHANNEL), dark, bevel=0.003)
    kit.box("block_glow", (0.046, 0.006, 0.010), (0, 0.110, CHANNEL), glow, bevel=0.0)

kit.hand("r", centre=(0, -0.058, -0.072), axis=(0, 0.30, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.232, -0.010), axis=(0, 1, 0), knuckles=(0.7, 0, -0.7), grip_r=0.021,
         index_curl=0.95, grip="cradle")

kit.clip("fire", {
    "body": [(0.00, (0, 0, 0), (0, 0, 0)), (0.03, (0, -0.006, 0.002), (0.02, 0, 0)),
             (0.10, (0, 0, 0), (0, 0, 0))],
    "block": [(0.00, (0, 0, 0), (0, 0, 0)), (0.025, (0, -0.028, 0), (0, 0, 0)),
              (0.10, (0, 0, 0), (0, 0, 0))],
})

# Turned in a little (the muzzle toward the middle of the view) so the rails' left side opens to the
# eye instead of running straight away from it.
kit.finish("railgun", out, turn=Matrix.Rotation(0.09, 4, "Z"))
