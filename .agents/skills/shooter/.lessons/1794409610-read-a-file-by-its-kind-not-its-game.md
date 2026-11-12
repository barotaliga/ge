---
title: A second classic game is a content row, a game row and a thin adapter over adapter_parts
date: 2026-11-11
scope: games/shooters_edge/client/adapter_parts.cpp, games/shooters_edge/client/sequel_adapter.cpp, plugins/brush/wal.cpp
concepts: [sequel, second-game, adapter, adapter-parts, wal, palette, content-row, remix, labs-data]
---

The calls two games answer alike live in `client/adapter_parts.hpp`, and they pick a reader by
the FILE (a map that carries its pictures or one whose wall textures are `textures/<name>.wal`;
a model by its extension), never by the game row, so a pack of one game in another's folder
reads. A game's own `.cpp` keeps only its pictures, menu, held weapons and demos. The world
draw's palette comes from the content row (`game_palette`, a PCX's tail for the sequel): never
write `gfx/palette.lmp` in shared code. `GameProfile` rows need every field named (the build's
missing-field-initializers). The sequel's demo pack for labs: clone the public install repository
Niclas named (porting/shooters-edge/sequel-plan.md) outside the tree and pass `--game` to
`se-sequel`; its pictures stay private.
