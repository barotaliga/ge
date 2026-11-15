// The map compiler's internal steps past the tree: the box hulls, the light, the version 29
// and 38 layouts. compile.cpp runs them in that order.
//
// - **Hulls** (`build_hull`): every brush that stops a body (solid, sky, clip) grown by the box:
//   its six axial sides added at its bounds (a bevel where it has none), then each side moved out
//   by the box's corner furthest against it, `d − Σ n_k·(n_k > 0 ? mins_k : maxs_k)`; the grown
//   brushes split to a tree as the world's are, and that tree written as clip nodes, a subtree
//   whose leaves are all one contents folded into it (−1 open, −2 solid).
// - **Light** (`light_faces`): a luxel every 16 texels of each drawn face (special faces have
//   none), its point a unit off the face (pulled toward the face's middle while it is inside a
//   wall), lit by each light it sees (a segment through the world's tree meeting nothing opaque):
//   `(value − distance × wait) × (½ + ½ cos θ)`, θ off the face's normal; the sun (`_sunlight`,
//   `_sun_mangle` in the world's keys) where a ray toward it first meets sky. Never below the
//   world's least light (`light` or `_minlight` in its keys), never above 255. A light with a
//   `style` lights its own map of the faces it reaches (four styles a face at most).
//
// ge:summary The map compiler's steps past the tree: the box hulls, the light, the v29 layout
// ge:keywords map-compiler hull clip-node bevel expand light luxel shadow sun minlight style
// ge:keywords write-v29 layout
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/tree.hpp"

namespace ge::brush::mapc {

struct ClipOut {
  int plane = 0;                // canonical index
  int32_t child[2] = {-1, -1};  // >= 0 a clip node (in this hull's list), < 0 a contents number
};
struct Hull {
  std::vector<ClipOut> nodes;
  int head = 0;
};

// The hull of `brushes` (those that stop bodies) grown by the box `mins..maxs`, over `lo..hi`.
Hull build_hull(const std::vector<Brush>& brushes, const V3& mins, const V3& maxs, const V3& lo,
                const V3& hi, Planes* planes);

struct Light {
  V3 at = {0, 0, 0};
  double value = 300, wait = 1;
  uint8_t style = 0;
};
struct Sun {
  double value = 0;
  V3 toward = {0, 0, 1};  // a unit vector toward the sun
};
struct LightSet {
  std::vector<Light> lights;
  Sun sun;
  double minlight = 0;
};
LightSet lights_of(const Source& src);

// Every face of `tree` lit; shadows from `world`'s tree. Adds the luxels to `*count`.
void light_faces(const LightSet& set, const Tree& world, const Planes& planes,
                 const std::vector<TexInfo>& texinfos, Tree* tree, size_t* count);

// A brush as the version 38 family keeps it: oriented plane ids facing out (axial bevels added),
// each side's texinfo (−1 a bevel, drawn nowhere), its bounds; `leaf_index` is the index the
// tree's leaves name it by (−1 a brush in no tree, a clip brush).
struct BrushOut {
  uint32_t contents = kSolid;
  std::vector<int> planes, texinfos;
  V3 lo = {0, 0, 0}, hi = {0, 0, 0};
  int leaf_index = -1;
};

// A model laid out: its tree and hulls (29), its brushes (38).
struct ModelOut {
  Tree* tree = nullptr;
  Hull hulls[2];
  V3 lo = {0, 0, 0}, hi = {0, 0, 0};
  std::vector<BrushOut> brushes;
};
struct Layout {
  const Planes* planes = nullptr;
  std::vector<std::string> textures;
  std::vector<TextureImage> images;  // a texture's picture
  std::vector<TexInfo> texinfos;
  std::vector<ModelOut> models;  // [0] the world
  std::string entities;
};
// The version 29 file; false with the reason when a count passes the family's limits.
bool write_v29_map(const Layout& layout, std::vector<uint8_t>* out, std::string* error,
                   CompileReport* report);

// The version 38 file; false with the reason when a count passes the family's limits.
bool write_v38_map(const Layout& layout, std::vector<uint8_t>* out, std::string* error,
                   CompileReport* report);

// A pattern for a texture nobody supplied: 64 × 64 (the sky's 256 × 128), two palette indices
// chosen by the name.
TextureImage pattern_image(const std::string& name);

}  // namespace ge::brush::mapc
