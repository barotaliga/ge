---
title: The classic rules run on a brush (version 38) map by tracing its models as brush solids
date: 2026-11-13
scope: plugins/shooter/server.hpp, plugins/shooter/trace.cpp, games/shooters_edge/server/qc_server.cpp, games/shooters_edge/client/remix.cpp
concepts: [classic-remix, model-solids, brush-tracer, version-38, other-game-map, palette]
---

A brush map has no clip hulls, so the classic rules' hull trace has nothing to walk. Fill
`sh::Server::model_solids` (index = model number) with `sh::brushes_solid` over a
`ge::brush::BrushTracer` the QC server keeps alive, and `world_contents`, `clip_to_entity` and the
client's `add_solid` take the solid instead of the hull: a func_wall's `*1` then blocks like any
wall. The rules picked by the map's family (`open_map`: brushes run the sequel's rules) must be
overridden for such a map (`LoadedMap::program_rules`). In the window, another game's map reads
its textures through the other game's packs with the played game's mounted OVER them, so models
and sounds stay the played game's, and its walls and sky take the map game's palette while
models keep the played palette. `se-client --remix=<install> --map=<map>` draws it.
