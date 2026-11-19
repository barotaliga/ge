// plugins/classic/app/look: the picture's cvars take the game's Picture as their defaults (a game
// that sets a classic look and other strengths keeps them, and its values text never names
// another default), the shared defaults are the modern look's, and the teamcolor words parse.
//
// ge:summary Tests a shooter's picture cvars: a game's own defaults, the shared ones, colours
// ge:keywords test look picture-settings game-defaults r_look r_fx_lights values_for_default
// ge:skill shooter testing
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

TEST(the_view_breathes_only_in_a_liquid_and_only_with_r_waterwarp) {
  lk::Picture p;
  const double fov = 1.2;
  CHECK(lk::warped_fov(p, false, fov, 0.7) == fov);
  CHECK(lk::warped_fov(p, true, fov, 0) == fov);  // from rest
  CHECK(lk::warped_fov(p, true, fov, 0.5) != fov);
  p.water_warp = false;
  CHECK(lk::warped_fov(p, true, fov, 0.5) == fov);
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
}
