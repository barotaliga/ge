---
title: The window reaches a game's own saves through RunnerCalls, never q2_of
date: 2026-11-15
scope: games/shooters_edge/server/rules_runner.hpp, games/shooters_edge/client/listen.cpp
concepts: [runner-calls, saves, autosave, seams, shooter]
---

`RunnerCalls::save_problem`, `save`, `take_autosave` and `last_sequence` are filled by rules that
save their own single-player game (Quake II's table today); null means the window's campaign
save (the classic program's) does it. A new per-game question the window asks becomes a call or
a trait on the table with a null default, so every other table (qc, qvm, native) keeps compiling
with its positional initializer unchanged.
