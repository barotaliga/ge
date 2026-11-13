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
//   one plays its frames at their intervals, looping (`frame_in`), or flows between them
//   (`frames_between`).
// - THE LIGHT UNDER IT (`LightProbe`, `light_under`): the map's lightmap where a line straight
//   down from the point meets the floor, in the lightmap's units (1: the face as painted), from a
//   grid of the floor's triangles made once a map.
//
// ge:summary Classic models, sprites and brush items as the engine's morph meshes and sprites
// ge:keywords classic-models mdl spr brush-item morph_from_alias morph_from_brush sprite_frames_of
// ge:keywords palette tint shirt pants fullbright glow skin frame-group light-probe light_under
// ge:keywords entity-pose items armour health weapons flames frames_between frame-lerp
// ge:keywords player-skin player_skin skin_of_indices skins-pcx reversed-rows
// ge:skill render shooter morph
// ge:test tests/shooters_edge_classic_models_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/map.hpp"
#include "morph/model.hpp"
#include "render/morph.hpp"
#include "render/sprites.hpp"

namespace shooters_edge::models {

inline constexpr int kFullbrightFirst = 224;  // palette indices from here light themselves
inline constexpr uint32_t kShirtRow = 1;      // the palette row a player's top colour replaces
inline constexpr uint32_t kPantsRow = 6;      // and his bottom colour
inline constexpr uint32_t kColourRows = 14;   // the palette's rows a colour can name
inline constexpr uint32_t kReversedRow = 8;   // rows from here run bright to dark: read backwards

// The palette's colour rows as tint ramps (row r: indices 16 r .. 16 r + 15, linear; from
// kReversedRow up the other way round, 16 r + 15 down to 16 r, since those rows are stored bright
// to dark), so a tinted texel drawn with its own row's tint is its own colour, and a skin's dark
// folds stay dark in every colour.
ge::render::TintRamps palette_ramps(std::span<const uint8_t> palette);

// Skin `skin` (clamped) of `model` as the engine's morph model.
ge::render::MorphModel morph_from_alias(const ge::morph::Model& model,
                                        std::span<const uint8_t> palette, int skin);
// A skin of palette `indices` (`width` x `height`, rows from the top) as the morph pass draws it:
// the fullbright range glowing, the shirt's and the pants' rows tinted.
ge::render::MorphSkin skin_of_indices(std::span<const uint8_t> indices, uint32_t width,
                                      uint32_t height, std::span<const uint8_t> palette);

// A PLAYER'S OWN SKIN: a picture of palette indices (a `skins/<name>.pcx`, its own palette
// ignored) laid at the top left of a kPlayerPictureWidth x kPlayerPictureHeight field of index 0,
// and the field's top-left kPlayerSkinWidth x kPlayerSkinHeight stretched over the whole of the
// figure's coordinates, whatever its own skin's size; tinted and lit as the model's own skin is.
// False (`out` untouched) for a picture larger than the field or damaged.
inline constexpr int kPlayerPictureWidth = 320, kPlayerPictureHeight = 200;
inline constexpr int kPlayerSkinWidth = 296, kPlayerSkinHeight = 194;
bool player_skin(const ge::assets::IndexedImage& picture, std::span<const uint8_t> palette,
                 ge::render::MorphSkin* out);

// WHICH SKIN A PLAYER WEARS. The player's choice (look.hpp's noskins, baseskin and allskins):
struct SkinChoice {
  int noskins = 0;            // 0, 2: the skins drawn; 1: every player in the figure's own skin
  std::string base = "base";  // the skin of a player who names none, or one the files lack
  std::string all;            // one skin on every player ("": each their own)
};
// A skin name as looked up: its extension dropped, lower case, cut to kSkinNameChars; empty for a
// name that climbs out of `skins/` (holds "..") or starts with a dot.
inline constexpr size_t kSkinNameChars = 15;
std::string skin_key(std::string_view name);
// The skin a player naming `skin` wears, out of `read` (skin_key -> its morph model): `all` over
// their own, `base` for none or one not read, and the figure's own (render's kOwnSkin) under
// `noskins 1` or when neither is there.
uint32_t choose_skin(const SkinChoice& choice, std::string_view skin,
                     const std::unordered_map<std::string, uint32_t>& read);

// THE SECOND GAME'S LOOKS: a player's `skin` is "<model>/<skin>", the figure's folder under
// `players/` and a picture in it. Which figure and skin they are drawn in, out of the folders whose
// figure was read (`figures`, by lower-case folder) and the skins read (`skins`, by
// "<folder>/<skin>"): the folder up to the first '/' (else up to a backslash; with neither, none
// and the rest after its first character); a folder not read is kDefaultFigure's; a skin missing
// from the folder is the default figure's of that name (the figure turning to it too), else the
// folder's kDefaultSkin. `noskins 1` or no look: the default figure in its default skin. `folder`
// empty when not even the default figure was read; `skin` kOwnSkin when no skin was read.
inline constexpr std::string_view kDefaultFigure = "male", kDefaultSkin = "grunt";
struct Look {
  std::string folder;
  uint32_t skin = ge::render::kOwnSkin;
};
Look choose_look(const SkinChoice& choice, std::string_view look,
                 const std::unordered_map<std::string, uint32_t>& figures,
                 const std::unordered_map<std::string, uint32_t>& skins);

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
// The same drawn between its frames: the frame shown (`a`), the next round the loop (`b`) and the
// share of the way to it (`f`, 0..1), so a looping group (a torch's flame) flows rather than
// steps. A lone frame: `a` = `b`, `f` 0.
void frames_between(const Groups& groups, int group, double t, uint32_t& a, uint32_t& b, float& f);

// The rotation an entity's angles (degrees: pitch, yaw, roll, the file's) give a model drawn in
// the engine's frame. `alias`: a vertex-animated model, whose pitch the classic draws reversed.
void rotation_of(const float angles[3], bool alias, float out[4]);

// The colour a skin's glowing texels paint (linear, its brightest channel 1: a grenade's ends a
// pure red), and their share of the skin; 0 (and black) when nothing in it glows.
float glow_colour(const ge::render::MorphSkin& skin, float rgb[3]);

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
