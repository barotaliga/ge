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
// - THE LOOK AGES (`look_at`): a scorch glows with embers for its first `ember_s` (the `er`, `eg`,
//   `eb` a renderer adds as light and a light pass may cast) and then keeps as soot; a hole keeps;
//   blood darkens from a wet dark red to a near black brown over `dry_s` (fast at first, as it
//   thickens, then slowly), its gloss going first, and fades to nothing over the last `fade_s` of
//   its `life_s` (a decal's own `life_s` over its kind's: a game's setting), and `expire` frees
//   it. A burn line and a melt glow like a scorch, hot along their middle, and cool to black.
// - BLOOD THAT MOVES (`DecalLook::spread`, `MaskSample::arrive`): a pool spreads out over its first
//   seconds and a drip runs down the wall; the mask says when the blood reaches each texel (0 at
//   once .. 1 last) and the look how far it has got. A pool pressed again larger in its own slot
//   (`DecalAt::slot`, `spread_from` the old size's share of the new) spreads on from where it was.
//   blood_pool: a puddle on a floor, lobed, its rim darker where it dries first; blood_drip: a
//   blot with runs below it, each ending in a bead, a box `kDripAspect` times as long as wide laid
//   down the wall (`along` the world's down).
// - A LINE (`DecalAt::length_m`, `along`): a box `length_m` along `along` (laid flat on the
//   surface) and `size_m` across; its mask's u runs along it, its ends thinned so pieces laid end
//   to end, each overlapping the last by its width, read as one unbroken line.
// - THE POOL is made once (`make_pool`): `capacity` decals of at most `verts_per_decal` vertices.
//   A new decal takes a free slot, else the oldest one's; one that would need more vertices than a
//   slot holds keeps only the triangles nearest its centre that fit.
//
// Deterministic: placement and masks use exact arithmetic, libm and the lattice hash.
//
// ge:summary Decals that age: scorch, bullet holes, blood clipped to surfaces in a fixed pool
// ge:keywords detail decal scorch bullet-hole blood splat clip box mask atlas age fade dry pool
// ge:keywords place_decal expire look_at mask_at bake_atlas decal-kind decal-vertex burn-line melt
// ge:keywords blood-pool blood-drip drip pool spread arrive dry darken look_of is_blood
// ge:keywords blood-print boot-print footprint bloody-footprints tread strength
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

// burn_line: a scorched line a beam traces as it sweeps over a wall, its pieces long boxes laid
// end to end; melt: the pool a beam held on one spot burns into the wall, pressed again larger in
// its own slot as the beam stays (`DecalAt::slot`); blood_pool: blood gathered on a floor, pressed
// again larger as more falls in it; blood_drip: blood running down a wall from a splash;
// blood_print: a boot's print stepped out of wet blood, its toe toward -v (`spin_rad` turns it),
// odd variants the right foot.
enum class DecalKind : uint8_t {
  scorch,
  bullet_hole,
  blood,
  burn_line,
  melt,
  blood_pool,
  blood_drip,
  blood_print
};
inline constexpr int kDecalKinds = 8;
// Blood of any kind: paint, drying, its own life.
constexpr bool is_blood(DecalKind k) {
  return k == DecalKind::blood || k == DecalKind::blood_pool || k == DecalKind::blood_drip ||
         k == DecalKind::blood_print;
}
// A drip's box: this many times as long (down the wall) as it is wide.
inline constexpr double kDripAspect = 3;
// Where a drip's blot sits along its box (u, from the top): the splash it runs from.
inline constexpr double kDripBlotU = 0.38 / kDripAspect;

struct DecalKindSpec {
  std::string_view name;
  double life_s;         // freed after this long; 0 keeps it until its slot is needed
  double fade_s;         // the last stretch of its life it fades over
  double dry_s;          // how long its colour takes to dry (blood)
  double depth_share;    // the box's half depth as a share of the decal's size
  double max_angle_deg;  // triangles turned further than this from the decal's normal are left
  double ember_s;        // how long it glows when fresh (a scorch's embers); 0 never
};
const DecalKindSpec& decal_spec(DecalKind kind);

// The colour (linear 0..1), opacity, gloss and glow a mark has at `age_s`.
struct DecalLook {
  double r, g, b;
  double opacity;
  double gloss;                   // 0 matte .. 1 wet
  double er = 0, eg = 0, eb = 0;  // the light it gives off, linear, may pass 1 (embers)
  double spread = 1;              // how far blood has spread or run (the mask's `arrive`), 0..1
};
// `ember_s` > 0: its glow lasts that long instead of the kind's (a big blast's mark cools slower).
// `life_s` > 0: blood's life instead of the kind's (its drying and fading shortened to fit), < 0
// never fading. `spread_from`: the share of its spread it starts at (a pool pressed again larger).
DecalLook look_at(DecalKind kind, double age_s, double ember_s = 0, double life_s = 0,
                  double spread_from = 0);

struct MaskSample {
  double cover;  // 0..1
  double shade;  // -1 (a hole's dark wall) .. +1 (a raised rim)
  double ember =
      0;  // 0..1: where a fresh mark glows (a scorch's embers); the look's glow times this
  double arrive = 0;  // blood: when it reaches here, as a share of its spread (0 at once .. 1)
};
// The mark of variant `variant` at (u, v) in [0, 1]^2; zero outside.
MaskSample mask_at(DecalKind kind, uint32_t variant, double u, double v);

// Every kind's `variants` masks side by side, each `size` px square, in one RGBA8 picture
// `size * variants` wide and `size * kDecalKinds` tall (kind by row): r = cover, g = the shade
// mapped to 0..255, b = the ember (blood: its `arrive`), a = cover.
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
  double length_m = 0;     // a long mark's length (0: square)
  double ember_s = 0;      // its glow's length (0: its kind's)
  double life_s = 0;       // its life (0: its kind's; < 0 never fades nor is freed)
  double spread_from = 0;  // the share of its spread it was pressed at (look_at's)
  double spin_rad = 0;     // its turn about its normal, as pressed
  uint32_t count = 0;      // vertices in the slot, three a triangle
  double strength = 1;     // its opacity's share (a print fainter each step)
};
// The look of slot `d` at `now_s` (look_at with its own glow, life and spread; its opacity times
// its strength).
DecalLook look_of(const Decal& d, double now_s);

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
  // A long mark: `length_m` along `along` (the surface's own direction is taken from it; zero:
  // `spin_rad` turns it), `size_m` across. 0: square.
  double length_m = 0;
  Vec3 along{0, 0, 0};
  // Pressed again in place: slot `slot` is taken when it still holds the decal placed as
  // `serial` (a melt grown larger in its own slot), else a slot as any new decal's.
  int slot = -1;
  uint32_t serial = 0;
  double ember_s = 0;  // how long it glows; 0: its kind's (decal_spec)
  double life_s = 0;   // how long it lasts; 0: its kind's (decal_spec), < 0 until its slot is taken
  double spread_from = 0;  // blood pressed again larger: the old size's share of the new
  double strength = 1;     // its opacity's share, 0..1 (a print fainter each step out of a pool)
};
// The decal pressed onto `receivers`: its slot, or -1 when no surface under it faces its way.
int place_decal(DecalPool& pool, const Receivers& receivers, const DecalAt& d);
// Every decal past its life (its own, else its kind's) at `now_s` freed.
void expire(DecalPool& pool, double now_s);
uint32_t live_count(const DecalPool& pool);
// Slot `i`'s vertices.
std::span<const DecalVertex> decal_verts(const DecalPool& pool, uint32_t i);

}  // namespace ge::detail
