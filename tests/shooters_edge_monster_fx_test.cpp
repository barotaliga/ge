// The monsters' attacks in the modern look (games/shooters_edge/app/monster_fx.hpp): the pack's
// shot models and the format's tracer bits name their kinds; the same seed throws the same trail;
// the classic tracer lays specks either side of its line in turn; every modern kind trails, glows
// and leaves a splash off the face it hits; a splash's light is gone by kSplashLightS.
//
// ge:summary Tests the monsters' missiles in the modern look: kinds, trails, glows, splashes
// ge:keywords test monster-fx acid flame void lava laser flesh tracer splash ce_monsters shooter
// ge:skill shooter
#include <cstring>
#include <vector>

#include "check.hpp"
#include "classicenhance/replace.hpp"
#include "shooters_edge/app/monster_fx.hpp"

namespace {

namespace fx = shooters_edge::effects;
namespace gr = ge::render;

bool same(const fx::Particles& a, const fx::Particles& b) {
  return a.size() == b.size() &&
         (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof a[0]) == 0);
}

constexpr fx::MonsterShot kModern[] = {fx::MonsterShot::acid, fx::MonsterShot::flame,
                                       fx::MonsterShot::void_ball, fx::MonsterShot::lava,
                                       fx::MonsterShot::laser, fx::MonsterShot::flesh};

}  // namespace

TEST(the_packs_shot_models_and_the_tracer_bits_name_the_kinds) {
  const auto kind = [](const char* path) {
    const ge::classicenhance::Replacement* r = ge::classicenhance::replacement_of(path);
    return r == nullptr ? fx::MonsterShot::none : fx::monster_shot_of(r->effect);
  };
  CHECK(kind("progs/w_spike.mdl") == fx::MonsterShot::acid);
  CHECK(kind("progs/k_spike.mdl") == fx::MonsterShot::flame);
  CHECK(kind("progs/v_spike.mdl") == fx::MonsterShot::void_ball);
  CHECK(kind("progs/lavaball.mdl") == fx::MonsterShot::lava);
  CHECK(kind("progs/laser.mdl") == fx::MonsterShot::laser);
  CHECK(kind("progs/zom_gib.mdl") == fx::MonsterShot::flesh);
  CHECK(kind("progs/missile.mdl") == fx::MonsterShot::none);
  CHECK(fx::monster_shot_of_flags(fx::kTracerGreen) == fx::MonsterShot::acid);
  CHECK(fx::monster_shot_of_flags(fx::kTracerOrange) == fx::MonsterShot::flame);
  CHECK(fx::monster_shot_of_flags(fx::kTracerPurple) == fx::MonsterShot::void_ball);
  CHECK(fx::monster_shot_of_flags(fx::kTrailRocket) == fx::MonsterShot::none);
}

TEST(the_classic_tracer_lays_specks_either_side_of_its_line_in_turn) {
  const double from[3] = {0, 1, 0}, to[3] = {0.76, 1, 0};  // just over 30 units along x
  fx::Particles out;
  const int count = fx::classic_tracer(fx::MonsterShot::acid, from, to, 0, 1, {}, &out);
  CHECK(out.size() == size_t(10));
  CHECK(count == 10);
  CHECK(out[0].v[2] * out[1].v[2] < 0);  // one each side, square to the flight
  CHECK(out[0].v[0] == 0);
  CHECK(out[0].kelvin < 0 && out[0].life == 0.5f);
  fx::Particles none;
  fx::classic_tracer(fx::MonsterShot::laser, from, to, 0, 1, {}, &none);
  CHECK(none.empty());  // a laser is its model alone
}

TEST(every_modern_kind_trails_the_same_for_the_same_seed_in_both_variants) {
  const double from[3] = {0, 1, 0}, to[3] = {1, 1, 0};
  for (const fx::MonsterShot k : kModern)
    for (const int v : {1, 2}) {
      fx::Particles a, b, c;
      fx::monster_trail(k, v, from, to, 5, &a);
      fx::monster_trail(k, v, from, to, 5, &b);
      fx::monster_trail(k, v, from, to, 6, &c);
      CHECK(!a.empty());
      CHECK(same(a, b));
      CHECK(!same(a, c));
    }
}

TEST(a_splash_throws_off_the_face_it_hit_and_its_light_fades_out) {
  const double at[3] = {2, 1, 0}, normal[3] = {-1, 0, 0}, dir[3] = {1, 0, 0};
  for (const fx::MonsterShot k :
       {fx::MonsterShot::acid, fx::MonsterShot::flame, fx::MonsterShot::laser}) {
    fx::Particles out;
    std::vector<gr::Blast> blasts;
    fx::monster_splash(k, 1, at, normal, dir, 0, 3, 0, &blasts, &out);
    CHECK(!out.empty());
    for (const gr::Particle& p : out) CHECK(p.v[0] < 0.3f);  // back off the wall, never into it
    fx::Light light;
    CHECK(fx::splash_light(k, 1, at, 0.01, &light));
    CHECK(!fx::splash_light(k, 1, at, fx::kSplashLightS, &light));
  }
  // The fire bolt bursts into a small fireball in the first variant.
  std::vector<gr::Blast> blasts;
  fx::Particles out;
  fx::monster_splash(fx::MonsterShot::flame, 1, at, normal, dir, 0, 3, 0, &blasts, &out);
  CHECK(blasts.size() == size_t(1) && blasts[0].look == &fx::flame_burst_look());
}

TEST(the_shots_glow_and_the_fire_ones_carry_a_flame) {
  const double at[3] = {0, 1, 0}, dir[3] = {1, 0, 0};
  for (const fx::MonsterShot k : kModern) {
    fx::Light light;
    CHECK(fx::monster_glow(k, 1, at, dir, 1, 0, &light) == (k != fx::MonsterShot::flesh));
    gr::MuzzleFire flame;
    const bool fire = k == fx::MonsterShot::flame || k == fx::MonsterShot::lava;
    CHECK(fx::monster_flame(k, 1, at, dir, 1, 0, &flame) == fire);
  }
}
