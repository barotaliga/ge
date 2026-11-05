// Shooter's Edge's enhanced weapon effects (games/shooters_edge/app/weapon_fx.hpp): a blast is a
// main ball and its later billows with the same throw for the same seed, a dome off a surface that
// throws into the half space it faces; the rocket's trail is puffs that spread and its flame points
// back along its flight; a hit throws what its surface throws and nothing off the sky; the bolt's
// sparks leave the face; a beam's light is capped so it never washes a room out.
//
// ge:summary Tests Shooter's Edge's enhanced weapon effects: blasts, trails, exhaust, hits, bolt
// ge:keywords test weapon-fx blast billows dome trail exhaust hit bolt dynamic-light shooter
// ge:skill shooter
#include <algorithm>
#include <cmath>
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

TEST(a_hit_throws_its_surfaces_burst_in_the_walls_colour_and_nothing_off_the_sky) {
  const double at[3] = {0, 0, 0}, n[3] = {0, 0, -1}, dir[3] = {0, 0, 1};
  std::vector<gr::Blast> dust;
  fx::Particles stone, metal, sky;
  fx::WallHit w;
  for (int k = 0; k < 3; ++k) w.at[k] = at[k], w.normal[k] = n[k], w.dir[k] = dir[k];
  w.floor_y = -1.5;
  w.colour[0] = 0.3f, w.colour[1] = 0.2f, w.colour[2] = 0.1f;  // a brown wall
  fx::hit(w, 5, 0.0, &dust, &stone);
  fx::WallHit steel = w;
  steel.surface = "metal";
  steel.nail = true;
  fx::hit(steel, 5, 0.0, nullptr, &metal);
  fx::WallHit sky_hit = w;
  sky_hit.surface = "sky";
  fx::hit(sky_hit, 5, 0.0, &dust, &sky);
  CHECK(dust.size() == 1);  // the stone's puff; the sky none
  CHECK(!stone.empty() && !metal.empty() && sky.empty());
  int flakes = 0, browned = 0;
  for (const auto& p : stone) {
    CHECK(p.v[2] <= 1e-6f);     // off the face, back toward the shooter
    if (p.grow > 0) continue;   // the wisp
    CHECK(p.floor_y == -1.5f);  // what it knocks loose comes down on the floor under it
    flakes += p.bounce > 0 ? 1 : 0;
    browned += p.tint[0] > p.tint[2] ? 1 : 0;  // a hint of the wall's own colour
    CHECK(std::abs(p.tint[0] - 1) < 0.2f && std::abs(p.tint[2] - 1) < 0.2f);  // monotone
    CHECK(p.kelvin <= 0);                                                     // no sparks off stone
  }
  CHECK(flakes >= 2 && browned >= 2);
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

TEST(a_chip_hops_on_its_floor_and_comes_to_rest) {
  gr::Particle p{};
  p.p[1] = 1;
  p.v[0] = 1;
  p.v[1] = -4;
  p.life = 5;
  p.fall = 1;
  p.floor_y = 0;
  p.bounce = 0.3f;
  gr::step_particle(p, 0.3f);  // past the floor: a hop up at 0.3 of its speed, slowed along it
  CHECK(p.p[1] == 0 && p.v[1] > 0 && p.v[1] < 2.2f && p.v[0] < 1);  // (4 + 9.81 * 0.3) * 0.3
  for (int i = 0; i < 400; ++i) gr::step_particle(p, 0.01f);
  CHECK(p.p[1] == 0 && p.v[0] == 0 && p.v[1] == 0);  // lying still
  CHECK(p.age >= p.life * 0.7f);                     // and fading
}

TEST(the_rockets_motor_lights_the_world_from_its_nozzle_and_wavers) {
  const double at[3] = {0, 1, 0}, dir[3] = {1, 0, 0}, still[3] = {0, 0, 0};
  const fx::Light l = fx::motor_light(at, dir, 3, 1.0);
  CHECK(std::abs(l.pos[0] + fx::kNozzleM) < 1e-12 && l.pos[1] == 1);  // behind it, at the nozzle
  CHECK(l.radius == fx::kMotorLightM);
  CHECK(l.colour[0] > l.colour[1] && l.colour[1] > l.colour[2]);  // a flame's orange-yellow
  const fx::Light on = fx::motor_light(at, still, 3, 1.0);
  CHECK(on.pos[0] == 0);
  double lo = 1e9, hi = 0;
  for (int i = 0; i < 200; ++i) {
    const double r = fx::motor_light(at, dir, 3, 1.0 + i * 0.005).colour[0];
    lo = std::min(lo, r);
    hi = std::max(hi, r);
  }
  CHECK(hi > lo * 1.1 && lo >= 0.8 * fx::kMotorLevel - 1e-9 && hi <= fx::kMotorLevel + 1e-9);
  CHECK(fx::motor_light(at, dir, 3, 1.2).colour[0] != fx::motor_light(at, dir, 4, 1.2).colour[0]);
}

TEST(a_nails_sparks_off_metal_flick_a_light_that_fades_at_once) {
  const double at[3] = {1, 2, 3};
  fx::Light first, later, gone;
  CHECK(fx::spark_light(at, 0.0, &first) && fx::spark_light(at, 0.05, &later));
  CHECK(first.pos[0] == 1 && first.radius == 1.5);
  CHECK(later.colour[0] < first.colour[0] && first.colour[0] > first.colour[2]);  // warm, fading
  CHECK(!fx::spark_light(at, fx::kSparkLightS, &gone) && !fx::spark_light(at, -0.01, &gone));
}

TEST(a_rocket_pops_drops_and_settles_onto_its_line) {
  const double dir[3] = {0, 0, 1};
  fx::RocketLaunch still;
  still.seed = 9;
  double off[3];
  fx::launch_offset(0, dir, still, off);
  CHECK(off[0] == 0 && off[1] == 0 && off[2] == 0);  // at the muzzle, on its line
  fx::launch_offset(0.1, dir, still, off);
  CHECK(off[2] < -0.2 && off[1] < -0.1);  // behind its true place and under it
  CHECK(std::abs(off[0]) < 0.05);         // a waver at most across it
  fx::launch_offset(fx::kLaunchS, dir, still, off);
  CHECK(off[0] == 0 && off[1] == 0 && off[2] == 0);  // on its line again
  // The shooter's sway: a turn to the left leaves the rocket right of the line, his strafe carries
  // it with him; capped.
  double kick[3];
  const double v[3] = {0, 0, 0};
  fx::launch_kick(dir, v, -2.0, 0, kick);
  CHECK(kick[0] > 0.1);  // right of a line along +z is +x
  const double strafe[3] = {-3, 0, 2};
  fx::launch_kick(dir, strafe, 0, 0, kick);
  CHECK(kick[0] < 0 && std::abs(kick[2]) < 1e-12);  // across the line only
  fx::launch_kick(dir, v, 100.0, 50.0, kick);
  CHECK(std::sqrt(kick[0] * kick[0] + kick[1] * kick[1] + kick[2] * kick[2]) <= fx::kKickM + 1e-9);
  fx::RocketLaunch swayed = still;
  swayed.kick[0] = 0.4;
  double a[3], b[3];
  fx::launch_offset(0.12, dir, still, a);
  fx::launch_offset(0.12, dir, swayed, b);
  CHECK(b[0] - a[0] > 0.3);
}
