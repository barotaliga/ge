// LOGINS ON QUAKE II'S SERVER (docs/accounts.md § From a classic client): the engine's logins
// (engine/server/login.hpp) wired into Quake II's net server (q2_net_server.hpp) for a stock
// client of Quake II. Its client forwards a word it does not know only once the player is in the
// game, so the seat enters as a guest (a spectator, where `sv_login` asks it to log in) and types
// `login <name> <code>`; while connecting it types `cmd login ...`. The server takes the line
// before the rules see it (`line_hook`), so a login is never chat. Each line the player reads is
// cut to the 38 characters its console shows, at a space where there is one. Logged in, the seat
// is shown by its account's name alone (Quake II has no login key on its scoreboard). A guest
// under a registered name plays as `player` (or `player2`, ...). A seat out of time is dropped.
//
//   q2_login_open(l, server, settings, own, link, unix_us, error)
//   q2_login_frame(l, now_s)   once a frame, after q2_net_frame
//
// A server with `sv_ranked` offers its free players' seats to the matchmaker (logins_offer), the
// ladder's game its folder (`game`), signed by `logins.signer` when the master is another process.
//
// ge:summary Logins on Quake II's server: the line taken before the rules, 38-wide prompts
// ge:keywords q2-login login sv_login login-prompt reserved-names q2_login_frame sv_ranked
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_login_test.cpp
#pragma once

#include <string>
#include <vector>

#include "server/login.hpp"
#include "shooters_edge/net/q2_net_server.hpp"

namespace shooters_edge::net {

constexpr size_t kQ2PromptWidth = 38;  // the characters a line of its console shows
inline constexpr const char* kQ2DefaultName = "player";

struct Q2Login {
  ge::server::Logins logins;
  Q2NetServer* server = nullptr;
  int64_t (*unix_us)() = nullptr;
  std::string game = "baseq2";  // the ladder's game: Quake II's folder
};

// How Quake II's names fold: lower case, look-alikes folded, at most 15 characters.
ge::account::NameRules q2_name_rules();

// `text` cut into lines of at most `width` characters, at spaces where it can.
std::vector<std::string> q2_wrap(const std::string& text, size_t width = kQ2PromptWidth);

// The logins over `own` (the server's own service) or a master at `set.master` asked over `link`,
// bound to `s` (its hooks set). False with `error`.
bool q2_login_open(Q2Login& l, Q2NetServer& s, const ge::server::LoginSettings& set,
                   ge::account::Service* own, ge::net::Transport* link, int64_t (*unix_us)(),
                   std::string& error);

// Once a frame after the server's: the seats synced, the asks run, what they ask done.
void q2_login_frame(Q2Login& l);

}  // namespace shooters_edge::net
