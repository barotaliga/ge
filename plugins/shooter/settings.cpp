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
    {"sv_mintic", "the shortest physics step, s (the server's frame)", 0.001, 0.1, false, false,
     false, GE_SETTING_FIELD(mintic, double)},
    {"sv_maxtic", "the longest physics step, s", 0.001, 0.1, false, false, false,
     GE_SETTING_FIELD(maxtic, double)},
    {"sv_maxvelocity", "the fastest anything moves on an axis, units/s", 0, 100000, false, false,
     false, GE_SETTING_FIELD(maxvelocity, float)},
    {"sv_speedcheck", "1: a player whose moves add up to more time than passed is held back", 0, 1,
     false, true, false, GE_SETTING_SWITCH(speedcheck)},
    {"sv_minpitch", "the lowest the view may look, degrees (negative is up)", -90, 0, false, false,
     false, GE_SETTING_FIELD(minpitch, float)},
    {"sv_maxpitch", "the furthest the view may look down, degrees", 0, 90, false, false, false,
     GE_SETTING_FIELD(maxpitch, float)},
    {"edgefriction", "the original move's friction factor at a ledge", 0, 100, false, false, false,
     GE_SETTING_FIELD(edgefriction, float)},
    {"sv_nostep", "1: the original move never steps up", 0, 1, false, true, false,
     GE_SETTING_SWITCH(nostep)},
    {"sv_gravity", "the world's gravity, units/s^2", -100000, 100000, true, false, false,
     GE_SETTING_FIELD(move.gravity, float)},
    {"sv_maxspeed", "a player's run speed, units/s", 0, 100000, true, false, false,
     GE_SETTING_FIELD(move.maxspeed, float)},
    {"sv_spectatormaxspeed", "a spectator's flying speed, units/s", 0, 100000, true, false, false,
     GE_SETTING_FIELD(move.spectatormaxspeed, float)},
    {"sv_stopspeed", "friction acts on at least this speed, units/s", 0, 100000, true, false, false,
     GE_SETTING_FIELD(move.stopspeed, float)},
    {"sv_accelerate", "the acceleration on the ground and in the air", 0, 1000, true, false, false,
     GE_SETTING_FIELD(move.accelerate, float)},
    {"sv_airaccelerate", "the air's acceleration as clients are told (the move uses sv_accelerate)",
     0, 1000, true, false, false, GE_SETTING_FIELD(move.airaccelerate, float)},
    {"sv_wateraccelerate", "the acceleration in water", 0, 1000, true, false, false,
     GE_SETTING_FIELD(move.wateraccelerate, float)},
    {"sv_friction", "the ground's friction", 0, 1000, true, false, false,
     GE_SETTING_FIELD(move.friction, float)},
    {"sv_waterfriction", "the water's friction", 0, 1000, true, false, false,
     GE_SETTING_FIELD(move.waterfriction, float)},
    {"pm_ktjump", "0..1: how far a weak jump (a rising player's) is pulled up to a full one", 0, 1,
     true, false, true, GE_SETTING_FIELD(move.ktjump, float)},
    {"pm_bunnyspeedcap", "the air's speed gain capped at sv_maxspeed times this (0 off)", 0, 100,
     true, false, true, GE_SETTING_FIELD(move.bunnyspeedcap, float)},
    {"pm_slidefix", "1: a player on a slope keeps its downward speed", 0, 1, true, true, true,
     GE_SETTING_SWITCH(move.slidefix)},
    {"pm_airstep", "1: steps are climbed in the air too", 0, 1, true, true, true,
     GE_SETTING_SWITCH(move.airstep)},
    {"pm_pground", "1: standing on the ground carries over between moves", 0, 1, true, true, true,
     GE_SETTING_SWITCH(move.pground)},
    {"pm_speedjump",
     "0..1: players with cl_speedjump 1 gain this share of the best strafe's speed holding forward "
     "in the air, and hop while jump is held (0 off)",
     0, 1, true, false, true, GE_SETTING_FIELD(move.speedjump, float)},
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
    if (s.info && s.get(set) != s.get(defaults)) out.emplace_back(s.name, setting_text(s, set));
  return out;
}

}  // namespace ge::shooter
