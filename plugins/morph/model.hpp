// A VERTEX-ANIMATED MODEL OR A SPRITE IN THE CLASSIC FORMATS, read into one view: the skins as
// palette indices, the mesh as triangles over vertices that carry their texture coordinates, and
// every frame as a full set of positions (a morph target), named and timed as the file times
// them. Stated once here; read.hpp fills it from the file's bytes, refusing damage, and
// write.hpp makes fixture files that read back into it.
//
// - **Vertices** are split where the file shares a position between two texture coordinates (a
//   vertex on the skin's seam used by a back-facing triangle reads the skin's right half), so a
//   vertex here has exactly one position a frame and one `st`.
// - **Coordinates** are the file's: model units, z up, already scaled and translated by the
//   header (a frame's packed bytes times `scale` plus `translate`). `to_engine` in
//   `plugins/brush/map.hpp` turns them into the engine's frame, the same units as a map's.
// - **Normals** are not read: the file's are indices into a fixed table of directions, and a
//   reader that recomputes them from the frame's own triangles needs no table.
// - **Frames** in a group play in turn at the group's intervals; a lone frame is a group of one
//   with no interval. **Skins** likewise.
//
// ge:summary Classic vertex-animated models and sprites in one view: skins, mesh, morph frames
// ge:keywords morph model mdl sprite spr skin frame group interval vertex-animation seam palette
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ge::morph {

struct Skin {
  int width = 0, height = 0;
  std::vector<uint8_t> indices;  // width * height palette indices, rows top to bottom
  float interval = 0;            // seconds this skin shows in its group; 0 for a lone skin
  int group = 0;                 // which group of skins it is in
};

struct Frame {
  std::string name;
  std::vector<float> positions;  // xyz a vertex, model units
  std::array<float, 3> min{}, max{};
  float interval = 0;  // seconds this frame shows in its group; 0 for a lone frame
  int group = 0;
};

struct Model {
  std::vector<Skin> skins;
  std::vector<float> st;          // two a vertex: texture coordinates, 0..1 across the skin
  std::vector<uint32_t> indices;  // three a triangle, counter-clockwise seen from outside
  std::vector<Frame> frames;
  std::vector<int> source_vertex;  // a vertex's index in the file (before the seam split)
  std::array<float, 3> eye{};
  float radius = 0;
  uint32_t flags = 0;  // the file's effect flags (a trail, a spin), passed through
  bool synced = true;  // the file's sync type: every copy animates in step
  uint32_t vertex_count() const { return static_cast<uint32_t>(st.size() / 2); }
};

// A sprite: frames of indexed pictures, each with its origin, facing the viewer as `facing` says.
enum class Facing : uint8_t { upright, facing, parallel, oriented, parallel_oriented };

struct SpriteFrame {
  int width = 0, height = 0;
  int origin_x = 0, origin_y = 0;  // the picture's top left against the sprite's origin, units
  std::vector<uint8_t> indices;
  float interval = 0;
  int group = 0;
};

struct Sprite {
  Facing facing = Facing::parallel;
  float radius = 0;
  int max_width = 0, max_height = 0;
  std::vector<SpriteFrame> frames;
};

}  // namespace ge::morph
