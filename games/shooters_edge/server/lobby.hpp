// THE LOBBY BEFORE THE MATCH, Shooter's Edge's: the engine's match flow (engine/server/match.hpp,
// lifted from here in M53c) and what a team is in this game. THE ENGINE'S RULES: who is in the
// game, on which team, and who is ready; the WARMUP they free-roam the map in until the match
// starts, and the COUNTDOWN between.
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
// ge:keywords team_colour palette-row
// ge:skill shooter net
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include "server/match.hpp"

namespace shooters_edge::server {

struct QcServer;

using ge::server::kLobbySeats;
using ge::server::Lobby;
using ge::server::lobby_back;
using ge::server::lobby_command;
using ge::server::lobby_due;
using ge::server::lobby_join;
using ge::server::lobby_leader;
using ge::server::lobby_leave;
using ge::server::lobby_ready;
using ge::server::lobby_ready_count;
using ge::server::lobby_seated;
using ge::server::lobby_seconds_left;
using ge::server::lobby_smaller_team;
using ge::server::lobby_start;
using ge::server::lobby_team;
using ge::server::LobbyPhase;
using ge::server::LobbySeat;
using ge::server::LobbySettings;
using ge::server::LobbyWhy;
using ge::server::phase_name;
using ge::server::Team;
using ge::server::team_name;
using ge::server::team_named;
using ge::server::why_text;

// The palette row a team's player wears (red 4, blue 13; 0 for none).
int team_colour(Team t);

// Seat `slot`'s team put on server `s`: the user info's team and colours, and the progs' `.team`
// field when it has one. The slot must be connected.
void lobby_apply(QcServer& s, const Lobby& l, int slot);

}  // namespace shooters_edge::server
