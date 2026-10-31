// The lobby before the match: the rules in lobby.hpp's header.
//
// ge:summary The lobby's rules: seats, teams, ready and break, the countdown, the match's start
// ge:keywords lobby warmup countdown ready break team leader lobby_command lobby_due lobby_apply
// ge:skill shooter net
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/server/lobby.hpp"

#include <cmath>

#include "progs/vm.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace {

bool seated(const Lobby& l, int slot) {
  return slot >= 0 && slot < kLobbySeats && l.seats[static_cast<size_t>(slot)].used;
}

LobbySeat& seat(Lobby& l, int slot) { return l.seats[static_cast<size_t>(slot)]; }

void say(Lobby& l, std::string line) { l.said.push_back(std::move(line)); }

// A refusal said with what to do instead.
LobbyWhy refuse(Lobby& l, int slot, std::string_view what, LobbyWhy why) {
  std::string line = std::string(what) + ": " + why_text(why);
  switch (why) {
    case LobbyWhy::in_match:
      line += "; next: play the match, the lobby comes back when the map changes";
      break;
    case LobbyWhy::not_leader:
      line += "; next: `ready`, the countdown starts when everyone is";
      break;
    case LobbyWhy::bad_team:
      line += "; next: `team red`, `team blue`, `team auto` or `team switch`";
      break;
    case LobbyWhy::no_seat:
      line += " (slot " + std::to_string(slot) + "); next: join the game first";
      break;
    default:
      break;
  }
  say(l, std::move(line));
  return why;
}

void stop_countdown(Lobby& l, const std::string& why) {
  if (l.phase != LobbyPhase::countdown) return;
  l.phase = LobbyPhase::warmup;
  l.stopped++;
  say(l, "countdown stopped: " + why);
}

void begin_countdown(Lobby& l, double now, const std::string& why) {
  l.phase = LobbyPhase::countdown;
  l.countdown_ends = now + l.settings.countdown_s;
  l.countdowns++;
  say(l, why + ": the match starts in " + std::to_string(lobby_seconds_left(l, now)) + " s");
}

// Warmup: a countdown when everyone seated is ready and there are enough of them.
void check(Lobby& l, double now) {
  if (l.phase != LobbyPhase::warmup) return;
  const int n = lobby_seated(l);
  if (n >= l.settings.min_players && n > 0 && lobby_ready_count(l) == n)
    begin_countdown(l, now, "everyone is ready");
}

}  // namespace

const char* phase_name(LobbyPhase p) {
  switch (p) {
    case LobbyPhase::warmup:
      return "warmup";
    case LobbyPhase::countdown:
      return "countdown";
    case LobbyPhase::match:
      return "match";
  }
  return "?";
}

const char* team_name(Team t) {
  switch (t) {
    case Team::red:
      return "red";
    case Team::blue:
      return "blue";
    case Team::none:
      break;
  }
  return "none";
}

Team team_named(std::string_view w) {
  if (w == "red" || w == "r") return Team::red;
  if (w == "blue" || w == "b") return Team::blue;
  return Team::none;
}

int team_colour(Team t) { return t == Team::red ? 4 : t == Team::blue ? 13 : 0; }

const char* why_text(LobbyWhy w) {
  switch (w) {
    case LobbyWhy::ok:
      return "done";
    case LobbyWhy::no_seat:
      return "not in the game";
    case LobbyWhy::in_match:
      return "the match is on";
    case LobbyWhy::not_leader:
      return "only the leader may start the match";
    case LobbyWhy::same:
      return "already so";
    case LobbyWhy::bad_team:
      return "no such team";
  }
  return "?";
}

int lobby_seated(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used ? 1 : 0;
  return n;
}

int lobby_ready_count(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used && s.ready ? 1 : 0;
  return n;
}

int lobby_leader(const Lobby& l) {
  for (int i = 0; i < kLobbySeats; i++)
    if (l.seats[static_cast<size_t>(i)].used) return i;
  return -1;
}

int lobby_seconds_left(const Lobby& l, double now) {
  if (l.phase != LobbyPhase::countdown) return 0;
  const double left = l.countdown_ends - now;
  return left <= 0 ? 0 : static_cast<int>(std::ceil(left - 1e-9));
}

Team lobby_smaller_team(const Lobby& l, int except) {
  int red = 0, blue = 0;
  for (int i = 0; i < kLobbySeats; i++) {
    const LobbySeat& s = l.seats[static_cast<size_t>(i)];
    if (!s.used || i == except) continue;
    red += s.team == Team::red ? 1 : 0;
    blue += s.team == Team::blue ? 1 : 0;
  }
  return blue < red ? Team::blue : Team::red;
}

void lobby_join(Lobby& l, int slot, const std::string& name, bool bot, double now) {
  if (slot < 0 || slot >= kLobbySeats) return;
  LobbySeat& s = seat(l, slot);
  s = {};
  s.team = lobby_smaller_team(l, slot);
  s.used = true;
  s.bot = bot;
  s.name = name;
  s.ready = bot && l.settings.bots_ready;
  say(l, name + " joins team " + team_name(s.team) + (s.ready ? ", ready" : ""));
  if (!s.ready) stop_countdown(l, name + " is not ready");
  check(l, now);
}

void lobby_leave(Lobby& l, int slot, double now) {
  if (!seated(l, slot)) return;
  say(l, seat(l, slot).name + " leaves");
  seat(l, slot) = {};
  if (lobby_seated(l) < l.settings.min_players || lobby_seated(l) == 0)
    stop_countdown(l, "too few players");
  check(l, now);
}

LobbyWhy lobby_ready(Lobby& l, int slot, bool ready, double now) {
  const char* what = ready ? "ready" : "break";
  if (!seated(l, slot)) return refuse(l, slot, what, LobbyWhy::no_seat);
  if (l.phase == LobbyPhase::match) return refuse(l, slot, what, LobbyWhy::in_match);
  LobbySeat& s = seat(l, slot);
  if (s.ready == ready) return refuse(l, slot, what, LobbyWhy::same);
  s.ready = ready;
  say(l, s.name + (ready ? " is ready (" : " is not ready (") +
             std::to_string(lobby_ready_count(l)) + " of " + std::to_string(lobby_seated(l)) + ")");
  if (!ready) stop_countdown(l, s.name + " took a break");
  check(l, now);
  return LobbyWhy::ok;
}

LobbyWhy lobby_team(Lobby& l, int slot, Team team, double now) {
  (void)now;
  if (!seated(l, slot)) return refuse(l, slot, "team", LobbyWhy::no_seat);
  if (l.phase == LobbyPhase::match) return refuse(l, slot, "team", LobbyWhy::in_match);
  LobbySeat& s = seat(l, slot);
  const Team to = team == Team::none ? lobby_smaller_team(l, slot) : team;
  if (to == s.team) return refuse(l, slot, "team", LobbyWhy::same);
  s.team = to;
  say(l, s.name + " moves to team " + team_name(to));
  return LobbyWhy::ok;
}

LobbyWhy lobby_start(Lobby& l, int slot, double now) {
  if (!seated(l, slot)) return refuse(l, slot, "start", LobbyWhy::no_seat);
  if (l.phase == LobbyPhase::match) return refuse(l, slot, "start", LobbyWhy::in_match);
  if (slot != lobby_leader(l)) return refuse(l, slot, "start", LobbyWhy::not_leader);
  if (l.phase == LobbyPhase::countdown) return refuse(l, slot, "start", LobbyWhy::same);
  begin_countdown(l, now, seat(l, slot).name + " starts the match");
  return LobbyWhy::ok;
}

bool lobby_command(Lobby& l, int slot, std::string_view line, double now) {
  while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
  while (!line.empty() && (line.back() == ' ' || line.back() == '\n')) line.remove_suffix(1);
  const size_t sp = line.find(' ');
  const std::string_view word = line.substr(0, sp);
  std::string_view rest = sp == std::string_view::npos ? std::string_view{} : line.substr(sp + 1);
  while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
  if (word == "ready") {
    lobby_ready(l, slot, true, now);
  } else if (word == "break" || word == "notready") {
    lobby_ready(l, slot, false, now);
  } else if (word == "toggleready") {
    lobby_ready(l, slot, !(seated(l, slot) && seat(l, slot).ready), now);
  } else if (word == "team") {
    if (rest.empty()) {
      if (seated(l, slot))
        say(l, std::string("team: you are on ") + team_name(seat(l, slot).team));
      else
        refuse(l, slot, "team", LobbyWhy::no_seat);
    } else if (rest == "auto") {
      lobby_team(l, slot, Team::none, now);
    } else if (rest == "switch") {
      const Team now_on = seated(l, slot) ? seat(l, slot).team : Team::none;
      lobby_team(l, slot, now_on == Team::red ? Team::blue : Team::red, now);
    } else if (const Team t = team_named(rest); t != Team::none) {
      lobby_team(l, slot, t, now);
    } else {
      refuse(l, slot, "team " + std::string(rest), LobbyWhy::bad_team);
    }
  } else if (word == "start") {
    lobby_start(l, slot, now);
  } else if (word == "lobby") {
    std::string line_out = std::string("lobby: ") + phase_name(l.phase) + ", " +
                           std::to_string(lobby_ready_count(l)) + " of " +
                           std::to_string(lobby_seated(l)) + " ready";
    if (l.phase == LobbyPhase::countdown)
      line_out += ", " + std::to_string(lobby_seconds_left(l, now)) + " s to go";
    say(l, line_out);
    for (int i = 0; i < kLobbySeats; i++) {
      const LobbySeat& s = l.seats[static_cast<size_t>(i)];
      if (!s.used) continue;
      say(l, "  " + std::to_string(i) + " " + s.name + " " + team_name(s.team) +
                 (s.ready ? " ready" : " not ready") + (s.bot ? " (bot)" : "") +
                 (i == lobby_leader(l) ? " (leader)" : ""));
    }
  } else {
    return false;
  }
  return true;
}

bool lobby_due(Lobby& l, double now) {
  if (l.phase != LobbyPhase::countdown || now < l.countdown_ends) return false;
  l.phase = LobbyPhase::match;
  l.matches++;
  say(l, "the match begins");
  return true;
}

void lobby_back(Lobby& l) {
  l.phase = LobbyPhase::warmup;
  for (LobbySeat& s : l.seats)
    if (s.used) s.ready = s.bot && l.settings.bots_ready;
  say(l, "warmup: say ready when you are");
}

void lobby_apply(QcServer& s, const Lobby& l, int slot) {
  if (!seated(l, slot)) return;
  const Team t = l.seats[static_cast<size_t>(slot)].team;
  const std::string colour = std::to_string(team_colour(t));
  set_user_info(s, slot, "team", team_name(t));
  set_user_info(s, slot, "topcolor", colour);
  set_user_info(s, slot, "bottomcolor", colour);
  if (s.progs == nullptr) return;
  if (const ge::progs::Def* d = ge::progs::find_field(*s.progs, "team"); d != nullptr)
    *ge::progs::entity_field(s.q.vm, slot + 1, d->offset) =
        ge::progs::as_word(static_cast<float>(static_cast<int>(t)));
}

}  // namespace shooters_edge::server
