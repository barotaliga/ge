// Shooter's Edge's effects and looks (games/shooters_edge/app): each burst is the same particles
// for the same seed, glows where it should and fades as it should; the screen's blend is nothing
// in clear air and stacks the liquid, the powers and a hurt; the classic grade keeps the palette's
// own colours.
//
// ge:summary Tests Shooter's Edge's effects (trails, bursts, lights, the blend) and its looks
// ge:keywords test effects particles explosion trail blend palette grade look shooter
// ge:skill shooter
#include <cstring>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/look.hpp"

namespace {

namespace fx = shooters_edge::effects;
namespace lk = shooters_edge::look;

bool same(const fx::Particles& a, const fx::Particles& b) {
  return a.size() == b.size() &&
         (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof a[0]) == 0);
}

}  // namespace

TEST(a_burst_is_the_same_particles_for_the_same_seed) {
  const double at[3] = {1, 2, 3}, n[3] = {0, 1, 0};
  using Burst = void (*)(const double*, uint32_t, fx::Particles*);
  const Burst bursts[] = {fx::explosion, fx::teleport_splash, fx::lava_splash};
  for (Burst b : bursts) {
    fx::Particles x, y, z;
    b(at, 7, &x);
    b(at, 7, &y);
    b(at, 8, &z);
    CHECK(!x.empty());
    CHECK(same(x, y));
    CHECK(!same(x, z));
  }
  fx::Particles a, b;
  fx::impact(at, n, 3, &a);
  fx::impact(at, n, 3, &b);
  CHECK(same(a, b));
  // Every spark off a face leaves it.
  for (const auto& p : a) {
    if (p.kelvin > 0) CHECK(p.v[1] > 0);
  }
}

TEST(a_trail_lays_its_particles_along_the_stretch_it_flew) {
  const double from[3] = {0, 1, 0}, to[3] = {3, 1, 0};
  fx::Particles rocket, grenade;
  fx::rocket_trail(from, to, 1, &rocket);
  fx::grenade_trail(from, to, 1, &grenade);
  const size_t steps = size_t(3 / fx::kTrailStep);
  CHECK(rocket.size() == 2 * steps);  // an ember and a puff a step
  CHECK(grenade.size() == steps);     // smoke alone
  int embers = 0;
  for (const auto& p : rocket) {
    CHECK(p.p[0] >= 0 && p.p[0] <= 3);
    if (p.kelvin > 0) ++embers;
  }
  CHECK(embers == int(steps));
  for (const auto& p : grenade) CHECK(p.kelvin == 0);
}

TEST(an_explosion_glows_then_its_light_fades_to_nothing) {
  const double at[3] = {0, 0, 0};
  fx::Particles burst;
  fx::explosion(at, 1, &burst);
  int hot = 0;
  for (const auto& p : burst) hot += p.kelvin > 1400 ? 1 : 0;
  CHECK(hot >= 100);
  fx::Light a, b, c;
  CHECK(fx::explosion_light(at, 0, &a));
  CHECK(fx::explosion_light(at, 0.4, &b));
  CHECK(b.colour[0] < a.colour[0] && b.radius < a.radius);
  CHECK(!fx::explosion_light(at, fx::kExplosionLight, &c));
}

TEST(the_screen_blend_is_nothing_in_clear_air_and_stacks_what_is_held) {
  double rgba[4];
  fx::screen_blend({}, rgba);
  CHECK(rgba[3] == 0);
  fx::Screen water;
  water.medium = fx::Medium::water;
  fx::screen_blend(water, rgba);
  const double water_share = rgba[3];
  CHECK(water_share > 0 && rgba[2] > rgba[0]);  // blue-green
  fx::Screen lava;
  lava.medium = fx::Medium::lava;
  fx::screen_blend(lava, rgba);
  CHECK(rgba[3] > water_share && rgba[0] > rgba[2]);
  // A hit reddens it, and the flash fades to nothing.
  fx::Screen hurt;
  hurt.hurt = fx::take_hurt(0, 20);
  fx::screen_blend(hurt, rgba);
  CHECK(rgba[3] > 0 && rgba[0] > rgba[1] && rgba[0] > rgba[2]);
  CHECK(fx::fade_hurt(hurt.hurt, 10) == 0);
  // What the armour took of a blow flashes a duller amber under the red: more green in it.
  const double red_green = rgba[1];
  hurt.armour = fx::take_hurt(0, 20);
  fx::screen_blend(hurt, rgba);
  CHECK(rgba[0] > rgba[1] && rgba[1] > red_green);
  fx::Screen armour_only;
  armour_only.armour = fx::take_hurt(0, 20);
  fx::screen_blend(armour_only, rgba);
  CHECK(rgba[3] > 0 && rgba[1] > 0.3 && rgba[2] < rgba[1]);
  // The powers over the water: more of the picture blended than the water alone.
  water.power = true;
  water.shield = true;
  fx::screen_blend(water, rgba);
  CHECK(rgba[3] > water_share && rgba[3] <= 1);
}

TEST(the_classic_grade_keeps_the_palettes_colours) {
  std::vector<uint8_t> palette(768);
  for (size_t i = 0; i < 256; ++i) {
    palette[i * 3] = uint8_t(i);
    palette[i * 3 + 1] = uint8_t(255 - i);
    palette[i * 3 + 2] = uint8_t((i * 7) & 255);
  }
  const std::vector<float> table = lk::palette_grade(palette);
  CHECK(table.size() == ge::render::identity_grade().size());
  // Every entry is one of the palette's colours.
  for (size_t e = 0; e < table.size(); e += 3) {
    bool found = false;
    for (size_t c = 0; c < 256 && !found; ++c) {
      found = table[e] == float(palette[c * 3]) / 255.0f &&
              table[e + 1] == float(palette[c * 3 + 1]) / 255.0f &&
              table[e + 2] == float(palette[c * 3 + 2]) / 255.0f;
    }
    CHECK(found);
  }
  CHECK(lk::palette_grade({}) == ge::render::identity_grade());
}

TEST(the_looks_switch_the_world_renderer_and_the_post) {
  lk::Look l{};
  CHECK(lk::parse_look("classic", &l) && l == lk::Look::classic);
  CHECK(lk::parse_look("modern", &l) && l == lk::Look::modern);
  CHECK(!lk::parse_look("retro", &l));
  const auto c = lk::world_options(lk::Look::classic);
  CHECK(c.mode == ge::brush::app::Mode::classic && c.crisp);
  CHECK(lk::post_settings(lk::Look::classic).bloom == 0);
  const auto m = lk::world_options(lk::Look::modern);
  CHECK(m.mode == ge::brush::app::Mode::v2 && !m.crisp &&
        m.lighting == ge::brush::app::Lighting::lights);
  CHECK(lk::post_settings(lk::Look::modern).bloom > 0);
  CHECK(c.dynamic_strength == 1 && m.dynamic_strength < 1);
  CHECK(lk::effects_glow(lk::Look::classic) == 1 && lk::effects_glow(lk::Look::modern) < 1);
}

TEST(the_picture_cvars_tune_the_modern_look_and_leave_the_classic) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  CHECK(lk::look_of(pic) == lk::Look::modern);  // the window opens on the modern look
  CHECK(pic.ce.water && pic.ce.flames);         // and every enhancement on
  con.execute("ce_water 0; r_look classic");
  CHECK(!pic.ce.water && pic.ce.flames && lk::look_of(pic) == lk::Look::classic);
  con.execute(
      "r_look modern; r_fx_lights 0.8; r_fx_glow 9; r_bloom 0.1; r_lava_glow 0; r_slime_glow 2");
  CHECK(lk::look_of(pic) == lk::Look::modern);
  const auto o = lk::world_options(lk::look_of(pic), {}, pic);
  CHECK(o.dynamic_strength == 0.8 && o.lava_glow == 0 && o.slime_glow == 2);
  CHECK(lk::effects_glow(lk::Look::modern, pic) == 4);  // clamped to the cvar's range
  CHECK(lk::post_settings(lk::Look::modern, pic).bloom == 0.1);
  CHECK(lk::world_options(lk::Look::classic, {}, pic).dynamic_strength == 1);
  CHECK(lk::post_settings(lk::Look::classic, pic).bloom == 0);
  con.execute("r_look retro");
  CHECK(lk::look_of(pic) == lk::Look::modern);
}

TEST(ce_skies_picks_our_sky_or_the_packs_in_the_modern_look) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  CHECK(!lk::world_options(lk::Look::modern, {}, pic).pack_sky);  // on: our sky
  con.execute("ce_skies 0");
  CHECK(lk::world_options(lk::Look::modern, {}, pic).pack_sky);
  CHECK(!lk::world_options(lk::Look::classic, {}, pic).pack_sky);  // the classic's own already
}

TEST(ce_relief_and_ce_parallax_light_the_enhanced_textures_in_the_modern_look) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  const auto on = lk::world_options(lk::Look::modern, {}, pic);
  CHECK(on.relief && on.parallax > 0);
  con.execute("ce_parallax 0");
  const auto flat = lk::world_options(lk::Look::modern, {}, pic);
  CHECK(flat.relief && flat.parallax == 0);
  con.execute("ce_relief 0");
  CHECK(!lk::world_options(lk::Look::modern, {}, pic).relief);
}
