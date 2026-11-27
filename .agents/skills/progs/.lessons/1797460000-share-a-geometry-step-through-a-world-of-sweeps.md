---
title: Share a monster's geometry by passing a world of sweeps, and hide a test player by his eye
date: 2026-11-27
scope: games/shooters_edge/server/q2_monster_mind.hpp, games/shooters_edge/rules/campaign_q2_kinds.cpp, tests/shooters_edge_campaign_monsters_test.cpp
concepts: [remix, side-step, chase, tracebox, shared-mind, campaign, test-fixture]
---

A decision that needs the map (the side step's three box sweeps) still lifts into the shared
mind: a template over the vector type taking a world with `sweep(to)`, `length(v)` and
`axes(dir, forward, right)`. Quake II's server answers with its own sweep and view axes, so its
bits stay the same; the campaign answers with the `tracebox` call (builtin 90, added to
`calls.hpp`), `vlen` and `makevectors`, saving and restoring the view-axis globals round it. A
chase state the mind keeps across frames (`side_step`, `real_goal`) needs campaign fields, or the
mind's second half never runs. In a campaign test the host rewrites the player's origin every
frame, so setting it does not hide him; set his `view_ofs` z far below the floor each frame and
every eye-to-eye line to him is blocked.
