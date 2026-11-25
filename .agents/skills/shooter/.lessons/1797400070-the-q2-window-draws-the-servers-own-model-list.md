---
title: The window draws a Quake II listen server's own model list, so a rename belongs in that list
date: 2026-11-14
scope: games/shooters_edge/server/q2_server.cpp, games/shooters_edge/client/world_models.cpp, games/shooters_edge/client/q2_adapter.cpp, games/shooters_edge/client/held.cpp
concepts: [classic-remix, cr-items, sv-wear, q2, model-list, brush-model, palette, held-weapons]
---

Renaming only `RulesWorld::models` (what the wire and a remote client read) changed nothing in the
window: its world models are read from the pool's `RulesCommon::models` (`l.server->models`), so the
Quake II's own md2 items were drawn at the classic heights and looked sunk (a centred box lowered 15
units shows only its top). Rename `s.models` and `s.sounds` in place in `fill_lists`, after every
start-time lookup by name, and copy them to the world. To tell which model is drawn, compare the
same spot under both games' rules: a model lying on its side was the other game's model, not a bad
rotation. Quake II's console takes `set <cvar> <value>` too now, so the window's server lines work
for both rules. A classic `.mdl` in Quake II window wants stand-in skins through
`gfx/palette.lmp`; a classic `.bsp` box has no stand-in, so its texture indices are remapped to the
nearest Quake II colour, glowing (from 224) only to glowing. Quake II hand's frames index its own
model's runs; on a classic view model (rest + firing frames) spread the firing run over them and
show the rest otherwise (`q2_frame`).
