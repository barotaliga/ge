// plugins/classic/app/look: the picture's cvars take the game's Picture as their defaults (a game
// that sets a classic look and other strengths keeps them, and its values text never names
// another default), the shared defaults are the modern look's, and the teamcolor words parse.
//
// ge:summary Tests a shooter's picture cvars: a game's own defaults, the shared ones, colours
// ge:keywords test look picture-settings game-defaults r_look r_fx_lights values_for_default
// ge:skill shooter testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "classic/app/look.hpp"
#include "console/console.hpp"
#include "console/words.hpp"

namespace lk = ge::classic::look;
namespace con = ge::console;

namespace {

// The cvar `name` registered on `c`; a test fails (and an empty one stands in) when it is not.
const con::Cvar& cvar(const con::Console& c, const char* name) {
  static const con::Cvar kNone;
  const con::Cvar* v = c.cvars().find(name);
  CHECK_MSG(v != nullptr, std::string(name) + " is not registered");
  return v != nullptr ? *v : kNone;
}

}  // namespace

TEST(the_shared_defaults_are_the_modern_looks_tuning) {
  con::Console c;
  lk::Picture p;
  lk::add_cvars(c, p);
  const con::Cvar& look = cvar(c, "r_look");
  CHECK(look.fallback == "modern" && p.look == "modern");
  CHECK(con::stated_default(look.values) == "modern");
  CHECK(cvar(c, "r_fx_lights").fallback == "0.2");
  CHECK(cvar(c, "r_rocketlight").fallback == "1");
  CHECK(cvar(c, "r_wallcolor").fallback == "150 140 125");
  CHECK(cvar(c, "ce_water_quality").fallback == "high");
  for (const con::Cvar* v : c.cvars().sorted())
    CHECK_MSG(con::words_fault(*v).empty(), v->name + ": " + con::words_fault(*v));
}

TEST(a_game_keeps_its_own_defaults) {
  con::Console c;
  lk::Picture p;
  p.look = "classic";  // a game whose picture starts classic, its effects at full strength
  p.fx_lights = 1;
  p.rocket_light = false;
  p.water_quality = "low";
  p.base_skin = "";
  lk::add_cvars(c, p);
  const con::Cvar& look = cvar(c, "r_look");
  CHECK(look.fallback == "classic" && look.is_default() && p.look == "classic");
  // Its words name no other default: modern's mark is gone, none put in its place.
  CHECK(look.values.find("the default") == std::string::npos);
  CHECK(con::stated_default(cvar(c, "r_fx_lights").values).empty());
  CHECK(cvar(c, "r_fx_lights").fallback == "1" && p.fx_lights == 1);
  CHECK(cvar(c, "r_rocketlight").fallback == "0" && !p.rocket_light);
  CHECK(cvar(c, "ce_water_quality").fallback == "low");
  CHECK(cvar(c, "baseskin").values.find("the default") == std::string::npos);
  for (const con::Cvar* v : c.cvars().sorted())
    CHECK_MSG(con::words_fault(*v).empty(), v->name + ": " + con::words_fault(*v));
  // The player's setting moves off the game's default and back.
  c.execute("r_look modern");
  CHECK(p.look == "modern" && lk::look_of(p) == lk::Look::modern);
  c.execute("reset r_look");
  CHECK(p.look == "classic" && lk::look_of(p) == lk::Look::classic);
}

TEST(the_centre_message_stays_as_long_as_scr_centertime_says) {
  con::Console c;
  lk::Picture p;
  lk::add_cvars(c, p);
  CHECK(cvar(c, "scr_centertime").fallback == "2" && p.centre_time == 2);
  c.execute("scr_centertime 5");
  CHECK(p.centre_time == 5);
  c.execute("scr_centertime 100");  // past its range: held at the most
  CHECK(p.centre_time == 60);
}

TEST(r_fullbright_is_a_cheat_that_lights_every_face_as_painted) {
  con::Console c;
  lk::Picture p;
  lk::add_cvars(c, p);
  c.execute("r_fullbright 1");  // refused: sv_cheats is 0
  CHECK(!p.fullbright && !lk::world_options(lk::Look::classic, {}, p).fullbright);
  c.execute("sv_cheats 1");
  c.execute("r_fullbright 1");
  CHECK(p.fullbright);
  CHECK(lk::world_options(lk::Look::classic, {}, p).fullbright);
  CHECK(lk::world_options(lk::Look::modern, {}, p).fullbright);
  c.execute("sv_cheats 0");  // the permission gone, the cheat goes with it
  CHECK(!p.fullbright);
}

TEST(a_forced_colour_is_off_one_colour_or_top_and_bottom) {
  int top = 0, bottom = 0;
  CHECK(lk::parse_forced_colours("4", "", &top, &bottom) && top == 4 && bottom == 4);
  CHECK(lk::parse_forced_colours("2", "16", &top, &bottom) && top == 2 && bottom == 13);
  CHECK(!lk::parse_forced_colours("red", "", &top, &bottom) && top == 2);
  CHECK(lk::parse_forced_colours("off", "", &top, &bottom) && top == -1 && bottom == -1);
}

TEST(r_waterwarp_switches_the_wobble_under_water) {
  lk::Picture p;
  CHECK(lk::world_options(lk::Look::modern, {}, p).water_warp);
  p.water_warp = false;
  CHECK(!lk::world_options(lk::Look::modern, {}, p).water_warp);
}

TEST(the_last_classic_enhance_switches_reach_the_modern_looks_options) {
  con::Console c;
  lk::Picture p;
  lk::add_cvars(c, p);
  const auto modern = [&] { return lk::world_options(lk::Look::modern, {}, p); };
  CHECK(modern().flicker && modern().reflections > 0);
  c.execute("ce_flicker 0");
  c.execute("ce_reflections 0");
  CHECK(!modern().flicker && modern().reflections == 0);
  // The classic look draws none of them.
  c.execute("ce_reflections 1");
  CHECK(lk::world_options(lk::Look::classic, {}, p).reflections == 0);
  // ce_ao: the contact shade at its strength and its radius in metres, the classic look none.
  CHECK(modern().ao == 0.8 && std::abs(modern().ao_radius_m - 48 * 0.025) < 1e-12);
  c.execute("ce_ao_strength 0.5");
  c.execute("ce_ao_radius 64");
  CHECK(modern().ao == 0.5 && std::abs(modern().ao_radius_m - 1.6) < 1e-12);
  c.execute("ce_ao 0");
  CHECK(modern().ao == 0);
  c.execute("ce_ao 1");
  CHECK(lk::world_options(lk::Look::classic, {}, p).ao == 0);
}

// The classic look is shown as worked out (no exposure, no tone curve), and neither look's tone
// curve has its toe, which turned the classic games' dark greys and blues brown.
TEST(the_classic_look_is_shown_as_worked_out_and_neither_has_the_toe) {
  namespace look = ge::classic::look;
  const ge::render::PostSettings c = look::post_settings(look::Look::classic);
  const ge::render::PostSettings m = look::post_settings(look::Look::modern);
  CHECK(c.display == ge::brush::app::Options{}.brightness && m.display == 0);
  CHECK(!c.toe && !m.toe);
  CHECK(ge::render::PostSettings{}.toe && ge::render::PostSettings{}.display == 0);  // the engine's
  const look::Picture p;
  CHECK(look::light_scale(p) == 1);  // intensity 2, gl_modulate 1
  const ge::brush::app::Options o = look::world_options(look::Look::classic, {}, p);
  CHECK(o.classic_light == 1 && o.classic_lift == 0);
}

// The sharpen after the upscale is on only while r_scale is under 1, at r_scale_sharpen's strength
// held to 0..1; at r_scale 1 it is 0, which the post pass skips (the picture as it was).
TEST(the_upscale_sharpen_is_on_only_under_r_scale_1) {
  namespace look = ge::classic::look;
  look::Picture p;
  CHECK(look::upscale_sharpen(p) == 0);
  p.render_scale = 0.5;
  CHECK(look::upscale_sharpen(p) == 0);  // crisp squares (r_scale_smooth 0): never sharpened
  p.scale_smooth = true;
  CHECK(look::upscale_sharpen(p) == 0.5);
  p.scale_sharpen = 3;
  CHECK(look::upscale_sharpen(p) == 1);
  p.scale_sharpen = 0;
  CHECK(look::upscale_sharpen(p) == 0);
  p.render_scale = 0.75;  // drawn at the window's size: nothing upscaled
  CHECK(look::upscale_sharpen(p) == 0);
}

// r_scale draws at whole ratios only, the nearest of 1, 0.5 and 0.25 by ratio.
TEST(r_scale_draws_at_whole_ratios_only) {
  namespace look = ge::classic::look;
  look::Picture p;
  for (const auto& [asked, drawn] : {std::pair{1.0, 1.0},
                                     {0.75, 1.0},
                                     {0.7, 0.5},
                                     {0.5, 0.5},
                                     {0.36, 0.5},
                                     {0.35, 0.25},
                                     {0.25, 0.25}}) {
    p.render_scale = asked;
    CHECK(look::render_step(p) == drawn);
  }
}

TEST(quake_iis_picture_switches_reach_the_world_and_another_games_change_nothing) {
  // Another game's defaults: the sky drawn and smoothed, the textures as painted, the renderer's
  // own four taps, every speck its own size.
  lk::Picture p;
  ge::brush::app::Options o = lk::world_options(lk::Look::modern, {}, p);
  CHECK(!o.fast_sky && !o.crisp_sky && o.saturation == 1 && !o.invert && o.anisotropy == 4);
  CHECK(ge::classic::q2_speck_scale(p.q2fx, 1000) == 1);
  // gl_drawsky 0: a black sky; gl_bilerp_skies 0: a sharp one; gl_saturation, gl_invert and
  // gl_anisotropy (every game's) on the walls' and floors' textures, in either look.
  p.q2fx.draw_sky = false;
  p.q2fx.smooth_sky = false;
  p.q2fx.saturation = 0.25;
  p.q2fx.invert = true;
  p.anisotropy = 8;
  for (const lk::Look look : {lk::Look::classic, lk::Look::modern}) {
    o = lk::world_options(look, {}, p);
    CHECK(o.fast_sky && o.sky_colour[0] == 0 && o.sky_colour[1] == 0 && o.sky_colour[2] == 0);
    CHECK(o.crisp_sky && o.saturation == 0.25 && o.invert && o.anisotropy == 8);
  }
  // gl_partscale 2 (Quake II's default): as made within 20 units, 5 times at 1000; 4 twice that.
  p.q2fx.part_scale = 2;
  CHECK(ge::classic::q2_speck_scale(p.q2fx, 10) == 1);
  CHECK(std::abs(ge::classic::q2_speck_scale(p.q2fx, 1000) - 5) < 1e-12);
  p.q2fx.part_scale = 4;
  CHECK(std::abs(ge::classic::q2_speck_scale(p.q2fx, 1000) - 10) < 1e-12);
}

TEST(quake_iis_light_switches_split_the_walls_light_from_the_models) {
  // Every other game: the walls at light_scale, the models at 1, the renderer's own falloff and
  // stepped styles.
  lk::Picture p;
  CHECK(lk::world_light_scale(p) == 1 && lk::model_light(p, lk::Look::classic) == 1);
  ge::brush::app::Options o = lk::world_options(lk::Look::classic, {}, p);
  CHECK(o.dlight_falloff == 0 && !o.lerp_styles && o.lightmap_saturation == 1);
  // gl_modulate 2 with gl_modulate_world 0.5: the walls as before, the models twice as bright,
  // four times with gl_doublelight_entities; in the modern look over the world's own modulate.
  p.light_modulate = 2;
  p.q2fx.modulate_world = 0.5;
  CHECK(lk::world_light_scale(p) == 1 &&
        lk::world_options(lk::Look::classic, {}, p).classic_light == 1);
  CHECK(lk::model_light(p, lk::Look::classic) == 2);
  p.q2fx.double_light = true;
  CHECK(lk::model_light(p, lk::Look::classic) == 4);
  CHECK(lk::model_light(p, lk::Look::modern) == 4);  // over a world modulate of 2 x 0.5
  // gl_dlight_falloff 1 the soft edge, 0 the hard; gl_coloredlightmaps and cl_lerp_lightstyles.
  p.q2fx.dlight_falloff = 1;
  p.q2fx.lightmap_saturation = 0.5;
  p.q2fx.lerp_styles = true;
  o = lk::world_options(lk::Look::modern, {}, p);
  CHECK(o.dlight_falloff == 1 && o.lightmap_saturation == 0.5 && o.lerp_styles);
  p.q2fx.dlight_falloff = 0;
  CHECK(lk::world_options(lk::Look::classic, {}, p).dlight_falloff == 2);
}
