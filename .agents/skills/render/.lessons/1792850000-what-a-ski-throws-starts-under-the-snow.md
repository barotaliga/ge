---
title: What a ski throws starts under the snow, so bury a particle only on its way down
date: 2026-10-22
scope: engine/render/particles_sim.wgsl
concepts: [particles, ground, sink, spray, powder, emission, kill]
---

The first spray lab's carve held no live particles at the shot although the arithmetic threw
grains every frame: a grain born at the skis' base is under the untouched surface wherever the
skis sink, and the flight killed anything below the ground's height on its first step. Kill a
particle at the ground only when it is falling (`v.y < 0`); one under the snow and rising is
coming up out of it. Check a pool's live count beside the births in a lab before tuning rates.
