// LAG COMPENSATION FOR HITSCAN on the classic protocol's server: a player's shot is traced
// against the other players where the shooter's screen showed them, not where they are now.
//
// THE HISTORY: each player keeps a ring of kAntilagSamples (server time, origin) samples,
// recorded after each frame his commands ran in.
// THE TARGET TIME (`antilag_target`): the server time of the frame the shooter's last packet
// acknowledged, plus 0.02 s (or one of his command frames, 1 / maxfps, when his ping is under
// 20 ms; a maxfps outside 20..1000 counts as 77), never later than now.
// A PLAYER AT THE TARGET TIME (`antilag_position`): later than his newest sample, extrapolated
// from it by his velocity times min(gap, 0.02 s); with one sample, that sample; earlier than his
// oldest, the oldest; otherwise interpolated between the samples on either side.
// THE TRACE: a trace the server marks lagged (the shooter or the shooter's owner is a player, and
// lag compensation is on) sees every other live, spawned, human player moved to his position at
// the target time; they are put back after it. Bots are never moved.
//
// ge:summary Lag compensation for hitscan: the players' origin history, the target time, rewinds
// ge:keywords antilag lag-compensation rewind hitscan history target-time extrapolate
// ge:keywords interpolate antilag_record antilag_target antilag_position lagged-trace
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_antilag_test.cpp
#pragma once

#include "shooter/vec.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;

constexpr int kAntilagSamples = 128;
constexpr double kAntilagLead = 0.02;  // s added to the acknowledged frame's time
constexpr int kAntilagPlayers = 32;

struct AntilagSample {
  double time = 0;  // the server's time, s
  sh::Vec3f origin;
};

struct AntilagRing {
  AntilagSample samples[kAntilagSamples];
  int count = 0;
  int head = 0;  // the next sample's index
};

struct AntilagHistory {
  AntilagRing players[kAntilagPlayers];
};

void antilag_record(AntilagHistory& h, int slot, double time, sh::Vec3f origin);
void antilag_clear(AntilagHistory& h, int slot);

// The time a shooter's shot is traced at.
double antilag_target(double acked_frame_time, double ping_s, int maxfps, double now);

// Where a player was at `t`; false when he has no sample.
bool antilag_position(const AntilagRing& ring, double t, sh::Vec3f velocity, sh::Vec3f& out);

}  // namespace shooters_edge::net
