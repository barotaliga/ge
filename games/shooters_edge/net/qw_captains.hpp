// QUAKEWORLD'S CAPTAIN MODE: two captains elected by the players, then the rest picked in turn by
// number, on the engine's election (engine/server/election.hpp) and draft (engine/server/draft.hpp)
// in QuakeWorld's own words. Before a match, in a team game (teamplay on).
//
// ELECTED: a player types `captain`. Refused: in a match (nothing said), out of team play ("No
// team picking in non team mode"), fewer than 3 players ("Not enough players present"), two
// captains already ("Only 2 captains are allowed"), another election open, the 30-second wait
// after his last one, no team name ("Set your team name first"), the other captain's team name or
// colours. Otherwise the server's one election opens for him ("<name> has requested captain
// status!"): the others type `yes` (`no` takes it back), at least 2 and `k_vp_captain` percent
// (51) of the players, 60 seconds. Passed: "<name> becomes a captain", and with one "One more
// captain should be elected". `captain` again calls off his election, or resigns ("<name> is no
// longer a captain").
//
// PICKING: with both elected (and still 3 players: else "Captain modes exited"), "Both captains
// elected" and "Team picking begins": every other player loses his team, colours and skin and is
// given a number, 1, 2, 3..., shown as his frags, and is no longer ready; the captains read "Use
// numbers or impulses to choose". The one who picks first is drawn from the server's seed
// ("<captain> is picking"), then A, B, B, A...: a captain's number key (`impulse N`) picks that
// player, who takes the captain's team name and colours ("<player> set to team <team>"; to him "You
// were picked by <captain>" and "Time to go ready"). The last one goes by himself; then "All
// players chosen. Captain modes exited" and each captain reads "You are no longer a captain". "It's
// not your turn", "No such player. Pick another one". While picking nobody fires, a number key does
// nothing else, nobody waiting changes team ("You may not change team"), a joining player gets the
// lowest free number. A captain who leaves ("A captain has left") or resigns ends the picking
// ("Player picking aborted", the numbers cleared), then "1 captain present" or "No captain
// present".
//
// THE TEAM NAME at any time: no change during a match or its countdown ("You may not change team
// during game"), nor to none for a ready player in team play ("Empty team not allowed"); a
// refused change is put back in the player's own settings.
//
// A program of the server's own runs this; a mod's program keeps its own words.
//
// ge:summary QuakeWorld's captain mode: captains elected, then numbered picks in turn A, B, B, A
// ge:keywords qw-captains captain-mode team-picking captain k_vp_captain pick-order impulse-pick
// ge:keywords qw_captains_line qw_captains_impulse qw_captains_info qw_captains_frame
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_captains_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "server/draft.hpp"
#include "server/election.hpp"
#include "server/match.hpp"
#include "server/server.hpp"

namespace shooters_edge::net {

struct QwServer;

struct QwCaptains {
  std::array<int, 2> captain{-1, -1};      // the captains' slots, in the order elected
  std::array<std::string, 2> team;         // each captain's team name, when picking began
  std::array<std::string, 2> top, bottom;  // and colours
  ge::server::Draft draft;
  uint32_t fire_told = 0;  // a bit a slot told that nobody fires while picking
  uint32_t seen = 0;       // the players last frame, a bit a slot
};

// What one call needs of the game. `election` is the server's one election (any role).
struct QwCaptainsGame {
  QwServer* qw = nullptr;
  ge::server::Election* election = nullptr;
  ge::server::Lobby* lobby = nullptr;  // sv_lobby's (null: none): picking un-readies the waiting
  const ge::server::ServerConfig* settings = nullptr;  // k_vp_captain (unset: 51)
  bool team_mode = false;                              // teamplay on
  bool in_match = false;                               // a match or its countdown
  double now = 0;                                      // the server's clock, s
  uint64_t seed = 0;                                   // draws who picks first
};

// A client's line (`captain`, `yes` and `no` while an election is open): true when it was taken.
bool qw_captains_line(QwCaptains& c, const QwCaptainsGame& g, int slot, const std::string& line);
// A client's impulse: 0 when it was a pick or is swallowed while picking, else the impulse. Only a
// `fresh` one (the newest move's) picks; a repeated one is only swallowed.
uint8_t qw_captains_impulse(QwCaptains& c, const QwCaptainsGame& g, int slot, uint8_t impulse,
                            bool fresh);
// A client's move's buttons: no fire while picking.
uint8_t qw_captains_buttons(QwCaptains& c, const QwCaptainsGame& g, int slot, uint8_t buttons);
// A client's `setinfo`: false refuses a team change it may not make now (and puts his own back).
bool qw_captains_info(QwCaptains& c, const QwCaptainsGame& g, int slot, const std::string& key,
                      const std::string& value);
// Once a frame: who joined (a number while picking) and who left (a captain's or a candidate's
// end), the election's timeout and a passed captain taken in.
void qw_captains_frame(QwCaptains& c, const QwCaptainsGame& g);
// Whether team picking is under way (status lines).
bool qw_captains_picking(const QwCaptains& c);

}  // namespace shooters_edge::net
