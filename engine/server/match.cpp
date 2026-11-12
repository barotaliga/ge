// The lobby before the match, defined: the rules in match.hpp's header.
//
// ge:summary The lobby's rules: seats, teams, ready and break, the countdown, the match's start
// ge:keywords lobby warmup countdown ready break team leader lobby_command lobby_due
// ge:skill net
// ge:test tests/server_match_test.cpp
#include "server/match.hpp"

#include <cmath>

namespace ge::server {

namespace {

// The match or its intermission: ready, break and teams wait for the next warmup.
bool in_play(const Lobby& l) {
  return l.phase == LobbyPhase::match || l.phase == LobbyPhase::intermission;
}

bool seated(const Lobby& l, int slot) {
  return slot >= 0 && slot < kLobbySeats && l.seats[static_cast<size_t>(slot)].used;
}

LobbySeat& seat(Lobby& l, int slot) { return l.seats[static_cast<size_t>(slot)]; }

void say(Lobby& l, LobbyNote note, std::string line) {
  l.said.push_back(std::move(line));
  l.notes.push_back(note);
}

// A refusal said with what to do instead.
LobbyWhy refuse(Lobby& l, int slot, std::string_view what, LobbyWhy why) {
  std::string line = std::string(what) + ": " + why_text(why);
  switch (why) {
    case LobbyWhy::in_match:
      line += ". The lobby comes back when the map changes.";
      break;
    case LobbyWhy::not_leader:
      line += ". Type \"ready\": the countdown starts when everyone is ready.";
      break;
    case LobbyWhy::bad_team:
      line += ". Use team red, team blue, team auto or team switch.";
      break;
    case LobbyWhy::no_seat:
      line += ". Join the game first.";
      break;
    case LobbyWhy::too_few:
      line += " (" + std::to_string(lobby_seated(l)) + " seated, " +
              std::to_string(l.settings.min_players) +
              " needed). Wait for another player to join, or add a bot.";
      break;
    default:
      break;
  }
  say(l, {LobbyNoteKind::refused, slot, static_cast<uint8_t>(why)}, std::move(line));
  return why;
}

// `by` the seat whose break stopped it, or -1.
void stop_countdown(Lobby& l, const std::string& why, int by = -1) {
  if (l.phase != LobbyPhase::countdown) return;
  l.phase = LobbyPhase::warmup;
  l.stopped++;
  say(l, {LobbyNoteKind::stopped, by}, "countdown stopped: " + why);
}

// `by` the leader who started it, or -1 when everyone is ready.
void begin_countdown(Lobby& l, double now, const std::string& why, int by = -1) {
  l.phase = LobbyPhase::countdown;
  l.countdown_ends = now + l.settings.countdown_s;
  l.countdowns++;
  say(l, {LobbyNoteKind::countdown, by},
      why + ": the match starts in " + std::to_string(lobby_seconds_left(l, now)) + " s");
}

// Warmup: a countdown when everyone seated is ready and there are enough of them.
void check(Lobby& l, double now) {
  if (l.phase != LobbyPhase::warmup) return;
  const int n = lobby_seated(l);
  if (n >= l.settings.min_players && n > 0 && lobby_humans(l) >= l.settings.humans &&
      lobby_ready_count(l) == n)
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
    case LobbyPhase::intermission:
      return "intermission";
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
    case LobbyWhy::too_few:
      return "too few players to start";
    case LobbyWhy::no_screen:
      return "this game has no lobby screen to sit out on";
  }
  return "?";
}

int lobby_seated(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used ? 1 : 0;
  return n;
}

bool lobby_on_map(const Lobby& l, int slot) {
  if (!seated(l, slot)) return false;
  return in_play(l) || l.seats[static_cast<size_t>(slot)].warm;
}

int lobby_warm_count(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used && s.warm ? 1 : 0;
  return n;
}

int lobby_ready_count(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used && s.ready ? 1 : 0;
  return n;
}

int lobby_humans(const Lobby& l) {
  int n = 0;
  for (const LobbySeat& s : l.seats) n += s.used && !s.bot ? 1 : 0;
  return n;
}

int lobby_leader(const Lobby& l) {
  if (l.seats[0].used) return 0;
  for (int i = 0; i < kLobbySeats; i++)
    if (l.seats[static_cast<size_t>(i)].used && !l.seats[static_cast<size_t>(i)].bot) return i;
  for (int i = 0; i < kLobbySeats; i++)
    if (l.seats[static_cast<size_t>(i)].used) return i;
  return -1;
}

int lobby_seconds_left(const Lobby& l, double now) {
  double ends = 0;
  if (l.phase == LobbyPhase::countdown)
    ends = l.countdown_ends;
  else if (l.phase == LobbyPhase::intermission)
    ends = l.intermission_ends;
  else
    return 0;
  const double left = ends - now;
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
  s.warm = bot || l.settings.warm_on_join;
  say(l, {LobbyNoteKind::joined, slot},
      name + " joins team " + team_name(s.team) + (s.ready ? ", ready" : "") +
          (s.warm ? "" : ", in the lobby"));
  if (!s.ready) stop_countdown(l, name + " is not ready");
  check(l, now);
}

void lobby_leave(Lobby& l, int slot, double now) {
  if (!seated(l, slot)) return;
  say(l, {LobbyNoteKind::left, slot}, seat(l, slot).name + " leaves");
  seat(l, slot) = {};
  if (lobby_seated(l) < l.settings.min_players || lobby_seated(l) == 0)
    stop_countdown(l, "too few players");
  else if (lobby_humans(l) < l.settings.humans)
    stop_countdown(l, "no player left to play it");
  check(l, now);
}

LobbyWhy lobby_ready(Lobby& l, int slot, bool ready, double now) {
  const char* what = ready ? "ready" : "break";
  if (!seated(l, slot)) return refuse(l, slot, what, LobbyWhy::no_seat);
  if (in_play(l)) return refuse(l, slot, what, LobbyWhy::in_match);
  LobbySeat& s = seat(l, slot);
  if (s.ready == ready) return refuse(l, slot, what, LobbyWhy::same);
  s.ready = ready;
  say(l, {ready ? LobbyNoteKind::ready : LobbyNoteKind::unready, slot},
      s.name + (ready ? " is ready (" : " is not ready (") + std::to_string(lobby_ready_count(l)) +
          " of " + std::to_string(lobby_seated(l)) + ")");
  if (!ready) stop_countdown(l, s.name + " took a break", slot);
  check(l, now);
  return LobbyWhy::ok;
}

LobbyWhy lobby_team(Lobby& l, int slot, Team team, double now) {
  (void)now;
  if (!seated(l, slot)) return refuse(l, slot, "team", LobbyWhy::no_seat);
  if (in_play(l)) return refuse(l, slot, "team", LobbyWhy::in_match);
  LobbySeat& s = seat(l, slot);
  const Team to = team == Team::none ? lobby_smaller_team(l, slot) : team;
  if (to == s.team) return refuse(l, slot, "team", LobbyWhy::same);
  s.team = to;
  say(l, {LobbyNoteKind::moved, slot}, s.name + " moves to team " + team_name(to));
  return LobbyWhy::ok;
}

LobbyWhy lobby_start(Lobby& l, int slot, double now) {
  if (!seated(l, slot)) return refuse(l, slot, "start", LobbyWhy::no_seat);
  if (in_play(l)) return refuse(l, slot, "start", LobbyWhy::in_match);
  if (slot != lobby_leader(l)) return refuse(l, slot, "start", LobbyWhy::not_leader);
  if (l.phase == LobbyPhase::countdown) return refuse(l, slot, "start", LobbyWhy::same);
  if (lobby_seated(l) < l.settings.min_players || lobby_humans(l) < l.settings.humans)
    return refuse(l, slot, "start", LobbyWhy::too_few);
  begin_countdown(l, now, seat(l, slot).name + " starts the match", slot);
  return LobbyWhy::ok;
}

LobbyWhy lobby_warmup(Lobby& l, int slot, bool warm, double now) {
  (void)now;
  const char* what = warm ? "warmup" : "sitout";
  if (!seated(l, slot)) return refuse(l, slot, what, LobbyWhy::no_seat);
  if (in_play(l)) return refuse(l, slot, what, LobbyWhy::in_match);
  LobbySeat& s = seat(l, slot);
  if (s.warm == warm) return refuse(l, slot, what, LobbyWhy::same);
  if (!warm && l.settings.warm_on_join && !s.bot) return refuse(l, slot, what, LobbyWhy::no_screen);
  s.warm = warm;
  say(l, {warm ? LobbyNoteKind::warm : LobbyNoteKind::sat_out, slot},
      s.name + (warm ? " joins the warmup" : " goes back to the lobby"));
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
        say(l, {LobbyNoteKind::told, slot},
            std::string("team: you are on ") + team_name(seat(l, slot).team));
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
  } else if (word == "warmup") {
    lobby_warmup(l, slot, true, now);
  } else if (word == "sitout") {
    lobby_warmup(l, slot, false, now);
  } else if (word == "togglewarmup") {
    lobby_warmup(l, slot, !(seated(l, slot) && seat(l, slot).warm), now);
  } else if (word == "lobby") {
    std::string line_out = std::string("lobby: ") + phase_name(l.phase) + ", " +
                           std::to_string(lobby_ready_count(l)) + " of " +
                           std::to_string(lobby_seated(l)) + " ready";
    if (l.phase == LobbyPhase::countdown)
      line_out += ", " + std::to_string(lobby_seconds_left(l, now)) + " s to go";
    say(l, {LobbyNoteKind::told, slot}, line_out);
    for (int i = 0; i < kLobbySeats; i++) {
      const LobbySeat& s = l.seats[static_cast<size_t>(i)];
      if (!s.used) continue;
      say(l, {LobbyNoteKind::told, slot},
          "  " + std::to_string(i) + " " + s.name + " " + team_name(s.team) +
              (s.ready ? " ready" : " not ready") + (s.warm ? " in warmup" : " in lobby") +
              (s.bot ? " (bot)" : "") + (i == lobby_leader(l) ? " (leader)" : ""));
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
  say(l, {LobbyNoteKind::begins, -1}, "the match begins");
  return true;
}

void lobby_end(Lobby& l, double now) {
  if (l.phase != LobbyPhase::match) return;
  l.phase = LobbyPhase::intermission;
  l.intermission_ends = now + l.settings.intermission_s;
  say(l, {LobbyNoteKind::over, -1},
      l.settings.intermission_s > 0 ? "the match is over: the next map in " +
                                          std::to_string(lobby_seconds_left(l, now)) + " s"
                                    : std::string("the match is over"));
}

bool lobby_next_due(Lobby& l, double now) {
  if (l.phase != LobbyPhase::intermission || now < l.intermission_ends) return false;
  l.phase = LobbyPhase::match;  // until the game's map change brings the warmup (`lobby_back`)
  l.intermission_ends = 0;
  return true;
}

void lobby_back(Lobby& l) {
  l.phase = LobbyPhase::warmup;
  for (LobbySeat& s : l.seats)
    if (s.used) {
      s.ready = s.bot && l.settings.bots_ready;
      s.warm = s.bot || l.settings.warm_on_join;
    }
  say(l, {LobbyNoteKind::warmup, -1}, "warmup: say ready when you are");
}

}  // namespace ge::server
