// The ski-area build's sixth stage: the mountain's own features round the runs, the hub and its
// wind tunnels, and everything groomed that a skier walks or rides off.
//
// ge:summary Stage 6 of the ski-area build: off-piste features, crust, hub, grooming, tunnels
// ge:keywords resort-features off-kickers cliffs crust hub wind-tunnels groom distance-to
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include <vector>

#include "core/heightfield.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/hub.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/lift_chain.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/station_pad.hpp"
#include "mountain/mapgen/summit_ramps.hpp"
#include "mountain/mapgen/surface.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::hypot;

namespace {

// How far past a run's edge a ramp's grooming stops, m (the powder beside a run, R10).
constexpr double kRampGroom = rules::track::shoulder_packed + 4;
// How far round a point the nearest run is looked for, m.
constexpr double kFeatureReach = 400;

}  // namespace

StageResult resort_features(ResortAttempt& a) {
  NetHit hit;
  const auto never = [](int, double) { return false; };
  std::vector<const SummitRamp*> ramps;
  for (const auto& [lift, off] : a.ramps) {
    for (const SummitRamp& r : off) ramps.push_back(&r);
  }
  // Read as a race piste's distance from its line (R4, R22 keep off one by their clearance past
  // its half-width) where a run is wider, and from its centreline where it is narrower; a pad
  // and the ground eased into it, a ramp and the way to the next queue kept off as a run is.
  const DistanceTo distance_to = [&](double x, double z) {
    a.net.nearest(x, z, kFeatureReach, never, hit);
    double d = hit.distance - num::max(0.0, hit.width - rules::track::width.max) / 2;
    for (const StationPad& p : a.pads) {
      d = num::min(d, hypot(x - p.x, z - p.z) - p.r - resort_rules::lift::pad_blend);
    }
    for (const SummitRamp* r : ramps) d = num::min(d, off_ramp(*r, x, z));
    if (a.way) d = num::min(d, off_way(*a.way, x, z));
    return d;
  };
  a.off_kickers = lay_off_kickers(a.rng, a.plan, a.ground, distance_to);
  std::vector<Cliff> drops;
  for (const BuiltRun& b : a.built) drops.insert(drops.end(), b.drops.begin(), b.drops.end());
  const Track no_track{};
  a.cliffs = lay_cliffs(a.sub, a.plan, a.ground, no_track, a.off_kickers, drops, distance_to);
  a.crust = lay_crust(a.sub, *a.region, a.ground);
  if (a.crust) fold_surface(a.packed, a.stamp.dist, *a.region, *a.crust);

  // ── THE HUB AND ITS WIND TUNNELS (R29, R30): round every finish on the floor and every valley
  // bottom station.
  const double base_z = a.plan.base_z;
  std::vector<PlanPoint> floor;
  for (const BuiltRun& b : a.built) {
    const WalkedRun& w = b.walked;
    if (w.into || w.spec.to) continue;
    const TrackPoint& end = w.track.points.back();
    floor.push_back({end.x, end.z});
  }
  for (const LiftPlan& l : a.lift_plans) {
    if (l.bottom.z >= base_z - 1) floor.push_back(l.bottom);
  }
  a.hub = plan_hub(a.plan, a.ground, floor, a.sub);
  const std::vector<float> unhubbed = a.packed.data;
  groom_hub(a.hub.hub, a.packed);
  // A drift the hub's grooming reaches into is groomed over there, the run's whole width, and cut
  // back to the fresh snow left of it (R17, R29).
  for (BuiltRun& b : a.built) trim_drifts(b.walked.track.points, b.drifts, a.packed, unhubbed);
  // The pads, a drag's top among them (the ground its rider is let go on, R26), the way and the
  // ramps groomed, never over a run's snow.
  std::vector<StationPad> pads = a.pads;
  pads.insert(pads.end(), a.drag_tops.begin(), a.drag_tops.end());
  const OnRun run_at = [&](double x, double z) { return a.net.covers(x, z, 0, hit); };
  groom_pads(pads, a.packed, run_at);
  if (a.way) groom_way(*a.way, a.packed, run_at);
  std::vector<RampRun> runs;
  if (a.shape.lean > 0) {
    runs.reserve(a.built.size());
    for (const BuiltRun& b : a.built) runs.push_back({&b.walked.spec, &b.walked.track.points});
  }
  const RunCover cover = [&](double x, double z, double past) {
    return a.net.covers(x, z, past, hit) ? hit.run : -1;
  };
  groom_ramps(a.packed, a.ramps, pads, runs, cover, kRampGroom);
  a.tunnels = lay_tunnels(a.hub, a.ground);
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen
