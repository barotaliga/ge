---
title: Lay a lamp to its lighting class with a test, and pose a worn lamp off the figure
date: 2026-10-30
scope: plugins/mountain/app/night_lamps.hpp, games/fall_line/app/lamps.hpp
concepts: [night-lamps, floodlight, lux-class, headlamp, eye-lamp-lux, caster-slots, labs-options]
---

A floodlit piste laid by eye (one 30 000 cd flood a mast) lit a 40 m piste to a mean of 12 lx
with dark gaps of 0.15 lx between the masts: the pools read in a picture but fail the class a
night piste is built to. A test that sums each lamp's light on a face up across the piste (mean
and least, against the class) caught it at once; a small Python model of the same sums found
the two-flood layout in a minute. Lay lamps to the class first, then look.

A headlamp placed off the SkierState (base, tuck, lean) sat inside the posed figure's head, and
its unshadowed flood lit the helmet white from within: pose a worn lamp off the rig's joint
(`headlamp_of`, the posed `head`). Labs that build FrameOptions field by field do not see a new
cvar until they copy it (the world lab's `--cfg="r_lights 0"` changed nothing until it did), and
a lab measuring the moon must turn the lamps off or they key its picture.
