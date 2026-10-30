// THE CLASSIC MODELS AS THE ENGINE DRAWS THEM: a pack's vertex-animated models (.mdl), sprites
// (.spr) and brush items (a small map of its own: a health box, an ammo crate) turned into the
// engine's morph meshes and sprites (render/morph.hpp, render/sprites.hpp), and lit by the map they
// stand in.
//
// - A MODEL (`morph_from_alias`): every frame's positions in the engine's frame (metres, y up; the
//   file's x, z, y, so the reflection turns the winding back), its smooth normals, its skin through
//   the palette. A texel in the fullbright range glows; one in the shirt's or the pants' colour row
//   is TINTED (the morph pass's region A or B, its step the column), so a player is drawn in his
//   own colours and anything else in the palette's own (`kShirtRow`, `kPantsRow` as its tints).
//   One render model a skin: a model's skins (the three armours) are three models.
// - A BRUSH ITEM (`morph_from_brush`): the item map's world faces as one frame, each face's texture
//   copied (tiled as the face tiles it) into a rect of its own skin, so the morph pass draws it.
// - A SPRITE (`sprite_frames_of`): each frame through the palette, index 255 clear, its origin
//   where the file puts it; a texel a map unit.
// - THE ANIMATION (`Groups`): the entity's `frame` (or `skin`) names a group; a group of more than
//   one plays its frames at their intervals, looping (`frame_in`).
// - THE LIGHT UNDER IT (`LightProbe`, `light_under`): the map's lightmap where a line straight
//   down from the point meets the floor, in the lightmap's units (1: the face as painted), from a
//   grid of the floor's triangles made once a map.
//
// ge:summary Classic models, sprites and brush items as the engine's morph meshes and sprites
// ge:keywords classic-models mdl spr brush-item morph_from_alias morph_from_brush sprite_frames_of
// ge:keywords palette tint shirt pants fullbright glow skin frame-group light-probe light_under
// ge:keywords entity-pose items armour health weapons flames
// ge:skill render shooter morph
// ge:test tests/shooters_edge_classic_models_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "brush/map.hpp"
#include "morph/model.hpp"
#include "render/morph.hpp"
#include "render/sprites.hpp"

namespace shooters_edge::models {

inline constexpr int kFullbrightFirst = 224;  // palette indices from here light themselves
inline constexpr uint32_t kShirtRow = 1;      // the palette row a player's top colour replaces
inline constexpr uint32_t kPantsRow = 6;      // and his bottom colour
inline constexpr uint32_t kColourRows = 14;   // the palette's rows a colour can name

// The palette's colour rows as tint ramps (row r: indices 16 r .. 16 r + 15, linear), so a tinted
// texel drawn with its own row's tint is its own colour.
ge::render::TintRamps palette_ramps(std::span<const uint8_t> palette);

// Skin `skin` (clamped) of `model` as the engine's morph model.
ge::render::MorphModel morph_from_alias(const ge::morph::Model& model,
                                        std::span<const uint8_t> palette, int skin);
// An item map's world model as one frame, its faces' textures in its skin.
ge::render::MorphModel morph_from_brush(const ge::brush::Map& item,
                                        std::span<const uint8_t> palette);
// A sprite's frames, a texel a map unit.
std::vector<ge::render::SpriteFrame> sprite_frames_of(const ge::morph::Sprite& sprite,
                                                      std::span<const uint8_t> palette);
ge::render::SpriteFacing facing_of(ge::morph::Facing facing);

// A file's frames (or skins) by group: group g is frames [first[g], first[g] + count[g]), each
// shown until its `ends` (s, cumulative; empty for a lone frame).
struct Groups {
  std::vector<uint32_t> first, count;
  std::vector<std::vector<float>> ends;
};
Groups frame_groups(const ge::morph::Model& model);
Groups skin_groups(const ge::morph::Model& model);
Groups sprite_groups(const ge::morph::Sprite& sprite);
// The frame group `group` shows at time `t` s (a group out of range: group 0).
uint32_t frame_in(const Groups& groups, int group, double t);

// The rotation an entity's angles (degrees: pitch, yaw, roll, the file's) give a model drawn in
// the engine's frame. `alias`: a vertex-animated model, whose pitch the classic draws reversed.
void rotation_of(const float angles[3], bool alias, float out[4]);

struct LightProbe {
  float cell = 64;  // map units
  int32_t x0 = 0, y0 = 0, nx = 0, ny = 0;
  std::vector<uint32_t> start;    // nx * ny + 1 offsets into `tris`
  std::vector<uint32_t> tris;     // the first index of a floor triangle in Map::indices
  std::vector<int32_t> lightmap;  // each triangle's surface's lightmap, as `tris`
};
LightProbe make_probe(const ge::brush::Map& map);
// The light (lightmap units, rgb) under `p` (map units); false (and 1s) when no floor is found
// within `depth` units below.
bool light_under(const LightProbe& probe, const ge::brush::Map& map, const float p[3], float rgb[3],
                 float depth = 512);

}  // namespace shooters_edge::models
