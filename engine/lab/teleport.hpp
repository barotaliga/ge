// TELEPORTS: the steps of an actor's track that move it farther than its speed allows. A lab that
// follows anything that moves (a racer, a crowd member, a machine, a rider handed from a lift to
// the snow) gives the track's samples, the time, the position and the speed each sample says the
// actor has, and gets back every step whose distance is more than the faster of its two ends'
// speeds carries over the step (times a slack, plus a floor for rounding). A handoff that sets a
// position without a matching velocity, a respawn, a wrapped index: each is one row here, at its
// step and time, with how far it went against how far it could have.
//
//   const auto jumps = gl::find_teleports(track);
//   gl::report_teleports(lab, "crowd.k3", jumps);  // a metric, a note a jump, a failure if asked
//
// ge:summary Teleports: the steps of a track that move farther than the speed allows
// ge:keywords teleport jump discontinuity track position speed handoff respawn snap find_teleports
// ge:keywords report_teleports lab metric
// ge:skill labs debug
// ge:test tests/lab_teleport_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "lab/lab.hpp"

namespace ge::lab {

// One sample of a track: when, where, and how fast the actor says it is going (m/s, |v| with the
// vertical in it).
struct TrackSample {
  double t, x, y, z, speed;
};

struct Teleport {
  size_t step;     // the sample it arrived at (from 1)
  double t;        // s, that sample's time
  double moved;    // m, how far it went since the sample before
  double allowed;  // m, how far its speed could have carried it
};

// Every step whose distance is over max(speed before, speed after) * dt * slack + floor_m.
std::vector<Teleport> find_teleports(const std::vector<TrackSample>& track, double slack = 1.5,
                                     double floor_m = 0.05);

// `<key>.teleports` as a metric, one note a teleport (the first `most`), and a lab failure when
// `fail` is set and there is any (for a lab whose subject must never jump).
void report_teleports(Lab& lab, const std::string& key, const std::vector<Teleport>& jumps,
                      bool fail = false, size_t most = 8);

}  // namespace ge::lab
