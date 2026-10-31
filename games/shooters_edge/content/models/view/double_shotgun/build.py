# The double shotgun's builder (held weapon 3): a side-by-side break-action twelve-bore. Two blued
# barrels joined by a rib with a brass bead, the action in case-hardened steel with scroll
# engraving and rounded fences, two exposed hammers, the top lever on the tang, two triggers in
# their guard, a checkered walnut splinter fore-end, and a walnut stock with a checkered
# semi-pistol grip and a steel butt plate. The right hand holds the grip, its index on the front
# trigger; the left hand cradles the fore-end and rides the barrels when they break.
#
# THE CLIP `fire`: the muzzle rocks up with the shot, then the gun is broken open on its hinge
# (the barrels, the fore-end and the left hand drop about 26 deg) and snapped shut (refire 0.7 s);
# the left arm follows by IK.
#
# ge:summary The double shotgun's Blender builder: side-by-side barrels, the action, the break
# ge:keywords double-shotgun side-by-side break-action builder blender hammers engraving clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "double_shotgun")
kit.stance = 40.0  # held bladed: the left shoulder forward, so the left hand reaches the fore-end

blued = wk.surface("blued", (0.045, 0.050, 0.064), mottle=0.25, mottle_scale=20, streak=0.16,
                   streak_axis="Y", streak_scale=450, wear=(0.42, 0.43, 0.45), wear_amount=0.45,
                   chip_scale=90, cavity=0.6)
case = wk.surface("case_hardened", (0.30, 0.29, 0.28), mottle=0.45, mottle_scale=55,
                  pattern=("diamond", 2400.0, 0.22), streak=0.1, wear=(0.62, 0.62, 0.63),
                  wear_amount=0.5, chip_scale=120, cavity=0.75)
dark_steel = wk.surface("dark_steel", (0.035, 0.036, 0.040), mottle=0.2, wear=(0.30, 0.30, 0.32),
                        wear_amount=0.35, cavity=0.55)
walnut = wk.surface("walnut", (0.28, 0.13, 0.06), grain=((0.09, 0.040, 0.016), 170.0, "Y"),
                    mottle=0.12, wear=(0.45, 0.27, 0.13), wear_amount=0.3, cavity=0.55)
checker = wk.surface("checkered_walnut", (0.25, 0.12, 0.055),
                     grain=((0.09, 0.040, 0.016), 170.0, "Z"), pattern=("diamond", 520.0, 0.45),
                     wear=(0.40, 0.24, 0.12), wear_amount=0.25, cavity=0.6)
brass = wk.surface("brass", (0.55, 0.38, 0.12), mottle=0.18, wear=(0.85, 0.70, 0.38),
                   wear_amount=0.5, cavity=0.45)

HINGE = (0, 0.046, -0.012)

# The action: the body with its fences, the tang and top lever, the hammers, the guard, triggers.
kit.box("action", (0.046, 0.080, 0.050), (0, 0.005, 0.000), case, bevel=0.004)
for sx in (1, -1):
    kit.lathe("fence_%d" % (sx + 1), [(0, 0), (0.0135, 0.000), (0.0145, 0.006), (0.0125, 0.012),
                                       (0, 0.014)], (sx * 0.0120, 0.044, 0.019), case, sides=24,
              bevel=0.0)
    hammer = [(-0.040, 0.010), (-0.026, 0.012), (-0.020, 0.024), (-0.024, 0.036),
              (-0.034, 0.040), (-0.040, 0.034), (-0.034, 0.026)]
    kit.prism("hammer_%d" % (sx + 1), hammer, 0.006, case, x=sx * 0.0175, bevel=0.0008)
    kit.screw("pin_%d" % (sx + 1), (sx * 0.0232, -0.010, -0.008), dark_steel, r=0.003,
              axis="X" if sx > 0 else "-X", depth=0.001)
    kit.screw("pivot_%d" % (sx + 1), (sx * 0.0232, 0.034, -0.012), dark_steel, r=0.0035,
              axis="X" if sx > 0 else "-X", depth=0.0012)
kit.box("tang", (0.022, 0.070, 0.008), (0, -0.065, 0.020), case, bevel=0.002)
kit.box("top_lever", (0.012, 0.040, 0.006), (0.006, -0.040, 0.028), blued, bevel=0.0015,
        rot=(0, 0, 0.35))
kit.prism("guard", [(-0.050, -0.022), (0.020, -0.022), (0.018, -0.030), (0.008, -0.054),
                    (-0.012, -0.064), (-0.036, -0.062), (-0.050, -0.048), (-0.056, -0.034)],
          0.014, dark_steel, bevel=0.0014)
for i, y in enumerate((0.004, -0.016)):
    kit.prism("trigger%d" % i, [(y, -0.022), (y + 0.006, -0.022), (y + 0.002, -0.040),
                                (y - 0.006, -0.050), (y - 0.009, -0.048), (y - 0.002, -0.036)],
              0.005, blued, bevel=0.0007)

# The stock: a semi-pistol grip into the butt, the butt plate.
kit.loft("grip", [(-0.088, 0, -0.012, 0.0160, 0.022, 2.4), (-0.098, 0, -0.050, 0.0168, 0.022, 2.4),
                  (-0.112, 0, -0.090, 0.0170, 0.021, 2.4), (-0.122, 0, -0.112, 0.0160, 0.018, 2.4),
                  (-0.126, 0, -0.120, 0.0140, 0.014, 2.4)], checker, sides=20)
stock = []
for y, top, bottom, half in ((-0.040, 0.026, -0.022, 0.0200), (-0.090, 0.028, -0.040, 0.0170),
                             (-0.170, 0.024, -0.075, 0.0185), (-0.270, 0.018, -0.105, 0.0200),
                             (-0.360, 0.014, -0.128, 0.0210), (-0.395, 0.013, -0.136, 0.0210)):
    stock.append((y, 0, (top + bottom) / 2, half, (top - bottom) / 2, 2.8))
kit.loft("stock", stock, walnut, sides=24)
kit.loft("butt_plate", [(-0.395, 0, -0.0615, 0.0215, 0.0755, 3.2),
                        (-0.401, 0, -0.0615, 0.0215, 0.0755, 3.2),
                        (-0.403, 0, -0.0615, 0.0195, 0.0730, 3.2)], dark_steel, sides=24, bevel=0.0)
kit.screw("plate_screw", (0, -0.403, -0.030), dark_steel, r=0.003, axis="-Y", depth=0.001)

# The barrels, the rib, the bead, the lumps and the fore-end: one part that breaks on the hinge.
with kit.group("barrels", pivot=HINGE):
    for sx in (1, -1):
        kit.lathe("barrel_%d" % (sx + 1), [(0.0094, 0.046), (0.0122, 0.046), (0.0118, 0.400),
                                           (0.0113, 0.740), (0.0117, 0.746), (0.0117, 0.752),
                                           (0.0094, 0.752)],
                  (sx * 0.0120, 0, 0.019), blued, sides=28)
    kit.box("rib", (0.008, 0.700, 0.004), (0, 0.399, 0.0315), blued, bevel=0.0007, taper=(0.7, 1))
    kit.box("rib_under", (0.010, 0.640, 0.006), (0, 0.380, 0.004), blued, bevel=0.0007)
    kit.lathe("bead", [(0, 0), (0.0022, 0.0006), (0.0028, 0.0028), (0.0018, 0.0048), (0, 0.0052)],
              (0, 0.742, 0.0335), brass, axis="Z", sides=12, bevel=0.0)
    kit.box("lumps", (0.022, 0.050, 0.020), (0, 0.072, -0.004), dark_steel, bevel=0.002)
    rings = []
    for i, y in enumerate((0.085, 0.100, 0.200, 0.290, 0.305)):
        half = 0.0205 if 0 < i < 4 else 0.0175
        rings.append((y, 0, -0.004, half, 0.0145 if 0 < i < 4 else 0.012, 2.6))
    kit.loft("fore_end", rings, checker, sides=24)
    kit.box("fore_iron", (0.016, 0.040, 0.010), (0, 0.098, -0.016), case, bevel=0.002)

kit.hand("r", centre=(0, -0.100, -0.065), axis=(0, 0.35, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.200, -0.004), axis=(0, 1, 0), knuckles=(0.7, 0, -0.7), grip_r=0.019,
         holds="barrels", index_curl=0.95, grip="cradle")

kit.clip("fire", {
    "body": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.05, (0, -0.012, 0.005), (0.06, 0, 0)),
        (0.16, (0, 0, 0), (0, 0, 0)),
        (0.66, (0, 0, 0), (0, 0, 0)),
    ],
    "barrels": [
        (0.00, (0, 0, 0), (0, 0, 0)),
        (0.14, (0, 0, 0), (0, 0, 0)),
        (0.27, (0, 0, 0), (-0.45, 0, 0)),
        (0.40, (0, 0, 0), (-0.45, 0, 0)),
        (0.52, (0, 0, 0), (0, 0, 0)),
        (0.66, (0, 0, 0), (0, 0, 0)),
    ],
})

kit.finish("double_shotgun", out)
