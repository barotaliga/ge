// The ski-area build's fifth stage: the ramps off the tops, and access as the pressed runs stand.
//
// ge:summary Stage 5 of the ski-area build: the ramps laid, the drags settled, the stations cleared
// ge:keywords resort-access ramps measured-access settle-drags clear-stations regrade-leg
// ge:keywords drag-tops
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/access_build.hpp"
#include "mountain/mapgen/drags.hpp"
#include "mountain/mapgen/lift_chain.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/station_clear.hpp"
#include "mountain/mapgen/summit_ramps.hpp"

namespace ge::mountain::mapgen {

namespace RR = resort_rules;
using ge::core::sample_field;

StageResult resort_access(ResortAttempt& a) {
  const ge::core::Heightfield& ground = a.ground;
  // Its own answer a call: the ramps' lines are walked on several threads at once.
  const RunCover cover = [&](double x, double z, double past) {
    NetHit hit;
    return a.net.covers(x, z, past, hit) ? hit.run : -1;
  };
  const HeightAt height = [&](double x, double z) { return sample_field(ground, x, z); };
  // ── THE RAMPS OFF THE TOPS (R26), off a run's snow and a station: on an area whose pads lean,
  // whose starts were placed under them with room for one.
  std::vector<RampRun> runs;
  if (a.shape.lean > 0) {
    runs.reserve(a.built.size());
    for (const BuiltRun& b : a.built) runs.push_back({&b.walked.spec, &b.walked.track.points});
  }
  std::function<double(double, double)> way;
  if (a.way) way = [w = *a.way](double x, double z) { return off_queue(w, x, z); };
  const RampFit fit = ramp_fit(a);
  a.ramps = lay_ramps(a.ground, a.pads, runs, cover, a.lift_plans, way, fit);

  // ── ACCESS AS THE RUNS MEASURE (R29): a piste the mountain made gentler than it was built may
  // now merge into a harder run, and one left out may have carried a skier home; a drag lift
  // brings it back, or the attempt is refused.
  std::vector<PlanRun> measured;
  measured.reserve(a.built.size());
  for (const BuiltRun& b : a.built) {
    const WalkedRun& w = b.walked;
    const bool road = w.spec.kind == RunKind::road;
    measured.push_back({w.spec.kind, road ? 0 : static_cast<int>(b.grade), w.spec.from,
                        &w.track.points, w.track.length, w.into, w.merge_start, w.spec.branch,
                        w.spec.to, !w.into && !w.spec.to});
  }
  const double base_z = a.plan.base_z;
  DragGround pressed;
  pressed.height = height;
  // Its own answer a call: the stations' readings run on several threads at once.
  pressed.piste = [&](double x, double z) {
    NetHit near;
    a.net.nearest(
        x, z, kWidest,
        [&](int r, double) { return a.built[size_t(r)].walked.spec.kind == RunKind::road; }, near);
    return PisteNear{near.distance, near.width, near.heading};
  };
  pressed.floor = [base_z](PlanPoint p) { return p.z >= base_z - 1; };
  pressed.slack = a.face ? RR::massif::real::least::harder : 0;
  const DragsSettled settled = settle_drags(
      pressed, measured, a.lift_plans, [&](size_t i) { return a.built[i].walked.spec.id; },
      [&](const std::string& id) {
        return std::any_of(a.built.begin(), a.built.end(),
                           [&](const BuiltRun& b) { return b.walked.spec.to == id; });
      });
  if (settled.refused) return settled.refused;
  a.settled_drags = settled.drags;
  a.settled_home = settled.home;
  for (BuiltRun& b : a.built) {
    std::optional<std::string>& to = b.walked.spec.to;
    if (!to) continue;
    const auto it = settled.renamed.find(*to);
    if (it != settled.renamed.end()) to = it->second;
  }

  // ── THE STATIONS BESIDE THE RUNS (R26) ──
  StationGround beside;
  beside.on_run = [&](double x, double z, double pad) {
    NetHit on;
    return a.net.covers(x, z, pad, on);
  };
  beside.on_piste = [&](double x, double z, double pad) {
    const PisteNear near = pressed.piste(x, z);
    return near.distance < near.width / 2 + pad;
  };
  beside.height = height;
  beside.floor = pressed.floor;
  // A real face's bottom station may stand on ground falling more across its footprint.
  if (a.face) beside.level = RR::massif::real::least::level;
  beside.link_ends = [&](const std::string& id) {
    std::vector<PlanPoint> ends;
    for (const BuiltRun& b : a.built) {
      if (b.walked.spec.to != id) continue;
      const TrackPoint& last = b.walked.track.points.back();
      ends.push_back({last.x, last.z});
    }
    return ends;
  };
  const bool chained = a.way.has_value();
  const std::optional<std::string> stuck =
      clear_stations(beside, a.lift_plans, [&](std::span<const LiftPlan> ls) {
        const PlanAccess v = reckon_access(measured, plan_lifts(pressed, ls), pressed.slack);
        return std::all_of(v.ok.begin(), v.ok.end(), [](bool ok) { return ok; }) &&
               v.orphans.empty() && (!chained || keeps_chain(ls, height, !a.face));
      });
  if (stuck) return "R26: " + *stuck + " stands on a run wherever its station is moved";
  if (chained) {
    a.way = chain_of(a.lift_plans);
    regrade_leg(a.ground, a.lift_plans, a.pads, a.ramps, a.face != nullptr);
  }

  // ── THE RAMPS OFF THE DRAGS' TOPS (R26), where they settled ──
  if (a.shape.lean > 0) {
    std::vector<std::string> froms;
    froms.reserve(runs.size());
    for (const RampRun& r : runs) froms.push_back(r.spec->from);
    a.drag_tops = drag_tops_of(a.lift_plans, froms, height);
    for (auto& [id, off] :
         lay_ramps(a.ground, a.drag_tops, runs, cover, a.lift_plans, nullptr, fit)) {
      a.ramps[id] = std::move(off);
    }
  }
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen
