// SEEDED DETAIL PROPS: rubble at the foot of walls, debris over a floor, moss in the damp, put on
// a level by RULES read off its shape, so a hall nobody dressed by hand still reads as used and
// old. Nothing is placed by hand and nothing is stored: the same level, rules and seed give the
// same props, bit for bit.
//
// - A RULE (`PropRule`) names a kind, the surfaces it may stand on (their tags and whether they
//   are floors or walls), a density per square metre where its weight is 1, and the weight: near
//   the OTHER surface (a floor's walls, found by eight level probes; a wall's floor, by one probe
//   down) it rises to 1 over `near_m`, out in the open it is `open_weight`, and a caller's damp
//   field (`DampAt`, 0..1, for water's edges and wet rooms) adds `damp_weight` (and a rule that
//   lives only in the wet keeps `dry_share` of the rest where it is dry). A low noise of
//   `clump_m` gathers props into heaps by `clump`.
// - THE SCATTER walks every triangle the rule may use, draws its count off its area (a stream a
//   triangle, so editing one room moves no prop in another), samples points in it, keeps one by
//   its weight, drops one on a face buried inside another solid (`buried_at`), and drops any
//   closer than `spacing_m` to a kept prop of the same rule.
// - A PROP is where it stands, the surface's up, a turn about it, a size and a variant; its shape
//   is `prop_mesh` (prop_shapes.hpp), and it sits `sink_share` of its size into the surface.
//
// - BY WHAT AND WHERE: a rule may be kept to surfaces of some classes (`classes`, surfaces.hpp:
//   the caller hands each tag's class in a `ScatterEnv`), and weighted apart under the open sky
//   and under a roof (`outdoor_weight`, `indoor_weight`; a point is outdoors when a ray straight
//   up meets nothing or a sky surface). So grass grows on grass out in the open, stones lie on
//   stone, spent shells on a metal deck, twigs only where trees could have dropped them.
//
// `default_rules` is the dressing a stone-and-timber interior takes; `clutter_rules` the floor
// clutter by surface class (stones, rocks, dirt, grass tufts, twigs, branches, moss, spent shells,
// bones); a game passes its own.
//
// ge:summary Seeded detail props (rubble, debris, moss) placed by rules on a level's shape
// ge:keywords detail props scatter rubble debris moss rule density near-wall damp clump seeded
// ge:keywords prop-rule scatter_props default_rules spacing poisson dressing clutter clutter_rules
// ge:keywords outdoor indoor open-sky surface-class grass pebble twig shell bone
// ge:skill detail
// ge:test tests/detail_props_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "detail/receivers.hpp"
#include "detail/surfaces.hpp"

namespace ge::detail {

enum class PropKind : uint8_t {
  rubble,
  debris,
  moss,
  pebble,  // a small rounded stone
  rock,    // a larger angular one
  dirt,    // a low clod of earth
  grass,   // a tuft of blades (a renderer may grow it with its own grass)
  twig,
  branch,  // a fallen limb with a fork or two
  shell,   // a spent cartridge case
  bone,
};
inline constexpr int kPropKinds = 11;
std::string_view prop_name(PropKind kind);

enum class PropPlace : uint8_t { floor, wall };

struct PropRule {
  PropKind kind = PropKind::rubble;
  PropPlace place = PropPlace::floor;
  uint64_t tags = ~uint64_t{0};  // the surface tags it may use (bit t for tag t < 64)
  double per_m2 = 1;
  double near_m = 0.8;     // the weight's reach from the other surface; 0: no rise
  double open_weight = 0;  // the weight beyond `near_m`
  double damp_weight = 0;  // added where the damp is 1
  double dry_share = 1;    // the share of the near and open weight kept where the damp is 0
  double clump_m = 1.5;
  double clump = 0.5;  // 0: even; 1: in heaps
  double min_size_m = 0.05, max_size_m = 0.25;
  double spacing_m = 0.08;
  double sink_share = 0.15;
  int variants = 4;
  uint32_t classes = ~0u;     // the surface classes it may use (`class_bit`)
  double outdoor_weight = 1;  // the weight's factor under the open sky
  double indoor_weight = 1;   // and under a roof
};

struct Prop {
  PropKind kind = PropKind::rubble;
  uint8_t variant = 0;
  uint16_t rule = 0;
  uint32_t tri = 0;  // the triangle it stands on
  Vec3 at{0, 0, 0};  // its base, sunk into the surface
  Vec3 up{0, 1, 0};
  double yaw_rad = 0;
  double size_m = 0;
};

// The damp at `p`, 0..1.
using DampAt = double (*)(const void* ctx, const Vec3& p);

// What the scatter reads off the level besides its triangles.
struct ScatterEnv {
  DampAt damp = nullptr;
  const void* ctx = nullptr;
  std::span<const SurfaceClass> class_of_tag;  // a tag past its end is `unknown`
  uint64_t sky_tags = 0;     // tags (below 64) of sky surfaces, which count as open sky above
  double sky_reach_m = 200;  // how far up a roof is looked for
};

struct ScatterReport {
  uint32_t tried[kPropKinds] = {};
  uint32_t placed[kPropKinds] = {};
  uint32_t too_close[kPropKinds] = {};
  uint32_t buried[kPropKinds] = {};  // sampled on a face hidden inside a solid
};

// `rules` scattered over `mesh` (binned as `receivers`) off `seed`.
std::vector<Prop> scatter_props(const TriMesh& mesh, const Receivers& receivers,
                                std::span<const PropRule> rules, uint32_t seed,
                                DampAt damp = nullptr, const void* ctx = nullptr,
                                ScatterReport* report = nullptr);
std::vector<Prop> scatter_props(const TriMesh& mesh, const Receivers& receivers,
                                std::span<const PropRule> rules, uint32_t seed,
                                const ScatterEnv& env, ScatterReport* report = nullptr);

// Rubble heaped at walls' feet, debris thin over open floors, moss on damp floors and low on damp
// walls; every tag.
std::vector<PropRule> default_rules();

// Floor clutter by what the floor is made of and whether it is under the sky: grass tufts, twigs
// and branches out in the open on grass and dirt; stones and rocks on stone and dirt; clods on
// dirt; moss in the damp; spent shells and bones thin over every floor that is not sky or liquid.
std::vector<PropRule> clutter_rules();

}  // namespace ge::detail
