// Shooter's Edge's enhanced weapon effects (games/shooters_edge/app/weapon_fx.hpp): a blast is a
// main ball and its later billows with the same throw for the same seed, a dome off a surface that
// throws into the half space it faces; the rocket's trail is puffs that spread and its flame points
// back along its flight; a hit throws what its surface throws and nothing off the sky; the bolt's
// sparks leave the face; a beam's light is capped so it never washes a room out.
//
// ge:summary Tests Shooter's Edge's enhanced weapon effects: blasts, trails, exhaust, hits, bolt
// ge:keywords test weapon-fx blast billows dome trail exhaust hit bolt dynamic-light shooter
// ge:skill shooter
#include <cstring>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/weapon_fx.hpp"

namespace {

namespace fx = shooters_edge::effects;
namespace gr = ge::render;

bool same(const fx::Particles& a, const fx::Particles& b) {
  return a.size() == b.size() &&
         (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof a[0]) == 0);
}

}  // namespace

TEST(a_blast_is_a_ball_and_its_later_billows_the_same_for_the_same_seed) {
  const double at[3] = {1, 2, 3};
  std::vector<gr::Blast> a, b;
  fx::Particles pa, pb, pc;
  fx::blast(at, nullptr, 7, 10.0, &a, &pa);
  fx::blast(at, nullptr, 7, 10.0, &b, &pb);
  fx::blast(at, nullptr, 8, 10.0, nullptr, &pc);
  CHECK(a.size() == size_t(1 + fx::kBillows));
  CHECK(same(pa, pb));
  CHECK(!same(pa, pc));
  CHECK(a[0].born_s == 10.0 && a[0].look == &fx::blast_look());
  for (size_t i = 1; i < a.size(); ++i) {
    CHECK(a[i].look == &fx::billow_look());
    CHECK(a[i].born_s > 10.0 && a[i].born_s <= 10.0 + fx::kBillowsBy);
    CHECK(a[i].at[1] > at[1]);  // up and out of the main ball
  }
  int hot = 0;
  for (const auto& p : pa) hot += p.kelvin > 2000 ? 1 : 0;
  CHECK(hot >= 300);  // the ember ball and the sparks
}

TEST(a_blast_off_a_surface_is_a_dome_that_throws_away_from_it) {
  const double at[3] = {0, 0, 0}, up[3] = {0, 1, 0};
  std::vector<gr::Blast> blasts;
  fx::Particles births;
  fx::blast(at, up, 3, 0.0, &blasts, &births);
  CHECK(blasts[0].on_surface && blasts[0].normal[1] == 1);
  for (size_t i = 1; i < blasts.size(); ++i) CHECK(blasts[i].at[1] >= 0);
  for (const auto& p : births) CHECK(p.v[1] >= 0);  // nothing thrown into the floor
}

TEST(a_blasts_light_follows_its_fire_and_goes_out) {
  const double at[3] = {0, 0, 0};
  std::vector<gr::Blast> blasts;
  fx::blast(at, nullptr, 1, 0.0, &blasts, nullptr);
  fx::Light first, later, gone;
  CHECK(fx::blast_light(blasts[0], 0.02, &first));
  CHECK(fx::blast_light(blasts[0], 0.4, &later));
  CHECK(later.colour[0] < first.colour[0]);
  CHECK(!fx::blast_light(blasts[0], 3.0, &gone));
}

TEST(the_rockets_trail_spreads_and_its_flame_points_back) {
  const double from[3] = {0, 1, 0}, to[3] = {3, 1, 0}, dir[3] = {1, 0, 0};
  fx::Particles fire, smoke;
  fx::fire_trail(from, to, 1, &fire);
  fx::smoke_trail(from, to, 1, &smoke);
  const size_t steps = size_t(3 / fx::kRocketStep);
  CHECK(fire.size() == 2 * steps);  // an ember and a puff a step
  CHECK(smoke.size() == size_t(3 / fx::kGrenadeStep));
  int puffs = 0;
  for (const auto& p : fire) puffs += p.grow > 0 ? 1 : 0;
  CHECK(puffs == int(steps));
  for (const auto& p : smoke) CHECK(p.grow > 0 && p.kelvin == 0);
  const gr::MuzzleFire f = fx::exhaust(to, dir, 4, 2.0);
  CHECK(f.pose.dir[0] == -1);  // out of the motor, back along the flight
  CHECK(f.pose.up[0] == 0 && f.pose.up[1] == 1);
  CHECK(f.shot.born_s < 2.0 && f.shot.look == &fx::exhaust_look());
}

TEST(a_hit_throws_its_surfaces_burst_and_nothing_off_the_sky) {
  const double at[3] = {0, 0, 0}, n[3] = {0, 0, -1}, dir[3] = {0, 0, 1};
  std::vector<gr::Blast> dust;
  fx::Particles stone, metal, sky;
  fx::hit(at, n, dir, "stone", false, 5, 0.0, &dust, &stone);
  fx::hit(at, n, dir, "metal", true, 5, 0.0, nullptr, &metal);
  fx::hit(at, n, dir, "sky", false, 5, 0.0, &dust, &sky);
  CHECK(dust.size() == 1);  // the stone's puff; the sky none
  CHECK(!stone.empty() && !metal.empty() && sky.empty());
  for (const auto& p : stone) CHECK(p.v[2] <= 1e-6f);  // off the face, back toward the shooter
  int sparks = 0;
  for (const auto& p : metal) sparks += p.kelvin > 0 ? 1 : 0;
  CHECK(sparks > 0);  // a nail off metal strikes sparks
}

TEST(the_bolts_sparks_leave_the_face_and_its_light_is_capped) {
  const double at[3] = {0, 0, 0}, n[3] = {1, 0, 0};
  fx::Particles sparks;
  fx::bolt_hit(at, n, 2, &sparks);
  CHECK(!sparks.empty());
  for (const auto& p : sparks) CHECK(p.v[0] >= 0 && p.kelvin > 5000);
  gr::LocalLight bright;
  bright.candela = 1e6f;
  const fx::Light l = fx::dynamic_light(bright);
  for (double c : l.colour) CHECK(c <= 1.2);
}

TEST(every_bolt_style_is_numbered_and_its_light_is_a_glow) {
  // 1 forked, 2 arc, 3 bolt (the default), 4 braid; out of range the default's.
  CHECK(&fx::bolt_look() == &fx::bolt_look(3));
  CHECK(&fx::bolt_look(0) == &fx::bolt_look(3) && &fx::bolt_look(9) == &fx::bolt_look(3));
  gr::BeamLook bolt, arc;
  CHECK(gr::beam_look("bolt", bolt) && gr::beam_look("arc", arc));
  CHECK(fx::bolt_look(3).wander == bolt.wander && fx::bolt_look(2).wander == arc.wander);
  CHECK(fx::bolt_look(4).filaments > 1);  // the braid's rope
  for (int s = 1; s <= 4; ++s) CHECK(fx::bolt_look(s).light_cd < bolt.light_cd);
}
