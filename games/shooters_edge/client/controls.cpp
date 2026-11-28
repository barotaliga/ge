// The player's keys and mouse (controls.hpp).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls read_controls turn_view buttons default_binds key_moves
// ge:keywords bind_defaults reset_binds reset-keys
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/controls.hpp"

#include <string>
#include <utility>
#include <vector>

#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

namespace {

// The default layout: a key, the command it runs (+voice talks, console/talk.hpp binding the
// other chat and voice keys: net/qw_remote.hpp; screenshot: engine/capture/screenshot.hpp).
constexpr const char* kDefaults[][2] = {
    {"w", "+forward"},
    {"s", "+back"},
    {"a", "+moveleft"},
    {"d", "+moveright"},
    {"up", "+forward"},
    {"down", "+back"},
    {"left", "+left"},
    {"right", "+right"},
    {",", "+moveleft"},
    {".", "+moveright"},
    {"space", "+jump"},
    {"mouse1", "+attack"},
    {"ctrl", "+attack"},
    {"mouse2", "+jump"},
    {"alt", "+strafe"},
    {"1", "impulse 1"},
    {"2", "impulse 2"},
    {"3", "impulse 3"},
    {"4", "impulse 4"},
    {"5", "impulse 5"},
    {"6", "impulse 6"},
    {"7", "impulse 7"},
    {"8", "impulse 8"},
    {"mwheelup", "impulse 10"},
    {"mwheeldown", "impulse 10"},
    {"v", "+voice"},
    {"f12", "screenshot"},
    {"tab", "+showscores"},
};

}  // namespace

void add_cvars(ge::console::Console& con, ControlSettings& c) {
  {
    ge::console::CvarLayer shooter(con.cvars(), kLayerShooter);
    ge::shooter::add_control_cvars(con, c);
  }
  ge::console::CvarLayer shell(con.cvars(), kLayerShell);
  add_quick_cvars(con, c.quick);
  add_group_cvars(con, c.groups);
  add_choice_cvars(con, c.choice);
  con.add_command("bind_defaults",
                  "bind_defaults: every key back to the game's own layout, your binds dropped",
                  [](ge::console::Console& cc, const ge::console::Args&) {
                    reset_binds(cc);
                    cc.print("Every key is back to the game's own layout.");
                  });
  con.add_command(
      "impulse",
      "impulse <n> [n ...]: send the mod impulse n (1..8 a weapon, 10 the next); with "
      "several, the first weapon of them you can fire",
      [&c](ge::console::Console& k, const ge::console::Args& a) {
        double n = 0;
        if (a.size() > 2)
          choose_list(c.choice, a);
        else if (a.size() < 2 || !ge::console::parse_number(a[1], n) || n < 1 || n > 255)
          k.print("impulse: impulse <1..255> (1..8 a weapon, 10 the next one)");
        else
          c.buttons.impulse = static_cast<int>(n);
      });
  c.showscores = &con.add_button("showscores", "+showscores: show the scoreboard while held");
  c.showteamscores =
      &con.add_button("showteamscores", "+showteamscores: show the teams' scoreboard while held");
}

void weapon_moves(ControlSettings& c, std::span<const std::string_view> game_groups,
                  const hud::Status& s, double t, Moves& m) {
  c.groups.game = game_groups;
  choice_moves(c.choice, s, m);
  group_moves(c.groups, s, t, m);
  quick_moves(c.quick, s, t, m);
}

bool scores_held(const ControlSettings& c) {
  return c.showscores != nullptr && c.showscores->held != 0;
}

bool team_scores_held(const ControlSettings& c) {
  return c.showteamscores != nullptr && c.showteamscores->held != 0;
}

void default_binds(ge::console::Console& con) {
  for (const auto& [key, command] : kDefaults)
    if (con.binds().get(ge::console::BindMode::game, key).empty())
      con.binds().set(ge::console::BindMode::game, key, command);
}

void reset_binds(ge::console::Console& con) {
  namespace gc = ge::console;
  // The game's own keys, as its defaults left them (console/binds.hpp § THE DEFAULTS).
  if (con.binds().restore_defaults()) return;
  // The console's own keys, in every mode, survive: without them nobody could type this again.
  std::vector<std::pair<std::pair<gc::BindMode, std::string>, std::string>> kept;
  for (const gc::BindMode m : {gc::BindMode::game, gc::BindMode::console})
    for (const char* key : {"`", "nonus"})
      if (const std::string& b = con.binds().get(m, key); !b.empty()) kept.push_back({{m, key}, b});
  con.binds().clear();
  for (const auto& [at, command] : kept) con.binds().set(at.first, at.second, command);
  default_binds(con);
}

}  // namespace shooters_edge::client
