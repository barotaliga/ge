// The bot's outing (outing.hpp).
//
// ge:summary outing_from, the two legs' lines and the outing's pilot
// ge:keywords outing outing_from to_lift_line from_top_line outing-pilot
// ge:skill sim
// ge:test tests/fall_line_lift_ride_test.cpp
#include "fall_line/sim/outing.hpp"

#include <cmath>
#include <cstddef>

#include "ai/path.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/mapgen/lift_line.hpp"

namespace fall_line::sim {

namespace ml = ge::mountain::lifts;
namespace mg = ge::mountain::mapgen;
namespace core = ge::core;

namespace {

// A point in the lift's line frame, in the world.
mg::PlanPoint on_line(const ml::LiftPlan& p, double u, double v) {
  return {p.bottom.x + p.dx * u + p.dz * v, p.bottom.z + p.dz * u - p.dx * v};
}

}  // namespace

void LineLayer::to(double x, double z, double width) {
  if (t_.points.empty()) {
    push(x, z, 0, width);
    return;
  }
  const mg::TrackPoint b = t_.points.back();
  const double d = core::hypot(x - b.x, z - b.z);
  if (d < kEvery / 2) return;
  const auto n = static_cast<int>(std::ceil(d / kEvery));
  for (int k = 1; k <= n; k++) {
    const double f = static_cast<double>(k) / n;
    push(b.x + (x - b.x) * f, b.z + (z - b.z) * f, b.s + d * f, width);
  }
}

mg::Track LineLayer::done() {
  auto& pts = t_.points;
  for (size_t i = 0; i < pts.size(); i++) {
    const size_t a = i == 0 ? 0 : i - 1;
    const size_t b = i + 1 >= pts.size() ? pts.size() - 1 : i + 1;
    if (a != b) pts[i].heading = core::libm::atan2(pts[b].x - pts[a].x, pts[b].z - pts[a].z);
  }
  t_.length = pts.empty() ? 0 : pts.back().s;
  return std::move(t_);
}

void LineLayer::push(double x, double z, double s, double width) {
  t_.points.push_back(mg::TrackPoint{x, z, level_.ground_at(x, z), s, 0, width});
}

Outing outing_from(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts, int32_t first) {
  Outing o;
  o.first = first;
  if (!level.resort || first < 0) return o;
  const mg::Track chain = run_chain(level, first);
  if (chain.points.empty()) return o;
  const mg::TrackPoint foot = chain.points.back();
  // THE GRAPH: a node for every run, then one for every lift (`runs + p`, by its plan's index),
  // and last his own run again, come back to off a lift.
  const auto runs = static_cast<int32_t>(level.resort->runs.size());
  const auto again = runs + static_cast<int32_t>(lifts.size());
  ge::ai::Search search(static_cast<size_t>(again) + 1);
  search.seed(first);
  const auto expand = [&](int32_t node) {
    if (node == first) {
      // From his run's foot, across the snow to every lift with a way off its top in reach.
      for (size_t p = 0; p < lifts.size(); p++) {
        const ml::LiftPlan& plan = lifts[p];
        const double d = core::hypot(plan.bottom.x - foot.x, plan.bottom.z - foot.z);
        const auto& ramps = level.resort->lifts[static_cast<size_t>(plan.lift)].ramps;
        const auto lift = runs + static_cast<int32_t>(p);
        if (d < outing::kLiftReach && !ramps.empty() && search.improves(lift, d)) {
          search.reach(lift, d, node);
        }
      }
      return;
    }
    if (node < runs) return;
    // From a lift's top, onto every run off its ramps: the first ramp's dearest by nothing, the
    // way back down the run he came down by `kBackDown`.
    const ml::LiftPlan& plan = lifts[static_cast<size_t>(node - runs)];
    double rank = 0;
    for (const mg::SummitRamp& r : level.resort->lifts[static_cast<size_t>(plan.lift)].ramps) {
      const int32_t k = run_index(level, r.run);
      if (k < 0) continue;
      const double cost =
          search.cost(node) + rank * outing::kRampRank + (k == first ? outing::kBackDown : 0);
      rank++;
      const int32_t to = k == first ? again : k;
      if (search.improves(to, cost)) search.reach(to, cost, node);
    }
  };
  // Done at the first run reached off a lift: the cheapest outing.
  const int32_t to = search.run(
      expand, [&](int32_t node) { return node == again || (node < runs && node != first); });
  if (to < 0) return o;
  o.next = to == again ? first : to;
  o.lift = lifts[static_cast<size_t>(search.prev(to) - runs)].lift;
  return o;
}

mg::Track to_lift_line(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts,
                       const Outing& o) {
  LineLayer line(level);
  for (const mg::TrackPoint& p : run_chain(level, o.first).points) line.to(p.x, p.z, p.width);
  const ml::LiftPlan& plan = lifts[static_cast<size_t>(o.lift)];
  const ml::LiftRide::Entry& e = plan.ride.entry;
  for (const double u : {e.at - outing::kLineUp, e.at, e.at + outing::kRunOn}) {
    const mg::PlanPoint at = on_line(plan, u, e.side);
    line.to(at.x, at.z, outing::kWalkWidth);
  }
  return line.done();
}

mg::Track from_top_line(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts,
                        const Outing& o) {
  LineLayer line(level);
  if (!level.resort) return line.done();
  const mg::ResortMap& resort = *level.resort;
  const ml::LiftPlan& plan = lifts[static_cast<size_t>(o.lift)];
  const mg::LetGo off = mg::let_go_of(plan.kind, plan.bottom, plan.top);
  line.to(off.x, off.z, outing::kWalkWidth);
  const mg::Run& next = resort.runs[static_cast<size_t>(o.next)];
  double from = 0;
  for (const mg::SummitRamp& r : resort.lifts[static_cast<size_t>(o.lift)].ramps) {
    if (r.run != next.id) continue;
    line.to(r.from.x, r.from.z, r.width);
    line.to(r.to.x, r.to.z, r.width);
    from = r.to.s;
    break;
  }
  for (const mg::TrackPoint& p : run_chain(level, o.next).points) {
    if (p.s > from) line.to(p.x, p.z, p.width);
  }
  return line.done();
}

OutingPilot::OutingPilot(const GameState& state, const Outing& o) {
  const mg::Level& level = *state.level;
  const auto& lifts = *state.lifts;
  const double foot = run_chain(level, o.first).length;
  up_ = plan_line(level, to_lift_line(level, lifts, o), state.spec, foot, outing::kWalkSpeed);
  up_.to_lift = true;
  down_ = plan_line(level, from_top_line(level, lifts, o), state.spec);
}

Controls OutingPilot::operator()(const GameState& state, int32_t i) const {
  const FreeRide& f = state.racers[static_cast<size_t>(i)].free;
  return bot_controls(state, i, f.lift.ridden > 0 ? down_ : up_);
}

}  // namespace fall_line::sim
