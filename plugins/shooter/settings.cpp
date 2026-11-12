// The shooter's server settings, defined: the table (each setting's name, range, help and field)
// and setting one from text.
//
// ge:summary The shooter's server settings table and its setter, defined
// ge:keywords server-settings settings_table set_setting changed_info
// ge:skill shooter net
// ge:test tests/shooter_settings_test.cpp
#include "shooter/settings.hpp"

#include <cstdlib>
#include <iterator>

#include "core/num_text.hpp"

namespace ge::shooter {

namespace {

// A field of the settings, read and written as a double.
#define GE_SETTING_FIELD(expr, type)                                   \
  [](const ServerSettings& s) { return static_cast<double>(s.expr); }, \
      [](ServerSettings& s, double v) { s.expr = static_cast<type>(v); }
#define GE_SETTING_SWITCH(expr)                               \
  [](const ServerSettings& s) { return s.expr ? 1.0 : 0.0; }, \
      [](ServerSettings& s, double v) { s.expr = v != 0; }

// The loop's first (read at map start), then the move's (live). The ranges keep a value a
// server can run: a step of 1 ms to a tenth of a second, a pitch inside straight up and down.
constexpr Setting kSettings[] = {
    {"sv_mintic", "The shortest physics step, which is the server's frame, in seconds.", 0.001, 0.1,
     false, false, false, "0.001..0.1: that many seconds (0.013 the default)",
     GE_SETTING_FIELD(mintic, double)},
    {"sv_maxtic", "The longest physics step, in seconds.", 0.001, 0.1, false, false, false,
     "0.001..0.1: that many seconds (0.1 the default)", GE_SETTING_FIELD(maxtic, double)},
    {"sv_maxvelocity", "The fastest anything may move along one axis, in units a second.", 0,
     100000, false, false, false, "0..100000: that speed (2000 the default)",
     GE_SETTING_FIELD(maxvelocity, float)},
    {"sv_speedcheck", "Whether a player whose moves add up to more time than passed is held back.",
     0, 1, false, true, false, "0: not checked; 1: held back (the default)",
     GE_SETTING_SWITCH(speedcheck)},
    {"sv_minpitch", "How far up the view may look, in degrees.", -90, 0, false, false, false,
     "-90..0: that many degrees, negative is up (-70 the default)",
     GE_SETTING_FIELD(minpitch, float)},
    {"sv_maxpitch", "How far down the view may look, in degrees.", 0, 90, false, false, false,
     "0..90: that many degrees (80 the default)", GE_SETTING_FIELD(maxpitch, float)},
    {"edgefriction", "How much harder the original move brakes where the ground ahead falls away.",
     0, 100, false, false, false,
     "1: no more than elsewhere; 0..100: that many times (2 the default)",
     GE_SETTING_FIELD(edgefriction, float)},
    {"sv_nostep", "Whether the original move never steps up onto a ledge.", 0, 1, false, true,
     false, "0: steps are climbed (the default); 1: never", GE_SETTING_SWITCH(nostep)},
    {"sv_gravity", "The world's gravity, in units a second squared.", -100000, 100000, true, false,
     false, "-100000..100000: that pull, negative is up (800 the default)",
     GE_SETTING_FIELD(move.gravity, float)},
    {"sv_maxspeed", "A player's running speed, in units a second.", 0, 100000, true, false, false,
     "0..100000: that speed (320 the default)", GE_SETTING_FIELD(move.maxspeed, float)},
    {"sv_spectatormaxspeed", "A spectator's flying speed, in units a second.", 0, 100000, true,
     false, false, "0..100000: that speed (500 the default)",
     GE_SETTING_FIELD(move.spectatormaxspeed, float)},
    {"sv_stopspeed", "The least speed friction brakes from, in units a second.", 0, 100000, true,
     false, false, "0..100000: that speed (100 the default)",
     GE_SETTING_FIELD(move.stopspeed, float)},
    {"sv_accelerate", "How fast a player gains speed on the ground and in the air.", 0, 1000, true,
     false, false, "0..1000: that rate (10 the default)", GE_SETTING_FIELD(move.accelerate, float)},
    {"sv_airaccelerate",
     "The air's acceleration as clients are told it, while the move itself uses sv_accelerate.", 0,
     1000, true, false, false, "0..1000: that rate (10 the default)",
     GE_SETTING_FIELD(move.airaccelerate, float)},
    {"sv_wateraccelerate", "How fast a player gains speed in water.", 0, 1000, true, false, false,
     "0..1000: that rate (10 the default)", GE_SETTING_FIELD(move.wateraccelerate, float)},
    {"sv_friction", "How hard the ground brakes a player.", 0, 1000, true, false, false,
     "0: no friction; 0..1000: that much (4 the default)", GE_SETTING_FIELD(move.friction, float)},
    {"sv_waterfriction", "How hard water brakes a player.", 0, 1000, true, false, false,
     "0: no friction; 0..1000: that much (4 the default)",
     GE_SETTING_FIELD(move.waterfriction, float)},
    {"pm_ktjump", "How far a weak jump, made while rising, is pulled up to a full one.", 0, 1, true,
     false, true, "0: not at all; 0..1: that share of the way; 1: all the way (the default)",
     GE_SETTING_FIELD(move.ktjump, float)},
    {"pm_bunnyspeedcap", "A cap on the speed gained in the air, as a multiple of sv_maxspeed.", 0,
     100, true, false, true, "0: no cap (the default); 0..100: that many times sv_maxspeed",
     GE_SETTING_FIELD(move.bunnyspeedcap, float)},
    {"pm_slidefix", "Whether a player landing on a slope keeps the speed down it.", 0, 1, true,
     true, true, "0: the speed is lost (the default); 1: kept", GE_SETTING_SWITCH(move.slidefix)},
    {"pm_airstep", "Whether steps are climbed in the air as on the ground.", 0, 1, true, true, true,
     "0: only on the ground (the default); 1: in the air too", GE_SETTING_SWITCH(move.airstep)},
    {"pm_pground", "Whether standing on the ground carries over from one move to the next.", 0, 1,
     true, true, true, "0: found again each move (the default); 1: carried over",
     GE_SETTING_SWITCH(move.pground)},
    {"pm_speedjump",
     "How much of the best strafe's gain the easy jump gives a player who turns it on with "
     "cl_speedjump 1.",
     0, 1, true, false, true,
     "0: the easy jump is off (the default); 0..1: that share of the best gain while forward and "
     "jump are held",
     GE_SETTING_FIELD(move.speedjump, float), true},
    {"sv_aimhelp_x",
     "How far sideways the aim help bends a granted player's missed shot onto a target, in units "
     "at the target.",
     0, 256, true, false, true, "0: no sideways help; 0..256: that far (24 the default)",
     GE_SETTING_FIELD(aimhelp_x, float)},
    {"sv_aimhelp_y",
     "How far up or down the aim help bends a granted player's missed shot onto a target, in "
     "units at the target.",
     0, 256, true, false, true, "0: no vertical help; 0..256: that far (48 the default)",
     GE_SETTING_FIELD(aimhelp_y, float)},
};

#undef GE_SETTING_FIELD
#undef GE_SETTING_SWITCH

}  // namespace

const Setting* settings_table(int& count) {
  count = static_cast<int>(std::size(kSettings));
  return kSettings;
}

const Setting* find_setting(std::string_view name) {
  for (const Setting& s : kSettings)
    if (name == s.name) return &s;
  return nullptr;
}

std::string setting_text(const Setting& s, const ServerSettings& set) {
  return core::num::to_string(s.get(set));
}

bool set_setting(ServerSettings& set, std::string_view name, std::string_view value,
                 std::string& error) {
  const Setting* s = find_setting(name);
  if (s == nullptr) {
    error = std::string(name) + " is not a shooter server setting; next: see `cvarlist sv_`";
    return false;
  }
  const std::string text(value);
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  const std::string range = core::num::to_string(s->low) + ".." + core::num::to_string(s->high);
  if (text.empty() || end != text.c_str() + text.size()) {
    error = std::string(s->name) + " \"" + text + "\" is not a number; next: set it in " + range;
    return false;
  }
  if (s->flag ? (v != 0 && v != 1) : !(v >= s->low && v <= s->high)) {
    error = std::string(s->name) + " " + text + " is outside " + (s->flag ? "0 or 1" : range) +
            "; next: set it in range (" + s->help + ")";
    return false;
  }
  s->put(set, v);
  return true;
}

void apply_live_settings(ServerSettings& to, const ServerSettings& from) {
  for (const Setting& s : kSettings)
    if (s.live) s.put(to, s.get(from));
}

std::vector<std::pair<std::string, std::string>> changed_info(const ServerSettings& set) {
  const ServerSettings defaults;
  std::vector<std::pair<std::string, std::string>> out;
  for (const Setting& s : kSettings)
    if (s.info && (s.always_info || s.get(set) != s.get(defaults)))
      out.emplace_back(s.name, setting_text(s, set));
  return out;
}

}  // namespace ge::shooter
