// THE ACCOUNTS A SERVER'S LOGINS ASK (login.hpp, docs/accounts.md § Logging in), opened for any
// host: the dedicated process (main.cpp) and a game's listen server alike. With sv_login on and no
// sv_accountmaster the server is its own master: its account service over `accounts.log` and
// `account.key` in `dir`, answering on sv_accountport. With sv_accountmaster set, a socket of its
// own to ask that master on. Either is handed to the server in its config (`accounts`,
// `account_link`, `unix_us`).
//
// THE MATCH REPORTS (sv_stats, docs/accounts.md § The match report): each match the server reports
// (Server::reports) goes to the same accounts. A server that is its own master takes them in
// process, counted as its own; one with a master elsewhere signs each with its own key
// (`server.key` in `dir`, made at the first start, its public half printed for the master's owner
// to trust) and sends it on a socket of its own until the master's signed answer comes. That needs
// the master's key (sv_accountkey): without it no report is sent, and the start says so.
//
//   server_accounts_open(a, c, dir, unix_us, error)   every frame: server_accounts_update(a, now)
//   server_accounts_reports(a, s, unix_us)   every frame after the server's: new reports sent
//   server_accounts_close(a)
//
// ge:summary The accounts a server's logins ask: its own service or a socket to its master
// ge:keywords server-accounts own-master account-link sv_accountport listen-server-login
// ge:skill net
// ge:test tests/server_login_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "account/host.hpp"
#include "net/transport.hpp"
#include "server/server.hpp"

namespace ge::server {

struct ServerAccounts {
  account::Host host;      // the server's own service (`hosted`)
  net::Transport link;     // the socket to a master elsewhere (`linked`)
  net::Transport reports;  // the reports' own socket to it (`reporting`)
  std::vector<account::ReportAsk> sending;
  net::SignKeys server;  // the key the server signs its reports with
  net::Key master_key{};
  net::Address master;
  uint64_t reported = 0;                      // the server's matches handed on so far
  uint64_t taken = 0, counted = 0, lost = 0;  // reports the master took, counted, never answered
  bool hosted = false, linked = false, reporting = false;
  uint8_t buffer[net::kMaxDatagram];
};

// Opens what `c.login` asks and sets `c.accounts` or `c.account_link`, and `c.unix_us`. Nothing
// when logins are off and no master is named. False with `error` (what failed and what to set).
// `said` takes a line for the owner when the server hosts its own accounts (its key).
bool server_accounts_open(ServerAccounts& a, ServerConfig& c, const std::string& dir,
                          int64_t (*unix_us)(), std::string& said, std::string& error);
void server_accounts_update(ServerAccounts& a, int64_t unix_us);
void server_accounts_reports(ServerAccounts& a, const Server& s, int64_t unix_us);
void server_accounts_close(ServerAccounts& a);

}  // namespace ge::server
