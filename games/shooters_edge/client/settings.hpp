// THE PLAYER'S SETTINGS IN THE WINDOW: where they are kept, the user info they send, and the keys
// the console's binds and the menu take.
//
// - **Kept**: `<game folder>/settings/config.cfg` (`settings_dir`) is the console's first search
//   directory, so `config.cfg` is read from it at start and every archived cvar and bind is
//   written back to it (`save_settings`) when the menu closes and when the game quits, with the
//   catalog of every setting beside it (`cvars.txt`, console/catalog.hpp), which the player's CLI
//   (`shooters_edge_cli settings`) describes and checks them by.
// - **The user info**: `topcolor` and `bottomcolor` (`add_user_info_cvars`, archived) and the
//   console's `name` go to the listen server's slot 0 whenever one changes or the server is a new
//   one (`send_user_info`); a lobby's team game keeps the team's colours over the player's own.
//   `cl_speedjump` (archived, 0 by default) asks the server for the easy jump (shooter/pmove.hpp
//   `speedjump`): sent the same way, and again whenever the server's slot lost it (a rejoin).
// - **The keys**: the mouse's buttons and wheel go to the game's binds (`mouse_to_binds`, the
//   keyboard's go with the console's keys); while CUSTOMIZE CONTROLS waits for a key, the first
//   key, button or wheel turn down this frame is handed to the menu (`keys_to_menu`).
//
// ge:summary The window's settings kept in config.cfg, the user info sent, the mouse on binds
// ge:keywords settings config-cfg save_settings settings_dir user-info topcolor bottomcolor name
// ge:keywords cl_speedjump easy-jump
// ge:keywords mouse_to_binds keys_to_menu mouse1 mwheelup customize-controls
// ge:skill console input
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "console/console.hpp"
#include "input/state.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/menu/game_menu.hpp"

namespace shooters_edge::client {

// `<basedir>/<game>/settings`.
std::filesystem::path settings_dir(const std::string& basedir, const std::string& game);
// Every archived cvar and bind written to the first search directory's config.cfg; a line saying
// what failed when it cannot be written.
bool save_settings(ge::console::Console& con);

struct UserInfo {
  int top = 0, bottom = 0;                                      // topcolor, bottomcolor
  bool speedjump = false;                                       // cl_speedjump
  const shooters_edge::server::RulesCommon* sent_to = nullptr;  // null: send again
  std::string name;
  int sent_top = -1, sent_bottom = -1;
};
// `topcolor`, `bottomcolor` and `cl_speedjump` over `info` (it must outlive `con`).
void add_user_info_cvars(ge::console::Console& con, UserInfo& info);
// The name and colours sent to the server playing when they changed or it is a new server.
void send_user_info(Listen& l, const ge::console::Console& con, UserInfo& info);

// This frame's mouse buttons and wheel through the game's binds.
void mouse_to_binds(ge::console::Console& con, const ge::input::InputState& in);
// The first key, button or wheel turn down this frame to the menu waiting for one.
void keys_to_menu(shooters_edge::menu::GameMenu& menu, shooters_edge::menu::Host& host,
                  const ge::input::Queue& queue, const ge::input::InputState& in);

}  // namespace shooters_edge::client
