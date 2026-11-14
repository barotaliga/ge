// The engine's commands of a player's game CLI, one function each, defined by subject:
// settings.cpp (the player's config and the server's server.cfg), servers.cpp (the browser, one
// server's info and players, the remote console), launch.cpp (hosting, joining, the logs) and
// account.cpp (a player's account at a master's account service).
// gamecli.cpp's table names them; nothing else calls them.
//
// ge:summary The engine's game CLI commands, declared for the table in gamecli.cpp
// ge:keywords gamecli commands settings servers players rcon host join logs where account
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp tests/gamecli_settings_test.cpp
#pragma once

#include "gamecli/gamecli.hpp"

namespace ge::gamecli {

int cmd_settings_list(Ctx& ctx);
int cmd_settings_get(Ctx& ctx);
int cmd_settings_set(Ctx& ctx);
int cmd_settings_reset(Ctx& ctx);
int cmd_server_settings(Ctx& ctx);
int cmd_settings_check(Ctx& ctx);
int cmd_settings_format(Ctx& ctx);
int cmd_server_check(Ctx& ctx);
int cmd_server_format(Ctx& ctx);
int cmd_server_set(Ctx& ctx);

int cmd_servers(Ctx& ctx);
int cmd_server_info(Ctx& ctx);
int cmd_players(Ctx& ctx);
int cmd_rcon(Ctx& ctx);
int cmd_route(Ctx& ctx);

int cmd_host(Ctx& ctx);
int cmd_host_list(Ctx& ctx);
int cmd_host_stop(Ctx& ctx);
int cmd_join(Ctx& ctx);
int cmd_link_open(Ctx& ctx);
int cmd_link_register(Ctx& ctx);

int cmd_account_signup(Ctx& ctx);
int cmd_account_signin(Ctx& ctx);
int cmd_account_code(Ctx& ctx);
int cmd_account_recover(Ctx& ctx);
int cmd_account_show(Ctx& ctx);
int cmd_friends_list(Ctx& ctx);
int cmd_friends_add(Ctx& ctx);
int cmd_friends_remove(Ctx& ctx);
int cmd_friends_block(Ctx& ctx);
int cmd_friends_unblock(Ctx& ctx);
int cmd_friends_invite(Ctx& ctx);
int cmd_friends_invites(Ctx& ctx);
int cmd_friends_status(Ctx& ctx);

// The window's program, and the window started with `argv` (its output in `<logs>/<log_name>.log`;
// `--dry-run` shows the command only); `what` finishes "the game is starting ...".
std::string window_path(const Ctx& ctx);
int start_window(Ctx& ctx, const std::vector<std::string>& argv, const std::string& log_name,
                 const std::string& what, const std::string& address);
int cmd_logs(Ctx& ctx);
int cmd_where(Ctx& ctx);

}  // namespace ge::gamecli
