// engine/detail/debris.hpp: a piece dropped on the test hall's floor bounces, is heard, slides
// and lies still along the floor at its radius over it, then fades and is let go; one thrown at a
// wall comes back off it; the pool reuses its oldest piece; a piece with nothing to land on is let
// go after its flight; the axes' turn round-trips. A spray of cases thrown across the hall is held
// bit for bit by tests/golden/detail_debris.txt.
//
// ge:summary Test: thrown debris bounces, is heard, slides, rests, fades; the pool; golden
// ge:keywords test detail debris bounce rest fade landing pool golden
// ge:skill detail testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "detail/debris.hpp"
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

gd::DebrisShape case_shape() {
  gd::DebrisShape s;
  s.radius_m = 0.01;
  s.life_s = 5;
  s.fade_s = 1;
  return s;
}

// The pool run in display frames of 1/60 s from `from_s` to `to_s`.
void run(gd::DebrisPool& pool, const gd::Receivers* r, gd::ReceiverScratch& scratch, double from_s,
         double to_s, std::vector<gd::DebrisLanding>* landings) {
  const int frames = static_cast<int>((to_s - from_s) * 60 + 1e-6);
  for (int i = 1; i <= frames; ++i)
    gd::step_debris(pool, r, scratch, from_s + i / 60.0, 1.0 / 60, landings);
}

}  // namespace

TEST(a_dropped_case_bounces_slides_and_lies_along_the_floor) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m, 1);
  gd::ReceiverScratch scratch;
  const gd::DebrisShape shapes[] = {case_shape()};
  gd::DebrisPool pool = gd::make_debris_pool(8, shapes);
  gd::DebrisThrow t;
  t.at = {0.5, 1.2, 2};
  t.vel = {1.5, 1, 0};
  t.spin = {5, 20, 3};
  CHECK(gd::throw_debris(pool, t, 0) == 0);
  std::vector<gd::DebrisLanding> landings;
  run(pool, &r, scratch, 0, 3, &landings);
  const gd::Debris& d = pool.pieces[0];
  CHECK(d.serial != 0);
  CHECK_MSG(d.still >= 0, "the case should lie still within 3 s");
  CHECK(!landings.empty());
  CHECK(landings[0].bounce == 1 && landings[0].normal.y > 0.99 && landings[0].speed > 3);
  for (size_t i = 1; i < landings.size(); ++i) CHECK(landings[i].speed < landings[0].speed);
  // On the floor at its radius, its long axis flat.
  CHECK(std::fabs(d.at.y - 0.01) < 1e-9);
  const gd::Vec3 long_axis = ge::core::rotate(d.turn, {1, 0, 0});
  CHECK(std::fabs(long_axis.y) < 1e-9);
  CHECK(d.at.x > 0.5);  // it went the way it was thrown
  // Shown whole until its last second, half way through that, then let go.
  CHECK(gd::debris_shown(pool, d, d.still + 3.9) == 1);
  CHECK(std::fabs(gd::debris_shown(pool, d, d.still + 4.5) - 0.5) < 1e-9);
  run(pool, &r, scratch, 3, d.still + 5.05, nullptr);
  CHECK(pool.pieces[0].serial == 0 && gd::debris_count(pool) == 0);
}

TEST(a_case_thrown_at_a_wall_comes_back_off_it) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m, 1);
  gd::ReceiverScratch scratch;
  const gd::DebrisShape shapes[] = {case_shape()};
  gd::DebrisPool pool = gd::make_debris_pool(4, shapes);
  // The right wall's inner face is at x 6.
  gd::DebrisThrow t;
  t.at = {5.5, 2, 2};
  t.vel = {4, 0, 0};
  gd::throw_debris(pool, t, 0);
  std::vector<gd::DebrisLanding> landings;
  run(pool, &r, scratch, 0, 0.3, &landings);
  const gd::Debris& d = pool.pieces[0];
  CHECK(!landings.empty() && landings[0].normal.x < -0.99);
  CHECK(d.vel.x < 0 && d.at.x < 6 - 0.01 + 1e-9);
}

TEST(the_pool_reuses_its_oldest_piece) {
  const gd::DebrisShape shapes[] = {case_shape()};
  gd::DebrisPool pool = gd::make_debris_pool(3, shapes);
  gd::DebrisThrow t;
  for (int i = 0; i < 3; ++i) CHECK(gd::throw_debris(pool, t, i) == i);
  CHECK(gd::throw_debris(pool, t, 3) == 0);
  CHECK(gd::throw_debris(pool, t, 4) == 1);
  CHECK(pool.pieces[0].serial == 4 && pool.pieces[1].serial == 5 && pool.pieces[2].serial == 3);
  // A kind the pool lacks is its first.
  t.kind = 7;
  CHECK(pool.pieces[size_t(gd::throw_debris(pool, t, 5))].kind == 0);
}

TEST(a_piece_with_nothing_under_it_is_let_go_after_its_flight) {
  const gd::DebrisShape shapes[] = {case_shape()};
  gd::DebrisPool pool = gd::make_debris_pool(2, shapes);
  gd::ReceiverScratch scratch;
  gd::throw_debris(pool, {}, 0);
  run(pool, nullptr, scratch, 0, 5.9, nullptr);
  CHECK(gd::debris_count(pool) == 1);
  run(pool, nullptr, scratch, 5.9, 6.1, nullptr);
  CHECK(gd::debris_count(pool) == 0);
}

TEST(the_turn_of_axes_carries_the_body_axes_onto_them) {
  const gd::Vec3 cases[][2] = {{{1, 0, 0}, {0, 1, 0}},
                               {{0, 0, 1}, {1, 0, 0}},
                               {{-1, 0, 0}, {0, -1, 0}},
                               {gd::normalized({1, 1, 0}), gd::normalized({-1, 1, 0})},
                               {{0, -1, 0}, {0, 0, -1}}};
  for (const auto& c : cases) {
    const ge::core::Quat q = gd::turn_of_axes(c[0], c[1]);
    const gd::Vec3 x = ge::core::rotate(q, {1, 0, 0}), y = ge::core::rotate(q, {0, 1, 0});
    CHECK(gd::length(gd::sub(x, c[0])) < 1e-12 && gd::length(gd::sub(y, c[1])) < 1e-12);
  }
}

TEST(a_spray_of_cases_is_held_bit_for_bit) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m, 1);
  gd::ReceiverScratch scratch;
  gd::DebrisShape brass = case_shape();
  gd::DebrisShape hull = case_shape();
  hull.radius_m = 0.012;
  hull.restitution = 0.25;
  const gd::DebrisShape shapes[] = {brass, hull};
  gd::DebrisPool pool = gd::make_debris_pool(16, shapes);
  ge::core::Rng rng = ge::core::make_rng(38);
  std::vector<gd::DebrisLanding> landings;
  double t = 0;
  for (int i = 0; i < 24; ++i) {
    gd::DebrisThrow th;
    th.at = {ge::core::range(rng, -4, 4), 1.4, ge::core::range(rng, 1, 8)};
    th.vel = {ge::core::range(rng, -2, 2), ge::core::range(rng, 0.5, 2),
              ge::core::range(rng, -2, 2)};
    th.spin = {ge::core::range(rng, -30, 30), ge::core::range(rng, -30, 30), 0};
    th.kind = static_cast<uint16_t>(i % 2);
    gd::throw_debris(pool, th, t);
    run(pool, &r, scratch, t, t + 0.1, &landings);
    t += 0.1;
  }
  run(pool, &r, scratch, t, t + 2, &landings);
  std::vector<std::vector<uint64_t>> got;
  for (const gd::Debris& d : pool.pieces) {
    ge::test::BitsHash h;
    h.add(d.at);
    h.add(d.vel);
    h.add(d.turn.x);
    h.add(d.turn.y);
    h.add(d.turn.z);
    h.add(d.turn.w);
    h.add(d.still);
    got.push_back({d.serial, d.bounces, h.h});
  }
  ge::test::BitsHash heard;
  for (const gd::DebrisLanding& l : landings) {
    heard.add(l.at);
    heard.add(l.speed);
  }
  got.push_back({landings.size(), heard.h});
  ge::test::check_golden("tests/golden/detail_debris.txt", got,
                         "per piece: serial, bounces, its state's hash; then the landings' count "
                         "and hash (detail_debris_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and say why in the "
                         "PR (skill: detail)");
}
