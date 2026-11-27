// CLASSIC REMIX ON A DEDICATED SERVER (`sesv`, M18u13c): the same cr_ settings the window's own
// match plays (client/remix.hpp), set as the server's rules (server.cfg, `+set cr_items q2`,
// `--cr_items=q2`), and the settings that say which game is served and where each game is
// installed, as the window has them (`fs_basegame`, `fs_basedir_<id>`: installs.hpp). The served
// game's install is the server's `--root` (or its `fs_basedir_<id>` on the command line); the
// other game's `fs_basedir_<id>` is the remix's.
//
// - **The rules** (`remix_rules`): every cr_ setting a server takes, with the window's words and
//   defaults (`bind_remix`), but `cr_menu`, which is the player's own; then `fs_basegame` and
//   each `fs_basedir_<id>`.
// - **The settings** (`remix_of_rules`): the rules as set, read into the window's `RemixSettings`.
// - **Opened** (`open_server_remix`): the other game, the first whose `fs_basedir_<id>` is set,
//   checked as the window checks two installs (both full games), its files opened under the
//   played game's and the parts worn (`remix_dress`): the same `RemixFiles` the window's
//   match plays from. The server then reads a map through it (`load_remixed_map`), sets the worn
//   lines on its rules (`server_remix_lines`) and tells every client in its server info
//   (`remix_info_lines`), so our client joining reads the remix back (`remix_told`).
//
// ge:summary Classic remix on a dedicated server: the cr_ rules, the games' installs, the remix
// ge:keywords remix dedicated-server sesv fs_basedir fs_basegame remix_rules remix_of_rules
// ge:keywords open_server_remix server_remix_lines other-install
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_dedicated_test.cpp
#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "server/server.hpp"
#include "shooters_edge/client/remix.hpp"

namespace shooters_edge::client {

// Every cr_ setting a dedicated server takes as its rules, then `fs_basegame` and the installs.
std::span<const ge::server::GameRule> remix_rules();

// Whether `rule` is one of `remix_rules`' (the server reads it, the mod never sees it).
bool is_remix_rule(std::string_view rule);

// The remix the server's rules as set ask for (an unset rule at the window's default).
RemixSettings remix_of_rules(const std::map<std::string, std::string>& rules);

// The remix the server's rules ask for, opened into `r` for `played` (a content profile's id) at
// the server's install `root`, its game folder `game`; `mod_rules`: a mod's own program plays.
// The console's line about it, "" when cr_enabled is off. Off when the rules or the installs do
// not allow it, the line saying why in the game's words.
std::string open_server_remix(RemixFiles& r, const std::map<std::string, std::string>& rules,
                              std::string_view played, const std::string& root,
                              const std::string& game, bool mod_rules);

// The lines a server's rules take each map while `r` is on, as the window's match takes them
// (listen.cpp): the worn arsenal and physics (left out while a mod's own rules play, but for
// `cr_over_mods`' physics), then the server info that tells the remix to every client.
std::vector<std::string> server_remix_lines(const RemixFiles& r);

}  // namespace shooters_edge::client
