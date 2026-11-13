// Logins on the classic endpoint (net/classic_login.hpp).
//
// ge:summary Logins on the classic endpoint: seats synced, lines taken, names, *auth, timeouts
// ge:keywords classic-login login auth-key login-timeout
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_login_test.cpp
#include "shooters_edge/net/classic_login.hpp"

#include <cstdio>
#include <vector>

#include "net/token.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;

void os_fresh(void*, uint8_t* out, size_t size) { ge::net::os_random_bytes(out, size); }

int64_t unix_now(const ClassicLogin& l) { return l.unix_us != nullptr ? l.unix_us() : 0; }

bool seat_of(const ClassicClient& c) { return c.state == ConnState::spawned && !c.local; }

void name_rule(void* ctx, int slot, std::string& name, bool connecting) {
  ClassicLogin& l = *static_cast<ClassicLogin*>(ctx);
  ClassicServer& s = *l.server;
  const ClassicClient& c = s.clients[slot];
  if (c.local) return;
  // A seat's login is its player's: one who left and another who took the slot share nothing.
  if (l.logins.seats[slot].userid != c.userid) gs::login_leave(l.logins, slot);
  if (gs::login_name(l.logins, slot, name, connecting) != gs::NameVerdict::reserved) return;
  std::vector<std::string> taken;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (i != slot && s.clients[i].state != ConnState::free) taken.push_back(s.clients[i].name);
  name = unique_name(s.cfg.default_name, taken);
}

// The key the server's tickets are signed with, once known, as `*accountkey` in the server info:
// our own client logs itself in when it holds a ticket signed by the same key.
void advertise_key(ClassicLogin& l) {
  ge::net::Key key{};
  if (l.advertised || !gs::login_key(l.logins, key)) return;
  l.advertised = true;
  const std::string tag = gs::login_key_tag(key);
  ClassicServer& s = *l.server;
  bool set = false;
  for (auto& [k, v] : s.cfg.info)
    if (k == kAccountKeyInfo) {
      v = tag;
      set = true;
    }
  if (!set) s.cfg.info.emplace_back(kAccountKeyInfo, tag);
  if (!s.serverinfo.empty()) info_set(s.serverinfo, kAccountKeyInfo, tag, kServerInfoBytes);
}

}  // namespace

ge::account::NameRules classic_name_rules() { return {15, true, false}; }

bool classic_login_wanted(const gs::ServerConfig& c) {
  return c.login.login > 0 || c.accounts != nullptr || c.account_link != nullptr;
}

bool classic_login_open(ClassicLogin& l, ClassicServer& s, const gs::ServerConfig& c,
                        std::string& error) {
  l.server = &s;
  l.unix_us = c.unix_us;
  if (!gs::logins_open(l.logins, c.login, classic_name_rules(), c.accounts, c.account_link,
                       os_fresh, nullptr, unix_now(l), error))
    return false;
  s.name_rule = name_rule;
  s.name_ctx = &l;
  advertise_key(l);
  return true;
}

bool classic_login_line(ClassicLogin& l, int slot, const std::string& line) {
  const ClassicClient& c = l.server->clients[slot];
  if (!seat_of(c)) return false;
  if (l.logins.seats[slot].userid != c.userid)
    gs::login_seat(l.logins, slot, c.userid, c.spectator, ge::net::to_string(c.address),
                   l.server->game->sv.time);
  return gs::login_line(l.logins, slot, line, l.server->game->sv.time, unix_now(l));
}

void classic_login_frame(ClassicLogin& l) {
  ClassicServer& s = *l.server;
  const double now = s.game->sv.time;
  for (int i = 0; i < sh::kMaxClients && i < gs::kLoginSeats; i++) {
    const ClassicClient& c = s.clients[i];
    gs::LoginSeat& seat = l.logins.seats[i];
    if (!seat_of(c)) {
      if (c.state == ConnState::free && seat.state != gs::LoginState::empty)
        gs::login_leave(l.logins, i);
      continue;
    }
    if (seat.state == gs::LoginState::empty || seat.userid != c.userid)
      gs::login_seat(l.logins, i, c.userid, c.spectator, ge::net::to_string(c.address), now);
  }
  gs::logins_frame(l.logins, now, unix_now(l));
  advertise_key(l);
  for (const std::string& note : l.logins.notes) {
    s.log.push_back("login: " + note);
    std::printf("shooters_edge_server: %s\n", note.c_str());
  }
  l.logins.notes.clear();
  std::vector<gs::LoginOut> out;
  out.swap(l.logins.out);
  for (const gs::LoginOut& o : out) {
    if (o.slot < 0 || o.slot >= sh::kMaxClients || s.clients[o.slot].state == ConnState::free)
      continue;
    switch (o.kind) {
      case gs::LoginOut::Kind::tell:
        client_print(s, o.slot, 2, o.text + "\n");
        break;
      case gs::LoginOut::Kind::tell_all:
        classic_print_all(s, o.text + "\n");
        break;
      case gs::LoginOut::Kind::name: {
        std::string name = o.text;
        if (s.clients[o.slot].name != name) classic_set_info(s, o.slot, "name", name);
        break;
      }
      case gs::LoginOut::Kind::auth:
        classic_set_info(s, o.slot, "*auth", o.text);
        break;
      case gs::LoginOut::Kind::drop: {
        const std::string name = s.clients[o.slot].name;
        classic_drop(s, o.slot, o.text);
        classic_print_all(s, name + " didn't log in in time.\n");
        break;
      }
    }
  }
}

}  // namespace shooters_edge::net
