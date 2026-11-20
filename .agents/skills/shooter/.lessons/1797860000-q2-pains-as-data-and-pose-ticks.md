---
title: Quake II monsters' pains and deaths are data; a pose changes only on a rules frame
date: 2026-11-20
scope: games/shooters_edge/server/q2_monster_pain.cpp
concepts: [pain, death, pain-animation, nightmare, pose, monster-test, test-hall, light-probe]
---

A kind's pain run and cry come from `q2_monster_pain.cpp` (by the blow's size, shrug odds, busy
frames, what skill 3 keeps, the knocked-up run); `monster_damage` hands the run to the pose as
`pain_run`, which plays over an attack and cuts it. A death run picked there (`death_run`) wins
over the pose's own pick. In a test, a pose is stepped once a rules frame (0.1 s, 8 host ticks of
13 ms), so `run(*h, 1)` after a blow often shows nothing new: run 8. A blow below about 166 u/s of
push is worn off by a flier's friction in one frame (stop speed 100, friction 6): test knockback
with a heavy blow. The test hall has no drawn faces, so `light_below` finds nothing (lit): give it
a floor of two triangles and a one-luxel lightmap to test the dark.
