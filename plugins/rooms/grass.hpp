// A ROOM LEVEL'S GRASS: the floors of every material whose surface names grass, grown into one
// cover for engine/grass to scatter and the renderer to draw. A material's surface reference
// `grass <recipe>` (a level's `surface <material> grass meadow height=0.6` line, or a cloned map's
// texture map row) names the kind: a preset and its overrides, as `grass::parse_kind` reads them.
// Materials with the same recipe share one kind; a cover holds at most `grass::kMaxKinds`.
//
// WHERE IT GROWS. On the drawn triangles of such a material whose face looks up: a brush face's
// outward normal, or a grown room's normal into the room, at least `kFloorUp` up (37 degrees off
// level), so a floor and a gentle ramp grow grass and a wall, a ceiling or a steep bank of the same
// material stay bare. A hidden face, a clip brush's and a liquid's never grow it.
//
// Pure: the same level and meshes give the same cover to the bit. The cover's seed is the rooms'
// seeds mixed (0 for a level of brushes alone), so a level re-seeded grows other blades.
//
// ge:summary A room level's grass: grass-surfaced materials' up-facing floors grown into a cover
// ge:keywords grass surface level floor cover recipe material brush room grow lawn meadow
// ge:keywords level-grass grass-surface add_mesh
// ge:skill rooms grass
// ge:test tests/rooms_grass_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "grass/cover.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/build.hpp"
#include "rooms/level.hpp"

namespace ge::rooms {

inline constexpr float kFloorUp = 0.8f;  // a growing face's normal y at the least

// The recipe a material's surface names when that surface is grass (`grass <recipe>`), else "".
std::string_view grass_recipe(const Material& material);

struct LevelGrass {
  grass::Cover cover;
  std::vector<int> kind;            // a material's kind in cover.kinds (-1: grows none)
  std::vector<std::string> faults;  // a recipe that does not parse, or one kind too many
};

// The level's grass kinds, one a distinct recipe, and the cover's seed; no triangles yet.
LevelGrass level_grass(const Level& level);
// The up-facing, drawn, solid triangles of a grass material in a level's brushes, added.
void grow_on_brushes(LevelGrass& grass, const BrushMesh& mesh, float min_up = kFloorUp);
// The floors of a grass material in a level's grown rooms (build.hpp), added. A triangle grows
// when its three corners wear the material.
void grow_on_rooms(LevelGrass& grass, std::span<const Mesh> rooms, float min_up = kFloorUp);

}  // namespace ge::rooms
