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

namespace {

// The reports' half with a master elsewhere: its address and key, the server's own key, a socket.
bool open_reports(ServerAccounts& a, ServerConfig& c, const std::string& dir, std::string& said,
                  std::string& error) {
  if (c.login.stats == 0) return true;
  if (c.login.key.empty()) {
    said = "match reports are off: set sv_accountkey to the master's key to send them";
    return true;
  }
  if (!gn::key_from_hex(c.login.key, a.master_key, error)) {
    error = "sv_accountkey: " + error + "; set it to the 64 hex digits the master prints";
    return false;
  }
  if (!gn::parse_address(c.login.master, a.master) &&
      !gn::resolve_ipv4(c.login.master, account::kAccountPort, a.master, &error)) {
    error = "sv_accountmaster " + c.login.master + " is not an address (" + error +
            "); set it to the master's host[:port]";
    return false;
  }
  if (a.master.port == 0) a.master.port = account::kAccountPort;
  gn::Key seed{};
  bool made = false;
  if (!account::load_or_make_key(dir + "/server.key", seed, made, error)) return false;
  a.server = gn::sign_keys(seed);
  gn::wipe(seed.data(), seed.size());
  gn::Address any;
  gn::parse_address("0.0.0.0:0", any);
  if (!gn::open_udp_transport(a.reports, any, &error)) {
    error = "cannot open a socket to send match reports on: " + error;
    return false;
  }
  a.reporting = true;
  c.account_signer = &a.server;
  said = "match reports go to " + gn::to_string(a.master) + ", signed with this server's key " +
         gn::key_to_hex(a.server.pub) + " (the master counts them once its owner trusts it)";
  return true;
}

}  // namespace

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
    if (!open_reports(a, c, dir, said, error)) {
      server_accounts_close(a);
      return false;
    }
    return true;
  }
  if (c.login.login == 0) return true;  // no accounts here, so no reports either
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

void server_accounts_reports(ServerAccounts& a, const Server& s, int64_t unix_us) {
  if (s.matches_reported < a.reported) a.reported = 0;  // a new server: its count starts again
  const uint64_t fresh = s.matches_reported - a.reported;
  a.reported = s.matches_reported;
  const size_t n = fresh < s.reports.size() ? static_cast<size_t>(fresh) : s.reports.size();
  for (size_t i = s.reports.size() - n; i < s.reports.size(); i++) {
    const gn::MatchReport& r = s.reports[i];
    if (a.hosted && s.config.login.stats != 0) {
      const account::Taken t = account::service_report(a.host.svc, a.host.svc.keys.pub, r);
      a.taken++;
      a.counted += t == account::Taken::counted ? 1 : 0;
    } else if (a.reporting) {
      account::ReportAsk ask;
      if (account::report_ask_start(ask, a.master, a.master_key, a.server, r, unix_us))
        a.sending.push_back(std::move(ask));
      else
        a.lost++;
    }
  }
  if (!a.reporting) return;
  for (account::ReportAsk& ask : a.sending) account::report_ask_update(ask, a.reports, unix_us);
  gn::Address from;
  int got = 0;
  while ((got = gn::transport_receive(a.reports, a.buffer, sizeof a.buffer, from, unix_us)) >= 0)
    for (account::ReportAsk& ask : a.sending)
      if (account::report_ask_read(ask, a.buffer, static_cast<size_t>(got), from)) break;
  for (size_t i = 0; i < a.sending.size();) {
    const account::ReportAsk& ask = a.sending[i];
    if (!ask.done) {
      i++;
      continue;
    }
    if (ask.answered) {
      a.taken++;
      a.counted += ask.taken == account::Taken::counted ? 1 : 0;
    } else {
      a.lost++;
    }
    a.sending.erase(a.sending.begin() + static_cast<std::ptrdiff_t>(i));
  }
}

void server_accounts_close(ServerAccounts& a) {
  if (a.hosted) account::host_close(a.host);
  if (a.linked) gn::close_transport(a.link);
  if (a.reporting) gn::close_transport(a.reports);
  a.sending.clear();
  a.hosted = a.linked = a.reporting = false;
}

}  // namespace ge::server
