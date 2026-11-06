---
title: A lab's own map must not be called arena: the listen loads its built-in test arena
date: 2026-11-06
scope: games/shooters_edge/client/listen.hpp, games/shooters_edge/labs/gpu_se_speedjump_lab.cpp
concepts: [load-named-map, arena, fixture-map, lab-map, map-name, built-in-arena]
---

`cl::load_named_map(files, "arena", …)` returns the built-in test arena (its stairs, its pit,
its own spawns), never a pack's `maps/arena.bsp`. A lab that wrote its own open room as
`maps/arena.bsp` found its player walled into a box 336 units across and its spawn ignored; the
paths chart showed it at once (a path with square corners in a room drawn far bigger). Name a
lab's map something no built-in uses (`sjarena`), and draw the run's path from above before
trusting a speed: a wall cuts speed the way a bug in the move would.
