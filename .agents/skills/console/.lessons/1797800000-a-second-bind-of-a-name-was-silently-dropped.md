---
title: A second bind of a name was silently dropped; declare how layers meet
date: 2026-11-18
scope: engine/console/cvar.cpp, engine/console/cvar_layers.cpp, games/shooters_edge/client/cvar_layers.cpp, games/shooters_edge/menu/game_menu.cpp, plugins/classicenhance/switches.cpp
concepts: [cvar-layer, overlap, clash, same-name, bind-twice, ce-switches, per-game-cvar]
---

`Cvars::bind` of a name already registered returned the first cvar and never wrote the second
field. The menu bound every `ce_` switch after the picture had, so a cfg's `ce_upscaler 0` never
reached the bake's copy, and nothing said so. Registrations now carry a layer (`CvarLayer`), and a
name a later layer meets must be declared `Overlap::same` (both fields written, its default) or
`Overlap::own` (the game's own meaning); an undeclared one is recorded in `clashes()`. The census
test `tests/shooters_edge_cvar_layers_test.cpp` registers the window's layers per game and fails
on a clash; it found the 40 switches on its first run. Never print clashes to a player: the
loaded game simply means its own thing by a name.
