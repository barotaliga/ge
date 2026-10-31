# The shotgun's builder (held weapon 2): a pump-action twelve-bore. A blued receiver with its
# ejection port and pins, a barrel with a ventilated rib and a brass bead, the magazine tube under
# it with its cap, a ribbed walnut pump on twin action bars, a walnut stock with a checkered pistol
# grip and a rubber recoil pad, the trigger and its guard, and a side-saddle of four shells on the
# left of the receiver (the side the player sees). The right hand holds the grip, its index on the
# trigger; the left hand holds the pump and rides it.
#
# THE CLIP `fire`: the muzzle rocks up a little with the shot (the placement's kick does the rest),
# then the pump is racked back and run home (refire 0.5 s); the left arm follows the pump by IK.
#
# ge:summary The shotgun's Blender builder: pump-action receiver, barrel, pump, stock, hands, clip
# ge:keywords shotgun pump-action builder blender walnut blued-steel side-saddle shells pump clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "shotgun")
kit.stance = 40.0  # held bladed: the left shoulder forward, so the left hand reaches the pump

blued = wk.surface("blued", (0.050, 0.055, 0.068), mottle=0.25, mottle_scale=20, streak=0.16,
                   streak_axis="Y", streak_scale=450, wear=(0.42, 0.43, 0.45), wear_amount=0.5,
                   chip_scale=90, cavity=0.6)
dark_steel = wk.surface("dark_steel", (0.035, 0.036, 0.040), mottle=0.2, wear=(0.30, 0.30, 0.32),
                        wear_amount=0.35, cavity=0.55)
walnut = wk.surface("walnut", (0.30, 0.15, 0.07), grain=((0.10, 0.045, 0.018), 170.0, "Y"),
                    mottle=0.12, wear=(0.45, 0.27, 0.13), wear_amount=0.3, cavity=0.55)
checker = wk.surface("checkered_walnut", (0.27, 0.13, 0.06),
                     grain=((0.10, 0.045, 0.018), 170.0, "Z"), pattern=("diamond", 520.0, 0.45),
                     wear=(0.40, 0.24, 0.12), wear_amount=0.25, cavity=0.6)
rubber = wk.surface("rubber", (0.025, 0.024, 0.024), mottle=0.2, mottle_scale=60,
                    pattern=("diamond", 300.0, 0.25), cavity=0.5)
hull = wk.surface("shell_hull", (0.42, 0.035, 0.025), mottle=0.15, streak=0.08, streak_axis="Z",
                  wear=(0.62, 0.20, 0.15), wear_amount=0.3, cavity=0.4)
brass = wk.surface("brass", (0.55, 0.38, 0.12), mottle=0.18, wear=(0.85, 0.70, 0.38),
                   wear_amount=0.5, cavity=0.45)

# The receiver: a long box, rounded on top, the ejection port on the right, pins on the left.
kit.box("receiver", (0.034, 0.215, 0.072), (0, 0.045, 0.012), blued, bevel=0.003)
kit.loft("receiver_top", [(y, 0, 0.048, 0.0168, 0.010, 2.6) for y in (-0.062, 0.152)], blued,
         sides=20)
kit.box("port_frame", (0.004, 0.072, 0.024), (0.0175, 0.075, 0.022), dark_steel, bevel=0.0008)
kit.box("bolt", (0.003, 0.060, 0.016), (0.0185, 0.073, 0.022), blued, bevel=0.0006)
for i, (y, z) in enumerate(((-0.030, -0.010), (0.100, -0.012), (0.035, 0.032))):
    kit.screw("pin_l%d" % i, (-0.017, y, z), dark_steel, r=0.0028, axis="-X", depth=0.0012)
    kit.screw("pin_r%d" % i, (0.017, y, z), dark_steel, r=0.0028, axis="X", depth=0.0012)
kit.box("loading_port", (0.022, 0.065, 0.004), (0, 0.085, -0.025), dark_steel, bevel=0.0008)
kit.box("slide_release", (0.004, 0.012, 0.008), (-0.0185, 0.000, -0.012), dark_steel,
        bevel=0.0008)

# The barrel with its bore, the rib on top on posts, the bead; the magazine tube and its cap.
kit.lathe("barrel", [(0.0095, 0.150), (0.0118, 0.150), (0.0118, 0.540), (0.0122, 0.546),
                     (0.0122, 0.556), (0.0095, 0.556)], (0, 0, 0.036), blued, sides=28)
kit.box("rib", (0.0075, 0.400, 0.003), (0, 0.352, 0.0505), blued, bevel=0.0006)
for i in range(9):
    kit.box("rib_post%d" % i, (0.004, 0.010, 0.004), (0, 0.170 + i * 0.045, 0.0478), blued,
            bevel=0.0005)
kit.lathe("bead", [(0, 0), (0.0022, 0.0006), (0.0028, 0.0028), (0.0018, 0.0048), (0, 0.0052)],
          (0, 0.545, 0.0525), brass, axis="Z", sides=12, bevel=0.0)
kit.cyl("mag_tube", 0.0115, 0.33, (0, 0.315, 0.004), blued, sides=24)
kit.lathe("mag_cap", [(0, 0.000), (0.0125, 0.000), (0.0135, 0.004), (0.0135, 0.020),
                      (0.0100, 0.026), (0, 0.026)], (0, 0.478, 0.004), blued, sides=24)
kit.box("barrel_band", (0.020, 0.012, 0.040), (0, 0.456, 0.020), blued, bevel=0.002)

# The trigger guard and the trigger.
kit.prism("guard", [(-0.026, -0.026), (0.052, -0.026), (0.050, -0.034), (0.040, -0.060),
                    (0.020, -0.070), (-0.006, -0.070), (-0.022, -0.058), (-0.030, -0.040)],
          0.016, dark_steel, bevel=0.0015)
kit.prism("trigger", [(0.012, -0.024), (0.020, -0.024), (0.016, -0.040), (0.008, -0.052),
                      (0.004, -0.050), (0.010, -0.038)], 0.006, blued, bevel=0.0008)

# The stock: the grip running down and back into the butt, the pad at its end.
kit.loft("grip", [(-0.050, 0, -0.030, 0.0165, 0.022, 2.4), (-0.068, 0, -0.070, 0.0170, 0.021, 2.4),
                  (-0.086, 0, -0.108, 0.0172, 0.021, 2.4), (-0.098, 0, -0.130, 0.0168, 0.019, 2.4),
                  (-0.103, 0, -0.140, 0.0150, 0.016, 2.4)], checker, sides=20)
stock = []
for y, top, bottom, half in ((-0.062, 0.044, -0.030, 0.0170), (-0.100, 0.040, -0.052, 0.0175),
                             (-0.180, 0.034, -0.082, 0.0190), (-0.280, 0.028, -0.110, 0.0205),
                             (-0.370, 0.024, -0.132, 0.0215), (-0.405, 0.023, -0.140, 0.0215)):
    stock.append((y, 0, (top + bottom) / 2, half, (top - bottom) / 2, 2.8))
kit.loft("stock", stock, walnut, sides=24)
kit.loft("pad", [(-0.405, 0, -0.0585, 0.0225, 0.083, 3.2), (-0.425, 0, -0.0585, 0.0225, 0.083, 3.2),
                 (-0.428, 0, -0.0585, 0.0205, 0.080, 3.2)], rubber, sides=24, bevel=0.0)
kit.loft("pad_spacer", [(-0.4045, 0, -0.0585, 0.0219, 0.0822, 3.2),
                        (-0.4075, 0, -0.0585, 0.0219, 0.0822, 3.2)], dark_steel, sides=24)
kit.screw("swivel", (0, -0.330, -0.125), dark_steel, r=0.004, axis="-Z", depth=0.004)

# The side-saddle on the left: a plate of four shells, brass heads up, red hulls down.
kit.box("saddle", (0.004, 0.090, 0.040), (-0.0195, 0.060, 0.006), dark_steel, bevel=0.001)
for i in range(4):
    y = 0.026 + i * 0.0225
    kit.cyl("hull%d" % i, 0.0098, 0.050, (-0.0315, y, 0.000), hull, axis="Z", sides=16)
    kit.lathe("head%d" % i, [(0, 0), (0.0105, 0), (0.0105, 0.0035), (0.0099, 0.0045),
                             (0.0099, 0.010), (0, 0.010)], (-0.0315, y, 0.022), brass, axis="Z",
              sides=16, bevel=0.0)

# The pump: ribbed walnut round the tube on its bars, its own part, the left hand on it.
PUMP_Y = (0.200, 0.345)
with kit.group("pump"):
    rings = []
    n = 14
    for i in range(n + 1):
        y = PUMP_Y[0] + (PUMP_Y[1] - PUMP_Y[0]) * i / n
        end = 0.004 if i in (0, n) else 0.0
        groove = 0.0 if i in (0, n) or i % 2 else 0.0025
        rings.append((y, 0, 0.002, 0.0215 - groove - end, 0.0205 - groove - end, 2.5))
    kit.loft("pump", rings, walnut, sides=24)
    for sx in (1, -1):
        kit.box("bar_%d" % (sx + 1), (0.003, 0.230, 0.007), (sx * 0.0145, 0.120, 0.004),
                dark_steel, bevel=0.0006)
kit.hand("r", centre=(0, -0.070, -0.075), axis=(0, 0.40, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.240, 0.000), axis=(0, 1, 0), knuckles=(0.7, 0, -0.7), grip_r=0.022,
         holds="pump", index_curl=0.95, grip="cradle")

kit.clip("fire", {
    "body": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.04, (0, -0.010, 0.004), (0.05, 0, 0)),
        (0.14, (0, 0, 0), (0, 0, 0)),
        (0.48, (0, 0, 0), (0, 0, 0)),
    ],
    "pump": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.12, (0, 0, 0), (0, 0, 0)),
        (0.22, (0, -0.072, 0), (0, 0, 0)),
        (0.27, (0, -0.072, 0), (0, 0, 0)),
        (0.36, (0, 0, 0), (0, 0, 0)),
        (0.48, (0, 0, 0), (0, 0, 0)),
    ],
})

kit.finish("shotgun", out)
