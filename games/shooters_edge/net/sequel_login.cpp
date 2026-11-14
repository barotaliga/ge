// Logins on the sequel's server (net/sequel_login.hpp).
//
// ge:summary Logins on the sequel's server: seats synced, lines taken, names, prompts wrapped
// ge:keywords sequel-login login login-prompt sequel_wrap
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_login_test.cpp
#include "shooters_edge/net/sequel_login.hpp"

#include <algorithm>

#include "net/token.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;

void os_fresh(void*, uint8_t* out, size_t size) { ge::net::os_random_bytes(out, size); }

int64_t unix_now(const SequelLogin& l) { return l.unix_us != nullptr ? l.unix_us() : 0; }

double now_s(const SequelLogin& l) { return l.server->now; }

// The seat at `slot` matches the client there (another player took it: a new seat).
void sync_seat(SequelLogin& l, int slot) {
  const SequelNetClient& c = l.server->clients[slot];
  gs::LoginSeat& seat = l.logins.seats[slot];
  if (c.stage == SequelSlot::free) {
    if (seat.state != gs::LoginState::empty) gs::login_leave(l.logins, slot);
    return;
  }
  if (seat.state == gs::LoginState::empty || seat.userid != c.userid)
    gs::login_seat(l.logins, slot, c.userid, c.spectator, ge::net::to_string(c.address), now_s(l));
}

std::string free_name(const SequelNetServer& s, int slot) {
  std::vector<std::string> taken;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (i != slot && s.clients[i].stage != SequelSlot::free) taken.push_back(s.clients[i].name);
  return unique_name(kSequelDefaultName, taken);
}

bool line_hook(void* ctx, int slot, const std::string& line) {
  SequelLogin& l = *static_cast<SequelLogin*>(ctx);
  sync_seat(l, slot);
  return gs::login_line(l.logins, slot, line, now_s(l), unix_now(l));
}

void name_rule(void* ctx, int slot, std::string& name, bool connecting) {
  SequelLogin& l = *static_cast<SequelLogin*>(ctx);
  // A seat's login is its player's: one who left and another who took the slot share nothing. A
  // name refused while connecting is told when the seat is taken (login_seat).
  if (l.logins.seats[slot].userid != l.server->clients[slot].userid)
    gs::login_leave(l.logins, slot);
  if (gs::login_name(l.logins, slot, name, connecting) == gs::NameVerdict::reserved)
    name = free_name(*l.server, slot);
}

void tell(SequelNetServer& s, int slot, const std::string& text) {
  for (const std::string& line : sequel_wrap(text)) sequel_net_print(s, slot, line + "\n");
}

// `sv_ranked`: the players' seats nobody has (spectators take none) offered to the matchmaker.
void ranked_offer(SequelLogin& l) {
  SequelNetServer& s = *l.server;
  if (l.logins.set.ranked.empty() || s.transport == nullptr) return;
  const int most = std::min(s.cfg.max_clients, sh::kMaxClients);
  int taken = 0;
  for (int i = 0; i < most; i++)
    if (s.clients[i].stage != SequelSlot::free && !s.clients[i].spectator) taken++;
  gs::logins_offer(l.logins, l.game, ge::net::to_string(s.transport->local),
                   s.transport->local.port, most - taken, unix_now(l));
}

}  // namespace

ge::account::NameRules sequel_name_rules() { return {15, false, false}; }

std::vector<std::string> sequel_wrap(const std::string& text, size_t width) {
  std::vector<std::string> out;
  std::string rest = text;
  while (rest.size() > width) {
    size_t cut = rest.rfind(' ', width);
    if (cut == std::string::npos || cut == 0) cut = width;
    out.push_back(rest.substr(0, cut));
    rest = rest.substr(cut);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
  }
  if (!rest.empty() || out.empty()) out.push_back(rest);
  return out;
}

bool sequel_login_open(SequelLogin& l, SequelNetServer& s, const gs::LoginSettings& set,
                       ge::account::Service* own, ge::net::Transport* link, int64_t (*unix_us)(),
                       std::string& error) {
  l.server = &s;
  l.unix_us = unix_us;
  if (!gs::logins_open(l.logins, set, sequel_name_rules(), own, link, os_fresh, nullptr,
                       unix_now(l), error))
    return false;
  s.line_hook = line_hook;
  s.name_rule = name_rule;
  s.hook_ctx = &l;
  return true;
}

void sequel_login_frame(SequelLogin& l) {
  SequelNetServer& s = *l.server;
  for (int i = 0; i < sh::kMaxClients && i < gs::kLoginSeats; i++) sync_seat(l, i);
  gs::logins_frame(l.logins, now_s(l), unix_now(l));
  ranked_offer(l);
  for (const std::string& note : l.logins.notes) s.log.push_back("login: " + note);
  l.logins.notes.clear();
  std::vector<gs::LoginOut> out;
  out.swap(l.logins.out);
  for (const gs::LoginOut& o : out) {
    if (o.kind == gs::LoginOut::Kind::tell_all) {
      for (int i = 0; i < sh::kMaxClients; i++) tell(s, i, o.text);
      continue;
    }
    if (o.slot < 0 || o.slot >= sh::kMaxClients || s.clients[o.slot].stage == SequelSlot::free)
      continue;
    switch (o.kind) {
      case gs::LoginOut::Kind::tell:
        tell(s, o.slot, o.text);
        break;
      case gs::LoginOut::Kind::name:
        sequel_net_rename(s, o.slot, o.text);
        break;
      case gs::LoginOut::Kind::unname:
        sequel_net_rename(s, o.slot, free_name(s, o.slot));
        break;
      case gs::LoginOut::Kind::auth:  // its scoreboard shows the name alone
        break;
      case gs::LoginOut::Kind::drop: {
        const std::string name = s.clients[o.slot].name;
        for (const std::string& line : sequel_wrap(o.text))
          sequel_net_print(s, o.slot, line + "\n");
        sequel_net_drop(s, o.slot, o.text);
        for (int i = 0; i < sh::kMaxClients; i++) tell(s, i, name + " didn't log in in time.");
        break;
      }
      case gs::LoginOut::Kind::tell_all:
        break;
    }
  }
}

}  // namespace shooters_edge::net
