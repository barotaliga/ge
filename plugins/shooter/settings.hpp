// THE SHOOTER'S SERVER SETTINGS BY NAME: every setting of the move (`MoveSettings`) and of the loop
// (`ServerSettings`) a server owner may change, as one table a game turns into server cvars, so
// any first-person shooter on the engine offers the same names with the same ranges and help:
//
//   the loop     sv_mintic sv_maxtic sv_maxvelocity sv_speedcheck sv_minpitch sv_maxpitch
//                edgefriction sv_nostep
//   the move     sv_gravity sv_maxspeed sv_spectatormaxspeed sv_stopspeed sv_accelerate
//                sv_airaccelerate sv_wateraccelerate sv_friction sv_waterfriction
//                pm_ktjump pm_bunnyspeedcap pm_slidefix pm_airstep pm_pground pm_speedjump
//
// A value outside a setting's range is refused (`set_setting`), never clamped, so a typo in a
// server.cfg is an error naming the setting and the range rather than a silently different game.
//
// WHEN EACH IS READ: the move's settings (`Setting::live`) may change between frames: the game
// writes them into the running server (`apply_live_settings`). The loop's are read when a map
// starts (the frame's length, a dedicated server's clock, is fixed for the server's run).
//
// WHAT A CLIENT IS TOLD: a client predicts its own move with the server's numbers. The `sv_` move
// settings travel in the connection's own data; the `pm_` switches (`Setting::info`) only in the
// server info, so those that differ from the defaults are written there (`changed_info`):
// `pm_ktjump 0` on a server that turned the jump fix off. At the defaults nothing is added.
//
// ge:summary The shooter's server settings by name: the move's and the loop's, ranges, server info
// ge:keywords server-settings sv_mintic sv_maxtic sv_maxvelocity sv_speedcheck sv_minpitch
// ge:keywords sv_maxpitch edgefriction sv_nostep sv_gravity sv_maxspeed sv_spectatormaxspeed
// ge:keywords sv_stopspeed sv_accelerate sv_airaccelerate sv_wateraccelerate sv_friction
// ge:keywords sv_waterfriction pm_ktjump pm_bunnyspeedcap pm_slidefix pm_airstep pm_pground
// ge:keywords pm_speedjump
// ge:keywords set_setting setting_text apply_live_settings changed_info server-info cvar
// ge:skill shooter net
// ge:test tests/shooter_settings_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "shooter/server.hpp"

namespace ge::shooter {

struct Setting {
  const char* name;  // the cvar's name: "sv_mintic"
  const char* help;  // what it does, with its unit
  double low, high;  // the range a value must be in (a switch: 0..1)
  bool live;         // the move's: may change between frames; else read at map start
  bool flag;         // a switch: 0 or 1
  bool info;         // told to clients in the server info when not at its default
  double (*get)(const ServerSettings& set);
  void (*put)(ServerSettings& set, double value);
};

// Every setting, the loop's first, in the order a game registers them.
const Setting* settings_table(int& count);
// The setting called `name`, or null.
const Setting* find_setting(std::string_view name);

// The value of `s` in `set`, as a cvar's text ("0.013", "1").
std::string setting_text(const Setting& s, const ServerSettings& set);
// `name` set to `value` in `set`: false with `error` (an unknown name, not a number, outside the
// range, a switch not 0 or 1), naming the setting, the range and the next action.
bool set_setting(ServerSettings& set, std::string_view name, std::string_view value,
                 std::string& error);

// Each live setting copied from `from` onto `to` (the running server's), the loop's left alone.
void apply_live_settings(ServerSettings& to, const ServerSettings& from);

// The `info` settings whose value in `set` differs from the defaults, as server info pairs in
// the table's order: {"pm_ktjump", "0"}. Empty at the defaults.
std::vector<std::pair<std::string, std::string>> changed_info(const ServerSettings& set);

}  // namespace ge::shooter
