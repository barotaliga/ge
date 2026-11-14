// A spectator following the play (classic_follow.hpp).
//
// ge:summary A spectator following the play: the choice, the server told, the move and teleport
// ge:keywords follow ptrack tmove track
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_follow_test.cpp
#include "shooters_edge/net/classic_follow.hpp"

#include <string>

#include "shooters_edge/net/protocol.hpp"

namespace shooters_edge::net {

namespace {

// The top fragger in the game (-1: nobody); a tie goes to the lowest slot.
int top_fragger(const ClassicClientEnd& c) {
  int best = -1;
  for (int k = 0; k < 32; k++) {
    if (!follow_candidate(c, k)) continue;
    if (best < 0 || c.frags[k] > c.frags[best]) best = k;
  }
  return best;
}

}  // namespace

bool follow_candidate(const ClassicClientEnd& c, int k) {
  // By the user info alone: a player out of the spectator's sight is sent no record until the
  // server is told to track him, and one who leaves has his name cleared.
  if (k < 0 || k >= 32 || k == c.playernum) return false;
  if (info_get(c.userinfos[k], "name").empty()) return false;
  const std::string spec = info_get(c.userinfos[k], "*spectator");
  return spec.empty() || spec == "0";
}

void follow_tick(ClassicFollow& f, ClassicClientEnd& c) {
  if (c.stage != ClientStage::active) return;
  const bool in = follow_candidate(c, f.slot);
  if (in && c.player_seen[f.slot] && c.players[f.slot].dead) {
    if (f.died_at < 0) f.died_at = c.now;
  } else {
    f.died_at = -1;
  }
  const int top = top_fragger(c);
  // Gone, or dead long enough: the top fragger at once; else one with more frags after the dwell.
  const bool lost = !in || (f.died_at >= 0 && c.now - f.died_at >= f.deathwait_s);
  const bool beaten =
      in && top >= 0 && c.frags[top] > c.frags[f.slot] && c.now - f.since >= f.dwell_s;
  const int want = lost || beaten ? top : f.slot;
  if (want != f.slot) {
    if (f.slot >= 0 && want >= 0) f.switches++;
    f.slot = want;
    f.since = c.now;
    f.died_at = -1;
    f.moved = false;
  }
  if (f.slot != f.told) {
    client_command(c, f.slot >= 0 ? "ptrack " + std::to_string(f.slot) : "ptrack");
    f.told = f.slot;
  }
}

sh::MoveCmd follow_command(ClassicFollow& f, ClassicClientEnd& c) {
  sh::MoveCmd cmd;
  if (f.slot < 0 || !c.player_seen[f.slot]) return cmd;
  const PlayerState& p = c.players[f.slot];
  if (p.has_command) cmd.angles = p.command.angles;
  if (!f.moved ||
      !(p.origin.x == f.moved_to.x && p.origin.y == f.moved_to.y && p.origin.z == f.moved_to.z)) {
    c.tmove = true;
    c.tmove_to = p.origin;
    f.moved_to = p.origin;
    f.moved = true;
  }
  return cmd;
}

}  // namespace shooters_edge::net
