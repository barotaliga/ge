// The aim spread (plugins/shooter/aim_spread.hpp): where a firing player's line crosses another
// player's box, as a share of the box from its middle; the spread of those points, near 0 for a
// line held on one point at every range and about 0.8 for one falling evenly over the box; and
// the server's frame sampling only living players holding the attack.
//
// ge:summary Tests the aim spread: samples off a box, a held point against an even spread
// ge:keywords aim-spread aimbot bot-likeness sample spread
// ge:skill shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/aim_spread.hpp"
#include "shooter/client.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;

namespace {

// A player's box (32 wide, 56 tall) standing at `at`.
sh::AimBox player_at(sh::Vec3f at) {
  return {at + sh::Vec3f{-16, -16, -24}, at + sh::Vec3f{16, 16, 32}};
}

sh::Vec3f unit(sh::Vec3f v) {
  sh::normalize(v);
  return v;
}

}  // namespace

TEST(aim_spread_a_line_through_the_middle_samples_zero_and_beside_samples_nothing) {
  const sh::AimBox box = player_at({400, 0, 0});  // its middle at z 4
  sh::AimSpread s;
  CHECK(sh::aim_spread_sample(s, {0, 0, 4}, {1, 0, 0}, {&box, 1}));
  CHECK(s.samples == 1 && s.sum_x == 0 && s.sum_y == 0);
  CHECK(!sh::aim_spread_sample(s, {0, 0, 4}, {-1, 0, 0}, {&box, 1}));  // behind
  CHECK(!sh::aim_spread_sample(s, {0, 40, 4}, {1, 0, 0}, {&box, 1}));  // 40 to the side
  CHECK(!sh::aim_spread_sample(s, {0, 0, 4}, {0, 0, 1}, {&box, 1}));   // straight up
  // A line 8 units to the left of the middle is half the half-width across.
  sh::AimSpread t;
  CHECK(sh::aim_spread_sample(t, {0, 8, 4}, {1, 0, 0}, {&box, 1}));
  CHECK_MSG(std::fabs(std::fabs(t.sum_x) - 0.5) < 1e-6, std::to_string(t.sum_x));
  // Of two boxes on the line, the nearer is taken.
  const sh::AimBox two[2] = {player_at({800, 0, 0}), player_at({300, 0, 20})};
  sh::AimSpread u;
  CHECK(sh::aim_spread_sample(u, {0, 0, 4}, {1, 0, 0}, two));
  CHECK_MSG(std::fabs(u.sum_y - (-20.0 / 28.0)) < 1e-5, std::to_string(u.sum_y));
}

TEST(aim_spread_a_held_point_is_tight_and_an_even_spread_is_not) {
  sh::AimSpread held, even;
  CHECK(sh::aim_spread_rms(held) == -1);
  int k = 0;
  for (int i = 0; i < 9; i++)
    for (int j = 0; j < 9; j++, k++) {
      const sh::Vec3f at{200.0f + 150.0f * static_cast<float>(k % 13),
                         37.0f * static_cast<float>(k % 7), 0};
      const sh::AimBox box = player_at(at);
      const sh::Vec3f head = at + sh::Vec3f{0, 0, 24};  // the aim bot's point: the head
      CHECK(sh::aim_spread_sample(held, {0, 0, 4}, unit(head - sh::Vec3f{0, 0, 4}), {&box, 1}));
      // A person's: anywhere on the box, row i and column j of a grid over it.
      const sh::Vec3f spot = at + sh::Vec3f{0, -14.0f + 3.5f * static_cast<float>(j),
                                            -22.0f + 6.5f * static_cast<float>(i)};
      CHECK(sh::aim_spread_sample(even, {0, 0, 4}, unit(spot - sh::Vec3f{0, 0, 4}), {&box, 1}));
    }
  const double h = sh::aim_spread_rms(held), e = sh::aim_spread_rms(even);
  CHECK_MSG(h >= 0 && h < 0.05, "a held point's spread " + std::to_string(h));
  CHECK_MSG(e > 0.6 && e < 1.0, "an even spread " + std::to_string(e));
}

TEST(aim_spread_the_server_samples_a_living_player_holding_the_attack) {
  Bench b;
  b.start();
  sh::connect_client(b.sv, 0, false, false);
  sh::connect_client(b.sv, 1, false, false);
  for (int e : {1, 2}) {
    sh::Edict& p = b.ent(e);
    p.solid = sh::kSolidSlidebox;
    p.health = 100;
    p.view_ofs = {0, 0, 22};
    sh::set_size(b.sv, e, sh::kPlayerMins, sh::kPlayerMaxs);
  }
  sh::set_origin(b.sv, 1, {0, 0, 24});
  sh::set_origin(b.sv, 2, {300, 0, 24});
  b.ent(1).v_angle = {0, 0, 0};   // facing +x, at player 2
  b.ent(2).v_angle = {0, 90, 0};  // facing away
  sh::AimSpread spreads[2];
  sh::AimSpread* into[2] = {&spreads[0], &spreads[1]};
  sh::aim_spread_frame(b.sv, into);
  CHECK(spreads[0].samples == 0);  // not firing
  b.ent(1).button0 = 1;
  b.ent(2).button0 = 1;
  sh::aim_spread_frame(b.sv, into);
  CHECK(spreads[0].samples == 1);
  CHECK(spreads[1].samples == 0);  // firing at nobody
  b.ent(2).health = 0;             // a dead target is crossed by nobody
  sh::aim_spread_frame(b.sv, into);
  CHECK(spreads[0].samples == 1);
}
