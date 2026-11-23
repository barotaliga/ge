---
title: A weapon stepped a frame a shot still blends each step, its flash snapped
date: 2026-11-23
scope: games/shooters_edge/view/hand.cpp
concepts: [viewmodel, nailgun, frame-blend, lerp, muzzle-flash, choppy, frame-a-shot, held-weapon]
---

Holding each shot's frame unblended (so the flash stands on the barrel that fires) made the
nailguns turn in ten hard steps a second, which players read as choppy even though every other
model blended. Blend each shot's frame from the last shot's over the clip's frame time and fix the
flash in the blend itself: a flash vertex on opposite sides of the eye's plane in the two frames is
drawn at frame b (`viewmodel::pose_frames`, `render/viewmodel.wgsl`), so it pops out whole. A
frame the server sends is blended over a FIXED 0.1 s, never the measured gap between updates: a
30 Hz recording's gaps alternate 0.067 and 0.1 s and turn the barrels at an uneven speed. Measure
it with `se-held`'s held-firing shots (still pictures while firing, largest move over the mean).
