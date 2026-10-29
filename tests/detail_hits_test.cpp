// engine/detail/hits.hpp: a shot leaves its hole where its ray lands, facing the shooter from
// either side of a wall, and none when it flies into the open; a wound's blood grows with the
// share of health it took and nothing bleeds for no loss; the same event leaves the same marks
// and another seed others; and the marks keep their bits in tests/golden/detail_hits.txt.
//
// ge:summary Test: shot holes on their ray, blood growing with health lost, seeds; golden
// ge:keywords test detail hit shot bullet-hole wound bleed blood damage golden
// ge:skill detail testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "detail/hits.hpp"
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

// The wound the lab shows: a victim in front of the back wall, shot from the hall's middle.
gd::WoundAt wound(double lost, uint32_t seed) {
  gd::WoundAt w;
  w.at = {2.2, 1.3, 8.7};
  w.shot_dir = gd::sub(w.at, {1.3, 1.5, 4.2});
  w.health_lost = lost;
  w.max_health = 100;
  w.seed = seed;
  return w;
}

// Every live decal's vertices, hashed in slot order.
uint64_t pool_hash(const gd::DecalPool& pool) {
  ge::test::BitsHash h;
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    if (!pool.decals[i].live) continue;
    h.add(pool.decals[i].size_m);
    for (const gd::DecalVertex& v : gd::decal_verts(pool, i)) {
      h.add(v.pos);
      h.add(v.u);
      h.add(v.v);
    }
  }
  return h.h;
}

double blood_area(const gd::DecalPool& pool) {
  double a = 0;
  for (const gd::Decal& d : pool.decals) a += d.live ? d.size_m * d.size_m : 0;
  return a;
}

}  // namespace

TEST(a_shot_holes_where_its_ray_lands) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(8);
  gd::ShotAt s;
  s.from = {-3.8, 1.6, 7};
  s.dir = {0, 0, 1};
  const gd::ShotResult hit = gd::shot_hit(pool, r, s);
  CHECK(hit.hit);
  CHECK(hit.slot == 0);
  CHECK(std::fabs(hit.at.z - 10) < 1e-9);
  CHECK(hit.normal.z < -0.999);
  CHECK(hit.tri >= 0 && hit.tri < static_cast<int>(m.tag.size()));
  CHECK(std::fabs(gd::normal_of(m, static_cast<uint32_t>(hit.tri)).z) > 0.999);
  CHECK(pool.decals[0].kind == gd::DecalKind::bullet_hole);
  for (const gd::DecalVertex& v : gd::decal_verts(pool, 0)) {
    CHECK(gd::length(gd::sub(v.pos, hit.at)) < 0.2);
  }
  // From outside the hall the same wall's far face takes it, facing out.
  s.from = {-3.8, 1.6, 14};
  s.dir = {0, 0, -1};
  const gd::ShotResult back = gd::shot_hit(pool, r, s);
  CHECK(back.hit && back.slot >= 0);
  CHECK(back.at.z > 10);
  CHECK(back.normal.z > 0.999);
}

TEST(a_shot_into_the_open_leaves_nothing) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool pool = gd::make_pool(8);
  gd::ShotAt s;
  s.from = {0.9, 1.5, 5};
  s.dir = {0.1, 1, 0.2};  // the hall has no roof
  const gd::ShotResult miss = gd::shot_hit(pool, r, s);
  CHECK(!miss.hit);
  CHECK(miss.slot == -1 && miss.tri == -1);
  CHECK(gd::live_count(pool) == 0);
}

TEST(blood_grows_with_the_health_lost) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  {
    gd::DecalPool pool = gd::make_pool(32);
    const gd::BleedReport none = gd::bleed(pool, r, wound(0, 7));
    CHECK(none.pressed == 0 && gd::live_count(pool) == 0);
  }
  int last_pressed = 0;
  double last_area = 0;
  for (double lost : {5.0, 25.0, 50.0, 100.0}) {
    gd::DecalPool pool = gd::make_pool(32);
    const gd::BleedReport b = gd::bleed(pool, r, wound(lost, 7));
    CHECK(b.share == lost / 100);
    CHECK(b.pressed >= 1);  // the pool under the wound at the least
    CHECK(b.pressed >= last_pressed);
    CHECK(blood_area(pool) > last_area);
    last_pressed = b.pressed;
    last_area = blood_area(pool);
    for (const gd::Decal& d : pool.decals) {
      if (d.live) CHECK(d.kind == gd::DecalKind::blood);
    }
  }
  // More than the bar holds bleeds as the whole bar.
  gd::DecalPool a = gd::make_pool(32), b = gd::make_pool(32);
  CHECK(gd::bleed(a, r, wound(100, 7)).pressed == gd::bleed(b, r, wound(250, 7)).pressed);
  CHECK(pool_hash(a) == pool_hash(b));
}

TEST(the_same_wound_bleeds_the_same) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  gd::DecalPool a = gd::make_pool(32), b = gd::make_pool(32), c = gd::make_pool(32);
  gd::bleed(a, r, wound(60, 3));
  gd::bleed(b, r, wound(60, 3));
  gd::bleed(c, r, wound(60, 4));
  CHECK(pool_hash(a) == pool_hash(b));
  CHECK(pool_hash(a) != pool_hash(c));
}

TEST(hits_keep_their_bits) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m);
  std::vector<std::vector<uint64_t>> got;
  {
    gd::DecalPool pool = gd::make_pool(16);
    for (uint32_t i = 0; i < 6; ++i) {
      gd::ShotAt s;
      s.from = {-0.8, 1.5, 4};
      s.dir = gd::normalized({-0.3 + 0.1 * i, 0.02 * i - 0.05, 1});
      s.seed = i;
      const gd::ShotResult h = gd::shot_hit(pool, r, s);
      got.push_back({static_cast<uint64_t>(h.slot + 1)});
    }
    got.push_back({pool_hash(pool)});
  }
  for (double lost : {5.0, 25.0, 50.0, 100.0}) {
    gd::DecalPool pool = gd::make_pool(32);
    const gd::BleedReport b = gd::bleed(pool, r, wound(lost, 7));
    got.push_back({static_cast<uint64_t>(b.pressed), pool_hash(pool)});
  }
  ge::test::check_golden("tests/golden/detail_hits.txt", got,
                         "six shots' slots and the pool's hash; then per wound (5, 25, 50, 100 "
                         "of 100): decals pressed and their hash (detail_hits_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
                         "detail-hits lab's pictures before and after in the PR (skill: detail)");
}
