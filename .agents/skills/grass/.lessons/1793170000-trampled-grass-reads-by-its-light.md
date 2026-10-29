---
title: Trampled grass reads by its light, not its shape
date: 2026-10-28
scope: engine/render/grass.wgsl
concepts: [trample, press, shading, floor-shade, path, wheel-track]
---

Laying the pressed blades flat was not enough: in long grass, from 4.5 m up, a walked path did not
show at all and wheel tracks were faint lines, because the flattened blades kept the root-dark
shading of a blade standing inside the field and the standing blades beside them hid the rest. The
path read once a pressed blade was shaded as open to the sky to its root (`max(along, pressed)` in
the floor shade and the albedo's root-to-tip mix) and a quarter paler (its back). Check a trample
change from above as well as at an angle (`gpu-grass`'s `trampled`, both rows), and check the press
data on the CPU first (an ASCII map of `press_at`) before blaming the draw.
