// THE LOBBY BEFORE THE MATCH: who is in the game, on which team, and who is ready; the WARMUP
// they free-roam the map in until the match starts, and the COUNTDOWN between.
//
// SEATS are the server's client slots: a seat is taken when a player or a bot joins and freed
// when they leave. Every seat is on one of two TEAMS, red or blue: a new seat joins the smaller
// (red on a tie), and a player moves with `team red|blue|auto|switch`. The team is the player's
// user info (`team`, and his colours `topcolor` and `bottomcolor`: red's palette row 4, blue's 13)
// and the mod's `.team` float (1 red, 2 blue) when its progs has one (`lobby_apply`).
//
// THE PHASES:
// - WARMUP. The map plays as a match would, every player spawned and free to roam, fight and pick
//   things up, but nothing counts: whatever is scored is thrown away when the match starts, and
//   the server's frag and time limits are held off (the listen sets them to 0 while warming up).
//   Each player says `ready` (or takes it back with `break`). A bot is ready as it joins when
//   `bots_ready`.
// - COUNTDOWN. When every seat is ready and at least `min_players` are seated, the countdown of
//   `countdown_s` starts; the leader may start it with `start` whoever is ready. A `break`, a
//   player joining unready, or the seats falling under `min_players` stops it, back to warmup.
// - MATCH. When the countdown runs out `lobby_due` says so once: the game restarts the map with
//   every seat on it (scores, items and the clock new) and the match is played to its limits.
//   When the mod changes the map at the match's end, `lobby_back` returns everyone to warmup
//   with every player unready.
//
// THE LEADER is slot 0 when it is seated (the listen server's own player), else the lowest seat:
// only the leader may `start`.
//
// THE WORDS (`lobby_command`, a console line or a client's command): `ready`, `break` (also
// `notready`), `toggleready`, `team <red|blue|auto|switch>`, `start`, `lobby` (the roster
// printed). Anything else is not the lobby's (false), for the mod or the console. Every act, and
// every refusal with why and what to do instead, is a line in `said` for the game to print.
//
// A pure function of the seats and the acts in order, on the server's clock: no wall clock, no
// randomness.
//
// ge:summary The lobby before the match: seats, teams, ready and break, warmup, the countdown
// ge:keywords lobby warmup countdown ready break notready toggleready team red blue auto switch
// ge:keywords start leader seats roster pre-match free-roam lobby_command lobby_due lobby_back
// ge:keywords lobby_apply topcolor bottomcolor userinfo match-start
// ge:skill shooter net
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::server {

struct QcServer;

constexpr int kLobbySeats = 32;

enum class LobbyPhase : uint8_t { warmup, countdown, match };
enum class Team : uint8_t { none, red, blue };

const char* phase_name(LobbyPhase p);
const char* team_name(Team t);
// `red`, `blue` (and their first letters); Team::none for anything else.
Team team_named(std::string_view word);
// The palette row a team's player wears (red 4, blue 13; 0 for none).
int team_colour(Team t);

struct LobbySettings {
  int min_players = 1;     // the fewest seated before a countdown starts
  double countdown_s = 5;  // the countdown's length, s
  bool bots_ready = true;  // a bot is ready as it joins
};

struct LobbySeat {
  bool used = false;
  bool bot = false;
  bool ready = false;
  Team team = Team::none;
  std::string name;
};

// Why an act was refused.
enum class LobbyWhy : uint8_t {
  ok,
  no_seat,     // the slot is not seated
  in_match,    // the match is on: ready, break and teams wait for the next warmup
  not_leader,  // only the leader may start
  same,        // already so (ready when ready, the team it is on)
  bad_team,    // `team` named no team
};
const char* why_text(LobbyWhy w);

struct Lobby {
  LobbySettings settings;
  LobbyPhase phase = LobbyPhase::warmup;
  double countdown_ends = 0;  // the server time the countdown runs out, s
  std::array<LobbySeat, kLobbySeats> seats;
  std::vector<std::string> said;  // lines for the game to print, in order (the game clears it)
  int matches = 0;                // matches started
  int countdowns = 0;             // countdowns started (stopped ones included)
  int stopped = 0;                // countdowns stopped before they ran out
};

// A seat taken by `name` (a bot's when `bot`), on the smaller team, ready when it is a bot and
// `bots_ready`; a countdown stops for a player who is not ready.
void lobby_join(Lobby& l, int slot, const std::string& name, bool bot, double now);
// The seat freed; a countdown stops if the seats fall under `min_players`.
void lobby_leave(Lobby& l, int slot, double now);

LobbyWhy lobby_ready(Lobby& l, int slot, bool ready, double now);
// `Team::none` is auto: the smaller of the other seats' teams.
LobbyWhy lobby_team(Lobby& l, int slot, Team team, double now);
// The leader starts the countdown whoever is ready.
LobbyWhy lobby_start(Lobby& l, int slot, double now);
// One of the words above for `slot`; false when the line is not the lobby's.
bool lobby_command(Lobby& l, int slot, std::string_view line, double now);

// True once, when a countdown runs out at `now`: the phase is then `match`, and the game restarts
// the map with every seat on it.
bool lobby_due(Lobby& l, double now);
// The match is over (the map changes): warmup again, every player unready (bots as they join).
void lobby_back(Lobby& l);

int lobby_seated(const Lobby& l);
int lobby_ready_count(const Lobby& l);
int lobby_leader(const Lobby& l);  // -1 with no seat taken
// Whole seconds left on the countdown, rounded up (0 outside it).
int lobby_seconds_left(const Lobby& l, double now);
// The team the next seat would join: the smaller, red on a tie (`except` left out of the count).
Team lobby_smaller_team(const Lobby& l, int except = -1);

// Seat `slot`'s team put on server `s`: the user info's team and colours, and the progs' `.team`
// field when it has one. The slot must be connected.
void lobby_apply(QcServer& s, const Lobby& l, int slot);

}  // namespace shooters_edge::server
