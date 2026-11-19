// THE PLAYER'S SETTINGS IN THE WINDOW: where they are kept, the user info they send, and the keys
// the console's binds and the menu take.
//
// - **Kept**: ONE CONFIG FOR THE SERIES, `<data folder>/settings/config.cfg`
//   (`series_settings_file`): the binds and every setting in our names, which every classic game
//   reads, a game's own spellings being synonyms of them (client/cvar_layers.hpp). Beside each
//   game, `<game folder>/settings/config.cfg` (`settings_dir`, the console's first search
//   directory) keeps only that game's own meanings of shared names (Quake II's `crosshair`).
//   Both are written (`save_settings`) when the menu closes, before another game is loaded and
//   when the game quits, with the catalog of every setting beside the game's (`cvars.txt`,
//   console/catalog.hpp), which the player's CLI (`secli settings`) describes and checks them by.
//   A game folder's config from before the series config held everything: it is read as the
//   game's own file and written back split.
// - **The user info**: `topcolor` and `bottomcolor` (`add_user_info_cvars`, archived) and the
//   console's `name` go to the listen server's slot 0 whenever one changes or the server is a new
//   one (`send_user_info`); a lobby's team game keeps the team's colours over the player's own.
//   `cl_speedjump` (archived, 0 by default) turns on the easy jump (shooter/speedjump_assist.hpp)
//   and says so: sent the same way, and again whenever the server's slot lost it (a rejoin).
//   `cl_speedjump_view` (archived, 0..1, 0.5) is how much of its swing the screen shows; not sent.
// - **The keys**: the mouse's buttons and wheel go to the game's binds (`mouse_to_binds`, the
//   keyboard's go with the console's keys); while CUSTOMIZE CONTROLS waits for a key, the first
//   key, button or wheel turn down this frame is handed to the menu (`keys_to_menu`).
//
// ge:summary The window's settings kept in config.cfg, the user info sent, the mouse on binds
// ge:keywords settings config-cfg save_settings settings_dir user-info topcolor bottomcolor name
// ge:keywords skin model player-model series-config one-config series_settings_file
// ge:keywords cl_speedjump cl_speedjump_view easy-jump
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
// The series' one config: `<data folder>/settings/config.cfg`, every classic game's; empty with no
// data folder (each game's file then holds everything).
std::filesystem::path series_settings_file(const std::string& data_root);
// The settings written; a line saying what failed when they cannot be. With `series`: the binds,
// aliases and every setting but the loaded game's own there (in our names, the value each has
// outside the game: console/cfg.hpp § SAVING ONE PART), and the game's own settings alone in the
// first search directory's config.cfg. Without it, or with a session's own config, all of them
// in that one file.
bool save_settings(ge::console::Console& con, const std::filesystem::path& series = {});

struct UserInfo {
  int top = 0, bottom = 0;                                      // topcolor, bottomcolor
  bool speedjump = false;                                       // cl_speedjump
  float speedjump_view = 0.5f;                                  // cl_speedjump_view
  const shooters_edge::server::RulesCommon* sent_to = nullptr;  // null: send again
  std::string name;
  int sent_top = -1, sent_bottom = -1;
  std::string skin, sent_skin;  // skin: the picture others draw you in
  // model: the figure others draw you as (players/<model>/ in their files; "": the game's own)
  std::string model, sent_model;
};
// `topcolor`, `bottomcolor`, `skin`, `model` and `cl_speedjump` over `info` (it must outlive
// `con`).
void add_user_info_cvars(ge::console::Console& con, UserInfo& info);
// The name and colours sent to the server playing when they changed or it is a new server.
void send_user_info(Listen& l, const ge::console::Console& con, UserInfo& info);

// This frame's mouse buttons and wheel through the game's binds.
void mouse_to_binds(ge::console::Console& con, const ge::input::InputState& in);
// The first key, button or wheel turn down this frame to the menu waiting for one.
void keys_to_menu(shooters_edge::menu::GameMenu& menu, shooters_edge::menu::Host& host,
                  const ge::input::Queue& queue, const ge::input::InputState& in);

}  // namespace shooters_edge::client
