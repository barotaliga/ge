---
title: The window draws a sequel listen server's own model list, so a rename belongs in that list
date: 2026-11-14
scope: games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/client/world_models.cpp, games/shooters_edge/client/sequel_adapter.cpp, games/shooters_edge/client/held.cpp
concepts: [classic-remix, cr-items, sv-wear, sequel, model-list, brush-model, palette, held-weapons]
---

Renaming only `RulesWorld::models` (what the wire and a remote client read) changed nothing in the
window: its world models are read from the pool's `RulesCommon::models` (`l.server->models`), so the
sequel's own md2 items were drawn at the classic heights and looked sunk (a centred box lowered 15
units shows only its top). Rename `s.models` and `s.sounds` in place in `fill_lists`, after every
start-time lookup by name, and copy them to the world. To tell which model is drawn, compare the
same spot under both games' rules: a model lying on its side was the other game's model, not a bad
rotation. The sequel's console takes `set <cvar> <value>` too now, so the window's server lines work
for both rules. A classic `.mdl` in the sequel window wants stand-in skins through
`gfx/palette.lmp`; a classic `.bsp` box has no stand-in, so its texture indices are remapped to the
nearest sequel colour, glowing (from 224) only to glowing. The sequel hand's frames index its own
model's runs; on a classic view model (rest + firing frames) spread the firing run over them and
show the rest otherwise (`sequel_frame`).
