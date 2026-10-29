---
title: A material grows grass by its surface word; read its floors off the built meshes, not the planes
date: 2026-10-29
scope: plugins/rooms/grass.cpp, plugins/rooms/labs/blades.hpp, tests/rooms_grass_test.cpp
concepts: [grass, surface, material, floor, cover, brush, room, clone, texmap, lab]
---

A level's grass lives in `grass.hpp`, not in the builders: `level_grass` reads the materials'
surface words and `grow_on_brushes` / `grow_on_rooms` read the meshes the builders already made,
so the CSG has removed every buried face (the floor under a pillar, a pool's bottom) before grass
sees it. A brush face's up is its winding's cross product (the stored normal is shaded when
smoothing is on); a grown room's is its vertex normals, which point into the room. In a clone,
a texture-map row that carries only a `surface` is accepted without a recipe reference, so the
clone lab can add a lawn row for any texture. In a test, compare the cover's area with the same
material's straight-up faces in the mesh: it catches a wall or a hidden face that slipped in.
The labs draw blades through `labs/blades.hpp` (both windings, three quads a blade); 70,000
blades at 320 px is a few seconds of CPU raster.
