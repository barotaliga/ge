// engine/detail/decals.hpp: a scorch at a wall's foot wraps onto the floor and the wall and stays
// inside its box; a decal over empty space or facing away from every surface is not placed; the
// pool takes free slots, then the oldest; blood dries and fades and is freed at the end of its
// life while a scorch keeps; masks are empty outside their circle; an overfull decal keeps its
// nearest triangles within the slot; and placements keep their bits in
// tests/golden/detail_decals.txt.
//
// ge:summary Test: decals clipped across surfaces, the pool, ageing, masks; golden
// ge:keywords test detail decal clip pool age blood scorch mask golden
// ge:skill detail testing
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "check.hpp"
#include "detail/decals.hpp"
#include "detail/solid.hpp"
#include "detail/test_hall.hpp"
#include "detail_support.hpp"

namespace {

namespace gd = ge::detail;

gd::TriMesh hall_mesh() {
  gd::TriMesh m;
  for (const gd::Solid& s : gd::test_hall().solids) gd::triangulate(s, gd::polygons_of(s), m);
  return m;
}

}  // namespace

TEST(a_scorch_wraps_round_a_corner) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(8);
  // At the right wall's foot (x 6), pointing out of the corner between it and the floor.
  gd::DecalAt d;
  d.kind = gd::DecalKind::scorch;
  d.at = {6, 0, 3};
  d.normal = gd::normalized({-1, 1, 0});
  d.size_m = 1.2;
  const int slot = gd::place_decal(pool, r, d);
  CHECK(slot == 0);
  bool on_floor = false, on_wall = false;
  for (const gd::DecalVertex& v : gd::decal_verts(pool, 0)) {
    CHECK(v.u > -1e-9 && v.u < 1 + 1e-9 && v.v > -1e-9 && v.v < 1 + 1e-9);
    CHECK(v.fade >= 0 && v.fade <= 1);
    if (std::fabs(v.pos.y - 0.001) < 1e-9) on_floor = true;
    if (std::fabs(v.pos.x - 5.999) < 1e-9) on_wall = true;
  }
  CHECK(on_floor && on_wall);
  CHECK(pool.decals[0].count % 3 == 0 && pool.decals[0].count > 0);
  // Over the open front of the hall there is nothing to land on.
  d.at = {0, 2, -3};
  d.normal = {0, 0, -1};
  d.size_m = 0.3;
  CHECK(gd::place_decal(pool, r, d) == -1);
  // Facing away from the floor under it: nothing faces its way.
  d.at = {0, 0, 2};
  d.normal = {0, -1, 0};
  CHECK(gd::place_decal(pool, r, d) == -1);
}

TEST(the_pool_takes_free_slots_then_the_oldest) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(3);
  gd::DecalAt d;
  d.kind = gd::DecalKind::bullet_hole;
  d.normal = {0, 0, -1};
  d.size_m = 0.12;
  std::vector<int> slots;
  for (int i = 0; i < 5; ++i) {
    d.at = {-4 + i * 0.5, 1.5, 10};
    d.now_s = i;
    slots.push_back(gd::place_decal(pool, r, d));
  }
  CHECK((slots == std::vector<int>{0, 1, 2, 0, 1}));
  CHECK(gd::live_count(pool) == 3);
  CHECK(pool.decals[0].born_s == 3 && pool.decals[2].born_s == 2);
}

TEST(blood_dries_fades_and_is_freed) {
  const gd::DecalKindSpec& s = gd::decal_spec(gd::DecalKind::blood);
  const gd::DecalLook fresh = gd::look_at(gd::DecalKind::blood, 0);
  const gd::DecalLook dried = gd::look_at(gd::DecalKind::blood, s.dry_s);
  CHECK(fresh.r > dried.r && fresh.gloss > dried.gloss);
  CHECK(gd::look_at(gd::DecalKind::blood, s.life_s - s.fade_s).opacity == fresh.opacity);
  CHECK(gd::look_at(gd::DecalKind::blood, s.life_s - s.fade_s / 2).opacity < fresh.opacity);
  CHECK(gd::look_at(gd::DecalKind::blood, s.life_s).opacity == 0);
  CHECK(gd::look_at(gd::DecalKind::scorch, 1e6).opacity > 0.5);
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(4);
  gd::DecalAt d;
  d.at = {1, 0, 3};
  d.size_m = 0.6;
  d.kind = gd::DecalKind::blood;
  gd::place_decal(pool, r, d);
  d.kind = gd::DecalKind::scorch;
  d.at = {-1, 0, 3};
  gd::place_decal(pool, r, d);
  gd::expire(pool, s.life_s - 1);
  CHECK(gd::live_count(pool) == 2);
  gd::expire(pool, s.life_s);
  CHECK(gd::live_count(pool) == 1 && pool.decals[1].live);
}

TEST(masks_cover_inside_their_circle_only) {
  for (int k = 0; k < gd::kDecalKinds; ++k) {
    const auto kind = static_cast<gd::DecalKind>(k);
    double inside = 0;
    for (int i = 0; i < 64; ++i) {
      for (int j = 0; j < 64; ++j) {
        const double u = (i + 0.5) / 64, v = (j + 0.5) / 64;
        const gd::MaskSample s = gd::mask_at(kind, 2, u, v);
        CHECK(s.cover >= 0 && s.cover <= 1 && s.shade >= -1 && s.shade <= 1);
        if ((u - 0.5) * (u - 0.5) + (v - 0.5) * (v - 0.5) >= 0.25) CHECK(s.cover == 0);
        inside += s.cover;
      }
    }
    CHECK_MSG(inside > 64 * 64 * 0.03, "kind " + std::to_string(k) + " covers " +
                                           std::to_string(inside));  // every kind marks something
    // The middle of a hole is the hole.
    if (kind == gd::DecalKind::bullet_hole) CHECK(gd::mask_at(kind, 0, 0.5, 0.5).shade == -1);
  }
  const std::vector<uint8_t> atlas = gd::bake_atlas(32, 4);
  CHECK(atlas.size() == static_cast<size_t>(32 * 4) * 32 * 3 * 4);
}

TEST(an_overfull_decal_keeps_its_nearest_triangles) {
  // A floor cut into many small triangles: a big decal over it holds more than one slot.
  gd::TriMesh m;
  for (int z = 0; z < 20; ++z) {
    for (int x = 0; x < 20; ++x) {
      const auto b = static_cast<uint32_t>(m.pos.size());
      const double x0 = x * 0.1, z0 = z * 0.1;
      m.pos.insert(m.pos.end(),
                   {{x0, 0, z0}, {x0, 0, z0 + 0.1}, {x0 + 0.1, 0, z0 + 0.1}, {x0 + 0.1, 0, z0}});
      m.idx.insert(m.idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
      m.tag.insert(m.tag.end(), {0, 0});
    }
  }
  const gd::Receivers r = gd::bin_receivers(m, 0.5);
  gd::DecalPool pool = gd::make_pool(1, 30);
  gd::DecalAt d;
  d.at = {1, 0, 1};
  d.size_m = 1;
  CHECK(gd::place_decal(pool, r, d) == 0);
  CHECK(pool.decals[0].count == 30);
  for (const gd::DecalVertex& v : gd::decal_verts(pool, 0)) {
    CHECK(gd::length(gd::sub(v.pos, {1, 0.001, 1})) < 0.3);
  }
}

TEST(placements_keep_their_bits) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(16);
  std::vector<std::vector<uint64_t>> got;
  const gd::DecalAt shots[] = {
      {{6, 0, 3}, gd::normalized({-1, 1, 0}), 1.2, 0.3, 0, 1, gd::DecalKind::scorch},
      {{4.1, 0.25, 7.6}, gd::normalized({-1, 1, -1}), 0.9, 1.1, 0, 2, gd::DecalKind::blood},
      {{-3, 2, 5.58}, {0, 0, -1}, 0.14, 2.2, 0, 3, gd::DecalKind::bullet_hole},
      {{-0.2, 2.8, 9.55}, gd::normalized({0, -0.3, -1}), 0.5, 0, 0, 0, gd::DecalKind::blood},
  };
  for (const gd::DecalAt& d : shots) {
    const int slot = gd::place_decal(pool, r, d);
    ge::test::BitsHash h;
    for (const gd::DecalVertex& v : gd::decal_verts(pool, static_cast<uint32_t>(slot))) {
      h.add(v.pos);
      h.add(v.u);
      h.add(v.v);
      h.add(v.fade);
    }
    got.push_back({static_cast<uint64_t>(slot), pool.decals[static_cast<size_t>(slot)].count, h.h});
  }
  ge::test::BitsHash masks;
  for (int k = 0; k < gd::kDecalKinds; ++k) {
    for (int i = 0; i < 48; ++i) {
      const gd::MaskSample s = gd::mask_at(static_cast<gd::DecalKind>(k), 1, (i + 0.5) / 48, 0.47);
      masks.add(s.cover);
      masks.add(s.shade);
    }
  }
  got.push_back({masks.h});
  ge::test::check_golden("tests/golden/detail_decals.txt", got,
                         "per placement: slot, vertices, their hash; then the masks' hash "
                         "(detail_decals_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
                         "detail-decals lab's pictures before and after in the PR (skill: detail)");
}
