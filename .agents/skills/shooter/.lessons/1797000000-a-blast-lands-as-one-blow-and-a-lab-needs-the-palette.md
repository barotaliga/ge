---
title: A blast's pellets land as one blow; a lab's effects need the pack's palette and the drawn models
date: 2026-11-09
scope: games/shooters_edge/qc/common/weapons.qc, games/shooters_edge/client/world_fx.cpp, games/shooters_edge/labs/gpu_se_deaths_lab.cpp
concepts: [gib, gibs, blood, pellets, multi-damage, quad, super-shotgun, blood-trail, model-flags, palette, fx-state, se-deaths]
---

Our campaign never gibbed from the quad and the double shotgun: each pellet was its own blow, so
the first two killed the body and the rest struck a corpse (`damage` returns on health 0 or
below). The classic sums a blast's pellets on one body into one blow; `fire_pellets_far` now does.

Three things a lab of the window's effects must set up as `main.cpp` does, or it measures the
wrong thing: `FxState::palette` (without it every classic speck is a grey of its index, and the
blood reads as wall chips), `FxState::book`, and `FxState::models` (the drawn models: the gib
trails read each model file's flags from `ModelKind::flags`). A monster's line of sight in a lab
must test the brush entities' boxes too (`in_mover`): doors are not in the map's hull, so
`hull_contents` alone stood the player behind a closed door. And a monster's `health` and the
player's `flags` are the loop's mirrored fields: set them on the `Edict`, not through
`set_field_named`, or the next call overwrites them.
