// Quake II's spectators in deathmatch (q2_server.hpp states the rules): the `spectator` wish a
// player's user info carries, the switch to or from watching 5 s after his last respawn with its
// passwords, limit and words, and the chase camera a spectator rides behind a player.
// q2_server.cpp, q2_commands.cpp and q2_bar.cpp call these; the match (net/q2_match.cpp) ends a
// coach's chase off his team.
//
// ge:summary Quake II's spectators: the spectator wish, the switch mid-match, the chase camera
// ge:keywords q2 spectator spectating chase chase-camera sidelines joined-the-game password
// ge:keywords maxspectators spectator-password stat-chase invnext invprev
// ge:skill shooter
// ge:test tests/shooters_edge_q2_spectate_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace shooters_edge::server {

struct Q2Server;

// Player `slot`'s wish from his `spectator` user info: in deathmatch only, any text but "" or
// "0" (the text is also the spectators' password when one is set).
bool spectate_wish(const Q2Server& s, int slot);

// Player `slot` kept on the sidelines (`held`) or let go: while held his wish is to watch, whatever
// his user info says; let go, his user info's wish again. The switch is the frame's, as any other.
void spectate_hold(Q2Server& s, int slot, bool held);

// The 10 Hz frame's switches, before anything else moves: each player in deathmatch whose wish is
// not what he is, 5 s after his respawn mark, outside the intermission. A refusal (a wrong
// password, the limit full) tells him why and sets his user info back; an accepted switch resets
// his score, says so to everyone and returns him to be placed again (`placed` per slot).
void spectate_frame(Q2Server& s, bool placed[]);

// A spectator's command: attack's press starts or ends a chase, the up move's press starts one or
// steps to the next player. True when he is chasing, so his own move is not run.
bool spectate_command(Q2Server& s, int slot, const ge::shooter::MoveCmd& c);

// Player `slot`'s chase ended: he flies free from where the camera was.
void spectate_stop(Q2Server& s, int slot);

// `invnext` (`dir` 1) and `invprev` (-1) while chasing: the next or previous player. False when
// he is not chasing (the command then selects items as usual).
bool spectate_step(Q2Server& s, int slot, int dir);

// After player `slot`'s command: every chase camera on him is placed behind his view (one on a
// player who has become a spectator steps on, or ends when there is no one else).
void spectate_follow(Q2Server& s, int slot);

// A chaser's stats (q2_bar.cpp): his target's, with the chase's name and the spectator flag.
void spectate_stats(const Q2Server& s, int slot, int16_t* stats);

}  // namespace shooters_edge::server
