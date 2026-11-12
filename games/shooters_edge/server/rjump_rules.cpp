// The rocket jump helper's gate and packets on every runner (rjump_rules.hpp states the rules).
//
// ge:summary The rocket jump helper's settings, grant check and packet rewrite for every runner
// ge:keywords rjump_packet rjump_allowed sv_rjump sv_rrjump rjump-grant
// ge:skill shooter
// ge:test tests/shooters_edge_rjump_test.cpp
#include "shooters_edge/server/rjump_rules.hpp"

#include <utility>

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace con = ge::console;

namespace {

void tell(RulesCommon& s, int slot, std::string text, double now) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.level = 2;  // a high print: the player's own news
  ev.time = now;
  ev.text = std::move(text);
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

const char* rjump_setting(sh::Rjump kind) {
  return kind == sh::Rjump::back ? "sv_rrjump" : "sv_rjump";
}

void add_rjump_settings(con::Cvars& cvars) {
  const uint32_t info = con::kServer | con::kServerInfo;
  cvars.add({.name = "sv_rjump",
             .fallback = "0",
             .flags = info,
             .help = "Whether players may use the rocket jump helper, +rjump, a rocket fired "
                     "straight down with a jump.",
             .kind = con::CvarKind::boolean,
             .values = "0: only a player granted it (the default); 1: every player"});
  cvars.add({.name = "sv_rrjump",
             .fallback = "0",
             .flags = info,
             .help = "Whether players may use the reverse rocket jump helper, +rrjump, a rocket "
                     "fired behind with a jump.",
             .kind = con::CvarKind::boolean,
             .values = "0: only a player granted it (the default); 1: every player"});
}

bool rjump_allowed(const RulesCommon& s, int slot, sh::Rjump kind) {
  if (kind == sh::Rjump::none || slot < 0 || slot >= sh::kMaxClients) return false;
  const con::Cvar* on = s.cvars.find(rjump_setting(kind));
  if (on != nullptr && on->on()) return true;
  return info_value(s.clients[slot].userinfo, kRjumpGrantKey) == "1";
}

bool rjump_packet(RulesCommon& s, sh::Packet& p, void* rules, RjumpReadyFn ready, double now) {
  if (p.client < 0 || p.client >= sh::kMaxClients) return false;
  sh::RjumpSeat& seat = s.rjumps[p.client];
  sh::MoveCmd& cmd = p.cmds[2];
  const sh::Rjump asked = sh::rjump_take(seat, cmd);
  if (asked != sh::Rjump::none && !rjump_allowed(s, p.client, asked)) {
    seat = {};
    tell(s, p.client, "The rocket jump helper is off on this server.\n", now);
    return false;
  }
  if (seat.kind == sh::Rjump::none) return false;
  std::string why;
  const sh::RjumpReady r = ready(rules, p.client, cmd, why);
  if (r == sh::RjumpReady::never && !why.empty()) tell(s, p.client, why, now);
  // The launcher the runner chose for the shot rides on the rewritten command.
  const uint8_t choose = cmd.impulse;
  const bool shot = sh::rjump_apply(seat, cmd, r);
  if (shot) cmd.impulse = choose;
  return shot;
}

}  // namespace shooters_edge::server
