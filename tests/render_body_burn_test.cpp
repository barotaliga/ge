// engine/render/body_burn.hpp: a body in lava heats at once and chars, in slime it is wet with acid
// and etched; out of either it wears off in a few seconds and goes cold; it throws flames only
// while hot, fumes only while wet, nothing under the liquid's level and nothing once cold; the
// same seed throws the same; the skin and the stir read the levels.
//
// ge:summary Tests for a body's scorch: the lags in and out, the births, the skin and the stir
// ge:keywords test render scorch burn charred acid fumes flames smoke births wear-off
// ge:skill render testing
#include <cstdio>
#include <vector>

#include "check.hpp"
#include "render/body_burn.hpp"

namespace {

namespace r = ge::render;

constexpr double kDt = 1.0 / 60;

void run(r::BodyBurn& s, double hot, double acid, double seconds) {
  const int steps = static_cast<int>(seconds / kDt + 0.5);
  for (int i = 0; i < steps; ++i) r::step_body_burn(s, r::body_burn_look(), hot, acid, kDt);
}

r::BurnShape body() {
  r::BurnShape b;
  b.feet[0] = 3, b.feet[1] = -0.4, b.feet[2] = 7;
  b.level_y = 0.3;  // waist deep
  return b;
}

// Births over `seconds` at `hot`, `acid`, each kind counted by its look.
struct Counts {
  int flames = 0, smoke = 0, fumes = 0, under = 0;
};
Counts throw_for(r::BodyBurn& s, double hot, double acid, double seconds) {
  Counts c;
  std::vector<r::Particle> out;
  const r::BurnShape b = body();
  const int steps = static_cast<int>(seconds / kDt + 0.5);
  for (int i = 0; i < steps; ++i) {
    r::step_body_burn(s, r::body_burn_look(), hot, acid, kDt);
    out.clear();
    r::body_burn_births(s, r::body_burn_look(), b, kDt, out);
    for (const r::Particle& p : out) {
      if (p.kelvin > 0)
        ++c.flames;
      else if (p.tint[1] > p.tint[0] + 0.05f)
        ++c.fumes;
      else
        ++c.smoke;
      if (p.p[1] < b.level_y) ++c.under;
    }
  }
  return c;
}

}  // namespace

TEST(lava_heats_at_once_and_chars_then_it_wears_off_in_seconds) {
  r::BodyBurn s = r::make_body_burn(7);
  run(s, 1, 0, 1.0);
  CHECK(s.heat > 0.9 && s.charred > 0.6);
  CHECK(s.acid == 0 && s.etched == 0);
  run(s, 0, 0, 1.0);
  CHECK(s.heat < 0.4 && s.charred > 0.3);  // out a second: cooling, still black
  run(s, 0, 0, 5.0);
  CHECK(s.heat == 0 && s.charred == 0 && r::body_burn_cold(s));  // out six: gone
}

TEST(a_foot_in_lava_heats_less_than_all_in) {
  r::BodyBurn feet = r::make_body_burn(1), all = r::make_body_burn(1);
  run(feet, 1.0 / 3, 0, 2.0);
  run(all, 1, 0, 2.0);
  CHECK(feet.heat > 0.25 && feet.heat < 0.4 && all.heat > feet.heat * 2);
}

TEST(slime_wets_with_acid_and_etches_then_dries_off) {
  r::BodyBurn s = r::make_body_burn(3);
  run(s, 0, 1, 2.0);
  CHECK(s.acid > 0.95 && s.etched > 0.7 && s.heat == 0 && s.charred == 0);
  run(s, 0, 0, 8.0);
  CHECK(r::body_burn_cold(s));
}

TEST(flames_only_while_hot_fumes_only_while_wet_none_under_the_level_none_cold) {
  r::BodyBurn s = r::make_body_burn(11);
  const Counts in_lava = throw_for(s, 1, 0, 1.0);
  std::printf("in lava a second: %d flames, %d smoke, %d fumes, %d under\n", in_lava.flames,
              in_lava.smoke, in_lava.fumes, in_lava.under);
  CHECK(in_lava.flames > 10 && in_lava.smoke > 10 && in_lava.fumes == 0 && in_lava.under == 0);
  const Counts after = throw_for(s, 0, 0, 3.0);  // the smoke goes on as it cools; the flames die
  CHECK(after.smoke > 0 && after.flames < in_lava.flames);
  throw_for(s, 0, 0, 6.0);
  const Counts cold = throw_for(s, 0, 0, 1.0);
  CHECK(cold.flames == 0 && cold.smoke == 0 && cold.fumes == 0);
  r::BodyBurn w = r::make_body_burn(11);
  const Counts in_slime = throw_for(w, 0, 1, 1.0);
  CHECK(in_slime.fumes > 8 && in_slime.flames == 0);
}

TEST(the_same_seed_throws_the_same) {
  r::BodyBurn a = r::make_body_burn(5), b = r::make_body_burn(5);
  std::vector<r::Particle> pa, pb;
  const r::BurnShape bd = body();
  for (int i = 0; i < 30; ++i) {
    r::step_body_burn(a, r::body_burn_look(), 1, 0.5, kDt);
    r::step_body_burn(b, r::body_burn_look(), 1, 0.5, kDt);
    r::body_burn_births(a, r::body_burn_look(), bd, kDt, pa);
    r::body_burn_births(b, r::body_burn_look(), bd, kDt, pb);
  }
  CHECK(pa.size() == pb.size() && !pa.empty());
  for (size_t i = 0; i < pa.size() && i < pb.size(); ++i) {
    CHECK_BITS(pa[i].p[0], pb[i].p[0]);
    CHECK_BITS(pa[i].v[1], pb[i].v[1]);
  }
}

TEST(the_skin_and_the_stir_read_the_levels) {
  r::BodyBurn s = r::make_body_burn(9);
  CHECK(r::body_burn_skin(s).charred == 0 && r::body_burn_stir(s).heat == 0);
  run(s, 1, 0, 1.0);
  const r::BurnSkin k = r::body_burn_skin(s);
  CHECK(k.charred == s.charred && k.embers == s.heat && k.etched == 0);
  const r::BurnStir st = r::body_burn_stir(s);
  CHECK(st.heat == s.heat && st.smoke > 0.5f);
}
