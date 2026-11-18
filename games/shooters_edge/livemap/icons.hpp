// THE ITEM ICONS OF A MAP PICTURE (the `map-overview` lab): a thing on a map (armour, a power, a
// weapon, a health or ammo box) as a picture of the game's own model from its pack, drawn on the
// CPU from three quarters above with its skin, lit from the upper left, cut out with a dark rim, so
// a picture of a map can show it larger than in the game. The first family's models (.mdl, and its
// health and ammo boxes, which are small maps) and the second's (.md2 with their pictures).
//
// THE LIVE MAP's FIGURES (live_map.hpp) are drawn the same way, at a fixed size rather than fitted:
// a model read once (`ModelArt`), one frame of it (the entity's frame number) in a player's shirt
// and pants colours (`art_mesh`), turned to the entity's yaw and seen from the south from three
// quarters above (`draw_figure`), its origin at the picture's middle, so a figure animates and
// turns in place without its size jumping from frame to frame.
//
// ge:summary The map picture's item icons: the pack's models drawn on the CPU, cut out
// ge:keywords item-icons model-icons icon cut-out three-quarter armour power weapon health ammo
// ge:keywords map-overview mdl md2 box-model live-map figure player-colours translation
// ge:keywords draw_figure model_art art_mesh
// ge:skill map-design labs
// ge:test tests/shooters_edge_live_map_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "morph/model.hpp"
#include "shooters_edge/client/files.hpp"
#include "ui/image.hpp"

namespace shooters_edge::livemap {

// Triangles with their texture: positions in map units (z up), texels into `tex` within each
// vertex's rectangle.
struct Mesh {
  std::vector<float> pos;   // xyz a vertex
  std::vector<float> st;    // texels
  std::vector<float> rect;  // x y w h a vertex
  std::vector<uint32_t> idx;
  std::vector<uint8_t> tex;  // RGBA
  int tw = 0, th = 0;
};

// `m` from three quarters above (`azimuth` round z from +x toward +y, `elevation` up), lit from
// the upper left, cut out on a clear ground with a dark rim, `size` px square.
ge::ui::Image draw_icon(const Mesh& m, int size, double azimuth_deg, double elevation_deg);
// A model file (.mdl or .md2) as a mesh, its skin `skin`; false when the files have none.
bool model_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
                const std::string& path, int skin, Mesh* out);
// A small map (the first family's health and ammo boxes) as a mesh.
bool box_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
              const std::string& path, Mesh* out);

// A model read once for many pictures: the file's frames and skins (their palette indices kept,
// so a player's colours are put on at drawing) and the palette.
struct ModelArt {
  ge::morph::Model model;
  std::vector<uint8_t> palette;  // 768 bytes
  std::vector<uint8_t> picture;  // the second family's skin as RGBA (its own picture file)
  int pw = 0, ph = 0;
  bool ok = false;
};
// `path` read into `out`; false (and `out->ok` false) when the files have no such model.
bool read_model_art(const shooters_edge::client::SearchPath& files,
                    std::span<const uint8_t> palette, const std::string& path, ModelArt* out);
// The palette index `i` of a skin in a player's colours: the shirt's sixteen (16..31) in `top`'s
// row, the pants' (96..111) in `bottom`'s, a colour of 8 or more running the other way; -1 keeps
// the skin's own.
int translate(int i, int top, int bottom);
// The file's frame an entity's frame number `group` shows at `time_s`: a group of frames played on
// its intervals, a lone frame itself; the first frame for a number the file lacks.
int frame_index(const ModelArt& a, int group, double time_s);
// Frame `frame` (an index into the file's frames) in skin `skin` and the colours.
Mesh art_mesh(const ModelArt& a, int frame, int skin, int top, int bottom);
// `m` with its nose up `pitch_deg`, turned to `yaw_deg` (counter-clockwise from +x seen from
// above, as the map's) and seen from the south from `elevation_deg` above, `px_per_unit` px a unit,
// its origin at the middle of a `size` px picture, cut out with a dark rim; what falls outside the
// picture is cut.
ge::ui::Image draw_figure(const Mesh& m, int size, double yaw_deg, double pitch_deg,
                          double elevation_deg, double px_per_unit);

enum class Tier : uint8_t { power, major, minor };

struct ItemKind {
  const char* cls;
  int flags;  // spawnflags it needs (-1 any)
  const char* model;
  int skin;
  bool box;  // a small map, not a model
  Tier tier;
  const char* tag;  // the badge's letters, and the label under a weapon
  uint32_t colour;  // its glow and badge
  int respawn;      // seconds, 0 none shown
};

// What the entity is (nullptr: not an item drawn, or one kept out of deathmatch).
const ItemKind* item_of(const ge::brush::Entity& e);
// The icon's size on the picture, px.
int icon_size(Tier t);

}  // namespace shooters_edge::livemap
