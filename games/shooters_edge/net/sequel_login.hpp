// LOGINS ON THE SEQUEL'S SERVER (docs/accounts.md § From a classic client): the engine's logins
// (engine/server/login.hpp) wired into the sequel's net server (sequel_net_server.hpp) for a stock
// client of the sequel. Its client forwards a word it does not know only once the player is in the
// game, so the seat enters as a guest (a spectator, where `sv_login` asks it to log in) and types
// `login <name> <code>`; while connecting it types `cmd login ...`. The server takes the line
// before the rules see it (`line_hook`), so a login is never chat. Each line the player reads is
// cut to the 38 characters its console shows, at a space where there is one. Logged in, the seat
// is shown by its account's name alone (the sequel has no login key on its scoreboard). A guest
// under a registered name plays as `player` (or `player2`, ...). A seat out of time is dropped.
//
//   sequel_login_open(l, server, settings, own, link, unix_us, error)
//   sequel_login_frame(l, now_s)   once a frame, after sequel_net_frame
//
// A server with `sv_ranked` offers its free players' seats to the matchmaker (logins_offer), the
// ladder's game its folder (`game`), signed by `logins.signer` when the master is another process.
//
// ge:summary Logins on the sequel's server: the line taken before the rules, 38-wide prompts
// ge:keywords sequel-login login sv_login login-prompt reserved-names sequel_login_frame sv_ranked
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_login_test.cpp
#pragma once

#include <string>
#include <vector>

#include "server/login.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace shooters_edge::net {

constexpr size_t kSequelPromptWidth = 38;  // the characters a line of its console shows
inline constexpr const char* kSequelDefaultName = "player";

struct SequelLogin {
  ge::server::Logins logins;
  SequelNetServer* server = nullptr;
  int64_t (*unix_us)() = nullptr;
  std::string game = "baseq2";  // the ladder's game: the sequel's folder
};

// How the sequel's names fold: lower case, look-alikes folded, at most 15 characters.
ge::account::NameRules sequel_name_rules();

// `text` cut into lines of at most `width` characters, at spaces where it can.
std::vector<std::string> sequel_wrap(const std::string& text, size_t width = kSequelPromptWidth);

// The logins over `own` (the server's own service) or a master at `set.master` asked over `link`,
// bound to `s` (its hooks set). False with `error`.
bool sequel_login_open(SequelLogin& l, SequelNetServer& s, const ge::server::LoginSettings& set,
                       ge::account::Service* own, ge::net::Transport* link, int64_t (*unix_us)(),
                       std::string& error);

// Once a frame after the server's: the seats synced, the asks run, what they ask done.
void sequel_login_frame(SequelLogin& l);

}  // namespace shooters_edge::net
