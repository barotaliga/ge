// Shooter's Edge's enhanced weapon effects (games/shooters_edge/app/weapon_fx.hpp): a blast is a
// main ball and its later billows with the same throw for the same seed, a dome off a surface that
// throws into the half space it faces; the rocket's trail is puffs that spread and its flame points
// back along its flight; a hit tears debris off the wall and no smoke, nothing off the sky; the
// bolt's sparks leave the face; a beam's light is capped so it never washes a room out.
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

TEST(a_hit_tears_debris_in_the_walls_colour_with_no_smoke_and_nothing_off_the_sky) {
  const double at[3] = {0, 0, 0}, n[3] = {0, 0, -1}, dir[3] = {0, 0, 1};
  fx::Particles stone, metal, sky, sand, water;
  fx::WallHit w;
  for (int k = 0; k < 3; ++k) w.at[k] = at[k], w.normal[k] = n[k], w.dir[k] = dir[k];
  w.floor_y = -1.5;
  w.colour[0] = 0.3f, w.colour[1] = 0.2f, w.colour[2] = 0.1f;  // a brown wall
  fx::hit(w, 5, &stone);
  fx::WallHit steel = w;
  steel.surface = "metal";
  steel.nail = true;
  fx::hit(steel, 5, &metal);
  fx::WallHit sky_hit = w;
  sky_hit.surface = "sky";
  fx::hit(sky_hit, 5, &sky);
  fx::WallHit sand_hit = w;
  sand_hit.surface = "sand";
  fx::hit(sand_hit, 5, &sand);
  fx::WallHit pool = w;
  pool.surface = "liquid";
  fx::hit(pool, 5, &water);
  CHECK(!stone.empty() && !metal.empty() && sky.empty() && !sand.empty());
  float big = 0, small = 1;
  int fine = 0, browned = 0, hop = 0;
  for (const auto& p : stone) {
    CHECK(p.v[2] <= 1e-6f);     // off the face, back toward the shooter
    CHECK(p.grow == 0);         // nothing spreads: no smoke, no dust cloud
    CHECK(p.fall == 1);         // everything falls on its arc
    CHECK(p.opacity == 1);      // solid pieces, not haze
    CHECK(p.kelvin <= 0);       // no sparks off stone
    CHECK(p.floor_y == -1.5f);  // it comes down on the floor under the hit
    big = std::max(big, p.size);
    small = std::min(small, p.size);
    fine += p.size < 0.011f ? 1 : 0;
    hop += p.bounce > 0 ? 1 : 0;
    browned += p.tint[0] > p.tint[2] ? 1 : 0;               // the wall's own colour
    CHECK(p.tint[2] < p.tint[1] && p.tint[1] < p.tint[0]);  // brown, as the wall is
  }
  CHECK(big >= 0.04f && small < 0.011f);  // big chunks and fine dirt
  CHECK(fine >= 8 && hop >= 4 && browned >= 4);
  for (const auto& p : sand) CHECK(p.size < 0.011f && p.grow == 0);  // sand sprays grains alone
  for (const auto& p : water) CHECK(p.grow == 0 && p.size < 0.03f);  // a splash's beads
  int sparks = 0, metal_pieces = 0;
  for (const auto& p : metal) {
    sparks += p.kelvin > 0 ? 1 : 0;
    metal_pieces += p.kelvin == 0 ? 1 : 0;
  }
  CHECK(sparks > 0);          // a nail off metal strikes sparks
  CHECK(metal_pieces >= 20);  // and every solid face breaks, metal too
  // In the wall's baked light the pieces shine as the wall does: brighter in more light.
  fx::WallHit bright = w, dim = w;
  for (int k = 0; k < 3; ++k) bright.light[k] = 1.6f, dim.light[k] = 0.4f;
  fx::Particles in_bright, in_dim;
  fx::hit(bright, 5, &in_bright);
  fx::hit(dim, 5, &in_dim);
  CHECK(in_bright.size() == in_dim.size() && in_bright.size() == stone.size());
  for (size_t i = 0; i < in_bright.size(); ++i) {
    CHECK(in_bright[i].kelvin == -1 && in_dim[i].kelvin == -1);
    CHECK(std::abs(in_bright[i].tint[1] - 4 * in_dim[i].tint[1]) < 1e-5f);
  }
}

TEST(the_bolts_sparks_leave_the_face) {
  const double at[3] = {0, 0, 0}, n[3] = {1, 0, 0};
  fx::Particles sparks;
  fx::bolt_hit(at, n, 2, &sparks);
  CHECK(!sparks.empty());
  for (const auto& p : sparks) CHECK(p.v[0] >= 0 && p.kelvin > 5000);
}

TEST(there_is_one_bolt_as_thin_as_the_classics_and_keyed_to_the_eye) {
  const gr::BeamLook& l = fx::bolt_look();
  CHECK(&l == &fx::bolt_look());
  // The classic's bolt is a zigzag some 30 cm across round a thin line: a core under 2 cm, a
  // sheath under 10 cm, kinked every few centimetres.
  CHECK(l.core_m <= 0.02f && l.glow_m <= 0.1f && l.step_m <= 0.15f);
  CHECK(l.filaments == 1);
  // As bright on the screen at night as by day.
  CHECK(l.core_keys > 1 && l.glow_keys > 0 && l.glow_keys < 1);
}

TEST(the_bolt_is_the_brightest_light_in_the_room) {
  gr::BeamShot shot;
  shot.to[2] = 6;
  shot.born_s = 0;
  fx::Light ll[gr::kBeamLights];
  const int n = fx::bolt_lights(shot, 0.2, ll);
  CHECK(n >= 3);  // the gun, along it, the hit
  double hit = 0;
  for (int i = 0; i < n; ++i) {
    CHECK(ll[i].radius == fx::kBoltReachM);
    CHECK(ll[i].boost == fx::kBoltBoost);
    for (double c : ll[i].colour) CHECK(c >= 0 && c <= 1.5);
  }
  for (double c : ll[n - 1].colour) hit = std::max(hit, c);
  CHECK(hit >= 0.5);  // where it lands, near the classic's full strength whatever the flicker
  // At r_fx_lights' default it lights as the classic's bolt does; let go, it is dark.
  fx::Light none[gr::kBeamLights];
  shot.end_s = 0.1;
  CHECK(fx::bolt_lights(shot, 1.0, none) == 0);
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

TEST(a_grenades_glow_is_faint_its_skins_colour_and_never_lights_its_own_body) {
  const double at[3] = {1, 2, 3};
  const float red[3] = {1, 0, 0};
  const fx::Light g = fx::grenade_glow(at, red, 42);
  CHECK(g.pos[0] == 1 && g.pos[1] == 2 && g.pos[2] == 3);
  CHECK(g.radius == fx::kGrenadeGlowM && g.radius < fx::kMotorLightM);
  CHECK(g.colour[0] == fx::kGrenadeGlowLevel && g.colour[1] == 0 && g.colour[2] == 0);
  CHECK(g.colour[0] < fx::kMotorLevel);  // well under a rocket's motor
  CHECK(g.entity == 42);
  // It lights a model beside it, but not the grenade it rides on.
  const fx::Light lights[1] = {g};
  double rgb[3], toward[3];
  fx::dynamic_light_at(lights, at, 1, rgb, toward, 7);
  CHECK(rgb[0] > 0 && rgb[1] == 0 && rgb[2] == 0);
  fx::dynamic_light_at(lights, at, 1, rgb, toward, 42);
  CHECK(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);
}
