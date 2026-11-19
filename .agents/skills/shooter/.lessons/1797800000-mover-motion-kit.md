---
title: Frame-stepped mover motion is plugins/shooter/mover_motion.hpp, templates over named fields
date: 2026-11-15
scope: plugins/shooter/mover_motion.hpp, games/shooters_edge/server/q2_map.cpp
concepts: [movers, doors, plats, trains, seams, shooter]
---

`begin_move`, `begin_turn` and `step_motion` are templates over any struct with the fields they
name (origin, angles, dest, adest, dir, velocity, avelocity, speed, accel, decel, frames, rest,
now_speed, next_speed, top, decel_span, last, accelerated), so a game keeps its own entity and
only adds a `MoverFrame` (step and rate). What happens on arrival (the game's act) stays the
game's: `step_motion` answers `arrived` and the caller runs it. Keep any lift bit-exact: the rate
multiply stays `v * f.rate` where it was `v * 10`, with the same operand order.
