// The player's keys and mouse (controls.hpp).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls read_controls turn_view buttons default_binds key_moves
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/controls.hpp"

#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

namespace {

// The default layout: a key, the command it runs (+voice talks, console/talk.hpp binding the
// other chat and voice keys: net/classic_remote.hpp; screenshot: engine/capture/screenshot.hpp).
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

}  // namespace shooters_edge::client
