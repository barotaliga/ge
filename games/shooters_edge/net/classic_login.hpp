// LOGINS ON THE CLASSIC ENDPOINT: the engine's logins (engine/server/login.hpp) wired into
// Shooter's Edge's server for a stock classic client. Each frame the endpoint's spawned clients are
// the login's seats (a local seat, a bot or a listen server's own player, never is one); a client's
// line goes to the login before the lobby and the rules (`login ...`, `say login ...`, the code
// after a two-step login, and chat held from a seat that must log in), so a login line is never
// chat and never reaches the mod or a recording, and a `login` command counts toward the chat
// flood rule as chat does. What the login asks of the game is done here: a line printed to the
// player or to everyone, the seat's name set to its account's, the `*auth` key set in its user
// info (the community's client draws it on its scoreboard), the seat dropped when its time runs
// out. A seat's name goes through the login too, on connecting and on renaming (`name_rule`): a
// registered name is a guest's no more (the game's default name instead), and a logged-in seat
// keeps its account's.
//
// The accounts are the server's (gs::ServerConfig::accounts, account_link): the dedicated process
// hosts its own service or opens a socket to its master (engine/server/main.cpp); a lab hands in a
// service on the simulated network.
//
// ge:summary Logins on the classic endpoint: seats synced, lines taken, names, *auth, timeouts
// ge:keywords classic-login login sv_login auth-key star-auth login-prompt reserved-names
// ge:keywords forced-name login-timeout
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_login_test.cpp
#pragma once

#include <string>

#include "server/login.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

struct ClassicLogin {
  ge::server::Logins logins;
  int64_t (*unix_us)() = nullptr;
  ClassicServer* server = nullptr;
};

// How a classic game's names fold (high-bit coloured text), for its reserved names.
ge::account::NameRules classic_name_rules();

// True when the settings ask for logins: sv_login on, or a service handed in.
bool classic_login_wanted(const ge::server::ServerConfig& c);

// The logins over `c`'s accounts, bound to `s` (its `name_rule` set). False with `error`.
bool classic_login_open(ClassicLogin& l, ClassicServer& s, const ge::server::ServerConfig& c,
                        std::string& error);

// A client's line: true when the login took it (the caller drops it).
bool classic_login_line(ClassicLogin& l, int slot, const std::string& line);

// Once a frame after the endpoint's: the seats synced, the asks run, what they ask done.
void classic_login_frame(ClassicLogin& l);

}  // namespace shooters_edge::net
