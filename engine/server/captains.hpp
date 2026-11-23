// THE TWO TEAMS OF A TEAM MATCH AND THEIR CAPTAINS, over the lobby's seats (match.hpp): each team's
// name, captain, lock and size, and the words a captain and an administrator use on them. The same
// for every team game on the engine (docs/servers.md § Teams and captains); the game seats its
// players in the lobby, applies what this book asks of it (a player put on a team or taken off
// one) and prints what it says.
//
// THE TEAMS are the lobby's red and blue, named by the game (`name`, a server's setting) and shown
// by those names; a word may name one as `1` or `a`, `2` or `b`, `red` or `blue`, or its name.
//
// THE CAPTAIN of a team is the first seat to join it while it has none ("<name> became captain of
// '<team>'"); when the captain leaves it the lowest seat still on it is captain, said the same
// way. A team that empties loses its captain and its lock. `captain` alone tells who is captain,
// or makes the caller captain of his team when it has none; `captain <player>` hands it to a
// teammate; an administrator names either team's: `captain <team> <player>`. `captains` lists
// both.
//
// WHAT A CAPTAIN MAY DO (an administrator too, on either team, at any phase):
// - `lockteam` (`lock`) and `unlockteam` (`unlock`): a locked team takes nobody by `team`; an
//   empty team is never locked.
// - `teamname <name>` (warmup only, 15 characters, no quotes or control characters; refused with
//   `names_locked`).
// - `invite <player>`: an unseated player (a spectator) is asked; `accept` takes it, even into a
//   match under way or a locked team. `pickplayer` (`pick`) puts him on the team at once when
//   `allow_pick`, else it is an invite. Invites end when the match starts or ends.
// - `kickplayer` (`removeplayer`, `remove`): a teammate taken off the team, to watch.
// - JOINING by `team <team>` (and `join`): refused while the team is locked or full
//   (`max_per_team`, 0 no limit), and outside the warmup (the lobby's own rule).
//
// THE GAME'S PART: each frame `teams_tick` after the seats change. The acts it applies are in
// `moves`: a slot to put on a team (`team` red or blue, from an accepted invite or a pick; the
// game seats it as a player and calls `teams_seat`) or to take off (`team` none: the game makes it
// a spectator). Every line is in `said` with beside it in `to` who it is for (-1 everyone).
//
// A pure function of the seats and the acts in order: no clock, no randomness but the shuffle's
// seed.
//
// ge:summary Two teams and their captains over the lobby: names, locks, invites, picks, removes
// ge:keywords captain captains team-captain lockteam unlockteam teamname invite accept pick
// ge:keywords pickplayer kickplayer removeplayer team-lock team-size max_per_team shuffle tdm
// ge:skill net
// ge:test tests/server_captains_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "server/match.hpp"

namespace ge::server {

struct TeamsSettings {
  std::array<std::string, 2> names = {"Red", "Blue"};  // the teams' names (red's, blue's)
  int max_per_team = 0;       // the most seats a team takes by `team` (0: no limit)
  bool allow_pick = false;    // a captain's pick puts a player on at once (else it invites)
  bool names_locked = false;  // `teamname` refused
};

struct TeamSide {
  int captain = -1;  // the captain's slot (-1: none)
  bool locked = false;
};

struct TeamMove {
  int slot = -1;
  Team team = Team::none;  // red or blue: put on that team; none: taken off to watch
};

struct Teams {
  TeamsSettings settings;
  std::array<TeamSide, 2> side;               // red's, blue's
  std::array<int8_t, kLobbySeats> invited{};  // the team a slot is invited to (Team as int; 0 none)
  std::vector<TeamMove> moves;                // for the game to apply, in order (it clears them)
  std::vector<std::string> said;              // lines for the game to print, in order
  std::vector<int> to;                        // each line's: -1 everyone, else that slot
};

// Who says a word: the slot, its name, whether it is an administrator, and the names of every
// slot on the server (seated or not; "" a free slot), so a word can name a player.
struct TeamsCaller {
  int slot = -1;
  bool admin = false;
  const std::array<std::string, kLobbySeats>* names = nullptr;
};

// The team a word names (`1`, `a`, `red`, `2`, `b`, `blue`, or a team's name in any case).
Team teams_named(const Teams& t, std::string_view word);
const std::string& teams_name(const Teams& t, Team team);
TeamSide& teams_side(Teams& t, Team team);
const TeamSide& teams_side(const Teams& t, Team team);

// After the seats change (once a frame): captains kept, empty teams unlocked, stale invites gone.
void teams_tick(Teams& t, const Lobby& l);
// A slot the game has seated for an accepted invite or a pick: on the team the move named, even
// in a match under way.
void teams_seat(Teams& t, Lobby& l, int slot, Team team);
// The match started or ended: invites gone; at its end the locks too.
void teams_match_began(Teams& t);
void teams_match_ended(Teams& t);
// The seated players dealt to the two teams in an order drawn from `seed`, every one unready; the
// last dealt to each team is its captain.
void teams_shuffle(Teams& t, Lobby& l, uint64_t seed);

// The words above; false when the line is not the teams'.
bool teams_command(Teams& t, Lobby& l, const TeamsCaller& who, std::string_view line, double now);

}  // namespace ge::server
