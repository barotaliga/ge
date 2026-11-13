---
title: A filmed camera beside the action checks its lines in steps thinner than any wall, and every look is filmed on several maps
date: 2026-11-13
scope: games/shooters_edge/labs/gpu_se_monster_fx_lab.cpp
concepts: [camera, line-of-sight, filming, gif, monster, real-map, lab-bug]
---

The `se-monster-fx` lab films a monster's attack from beside the line of fire. Its first
camera tested a few points between it and the subjects and landed inside walls (a sheet of
brown texture) or behind a pillar; testing every 4 map units (no wall is thinner) and trying
several side and back offsets before falling back to over the player's shoulder fixed it.
Even then one map can be hopeless (the big caster on its first map was filmed into a wall twice):
give each kind a list of maps and try another with `--map=` before tuning the camera again.
Look at every sheet before publishing; a lab that ran is not a film that shows the thing.
