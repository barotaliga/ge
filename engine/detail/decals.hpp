// DECALS THAT AGE: scorch marks, bullet holes and blood, pressed onto a level's surfaces where a
// game says (a blast, a shot, a wound) and kept in a fixed pool, so a fire fight leaves its marks
// and the frame never allocates for them.
//
// - PLACED BY CLIPPING. A decal is a box round its point, `size_m` square across the surface and
//   `depth` deep along its normal; every receiver triangle in the box (receivers.hpp) that faces
//   the decal's way within the kind's `max_angle_deg` is clipped to the box, and what is left is
//   the decal's own small mesh, lifted a millimetre off the surface, with its (u, v) across the
//   box and a `fade` that thins it on triangles turned away; a piece on a face buried inside a
//   solid (`buried_at`: a floor under the wall standing on it) is dropped. So a scorch at a wall's
//   foot wraps onto the floor, a blood splash runs over a step's nose, and nothing floats over a
//   gap.
// - THE MASK is a pure function of the kind, a variant number and (u, v) (`mask_at`): how much the
//   mark covers there and its relief (`shade`, -1 the dark wall of a hole .. +1 a raised rim). A
//   renderer bakes it once into an atlas (`bake_atlas`); a CPU lab reads it directly.
// - THE LOOK AGES (`look_at`): a scorch and a hole keep; blood darkens from wet red to dried brown
//   over `dry_s` and fades to nothing over the last `fade_s` of its `life_s`, and `expire` frees
//   it.
// - THE POOL is made once (`make_pool`): `capacity` decals of at most `verts_per_decal` vertices.
//   A new decal takes a free slot, else the oldest one's; one that would need more vertices than a
//   slot holds keeps only the triangles nearest its centre that fit.
//
// Deterministic: placement and masks use exact arithmetic, libm and the lattice hash.
//
// ge:summary Decals that age: scorch, bullet holes, blood clipped to surfaces in a fixed pool
// ge:keywords detail decal scorch bullet-hole blood splat clip box mask atlas age fade dry pool
// ge:keywords place_decal expire look_at mask_at bake_atlas decal-kind decal-vertex
// ge:skill detail
// ge:test tests/detail_decals_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "detail/receivers.hpp"

namespace ge::detail {

enum class DecalKind : uint8_t { scorch, bullet_hole, blood };
inline constexpr int kDecalKinds = 3;

struct DecalKindSpec {
  std::string_view name;
  double life_s;         // freed after this long; 0 keeps it until its slot is needed
  double fade_s;         // the last stretch of its life it fades over
  double dry_s;          // how long its colour takes to dry (blood)
  double depth_share;    // the box's half depth as a share of the decal's size
  double max_angle_deg;  // triangles turned further than this from the decal's normal are left
};
const DecalKindSpec& decal_spec(DecalKind kind);

// The colour (linear 0..1), opacity and gloss a mark has at `age_s`.
struct DecalLook {
  double r, g, b;
  double opacity;
  double gloss;  // 0 matte .. 1 wet
};
DecalLook look_at(DecalKind kind, double age_s);

struct MaskSample {
  double cover;  // 0..1
  double shade;  // -1 (a hole's dark wall) .. +1 (a raised rim)
};
// The mark of variant `variant` at (u, v) in [0, 1]^2; zero outside.
MaskSample mask_at(DecalKind kind, uint32_t variant, double u, double v);

// Every kind's `variants` masks side by side, each `size` px square, in one RGBA8 picture
// `size * variants` wide and `size * kDecalKinds` tall (kind by row): r = cover, g = the shade
// mapped to 0..255, b = 0, a = cover.
std::vector<uint8_t> bake_atlas(int size, int variants);

struct DecalVertex {
  Vec3 pos;
  double u, v;
  double fade;  // 0..1, by how far its triangle turns from the decal's normal
};

struct Decal {
  bool live = false;
  DecalKind kind = DecalKind::scorch;
  uint32_t variant = 0;
  uint32_t serial = 0;  // the order decals were placed in
  double born_s = 0;
  Vec3 at{0, 0, 0}, normal{0, 1, 0};
  double size_m = 0;
  uint32_t count = 0;  // vertices in the slot, three a triangle
};

struct DecalPool {
  uint32_t capacity = 0;
  uint32_t verts_per_decal = 0;
  std::vector<Decal> decals;       // `capacity` slots
  std::vector<DecalVertex> verts;  // `capacity * verts_per_decal`; slot i's from i * per
  uint32_t next_serial = 1;
  ReceiverScratch scratch;
  ReceiverScratch scratch_up;                    // the buried test's, beside the box query's
  std::vector<Vec3> poly, next;                  // the clip's working loops
  std::vector<DecalVertex> staged;               // a decal's vertices before they are kept
  std::vector<std::pair<double, size_t>> order;  // an overfull decal's triangles by distance
  std::vector<DecalVertex> kept;
};
DecalPool make_pool(uint32_t capacity, uint32_t verts_per_decal = 240);

struct DecalAt {
  Vec3 at{0, 0, 0};
  Vec3 normal{0, 1, 0};  // the surface's, out of it; unit
  double size_m = 0.5;
  double spin_rad = 0;  // the mark's turn about its normal
  double now_s = 0;
  uint32_t variant = 0;
  DecalKind kind = DecalKind::scorch;
};
// The decal pressed onto `receivers`: its slot, or -1 when no surface under it faces its way.
int place_decal(DecalPool& pool, const Receivers& receivers, const DecalAt& d);
// Every decal past its life at `now_s` freed.
void expire(DecalPool& pool, double now_s);
uint32_t live_count(const DecalPool& pool);
// Slot `i`'s vertices.
std::span<const DecalVertex> decal_verts(const DecalPool& pool, uint32_t i);

}  // namespace ge::detail
