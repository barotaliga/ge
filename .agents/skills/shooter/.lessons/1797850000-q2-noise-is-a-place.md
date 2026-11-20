---
title: A Quake II monster that hears a noise goes to it; only sight makes the player its enemy
date: 2026-11-20
scope: games/shooters_edge/server/q2_monsters.cpp
concepts: [hearing, noise, sound-target, trail, pursuit, monster-test, blaster]
---

`notice` sets `sound_target` and `sound_at` on a noise, never `enemy`; the monster turns on the
player through `found` when it sees him. A test that wakes a monster by firing at it must fire
away from it (a light guard dies to the doubled first bolt before it turns) or make the noise
another way (a jump), and must give it time to turn and see. A monster that lost sight keeps its
enemy for good (`pursue`: the last sighting, then the trail marks newer than `trail_time`);
nothing drops the enemy but its death.
