---
title: A series' one config saves each setting with its value outside the loaded game
date: 2026-11-18
scope: engine/console/cfg.cpp, engine/console/cvar_layers.cpp, games/shooters_edge/client/settings.cpp, games/shooters_edge/client/cvar_layers.cpp
concepts: [series-config, one-config, save-scope, own, same, set-aside, synonym]
---

One config for every game of a series cannot simply save the live cvars: while Quake II is
loaded, `crosshair` is Quake II's (`own`), so Quake's value would be lost or replaced
by a number that means another picture. `save_text(SaveScope)` writes only the layers a filter
accepts, each setting with the value it has there: an `own` row's set-aside definition gives its
kept value, a `same` row belongs to the layer that first registered it (the player's value if set,
else that layer's default, so Quake II's 2.5 never becomes Quake's). Read the series
file with the game's layer dropped (`without_game_cvars`), then put the layer back with its own
values and run the game's own file. Dropping a `same` layer now keeps a value the player set.
