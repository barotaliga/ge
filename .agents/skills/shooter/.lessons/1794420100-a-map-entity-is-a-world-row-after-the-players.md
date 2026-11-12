---
title: In the sequel a map entity's world row follows the world and the players, and fields are entities too
date: 2026-11-11
scope: games/shooters_edge/server/sequel_map.cpp, games/shooters_edge/server/sequel_server.cpp, tests/shooters_edge_sequel_server_test.cpp
concepts: [sequel, movers, world-rows, map-row, door-field, plat-field, trace, entity-count]
---

A trace through the sequel's map answers a WORLD ROW, not an entity index: row 0 is the world,
rows 1..kMaxClients the players, then the map's entities (`map_row(e)` = 1 + kMaxClients + e,
`map_entity_of_row` back), then the missiles and the drops. Compare a hit with `map_row(e)`, never
with `e`.

`map_spawn` adds entities of its own (a door team's trigger field, a plat's field above it), so the
entity count a test expects grows by one per plat and per door team, and the hidden list must skip
them (`trigger` set). A plat is solid from the first frame, so a golden whose player walks over a
plat's brush changes when the movers land: rewrite it in the same PR and say why.
