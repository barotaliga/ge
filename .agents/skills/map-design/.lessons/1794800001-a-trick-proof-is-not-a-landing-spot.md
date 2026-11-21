---
title: A trick's proof asks for its landing within 8 units; a plan writes where a player stands
date: 2026-11-15
scope: games/shooters_edge/labs/map_intent_lab.cpp, plugins/shooterbots/nav.cpp, plugins/shooterbots/jump_finder.cpp
concepts: [map-intent, trick, head-jump, hop, prove-link, arrive, calc-any-rocket-jump, starter-maps]
---

Building the starter maps, half the written tricks read NO though bots made them: `prove_link`
counts a moving jump only when it comes down within `arrive` (8 units) of `to`, so a hop over 400
units passed only with `to` at two exact spots, and a rocket jump through a window came down on
the sill 80 short of the balcony behind it. The lab now counts a jump that comes down on the
target's floor (18 or 24 up or down) when the rest is a walk, and calculates rocket jumps first
(`calc_any_rocket_jump`), printing where the nearest arc came down when none lands: that column
is what tells a too-tight window (the arc ended on the stairs below it) from a wrong `to`.

A head jump was proven from a start lifted 56 into the air, where nobody can jump: `map-moves`
read 32 for it. `prove_head_jump` stands a teammate's box under the jumper; a running head jump
now reads 64, and it flies about 190 units, so its `to` belongs against the wall it hits.

A start inside a stair (`window-rj` from 690,512 on the forge's stairs) fires no arc at all: a
`-` in the nearest-arc column with health 0 means the start is in a solid.
