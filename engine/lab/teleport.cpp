// See teleport.hpp.
//
// ge:summary find_teleports and report_teleports: a track's jumps found and reported
// ge:keywords teleport jump track speed find_teleports report_teleports
// ge:skill labs debug
// ge:test tests/lab_teleport_test.cpp
#include "lab/teleport.hpp"

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::lab {

std::vector<Teleport> find_teleports(const std::vector<TrackSample>& track, double slack,
                                     double floor_m) {
  std::vector<Teleport> out;
  for (size_t i = 1; i < track.size(); i++) {
    const TrackSample& a = track[i - 1];
    const TrackSample& b = track[i];
    const double moved = core::hypot3(b.x - a.x, b.y - a.y, b.z - a.z);
    const double allowed =
        core::num::max(a.speed, b.speed) * core::num::max(0.0, b.t - a.t) * slack + floor_m;
    if (moved > allowed) out.push_back({i, b.t, moved, allowed});
  }
  return out;
}

void report_teleports(Lab& lab, const std::string& key, const std::vector<Teleport>& jumps,
                      bool fail, size_t most) {
  lab.metric(key + ".teleports", static_cast<double>(jumps.size()), "count");
  for (size_t i = 0; i < jumps.size() && i < most; i++) {
    const Teleport& j = jumps[i];
    lab.note(key + ": teleport at step " + std::to_string(j.step) + " (t " +
             core::num::to_fixed(j.t, 3) + " s): moved " + core::num::to_fixed(j.moved, 2) +
             " m where its speed allows " + core::num::to_fixed(j.allowed, 2) + " m");
  }
  if (jumps.size() > most) {
    lab.note(key + ": " + std::to_string(jumps.size() - most) + " more teleports");
  }
  if (fail && !jumps.empty()) {
    const Teleport& j = jumps.front();
    lab.fail(key + " teleported " + std::to_string(jumps.size()) + " times, first at t " +
                 core::num::to_fixed(j.t, 3) + " s (" + core::num::to_fixed(j.moved, 2) +
                 " m where " + core::num::to_fixed(j.allowed, 2) + " m was possible)",
             "Find what set its position without its velocity at that step (a handoff, a "
             "respawn, a snap); skill: debug.");
  }
}

}  // namespace ge::lab
