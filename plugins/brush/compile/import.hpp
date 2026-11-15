// A BUILT MAP BROUGHT BACK TO A SOURCE: any family the reader takes (version 29, BSP2, 38, 46)
// turned into the compiler's source (source.hpp), which the editors' text writes (map_text.hpp),
// so a map can be cut, merged, mixed with another game's and built again (the map SDK,
// docs/levels.md § The map SDK).
//
// - THE BRUSHES: a family that keeps its brushes (38, 46) gives them back as they were, each side
//   on its plane with its texture, flags and value, the contents from its bits. A family that does
//   not (29, BSP2) gives the solid, liquid and sky leaves of its node tree, each leaf a convex cell
//   bounded by the planes on its path from the root, so each becomes one brush; a subtree holding
//   one contents throughout is one cell. The cells on the world's rim are closed by its box, 64
//   units out.
// - THE TEXTURES: a side takes the texture and projection of the surface lying on it (the surface
//   on the side's plane, facing the same way, nearest the side's middle), the projection solved
//   from the surface's largest triangle's texels. A side no surface lies on (a face against
//   another solid) takes the texture most of its brush's sides show; a liquid's or the sky's takes
//   a name that gives its contents (`*water`, `*slime`, `*lava`, `sky`).
// - THE ENTITIES: their keys as they were, a brush model's brushes moved under the entity whose
//   `model` names it (the key dropped); the world first.
//
// The textures' pictures stay in the map: only their names come across.
//
// ge:summary A built map of any family brought back to a compiler source: brushes, textures, keys
// ge:keywords import decompile bsp-to-map map-sdk cells leaves brushes projection solve
// ge:keywords common-format v29 bsp2 v38 v46
// ge:skill brush map-design
// ge:test tests/brush_import_test.cpp
#pragma once

#include <string>

#include "brush/compile/source.hpp"
#include "brush/map.hpp"

namespace ge::brush::mapc {

struct ImportReport {
  int brushes = 0;         // brushes written, every entity's
  int sides = 0;           // sides written
  int textured_sides = 0;  // sides a surface lay on
  int models = 0;          // brush models moved under their entities
};

// `map` as a source in `*out`. False with the reason when the map holds no world model.
bool import_map(const Map& map, Source* out, ImportReport* report, std::string* error);

}  // namespace ge::brush::mapc
