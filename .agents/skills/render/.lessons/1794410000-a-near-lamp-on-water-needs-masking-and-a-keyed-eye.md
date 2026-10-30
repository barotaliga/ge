---
title: A lamp near the water needs the facets' masking and an eye keyed to it, and a lab's before is the old shader's
date: 2026-10-30
scope: engine/render/wgsl/water_light.wgsl, engine/lab/labs/gpu_water_lab.cpp
concepts: [lamp-glitter, smith-masking, beckmann, eye-adaptation, lamp-lux, before-after]
---

The water's lamp glitter, written as the sun's (Beckmann over the unresolved slopes, F D / 4
cos_v), is fine for a far sun but grazes for a lamp low over the water near the eye. Add Smith's
masking for both rays (`smith_g1`), which the sun's term never needed. A pale band across the far
lake was not the glitter, though. It was the lamp's light into the water's own colour, seen by an
eye still keyed to a moonless night. A lab view with a lamp in it sets `FrameView::lamp_lux`, as
a game's frame does; a fiftieth of the lux under the lamp kept both the road of light and the
dark round it.

For an honest before of a shader change, set the shader files aside (copy them out, `git
checkout` the old ones), build, run the labs `--tag=before` with the new lab views already in,
then put the new shaders back and `--compare=before`. The views that do not reach the change come
back identical, and that check is free.
