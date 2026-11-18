// A VERTEX-ANIMATED MODEL AS THE OVERVIEW DRAWS IT (engine/overview): one frame of a model in one
// skin as the overview's textured mesh (`overview_mesh`), its skin painted from the palette through
// an optional remap of its indices (a player's colours) or taken from a picture the game read (the
// second family's skin files); and the frame a group shows at a time (`frame_at`), so a figure
// on a map from above animates as the game plays it. Any game on these model families gets its
// figures from here; it adds only what its colours and its things mean.
//
// ge:summary One frame of a vertex-animated model in a skin as the overview's mesh
// ge:keywords overview figure live-map frame-group skin remap player-colours overview_mesh frame_at
// ge:skill morph map-design
// ge:test tests/overview_plate_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "morph/model.hpp"
#include "overview/mesh_picture.hpp"

namespace ge::morph {

// How a skin is painted: its palette indices through `remap` (256 entries, or empty: as they
// are) into `palette` (768 bytes); a model with no skins of its own takes `picture` (RGBA,
// `pw` by `ph`).
struct SkinPaint {
  std::span<const uint8_t> palette;
  std::span<const uint8_t> remap;
  std::span<const uint8_t> picture;
  int pw = 0, ph = 0;
};

// Frame `frame` (an index into the frames, clamped) in skin `skin` (clamped) as the overview's
// mesh; empty when the model has no frames.
ge::overview::Mesh overview_mesh(const Model& m, int frame, int skin, const SkinPaint& paint);
// The frame the group `group` shows at `time_s`: a group of frames played on its intervals, a
// lone frame itself; the first frame for a group the model lacks.
int frame_at(const Model& m, int group, double time_s);

}  // namespace ge::morph
