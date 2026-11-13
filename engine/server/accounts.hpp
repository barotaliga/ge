// THE ACCOUNTS A SERVER'S LOGINS ASK (login.hpp, docs/accounts.md § Logging in), opened for any
// host: the dedicated process (main.cpp) and a game's listen server alike. With sv_login on and no
// sv_accountmaster the server is its own master: its account service over `accounts.log` and
// `account.key` in `dir`, answering on sv_accountport. With sv_accountmaster set, a socket of its
// own to ask that master on. Either is handed to the server in its config (`accounts`,
// `account_link`, `unix_us`).
//
//   server_accounts_open(a, c, dir, unix_us, error)   every frame: server_accounts_update(a, now)
//   server_accounts_close(a)
//
// ge:summary The accounts a server's logins ask: its own service or a socket to its master
// ge:keywords server-accounts own-master account-link sv_accountport listen-server-login
// ge:skill net
// ge:test tests/server_login_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "account/host.hpp"
#include "net/transport.hpp"
#include "server/server.hpp"

namespace ge::server {

struct ServerAccounts {
  account::Host host;   // the server's own service (`hosted`)
  net::Transport link;  // the socket to a master elsewhere (`linked`)
  bool hosted = false, linked = false;
};

// Opens what `c.login` asks and sets `c.accounts` or `c.account_link`, and `c.unix_us`. Nothing
// when logins are off and no master is named. False with `error` (what failed and what to set).
// `said` takes a line for the owner when the server hosts its own accounts (its key).
bool server_accounts_open(ServerAccounts& a, ServerConfig& c, const std::string& dir,
                          int64_t (*unix_us)(), std::string& said, std::string& error);
void server_accounts_update(ServerAccounts& a, int64_t unix_us);
void server_accounts_close(ServerAccounts& a);

}  // namespace ge::server
