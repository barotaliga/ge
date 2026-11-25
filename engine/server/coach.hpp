// A TEAM'S COACH: a spectator the players elect to watch one team's side of a team match and talk
// to it. The same for every team game on the engine (docs/servers.md § Coaches), beside the teams'
// captains (captains.hpp): the game asks this book who coaches which team, keeps a coach's camera
// on his team's players and routes his team chat (chat.hpp's `coach` seat).
//
// ONE COACH A TEAM, at most: red's and blue's. A coach is kept by his team, not its name, so a
// team renamed keeps its coach.
//
// THE ELECTION: a spectator says `coach <team>` (`1`, `a`, `red`, `2`, `b`, `blue` or the team's
// name) in the warmup of a team match, with at least `min_players` players seated. Everyone is
// told and the seated players answer `yes` (`no` takes a yes back). It passes the moment its yes
// reach `share` percent of the seated players (rounded up, 2 at least); it ends unpassed after
// `election_s`, when its candidate says `coach` again (withdrawn) or leaves, or when the match
// starts. One election at a time; a candidate whose election ended unpassed waits `wait_s` before
// asking again. Refused, with why: a player asking (only a spectator coaches), a match under way,
// too few players, a team that has a coach, another election or a vote under way.
//
// AN ADMINISTRATOR names a coach with no vote (`coach <team> <player>`) and takes one off
// (`uncoach <team>`), at any phase.
//
// A COACH STOPS by `coach` again (any phase), by leaving the server, or by taking a seat as a
// player. He stays through the match, its end and the map's change. `coaches` lists both.
//
// WHAT A COACH IS: his team's team chat both ways (chat.hpp); his camera follows only his team's
// players (`coach_may_follow`, `coach_next_follow`). Nothing else: no command of a captain's.
//
// A pure function of the seats and the acts in order on the server's clock: no wall clock, no
// randomness.
//
// ge:summary A team's coach: a spectator elected per team, his team chat and his camera on it
// ge:keywords coach coaches uncoach coach-election team-coach spectator-coach coach_command
// ge:keywords coach_tick coach_team coach_may_follow coach_next_follow sv_coach_share
// ge:skill net
// ge:test tests/server_coach_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "server/captains.hpp"
#include "server/match.hpp"

namespace ge::server {

struct CoachSettings {
  int share = 51;          // the share of the seated players whose yes elects (51..100 percent)
  int min_players = 3;     // the seated players an election needs
  double election_s = 60;  // an election's time
  double wait_s = 30;      // the wait after one's own election ended unpassed
};

struct Coaches {
  CoachSettings settings;
  std::array<int, 2> coach{-1, -1};  // red's, blue's coach (-1: none)
  int candidate = -1;                // the election's candidate (-1: none open)
  Team candidate_team = Team::none;
  uint32_t yes = 0;  // the seats that said yes, a bit each
  double opened = 0;
  std::array<double, kLobbySeats> wait_until{};
  std::vector<std::string> said;  // lines for the game to print, in order
  std::vector<int> to;            // each line's: -1 everyone, else that slot
};

// Who says a word: the slot, whether he is an administrator and a spectator, whether another
// vote is open (no election then), and every slot's name ("" a free slot).
struct CoachCaller {
  int slot = -1;
  bool admin = false;
  bool spectator = false;
  bool vote_open = false;
  const std::array<std::string, kLobbySeats>* names = nullptr;
};

// The team `slot` coaches (Team::none: he coaches none).
Team coach_team(const Coaches& c, int slot);
// The seated players an election counts and the yes it needs among them now.
int coach_voters(const Lobby& l);
int coach_needed(const Coaches& c, const Lobby& l);

// Once a frame, after the seats change: a coach or a candidate gone (his name "") or seated as a
// player stops, an election whose time ran out ends, and with `teams_on` false every coach goes.
void coach_tick(Coaches& c, const Lobby& l, const Teams& t,
                const std::array<std::string, kLobbySeats>& names, bool teams_on, double now);
// The match started: an election under way ends unpassed.
void coach_match_began(Coaches& c, const Teams& t);

// The words above; false when the line is not the coaches'. `teams_on`: a team match.
bool coach_command(Coaches& c, const Lobby& l, const Teams& t, const CoachCaller& who,
                   std::string_view line, bool teams_on, double now);

// Whether coach `slot` may follow `target` (a seated player of his team). Anyone else may follow
// anyone.
bool coach_may_follow(const Coaches& c, const Lobby& l, int slot, int target);
// The player coach `slot` follows instead of `from`: the next seat on his team after it (wrapping),
// -1 when his team has none.
int coach_next_follow(const Coaches& c, const Lobby& l, int slot, int from);

}  // namespace ge::server
