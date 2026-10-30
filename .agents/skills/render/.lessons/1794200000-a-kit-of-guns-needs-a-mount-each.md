---
title: One generic mount for a kit of guns frames none of them; a length and an offset each does
date: 2026-10-30
scope: games/shooters_edge/view/weapons.cpp, games/shooters_edge/labs/se_viewmodel_lab.cpp
concepts: [viewmodel, mount, offset, screen-share, held-weapon, first-person, framing, kit]
---

The engine's `viewmodel` lab mounts any folder of .glb guns with one rule: the kit's size times
0.6, gripped low a third from the back. Over Shooter's Edge's eight free guns, it drew most of
them over 40% of the picture and cut them at the right edge. The guns differ in shape, not only
in length: some carry a bulky drum or stock behind the grip, and that bulk sits next to the eye
and fills the picture. Each gun needed its own held length and its own forward offset (0.30 to
0.44 m, the bulky ones furthest out) before all eight covered about the same share. Measure
framing with a number (the placed box's share of the screen, the muzzle's screen point), not by
eye: three rounds of tuning, each judged by one table row a weapon, took minutes.
