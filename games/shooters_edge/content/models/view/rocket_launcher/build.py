# The rocket launcher's builder (held weapon 7): a shoulder-fired tube. The tube with its flared
# blast cone behind, a shoulder pad, hazard stripes and a warning band; a trigger housing under it
# with the pistol grip for the right hand and a vertical front grip for the left; a box sight on
# the left (the side the player sees) with its glowing reticle window; and the loaded rocket's
# warhead nose standing out of the muzzle.
#
# THE CLIP `fire`: the kick; the rocket leaves, so its nose drops back into the tube, and a fresh
# one slides forward into place before the next shot (refire 0.8 s).
#
# ge:summary The rocket launcher's Blender builder: the tube, blast cone, sight, rocket, the clip
# ge:keywords rocket-launcher tube blast-cone warhead hazard-stripes sight builder blender clip
# ge:skill held-weapons assets render

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from mathutils import Matrix  # noqa: E402

import ge_asset  # noqa: E402
import weapon_kit as wk  # noqa: E402

data, out = ge_asset.start()
kit = wk.Kit(data, "rocket_launcher")
kit.stance = 30.0

tube = wk.surface("tube_olive", (0.085, 0.095, 0.050), mottle=0.3, mottle_scale=18, streak=0.12,
                  streak_axis="Y", wear=(0.32, 0.31, 0.25), wear_amount=0.55, chip_scale=60,
                  cavity=0.65)
stripes = wk.surface("hazard", (0.75, 0.55, 0.04), mottle=0.15, stripes=((0.03, 0.03, 0.03), 220.0),
                     wear=(0.35, 0.33, 0.25), wear_amount=0.45, cavity=0.5)
label = wk.surface("label", (0.60, 0.58, 0.48), mottle=0.25, streak=0.2, streak_axis="Z",
                   wear=(0.25, 0.24, 0.20), wear_amount=0.5, cavity=0.5)
gunmetal = wk.surface("gunmetal", (0.11, 0.112, 0.118), mottle=0.3, streak=0.12, streak_axis="Y",
                      wear=(0.45, 0.45, 0.47), wear_amount=0.5, cavity=0.65)
polymer = wk.surface("polymer", (0.045, 0.046, 0.045), mottle=0.15, mottle_scale=50,
                     pattern=("diamond", 1500.0, 0.25), wear=(0.12, 0.12, 0.12), wear_amount=0.4,
                     cavity=0.6)
rubber = wk.surface("rubber", (0.025, 0.024, 0.024), mottle=0.2, pattern=("diamond", 300.0, 0.25),
                    cavity=0.5)
warhead = wk.surface("warhead", (0.12, 0.13, 0.07), mottle=0.2, wear=(0.35, 0.35, 0.30),
                     wear_amount=0.4, cavity=0.5)
fuse = wk.surface("fuse_band", (0.70, 0.55, 0.06), mottle=0.15, cavity=0.4)
glow = wk.surface("reticle", (1.00, 0.35, 0.10), mottle=0.0, cavity=0.0, glow=True)
glass = wk.surface("lens", (0.02, 0.05, 0.08), mottle=0.0, cavity=0.2)

TUBE_Z = 0.072  # the tube's axis above the grip hand
R = 0.044

# The tube, the blast cone, the muzzle ring, the bands.
kit.tube("tube", R, R - 0.004, 0.900, (0, 0.070, TUBE_Z), tube, sides=40, bevel=0.002)
kit.lathe("blast_cone", [(R - 0.004, 0.000), (R + 0.002, 0.000), (R + 0.028, -0.090),
                         (R + 0.030, -0.096), (R + 0.024, -0.096), (R - 0.004, -0.010)],
          (0, -0.380, TUBE_Z), gunmetal, sides=40, bevel=0.0)
kit.tube("muzzle_ring", R + 0.006, R - 0.004, 0.030, (0, 0.512, TUBE_Z), gunmetal, sides=40,
         bevel=0.002)
kit.tube("hazard_band", R + 0.0015, R - 0.002, 0.060, (0, -0.120, TUBE_Z), stripes, sides=40,
         bevel=0.0)
kit.tube("label_band", R + 0.0012, R - 0.002, 0.090, (0, 0.330, TUBE_Z), label, sides=40,
         bevel=0.0)
kit.tube("shoulder_pad", R + 0.007, R, 0.110, (0, -0.250, TUBE_Z), rubber, sides=40,
         bevel=0.003)
for y in (-0.230, 0.420):
    kit.tube("sling_band%d" % int(y * 1000), R + 0.005, R, 0.016, (0, y, TUBE_Z), gunmetal,
             sides=40, bevel=0.0015)

# The trigger housing and its grip, the front grip.
kit.box("housing", (0.036, 0.170, 0.040), (0, -0.010, TUBE_Z - R - 0.010), gunmetal, bevel=0.004)
kit.loft("grip", [(-0.040, 0, -0.012, 0.0170, 0.022, 2.4), (-0.050, 0, -0.050, 0.0175, 0.022, 2.4),
                  (-0.062, 0, -0.090, 0.0172, 0.021, 2.4), (-0.070, 0, -0.114, 0.0160, 0.018, 2.4),
                  (-0.073, 0, -0.120, 0.0140, 0.014, 2.4)], polymer, sides=20)
kit.prism("guard", [(-0.030, -0.006), (0.035, -0.006), (0.033, -0.014), (0.024, -0.040),
                    (0.006, -0.050), (-0.016, -0.048), (-0.030, -0.032)], 0.014, gunmetal,
          bevel=0.0014)
kit.prism("trigger", [(0.002, -0.006), (0.010, -0.006), (0.006, -0.024), (-0.002, -0.034),
                      (-0.006, -0.032), (0.000, -0.020)], 0.006, gunmetal, bevel=0.0008)
kit.loft("front_grip", [(z, 0, 0.200, 0.0175, 0.0200, 2.3) for z in (TUBE_Z - R, -0.075)],
         polymer, sides=20, rot=(math.pi / 2, 0, 0))
kit.box("front_clamp", (0.030, 0.034, 0.016), (0, 0.200, TUBE_Z - R - 0.004), gunmetal,
        bevel=0.002)

# The box sight on the left, its window glowing.
kit.box("sight_arm", (0.030, 0.024, 0.016), (-0.050, 0.080, TUBE_Z + 0.010), gunmetal,
        bevel=0.002)
kit.box("sight", (0.030, 0.080, 0.040), (-0.070, 0.080, TUBE_Z + 0.024), polymer, bevel=0.004)
kit.box("sight_hood", (0.032, 0.020, 0.044), (-0.070, 0.035, TUBE_Z + 0.024), rubber,
        bevel=0.003)
kit.box("sight_window", (0.018, 0.004, 0.022), (-0.070, 0.0245, TUBE_Z + 0.026), glow, bevel=0.0)
kit.box("sight_lens", (0.022, 0.003, 0.026), (-0.070, 0.1215, TUBE_Z + 0.024), glass, bevel=0.0)
for i, z in enumerate((0.008, 0.040)):
    kit.screw("sight_knob%d" % i, (-0.0855, 0.080, TUBE_Z + z), gunmetal, r=0.006, axis="-X",
              depth=0.004)

# The rocket's nose in the muzzle: its own part, gone with the shot and a fresh one run forward.
with kit.group("rocket", pivot=(0, 0.52, TUBE_Z)):
    kit.lathe("warhead", [(0, 0.000), (R - 0.006, 0.000), (R - 0.006, 0.030), (R - 0.010, 0.060),
                          (R - 0.022, 0.100), (0.008, 0.125), (0, 0.128)], (0, 0.500, TUBE_Z),
              warhead, sides=32, bevel=0.0)
    kit.tube("fuse", R - 0.0055, R - 0.012, 0.010, (0, 0.536, TUBE_Z), fuse, sides=32, bevel=0.0)

kit.hand("r", centre=(0, -0.058, -0.072), axis=(0, 0.30, 1), knuckles=(1, 0.35, 0),
         grip_r=0.018, index_curl=0.52)
kit.hand("l", centre=(0, 0.200, -0.040), axis=(0, 0, 1), knuckles=(-1, 0.3, 0), grip_r=0.019,
         index_curl=1.0)

kit.clip("fire", {
    "body": [(0.00, (0, 0, 0), (0, 0, 0)), (0.06, (0, -0.020, 0.008), (0.08, 0, 0)),
             (0.25, (0, 0, 0), (0, 0, 0)), (0.78, (0, 0, 0), (0, 0, 0))],
    "rocket": [(0.00, (0, 0, 0), (0, 0, 0)), (0.03, (0, -0.135, 0), (0, 0, 0)),
               (0.45, (0, -0.135, 0), (0, 0, 0)), (0.72, (0, 0, 0), (0, 0, 0)),
               (0.78, (0, 0, 0), (0, 0, 0))],
})

kit.finish("rocket_launcher", out, turn=Matrix.Rotation(0.06, 4, "Z"))
