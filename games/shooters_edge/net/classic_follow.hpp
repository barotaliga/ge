// A SPECTATOR FOLLOWING THE PLAY (docs/tv.md § The watcher): the camera a scripted classic client
// (classic_client.hpp) rides when it watches a match for a broadcast, so its recording plays back
// as one player's view rather than a spectator standing at the start.
//
// WHOM: the top fragger among the players in the game (named in their user info and not
// spectators, whether in sight or not; a tie goes to the lowest slot). The tracked player is kept
// for at least `dwell_s` and while he leads; once he has been dead `deathwait_s` (the death shown
// first), left, or turned spectator, the camera moves to the top fragger at once. A player with
// strictly more frags takes it after the dwell.
//
// HOW: the server is told (`ptrack <slot>` on the reliable stream, a bare `ptrack` with nobody to
// follow), so it sends the tracked player's stats, his weapon frame and his record wherever he is;
// each move the client sends no movement, the tracked player's own view angles, and a teleport
// (`tmove`) to his origin whenever he has moved, so what the server sends it is what he sees. A
// recording of it is a first-person recording of him (demo.hpp's tracking spectator).
//
//   ClassicFollow f;   cfg.brain = [&] { return follow_command(f, client); };
//   every tick, before client_tick: follow_tick(f, client)
//
// ge:summary A spectator following the play: the top fragger tracked by ptrack, tmove and his view
// ge:keywords follow spectator ptrack tmove track camera watcher top-fragger dwell deathwait
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_follow_test.cpp
#pragma once

#include "shooter/pmove.hpp"
#include "shooters_edge/net/classic_client.hpp"

namespace shooters_edge::net {

struct ClassicFollow {
  double dwell_s = 4;      // least time on a player before a better one takes the camera
  double deathwait_s = 2;  // a dead player stays on screen this long
  int slot = -1;           // the player followed (-1: nobody)
  double since = 0;        // when the camera came to him, the network's time, s
  double died_at = -1;     // when he was last seen dead (-1: alive)
  int told = -1;           // the slot the server was last told (-1: none, -2: never told)
  sh::Vec3f moved_to;      // the last teleport sent
  bool moved = false;
  int switches = 0;  // times the camera moved to another player
};

// Whether slot `k` is a player in the game, as the client last heard.
bool follow_candidate(const ClassicClientEnd& c, int k);
// The camera's choice for this tick, and the server told when it changed. Does nothing until the
// client is active.
void follow_tick(ClassicFollow& f, ClassicClientEnd& c);
// The move to send: no movement, the followed player's view angles; and the teleport to his origin
// queued on the client when he has moved.
sh::MoveCmd follow_command(ClassicFollow& f, ClassicClientEnd& c);

}  // namespace shooters_edge::net
