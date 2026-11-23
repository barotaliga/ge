---
title: A brush model turns about the map's origin, before its offset
date: 2026-11-23
scope: plugins/brush/app/bsp_draw.hpp, plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_cull.cpp, games/shooters_edge/client/world_models.cpp
concepts: [movers, rotating-brush-models, set_movers, quaternion, cull-box]
---

`ba::Mover::rotation` (a quaternion in the engine's frame, from `rotation_of` with the pitch not
reversed) turns a model's vertices and normals in `moved` before the offset is added. A rotating
model is built round its own centre, so its offset is the entity's origin; a door built in place
with angles left set would swing about the map's origin, which is why the programs clear a door's
angles after reading its move direction. The cull boxes turn with it (`turned_box`: the eight
corners), or a turned fan is culled by its upright box. The shadow pass still draws movers where
the map built them.
