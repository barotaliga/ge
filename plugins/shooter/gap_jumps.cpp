// ge:summary The gap-jump reader: hops over a pit or a chasm, their speed against a run's reach
// ge:keywords gap-jump speed-jump read_gap_jumps jump_reach pit chasm
// ge:skill shooter bots
// ge:test tests/shooter_gap_jumps_test.cpp
#include "shooter/gap_jumps.hpp"

#include <cmath>

#include "shooter/hull.hpp"

namespace ge::shooter {

namespace {

// The player's box traced down from `p` through the world and every model over that spot (as
// many as a move world holds; a column rarely has more than a few).
HullTrace trace_down(const MoveWorld& world, std::span<const BrushModel> models, Vec3f p,
                     float deep) {
  MoveWorld w;
  w.solids[0] = world.solids[0];
  w.count = 1;
  for (const BrushModel& m : models) {
    if (w.count >= kMaxSolids) break;
    if (m.mins.x > p.x + kPlayerMaxs.x || m.maxs.x < p.x + kPlayerMins.x ||
        m.mins.y > p.y + kPlayerMaxs.y || m.maxs.y < p.y + kPlayerMins.y || m.mins.z > p.z ||
        m.maxs.z < p.z - deep + kPlayerMins.z)
      continue;
    Solid& s = w.solids[w.count++];
    s = Solid{};
    s.kind = SolidKind::brush;
    s.point_hull = m.hulls[0];
    s.clip_hull = m.hulls[1];
    s.mins = m.mins;
    s.maxs = m.maxs;
  }
  return player_trace(w, p, {p.x, p.y, p.z - deep});
}

}  // namespace

float jump_time(float height, float jump_speed, float gravity) {
  const float disc = jump_speed * jump_speed - 2 * gravity * height;
  if (disc < 0) return 0;
  return (jump_speed + std::sqrt(disc)) / gravity;
}

float jump_reach(float speed, float height, float jump_speed, float gravity) {
  return speed * jump_time(height, jump_speed, gravity);
}

void read_gap_jumps(std::span<const TrackPoint> track, const HopReading& hops,
                    const MoveWorld& world, std::span<const BrushModel> models,
                    std::span<const std::pair<int32_t, int32_t>> blasts,
                    const GapJumpReadSettings& set, std::vector<GapJump>& out) {
  for (const Hop& h : hops.hops) {
    bool blast = false;
    for (const auto& [first, last] : blasts)
      blast = blast || (h.land >= first && h.takeoff <= last);
    if (blast) continue;
    if (h.speed_off < set.least_speed || h.speed_land > h.speed_off + set.boost) continue;
    const Vec3f from = track[static_cast<size_t>(h.takeoff) - 1].origin;
    const Vec3f to = track[static_cast<size_t>(h.land)].origin;
    const float across =
        std::sqrt((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
    if (across < set.least_across) continue;
    // A flight no longer than a jump's fall: else it walked a floor the hops did not see.
    const float fall_s = jump_time(to.z - from.z, set.jump_speed, set.gravity);
    if (fall_s <= 0 || h.air_s > fall_s + set.air_slack) continue;
    // The gap: the deepest floor under the flight, against the lower end.
    const float low = from.z < to.z ? from.z : to.z;
    float depth = 0;
    bool harm = false;
    for (int32_t f = h.takeoff; f < h.land; f++) {
      const Vec3f p = track[static_cast<size_t>(f)].origin;
      const HullTrace down = trace_down(world, models, p, set.deep);
      if (down.startsolid) continue;
      const float floor = down.fraction < 1 ? down.endpos.z : p.z - set.deep;
      if (low - floor <= depth) continue;
      depth = low - floor;
      const int c =
          water_contents(world, {down.endpos.x, down.endpos.y, down.endpos.z + kPlayerMins.z + 2});
      harm = down.fraction < 1 && (c == kContentsLava || c == kContentsSlime);
    }
    if (depth < set.gap) continue;
    GapJump j;
    j.takeoff = h.takeoff;
    j.land = h.land;
    j.t = track[static_cast<size_t>(h.takeoff)].t;
    j.from = from;
    j.to = to;
    j.speed = h.speed_off;
    j.heading = hops.frames[static_cast<size_t>(h.takeoff)].heading;
    j.across = across;
    j.height = to.z - from.z;
    j.air_s = h.air_s;
    j.depth = depth;
    j.harm = harm;
    j.run_reach = jump_reach(set.run, j.height, set.jump_speed, set.gravity);
    j.speed_needed = across > j.run_reach;
    out.push_back(j);
  }
}

}  // namespace ge::shooter
