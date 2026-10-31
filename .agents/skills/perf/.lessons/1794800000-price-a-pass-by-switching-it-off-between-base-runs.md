---
title: Price a pass by switching it off between two base runs; a kept cache still hitches when it refills all at once
date: 2026-10-31
scope: games/shooters_edge/labs/gpu_se_timedemo_lab.cpp, games/shooters_edge/client/timedemo.cpp, plugins/brush/app/bsp_draw.cpp
concepts: [price, switch-off, tiled-gpu, timestamp-query, drift, noise, shadow-cache, budget, hitch, lru]
---

On a tiled GPU (Apple's) seven passes of the world frame each read 0.73 to 0.93 ms, the HUD's
among them: each pass's timestamps stretch over the fragment work it overlaps, so a pass's own
time cannot rank the passes. The honest price is the whole frame with that thing switched off.
Played in order (base, every row, base again) the rows drifted with the machine: a build that
started midway made the last rows read 20 ms "dearer" than the first. Playing the base before
and after every row and reading each saving against the mean of its two neighbours takes slow
drift out, and the largest gap between neighbouring base runs is the noise a saving must clear.

A cache kept between frames still hitches when many entries go stale at once: the kept shadow
cubes were 0.2 ms most frames and 8.7 to 12.65 ms when a doorway brought a room of torches into
the nearest few (up to 32 cubes, each the whole map six times). Count the redraws per frame
first (here: 254 of 973 frames drew a cube, one drew 32). A budget a frame (two new cubes, the
rest lit unshadowed until their turn), least-recently-used reuse of the slots, and no cube for a
light that moves every frame took the worst frame to two cubes; the map's first frame still
draws them all so single-frame labs' pictures do not move.
