// The accounts a server's logins ask (server/accounts.hpp).
//
// ge:summary The accounts a server's logins ask: its own service or a socket to its master
// ge:keywords server-accounts own-master account-link
// ge:skill net
// ge:test tests/server_login_test.cpp
#include "server/accounts.hpp"

#include "net/crypto.hpp"
#include "net/udp_socket.hpp"

namespace ge::server {

namespace gn = ge::net;

bool server_accounts_open(ServerAccounts& a, ServerConfig& c, const std::string& dir,
                          int64_t (*unix_us)(), std::string& said, std::string& error) {
  c.unix_us = unix_us;
  if (!c.login.master.empty()) {
    gn::Address any;
    gn::parse_address("0.0.0.0:0", any);
    if (!gn::open_udp_transport(a.link, any, &error)) {
      error = "cannot open a socket to ask the account master on: " + error;
      return false;
    }
    a.linked = true;
    c.account_link = &a.link;
    return true;
  }
  if (c.login.login == 0) return true;
  const int port = c.login.port > 0 ? c.login.port : account::kAccountPort;
  gn::Address at;
  if (!gn::parse_address((c.ip.empty() ? "0.0.0.0" : c.ip) + ":" + std::to_string(port), at) ||
      !account::host_open(a.host, dir, at, account::HashCost{}, unix_us(), error)) {
    error = "sv_login is on and this server is its own account master, but " + error +
            ". Set sv_accountport to a free port, or sv_accountmaster to a master's address";
    return false;
  }
  a.hosted = true;
  said = "its own account master: " + std::to_string(a.host.svc.store.accounts.size()) +
         " accounts in " + dir + ", service on " + gn::to_string(at) + ", key " +
         gn::key_to_hex(account::service_key(a.host.svc));
  if (!a.host.warning.empty()) said += " (" + a.host.warning + ")";
  c.accounts = &a.host.svc;
  return true;
}

void server_accounts_update(ServerAccounts& a, int64_t unix_us) {
  if (a.hosted) account::host_update(a.host, unix_us);
}

void server_accounts_close(ServerAccounts& a) {
  if (a.hosted) account::host_close(a.host);
  if (a.linked) gn::close_transport(a.link);
  a.hosted = a.linked = false;
}

}  // namespace ge::server
