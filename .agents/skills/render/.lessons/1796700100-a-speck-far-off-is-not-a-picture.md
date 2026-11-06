---
title: A classic speck far off is not a picture; throw the effect on the yard instead
date: 2026-11-06
scope: games/shooters_edge/labs/gpu_se_timedemo_lab.cpp, engine/lab/labs/gpu_particle_effects_lab.cpp
concepts: [particles, particle-effects, classic-specks, lab-picture, explosion-scene]
---

The classic look's specks are 5 cm across. In a demo frame of a blast a room away, at 640 px,
they come to under a pixel, and the blast can be behind a wall that its light shines through,
so the frame shows a lit room and no specks, before and after. The `se-timedemo` blast picture
proves the effects are thrown (its light, the modern look's fireball), but not what an effect
looks like. To picture an effect, throw it on the explosion labs' yard
(`lab/labs/explosion_scene.hpp`) a few metres from the eye, fly the pool at 120 Hz, and take
settled stills at chosen moments, as `gpu-particle-effects` does. A GPU lab run under ASan in
the container ends with a leak report from the driver's threads: run it with
`ASAN_OPTIONS=detect_leaks=0`, as precheck does.
