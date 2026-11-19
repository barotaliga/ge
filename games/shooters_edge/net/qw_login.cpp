// Logins on the QuakeWorld endpoint (net/qw_login.hpp).
//
// ge:summary Logins on the QuakeWorld endpoint: seats synced, lines taken, names, *auth, timeouts
// ge:keywords qw-login login auth-key login-timeout
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_login_test.cpp
#include "shooters_edge/net/qw_login.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "net/token.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_game.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;

void os_fresh(void*, uint8_t* out, size_t size) { ge::net::os_random_bytes(out, size); }

int64_t unix_now(const QwLogin& l) { return l.unix_us != nullptr ? l.unix_us() : 0; }

bool seat_of(const QwClient& c) { return c.state == ConnState::spawned && !c.local; }

void name_rule(void* ctx, int slot, std::string& name, bool connecting) {
  QwLogin& l = *static_cast<QwLogin*>(ctx);
  QwServer& s = *l.server;
  const QwClient& c = s.clients[slot];
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
void advertise_key(QwLogin& l) {
  ge::net::Key key{};
  if (l.advertised || !gs::login_key(l.logins, key)) return;
  l.advertised = true;
  const std::string tag = gs::login_key_tag(key);
  QwServer& s = *l.server;
  bool set = false;
  for (auto& [k, v] : s.cfg.info)
    if (k == kAccountKeyInfo) {
      v = tag;
      set = true;
    }
  if (!set) s.cfg.info.emplace_back(kAccountKeyInfo, tag);
  if (!s.serverinfo.empty()) info_set(s.serverinfo, kAccountKeyInfo, tag, kServerInfoBytes);
}

// sv_ranked: the free player seats offered to the matchmaker, at the logins' own pace.
void ranked_offer(QwLogin& l) {
  QwServer& s = *l.server;
  if (l.logins.set.ranked.empty() || s.transport == nullptr) return;
  const int maxclients = s.game->sv.set.maxclients;
  const int most =
      maxclients - std::max(0, s.cfg.max_spectators) - std::max(0, s.cfg.max_vip_spectators);
  int taken = 0;  // players and bots in the players' seats
  for (int i = 0; i < maxclients && i < sh::kMaxClients; i++)
    if (s.clients[i].state != ConnState::free ? !s.clients[i].spectator : s.game->clients[i].active)
      taken++;
  gs::logins_offer(l.logins, kGameFolder, ge::net::to_string(s.transport->local),
                   s.transport->local.port, most - taken, unix_now(l));
}

}  // namespace

ge::account::NameRules qw_name_rules() { return {15, true, false}; }

bool qw_login_wanted(const gs::ServerConfig& c) {
  return c.login.login > 0 || c.accounts != nullptr || c.account_link != nullptr;
}

bool qw_login_open(QwLogin& l, QwServer& s, const gs::ServerConfig& c, std::string& error) {
  l.server = &s;
  l.unix_us = c.unix_us;
  if (!gs::logins_open(l.logins, c.login, qw_name_rules(), c.accounts, c.account_link, os_fresh,
                       nullptr, unix_now(l), error))
    return false;
  l.logins.signer = c.account_signer;
  s.name_rule = name_rule;
  s.name_ctx = &l;
  advertise_key(l);
  return true;
}

bool qw_login_line(QwLogin& l, int slot, const std::string& line) {
  const QwClient& c = l.server->clients[slot];
  if (!seat_of(c)) return false;
  if (l.logins.seats[slot].userid != c.userid)
    gs::login_seat(l.logins, slot, c.userid, c.spectator, ge::net::to_string(c.address),
                   l.server->game->sv.time);
  return gs::login_line(l.logins, slot, line, l.server->game->sv.time, unix_now(l));
}

void qw_login_frame(QwLogin& l) {
  QwServer& s = *l.server;
  const double now = s.game->sv.time;
  for (int i = 0; i < sh::kMaxClients && i < gs::kLoginSeats; i++) {
    const QwClient& c = s.clients[i];
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
  ranked_offer(l);
  advertise_key(l);
  for (const std::string& note : l.logins.notes) {
    s.log.push_back("login: " + note);
    std::printf("sesv: %s\n", note.c_str());
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
        qw_print_all(s, o.text + "\n");
        break;
      case gs::LoginOut::Kind::name: {
        std::string name = o.text;
        if (s.clients[o.slot].name != name) qw_set_info(s, o.slot, "name", name);
        break;
      }
      case gs::LoginOut::Kind::unname: {  // a registered name the master told of late
        std::vector<std::string> taken;
        for (int i = 0; i < sh::kMaxClients; i++)
          if (i != o.slot && s.clients[i].state != ConnState::free)
            taken.push_back(s.clients[i].name);
        qw_set_info(s, o.slot, "name", unique_name(s.cfg.default_name, taken));
        break;
      }
      case gs::LoginOut::Kind::auth:
        qw_set_info(s, o.slot, "*auth", o.text);
        break;
      case gs::LoginOut::Kind::drop: {
        const std::string name = s.clients[o.slot].name;
        qw_drop(s, o.slot, o.text);
        qw_print_all(s, name + " didn't log in in time.\n");
        break;
      }
    }
  }
}

}  // namespace shooters_edge::net
