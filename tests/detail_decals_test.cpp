// engine/detail/decals.hpp: a scorch at a wall's foot wraps onto the floor and the wall and stays
// inside its box; a decal over empty space or facing away from every surface is not placed; the
// pool takes free slots, then the oldest; blood dries and fades and is freed at the end of its
// life while a scorch keeps; masks are empty outside their circle; an overfull decal keeps its
// nearest triangles within the slot; and placements keep their bits in
// tests/golden/detail_decals.txt. A burn line runs the length of its long box, thinned at its
// ends; a melt pressed again grows in its own slot; both glow and cool to black. Blood is dark
// and darkens as it dries; a life of its own (a game's setting) shortens its drying and fading or
// keeps it; a pool spreads from its middle out and a drip runs down from its blot, a pool pressed
// again larger spreading on from its old edge.
//
// ge:summary Test: decals clipped across surfaces, the pool, ageing, masks; golden
// ge:keywords test detail decal clip pool age blood scorch mask golden
// ge:skill detail testing
#include <algorithm>
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
  // A scorch's embers glow when fresh, dim as they cool and are out by `ember_s`; nothing else
  // glows.
  const double ember_s = gd::decal_spec(gd::DecalKind::scorch).ember_s;
  CHECK(gd::look_at(gd::DecalKind::scorch, 0).er > 1);
  CHECK(gd::look_at(gd::DecalKind::scorch, ember_s / 2).er <
        gd::look_at(gd::DecalKind::scorch, 0).er);
  CHECK(gd::look_at(gd::DecalKind::scorch, ember_s).er == 0);
  CHECK(gd::look_at(gd::DecalKind::blood, 0).er == 0);
  CHECK(gd::look_at(gd::DecalKind::bullet_hole, 0).er == 0);
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

TEST(a_boot_print_has_a_ball_and_a_heel_mirrored_for_the_other_foot_and_fades_by_strength) {
  CHECK(gd::is_blood(gd::DecalKind::blood_print));
  // Blood under the ball and the heel, little at the arch between them, none off the sole.
  double ball = 0, heel = 0, arch = 0, side = 0;
  for (int i = 0; i < 16; ++i) {
    const double x = 0.47 + i / 300.0;
    ball += gd::mask_at(gd::DecalKind::blood_print, 0, x, 0.37).cover;
    heel += gd::mask_at(gd::DecalKind::blood_print, 0, x, 0.79).cover;
    arch += gd::mask_at(gd::DecalKind::blood_print, 0, x, 0.635).cover;
    side += gd::mask_at(gd::DecalKind::blood_print, 0, 0.1 + i / 300.0, 0.4).cover;
  }
  CHECK(ball > 4 && heel > 4 && arch < ball / 4 && side == 0);
  // The right foot is the left one mirrored: its ball's blood leans the other way.
  const auto lean = [](uint32_t var) {
    double sum = 0, at = 0;
    for (int i = 0; i < 64; ++i)
      for (int j = 0; j < 32; ++j) {
        const double u = i / 64.0, v = 0.15 + j / 80.0;
        const double c = gd::mask_at(gd::DecalKind::blood_print, var, u, v).cover;
        sum += c;
        at += c * (u - 0.5);
      }
    return at / sum;
  };
  CHECK(lean(0) * lean(1) < 0);
  // A print a few steps out of the pool: as dark, fainter.
  gd::Decal d;
  d.kind = gd::DecalKind::blood_print;
  d.strength = 0.5;
  const gd::DecalLook whole = gd::look_at(gd::DecalKind::blood_print, 1);
  const gd::DecalLook faint = gd::look_of(d, 1);
  CHECK(faint.r == whole.r && faint.opacity == 0.5 * whole.opacity);
}

TEST(blood_is_dark_darkens_and_lives_as_long_as_it_is_told) {
  for (const gd::DecalKind k : {gd::DecalKind::blood, gd::DecalKind::blood_pool,
                                gd::DecalKind::blood_drip, gd::DecalKind::blood_print}) {
    CHECK(gd::is_blood(k));
    const gd::DecalLook fresh = gd::look_at(k, 0), later = gd::look_at(k, 10),
                        dried = gd::look_at(k, gd::decal_spec(k).dry_s);
    // A dark red when wet, near black once dried; darker and duller as it goes.
    CHECK(fresh.r < 0.2 && fresh.r > 10 * fresh.g);
    CHECK(dried.r < 0.05 && dried.r < later.r && later.r < fresh.r);
    CHECK(dried.gloss < later.gloss && later.gloss < fresh.gloss);
    // Its own life: 20 s dries it by 12 s and fades it out at 20; < 0 keeps it.
    CHECK(gd::look_at(k, 12, 0, 20).r == dried.r);
    CHECK(gd::look_at(k, 15, 0, 20).opacity < fresh.opacity);
    CHECK(gd::look_at(k, 20, 0, 20).opacity == 0);
    CHECK(gd::look_at(k, 1e5, 0, -1).opacity == fresh.opacity);
  }
  CHECK(!gd::is_blood(gd::DecalKind::scorch) && !gd::is_blood(gd::DecalKind::melt));
  // A pool is darker and glossier than a splash.
  CHECK(gd::look_at(gd::DecalKind::blood_pool, 0).r < gd::look_at(gd::DecalKind::blood, 0).r);
  CHECK(gd::look_at(gd::DecalKind::blood_pool, 0).gloss >
        gd::look_at(gd::DecalKind::blood, 0).gloss);
  // A pool spreads and a drip runs, slowing, then stay; a splash is all there at once.
  for (const gd::DecalKind k : {gd::DecalKind::blood_pool, gd::DecalKind::blood_drip}) {
    const double a = gd::look_at(k, 0.5).spread, b = gd::look_at(k, 1.0).spread;
    CHECK(gd::look_at(k, 0).spread == 0 && a > 0 && b > a && b - a < a);
    CHECK(gd::look_at(k, 30).spread == 1);
    CHECK(gd::look_at(k, 0, 0, 0, 0.6).spread == 0.6);  // pressed again: on from where it was
  }
  CHECK(gd::look_at(gd::DecalKind::blood, 0).spread == 1);
  // The pool's blood reaches its middle first and its edge last; the drip's blot at once, its runs
  // later the further down.
  const gd::MaskSample mid = gd::mask_at(gd::DecalKind::blood_pool, 0, 0.5, 0.5);
  CHECK(mid.cover == 1 && mid.arrive < 0.05);
  double edge_arrive = 0;
  for (int i = 0; i < 64; ++i) {
    const gd::MaskSample s = gd::mask_at(gd::DecalKind::blood_pool, 0, 0.5 + i / 160.0, 0.5);
    if (s.cover > 0.5) edge_arrive = std::max(edge_arrive, s.arrive);
  }
  CHECK(edge_arrive > 0.6);
  CHECK(gd::mask_at(gd::DecalKind::blood_drip, 1, gd::kDripBlotU, 0.5).cover == 1);
  CHECK(gd::mask_at(gd::DecalKind::blood_drip, 1, gd::kDripBlotU, 0.5).arrive == 0);
  double low_arrive = 0, low_cover = 0;
  for (int i = 0; i < 64; ++i)
    for (int j = 0; j < 64; ++j) {
      const gd::MaskSample s =
          gd::mask_at(gd::DecalKind::blood_drip, 1, 0.4 + 0.6 * i / 64.0, j / 64.0);
      low_cover += s.cover;
      if (s.cover > 0.5) low_arrive = std::max(low_arrive, s.arrive);
    }
  CHECK(low_cover > 10 && low_arrive > 0.3);
  // A decal's own life frees it then; < 0 never.
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(4);
  gd::DecalAt d;
  d.at = {1, 0, 3};
  d.size_m = 0.6;
  d.kind = gd::DecalKind::blood_pool;
  d.life_s = 5;
  const int a = gd::place_decal(pool, r, d);
  d.at = {-1, 0, 3};
  d.life_s = -1;
  gd::place_decal(pool, r, d);
  gd::expire(pool, 4.9);
  CHECK(gd::live_count(pool) == 2);
  gd::expire(pool, 1e6);
  CHECK(gd::live_count(pool) == 1 && !pool.decals[static_cast<size_t>(a)].live);
  // Pressed again larger in its own slot, it keeps its turn and spreads on from its old edge.
  gd::DecalAt again = d;
  again.size_m = 0.9;
  again.slot = 1;
  again.serial = pool.decals[1].serial;
  again.spread_from = 0.6 / 0.9;
  again.spin_rad = 0.4;
  CHECK(gd::place_decal(pool, r, again) == 1);
  CHECK(pool.decals[1].size_m == 0.9 && pool.decals[1].spread_from == again.spread_from);
  CHECK(gd::look_of(pool.decals[1], pool.decals[1].born_s).spread == again.spread_from);
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
        // A burn line and a drip run the length of their boxes; the rest are round.
        if (kind != gd::DecalKind::burn_line && kind != gd::DecalKind::blood_drip &&
            (u - 0.5) * (u - 0.5) + (v - 0.5) * (v - 0.5) >= 0.25)
          CHECK(s.cover == 0);
        inside += s.cover;
      }
    }
    CHECK_MSG(inside > 64 * 64 * 0.03, "kind " + std::to_string(k) + " covers " +
                                           std::to_string(inside));  // every kind marks something
    // The middle of a hole is the hole.
    if (kind == gd::DecalKind::bullet_hole) CHECK(gd::mask_at(kind, 0, 0.5, 0.5).shade == -1);
  }
  const std::vector<uint8_t> atlas = gd::bake_atlas(32, 4);
  CHECK(atlas.size() == static_cast<size_t>(32 * 4) * 32 * gd::kDecalKinds * 4);
}

TEST(a_burn_line_runs_along_its_box_and_thins_at_its_ends) {
  // Black along its middle from end to end, nothing at its sides, thinned to nothing at its ends.
  for (uint32_t var = 0; var < 4; ++var) {
    for (int i = 2; i < 18; ++i) {
      const double u = 0.15 + 0.7 * i / 20.0;
      double most = 0;
      for (int j = 0; j < 9; ++j)
        most = std::max(most, gd::mask_at(gd::DecalKind::burn_line, var, u, 0.4 + 0.025 * j).cover);
      CHECK_MSG(most > 0.6, "var " + std::to_string(var) + " at u " + std::to_string(u));
      CHECK(gd::mask_at(gd::DecalKind::burn_line, var, u, 0.01).cover < 0.05);
    }
    CHECK(gd::mask_at(gd::DecalKind::burn_line, var, 0.001, 0.5).cover < 0.02);
    CHECK(gd::mask_at(gd::DecalKind::burn_line, var, 0.999, 0.5).cover < 0.02);
  }
  // Placed 2 m long and 0.14 m wide along a wall: its vertices span the length along the line.
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(4);
  gd::DecalAt d;
  d.kind = gd::DecalKind::burn_line;
  d.at = {6, 2, 3};  // on the right wall's face (x 6)
  d.normal = {-1, 0, 0};
  d.size_m = 0.14;
  d.length_m = 2;
  d.along = {0.3, 0, 1};  // into the wall a little: laid flat along it, so along +z
  const int slot = gd::place_decal(pool, r, d);
  CHECK(slot >= 0);
  if (slot < 0) return;
  double z0 = 1e9, z1 = -1e9, y0 = 1e9, y1 = -1e9;
  for (const gd::DecalVertex& v : gd::decal_verts(pool, static_cast<uint32_t>(slot))) {
    z0 = std::min(z0, v.pos.z), z1 = std::max(z1, v.pos.z);
    y0 = std::min(y0, v.pos.y), y1 = std::max(y1, v.pos.y);
  }
  CHECK(std::fabs(z1 - z0 - 2) < 1e-6 && std::fabs(y1 - y0 - 0.14) < 1e-6);
  CHECK(pool.decals[static_cast<size_t>(slot)].length_m == 2);
}

TEST(a_melt_pressed_again_grows_in_its_own_slot) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(4);
  gd::DecalAt d;
  d.kind = gd::DecalKind::melt;
  d.at = {6, 2, 3};
  d.normal = {-1, 0, 0};
  d.size_m = 0.2;
  const int first = gd::place_decal(pool, r, d);
  CHECK(first >= 0);
  if (first < 0) return;
  d.slot = first;
  d.serial = pool.decals[static_cast<size_t>(first)].serial;
  d.size_m = 0.6;
  d.now_s = 1;
  CHECK(gd::place_decal(pool, r, d) == first);
  CHECK(gd::live_count(pool) == 1 && pool.decals[static_cast<size_t>(first)].size_m == 0.6);
  // A stale serial (the slot taken by another since) takes a slot of its own.
  d.serial += 7;
  CHECK(gd::place_decal(pool, r, d) != first);
  // White-hot when fresh, black once it has cooled; a longer glow cools later.
  const double es = gd::decal_spec(gd::DecalKind::melt).ember_s;
  CHECK(gd::look_at(gd::DecalKind::melt, 0).er > 2);
  CHECK(gd::look_at(gd::DecalKind::melt, es).er == 0);
  CHECK(gd::look_at(gd::DecalKind::melt, es, 2 * es).er > 0);
  CHECK(gd::look_at(gd::DecalKind::burn_line, es).r < 0.05);
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
      masks.add(s.ember);
    }
  }
  got.push_back({masks.h});
  ge::test::check_golden("tests/golden/detail_decals.txt", got,
                         "per placement: slot, vertices, their hash; then the masks' hash "
                         "(detail_decals_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
                         "detail-decals lab's pictures before and after in the PR (skill: detail)");
}
