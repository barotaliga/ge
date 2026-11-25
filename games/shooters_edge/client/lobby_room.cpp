// The lobby on the listen server: lobby_room.hpp's header says what it does.
//
// ge:summary The lobby on the listen server: bots seated, warmup's limits held, the match restart
// ge:keywords lobby room addbot removebot fighter nav-graph room_enter room_packets
// ge:skill shooter bots
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/client/lobby_room.hpp"

#include <cmath>

#include "shooters_edge/server/commands.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::client {

hud::LobbyView lobby_view(const Room& r, double now, int me) {
  hud::LobbyView v;
  const sv::Lobby& l = r.lobby;
  v.phase = l.phase == sv::LobbyPhase::warmup      ? hud::LobbyShown::warmup
            : l.phase == sv::LobbyPhase::countdown ? hud::LobbyShown::countdown
                                                   : hud::LobbyShown::match;
  v.seconds_left = sv::lobby_seconds_left(l, now);
  const double left = l.countdown_ends - now;
  v.fraction = left > 0 ? left - std::floor(left) : 0;
  v.min_players = l.settings.min_players;
  const int leader = sv::lobby_leader(l);
  for (int i = 0; i < sv::kLobbySeats; i++) {
    const sv::LobbySeat& s = l.seats[static_cast<size_t>(i)];
    if (!s.used) continue;
    v.rows.push_back({s.name, static_cast<int>(s.team), s.ready, s.bot, i == leader, i == me,
                      sv::lobby_on_map(l, i)});
  }
  return v;
}

bool skill_named(const std::string& word, sb::AimSkill& out) {
  for (const sb::AimSkill k : {sb::AimSkill::novice, sb::AimSkill::average, sb::AimSkill::expert})
    if (word == sb::aim_skill_name(k)) {
      out = k;
      return true;
    }
  return false;
}

bool companion_words(const std::string& words, sb::AimSkill& skill, bool& helper,
                     std::string& bad) {
  size_t at = 0;
  while (at < words.size()) {
    const size_t sp = words.find(' ', at);
    const std::string w = words.substr(at, sp == std::string::npos ? std::string::npos : sp - at);
    at = sp == std::string::npos ? words.size() : sp + 1;
    if (w.empty() || skill_named(w, skill)) continue;
    if (w == "escort" || w == "helper") {
      helper = w == "helper";
      continue;
    }
    bad = w;
    return false;
  }
  return true;
}

int room_add_bot(Room& r, const sv::RulesRunner& rules, const sv::MapSpec& map, sb::AimSkill skill,
                 std::string& error) {
  const sv::RulesCommon& s = *rules;
  uint32_t taken = 0;  // the lobby's seats (its own player's too)
  for (int i = 0; i < sv::kLobbySeats && i < 32; i++)
    if (r.lobby.seats[static_cast<size_t>(i)].used) taken |= 1u << i;
  const int slot = bots::bot_seats_add(r.seats, rules, map, skill, taken, error);
  if (slot < 0) return -1;
  const std::string& name = r.seats.bots[static_cast<size_t>(slot)]->name;
  sv::lobby_join(r.lobby, slot, name + " (" + sb::aim_skill_name(skill) + ")", true, s.sv.time);
  sv::lobby_apply(rules, r.lobby, slot);
  return slot;
}

bool room_remove_bot(Room& r, const sv::RulesRunner& rules) {
  const sv::RulesCommon& s = *rules;
  const int i = bots::bot_seats_last(r.seats);
  if (i < 0) return false;
  rules.drop(i);
  bots::bot_seats_forget(r.seats, i);
  sv::lobby_leave(r.lobby, i, s.sv.time);
  return true;
}

void room_enter(Room& r, const sv::RulesRunner& rules, const sv::MapSpec& map) {
  const sv::RulesCommon& s = *rules;
  std::vector<int> gone;
  std::string error;
  if (!bots::bot_seats_nav(r.seats, rules, map, gone, error)) {  // a map no bot can walk
    r.lobby.said.push_back("the bots leave: " + error);
    for (const int i : gone) sv::lobby_leave(r.lobby, i, s.sv.time);
  }
  bots::bot_seats_connect(r.seats, rules);
  for (int i = 0; i < sv::kLobbySeats && i < sh::kMaxClients; i++)
    if (r.lobby.seats[static_cast<size_t>(i)].used && s.clients[i].active)
      sv::lobby_apply(rules, r.lobby, i);
  bots::bot_seats_remake(r.seats, rules);
  if (r.lobby.phase != sv::LobbyPhase::match) {
    rules.console("fraglimit 0");
    rules.console("timelimit 0");
  }
}

void room_packets(Room& r, const sv::RulesRunner& s, int msec, std::vector<sh::Packet>& out) {
  bots::bot_seats_packets(r.seats, s, msec, out);
}

}  // namespace shooters_edge::client
