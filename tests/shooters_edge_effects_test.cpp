// Shooter's Edge's effects and looks (games/shooters_edge/app): each burst is the same particles
// for the same seed, glows where it should and fades as it should; the screen's blend is nothing
// in clear air and stacks the liquid, the powers and a hurt; the classic grade keeps the palette's
// own colours.
//
// ge:summary Tests Shooter's Edge's effects (trails, bursts, lights, the blend) and its looks
// ge:keywords test effects particles explosion trail blend palette grade look shooter dust ash
// ge:skill shooter
#include <cmath>
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

// A palette whose entry i is (i, 255 - i, i / 2): every index's colour tells it apart.
std::vector<uint8_t> test_palette() {
  std::vector<uint8_t> p(768);
  for (size_t i = 0; i < 256; ++i) {
    p[i * 3] = uint8_t(i);
    p[i * 3 + 1] = uint8_t(255 - i);
    p[i * 3 + 2] = uint8_t(i / 2);
  }
  return p;
}

TEST(a_burst_is_the_same_specks_for_the_same_seed) {
  const std::vector<uint8_t> pal = test_palette();
  const double at[3] = {1, 2, 3};
  using Burst = void (*)(const double*, uint32_t, fx::Palette, fx::Particles*);
  const Burst bursts[] = {fx::explosion, fx::teleport_splash, fx::lava_splash};
  for (Burst b : bursts) {
    fx::Particles x, y, z;
    b(at, 7, pal, &x);
    b(at, 7, pal, &y);
    b(at, 8, pal, &z);
    CHECK(!x.empty());
    CHECK(same(x, y));
    CHECK(!same(x, z));
    for (const auto& p : x) CHECK(p.kelvin < 0);  // unlit specks of the palette's colours
  }
  fx::Particles a, b;
  fx::impact(at, 20, 3, pal, &a);
  fx::impact(at, 20, 3, pal, &b);
  CHECK(a.size() == 20 && same(a, b));
}

TEST(the_classic_explosion_is_1024_specks_on_its_two_ramps) {
  const std::vector<uint8_t> pal = test_palette();
  const double at[3] = {0, 0, 0};
  fx::Particles burst;
  fx::explosion(at, 1, pal, &burst);
  CHECK(burst.size() == 1024);
  int speeding = 0, slowing = 0;
  for (const auto& p : burst) {
    if (p.drag < 0) ++speeding;  // the bright ramp's, flung out faster and faster
    if (p.drag > 0) ++slowing;
    CHECK(p.life > 0.3f && p.life <= 0.8f);
    CHECK(p.fall > 0 && p.fall < 0.2f);  // the classic's 40 units a second squared
    for (int k = 0; k < 3; ++k) CHECK(p.v[k] >= -6.41f && p.v[k] <= 6.41f);  // 256 units a second
  }
  CHECK(speeding == 512 && slowing == 512);
  // Both ramps start at the palette's entry 111 at their brightest, its colour the pack's.
  float c[3];
  fx::palette_rgb(pal, 111, c);
  bool first = false;
  for (const auto& p : burst) first = first || (p.tint[0] == c[0] && p.tint[1] == c[1]);
  CHECK(first);
}

TEST(a_trail_lays_a_speck_every_three_units_rising) {
  const std::vector<uint8_t> pal = test_palette();
  const double from[3] = {0, 1, 0}, to[3] = {3, 1, 0};
  fx::Particles rocket, grenade;
  fx::rocket_trail(from, to, 1, pal, &rocket);
  fx::grenade_trail(from, to, 1, pal, &grenade);
  const size_t steps = size_t(3 / fx::kTrailStep);
  CHECK(rocket.size() == steps && grenade.size() == steps);
  for (const auto& p : rocket) {
    CHECK(p.p[0] >= -0.1 && p.p[0] <= 3.1);
    CHECK(p.fall < 0);  // warm: it rises
  }
  // The grenade's are the ramp's greys alone: the test palette's low entries, mostly green.
  for (const auto& p : grenade) CHECK(p.tint[1] > p.tint[0]);
}

TEST(an_explosions_light_is_the_classics_and_shrinks_to_nothing) {
  const double at[3] = {0, 0, 0};
  fx::Light a, b, c;
  CHECK(fx::explosion_light(at, 0, &a));
  CHECK(fx::explosion_light(at, 0.4, &b));
  CHECK(a.radius > 8.7 && a.radius < 8.8);  // 350 units
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

TEST(ce_bubbles_share_draws_a_third_of_the_bubbles_by_default_and_none_at_0) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  CHECK(pic.bubbles_share == 0.3);
  con.execute("ce_bubbles_share 0");
  CHECK(pic.bubbles_share == 0);
  con.execute("ce_bubbles_share 5");
  CHECK(pic.bubbles_share == 1);  // clamped to the cvar's range
}

TEST(ce_water_specks_is_off_by_default_and_reaches_the_modern_worlds_options) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  CHECK(lk::world_options(lk::Look::modern, {}, pic).water_specks == 0);
  con.execute("ce_water_specks 0.5");
  CHECK(lk::world_options(lk::Look::modern, {}, pic).water_specks == 0.5);
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

TEST(ce_rebake_adds_the_cached_bounce_in_the_modern_look_only) {
  ge::console::Console con;
  lk::Picture pic;
  lk::add_cvars(con, pic);
  CHECK(lk::world_options(lk::Look::modern, {}, pic).rebake_bounce);
  CHECK(!lk::world_options(lk::Look::classic, {}, pic).rebake_bounce);
  con.execute("ce_rebake 0");
  CHECK(!lk::world_options(lk::Look::modern, {}, pic).rebake_bounce);
}

double linear_land(double share) { return 0.7 * share; }

TEST(dust_blown_in_lands_on_the_body_as_it_appears_and_is_the_same_for_a_seed) {
  fx::Dust d;
  d.at[0] = 1, d.at[1] = 2, d.at[2] = 3;
  d.warp.low = float(d.low), d.warp.high = float(d.high);
  fx::Particles a, b;
  fx::dust_in(d, -1, 0.05, linear_land, 5, &a);
  fx::dust_in(d, -1, 0.05, linear_land, 5, &b);
  CHECK(!a.empty() && same(a, b));
  // An arrival a frame at a time: every flake born ends on the body, as its point appears.
  fx::Particles all;
  double was = -1;
  for (int f = 0; f <= 42; ++f) {
    const double age = f / 60.0;
    fx::Particles births;
    fx::dust_in(d, was, age, linear_land, 100 + uint32_t(f), &births);
    for (const auto& p : births) {
      CHECK(p.drag == 0 && p.fall == 0);
      CHECK(p.life >= 0.02f && age + p.life <= 0.7 + 1e-6);
      const double x = p.p[0] + p.v[0] * p.life - d.at[0], y = p.p[1] + p.v[1] * p.life - d.at[1],
                   z = p.p[2] + p.v[2] * p.life - d.at[2];
      CHECK(std::sqrt(x * x + z * z) <= d.half_width + 1e-3);
      CHECK(y >= d.low - 1e-3 && y <= d.high + 1e-3);
    }
    all.insert(all.end(), births.begin(), births.end());
    was = age;
  }
  CHECK(all.size() > 300);
}

TEST(dust_blown_off_is_thrown_once_wherever_the_cut_passes) {
  fx::Dust d;
  d.warp.low = float(d.low), d.warp.high = float(d.high);
  fx::Particles whole, first, second, none;
  fx::dust_off(d, 1, 0, 9, &whole);
  fx::dust_off(d, 1, 0.5, 9, &first);
  fx::dust_off(d, 0.5, 0, 9, &second);
  fx::dust_off(d, 0.5, 0.5, 9, &none);
  CHECK(whole.size() > 600 && none.empty());
  CHECK(first.size() + second.size() == whole.size());
  for (const auto& p : whole) CHECK(p.v[2] < 0);  // downwind, -z here
}

TEST(a_dynamic_light_adds_to_a_model_in_a_straight_line_to_nothing_at_its_radius) {
  const double p[3] = {0, 0, 0};
  const fx::Light rocket = fx::rocket_light(p);  // 200 units, level 200/128
  const fx::Light lights[1] = {rocket};
  double rgb[3], toward[3];
  // At its middle: a luxel of 200 over 255 in the probe's units, whatever the strength's 1.
  const double at_mid[3] = {0, 0, 0};
  fx::dynamic_light_at(lights, at_mid, 1, rgb, toward);
  CHECK(std::fabs(rgb[0] - 200.0 / 255.0) < 1e-9);
  CHECK(rgb[1] < rgb[0] && rgb[2] < rgb[1]);  // a white a shade warm
  // Half its radius off along +x: half the light, coming from -x.
  const double half[3] = {rocket.radius / 2, 0, 0};
  fx::dynamic_light_at(lights, half, 1, rgb, toward);
  CHECK(std::fabs(rgb[0] - 100.0 / 255.0) < 1e-9);
  CHECK(std::fabs(toward[0] + 1) < 1e-9 && toward[1] == 0 && toward[2] == 0);
  // The strength scales it; at the radius and past it, nothing.
  fx::dynamic_light_at(lights, half, 0.5, rgb, toward);
  CHECK(std::fabs(rgb[0] - 50.0 / 255.0) < 1e-9);
  const double edge[3] = {0, rocket.radius, 0};
  fx::dynamic_light_at(lights, edge, 1, rgb, toward);
  CHECK(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0 && toward[0] == 0 && toward[1] == 0);
  // Two lights either side add up, and the way leans to the nearer.
  const double q[3] = {rocket.radius / 2, 0, 0};
  const fx::Light two[2] = {rocket, fx::muzzle_light(q)};
  const double near_second[3] = {rocket.radius / 3, 0, 0};
  fx::dynamic_light_at(two, near_second, 1, rgb, toward);
  CHECK(rgb[0] > 200.0 / 255.0);
  CHECK(toward[0] > 0);
}

TEST(see_through_liquids_take_the_maps_alpha_else_the_players_else_waters) {
  namespace lk = shooters_edge::look;
  lk::Picture p;
  const double none[4] = {-1, -1, -1, -1};
  double a[4];
  lk::liquid_alphas(p, none, true, a);
  CHECK(a[0] == 1 && a[1] == 1 && a[2] == 1 && a[3] == 1);  // solid by default
  p.water_alpha = 0.4;
  p.lava_alpha = "0.9";
  p.tele_alpha = "word";  // not a number: water's
  lk::liquid_alphas(p, none, true, a);
  CHECK(a[0] == 0.4 && a[1] == 0.4 && a[2] == 0.9 && a[3] == 0.4);
  const double map[4] = {0.6, -1, 2, -1};  // the map's own wins, clamped to 1
  lk::liquid_alphas(p, map, true, a);
  CHECK(a[0] == 0.6 && a[1] == 0.6 && a[2] == 1 && a[3] == 0.6);
  lk::liquid_alphas(p, map, false, a);  // the server keeps them solid
  CHECK(a[0] == 1 && a[1] == 1 && a[2] == 1 && a[3] == 1);
}

TEST(a_maps_liquid_keys_and_a_servers_watervis) {
  namespace lk = shooters_edge::look;
  ge::brush::Map m;
  ge::brush::Entity world;
  world.pairs = {{"wateralpha", "0.5"}, {"_lavaalpha", "0.8"}, {"telealpha", "x"}};
  m.entities.push_back(world);
  double a[4];
  lk::map_liquid_alphas(m, a);
  CHECK(a[0] == 0.5 && a[1] < 0 && a[2] == 0.8 && a[3] < 0);
  lk::map_liquid_alphas(ge::brush::Map{}, a);
  CHECK(a[0] < 0 && a[3] < 0);
  CHECK(lk::watervis_allowed({}));  // absent: allowed
  CHECK(lk::watervis_allowed({{"hostname", "x"}, {"watervis", "1"}}));
  CHECK(!lk::watervis_allowed({{"watervis", "0"}}));  // the server keeps them solid
}
