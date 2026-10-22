---
title: A run's moments are scenes like the named views, stepped by the same driver
date: 2026-10-22
scope: games/fall_line/labs/gpu_world_lab.cpp
concepts: [world, moments, run, driver, bot, chase-camera, scene]
---

The `world` lab's `--moments=3,15,30` steps the game's own `Driver` with the bot (and
`--rivals` racers) to each second, then shoots the chase camera on the player like any named
view. Stepping once and shooting at each moment in order keeps the cost one run, and the
moments' states are deterministic, so the scene list (a deterministic `moments` metric) and the
check counters stay digestible while the pictures' GPU timings stay measured.
