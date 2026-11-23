// A CLASSIC FIGURE AS A PICTURE ON THE CPU: a model file of either family read once (`ModelArt`:
// its frames and skins, the skins' palette indices kept so a player's colours go on at drawing),
// one frame of it in a player's shirt and pants colours as the overview's textured mesh
// (`art_mesh`), and the pieces a player's look adds: a skin picture of palette indices worn in
// place of the model's own (`wear_indexed_skin`: the first family's `skins/<name>.pcx`) and a
// second mesh joined to the first (`join_meshes`: the weapon a figure carries). The drawing is the
// engine's (engine/overview/mesh_picture.hpp); the live map's figures and the player page's figure
// are both drawn from here.
//
// - **Colours** (`translate`): the shirt's sixteen indices (16..31) take row `top`'s colours, the
//   pants' (96..111) row `bottom`'s; a row of 8 or more runs the other way (those rows are stored
//   bright to dark), so the painter's shading is kept. -1 keeps the skin's own.
// - **A skin picture of indices** (`wear_indexed_skin`): laid at the top left of a 320 x 200 field
//   of index 0, and the field's top-left 296 x 194 stretched over the model's whole skin, whatever
//   the skin's own size (porting/ notes: player-skins).
//
// ge:summary A classic model read once and one frame of it as a CPU mesh in a player's colours
// ge:keywords figure-art model_art art_mesh translate player-colours shirt pants top bottom
// ge:keywords wear_indexed_skin skins-pcx join_meshes carried-weapon live-map player-page
// ge:skill classic morph
// ge:test tests/classic_figure_art_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "morph/model.hpp"
#include "overview/mesh_picture.hpp"

namespace ge::classic {

// A file of the game's by its path: viewed in a pack or copied into `store`; empty when none.
using ReadFile =
    std::function<std::span<const uint8_t>(std::string_view path, std::vector<uint8_t>& store)>;

// A model read once for many pictures: the file's frames and skins and the palette.
struct ModelArt {
  ge::morph::Model model;
  std::vector<uint8_t> palette;  // 768 bytes
  std::vector<uint8_t> picture;  // the second family's skin as RGBA (its own picture file)
  int pw = 0, ph = 0;
  bool ok = false;
};
// `path` read into `out`; false (and `out->ok` false) when the files have no such model. A
// `skin_path` names the picture worn in place of the model's own (a Quake II player's look:
// `players/male/grunt.pcx`).
bool read_model_art(const ReadFile& files, std::span<const uint8_t> palette,
                    const std::string& path, ModelArt* out, const std::string& skin_path = "");
// A skin picture of the second family (`path`, a .pcx through its own palette or `palette`, a
// .tga or a .png) as RGBA; empty when the files have none or it is damaged.
std::vector<uint8_t> read_skin_picture(const ReadFile& files, std::span<const uint8_t> palette,
                                       const std::string& path, int* w, int* h);
// The palette index `i` of a skin in a player's colours (above); -1 keeps the skin's own.
int translate(int i, int top, int bottom);
// The file's frame an entity's frame number `group` shows at `time_s`: a group of frames played on
// its intervals, a lone frame itself; the first frame for a number the file lacks.
int frame_index(const ModelArt& a, int group, double time_s);
// Frame `frame` (an index into the file's frames) in skin `skin` and the colours.
ge::overview::Mesh art_mesh(const ModelArt& a, int frame, int skin, int top, int bottom);

// The first family's skin picture of palette indices (`indices`, `w` x `h`, rows from the top)
// worn as `a`'s every skin (above); false (`a` untouched) when the picture is larger than the
// field or the model has no skins of indices.
inline constexpr int kSkinFieldWidth = 320, kSkinFieldHeight = 200;
inline constexpr int kSkinWornWidth = 296, kSkinWornHeight = 194;
bool wear_indexed_skin(ModelArt& a, std::span<const uint8_t> indices, int w, int h);

// `add` joined to `into`: its triangles, and its texture stacked under `into`'s (the rectangles
// moved with it), so one picture draws both.
void join_meshes(ge::overview::Mesh& into, const ge::overview::Mesh& add);

}  // namespace ge::classic
