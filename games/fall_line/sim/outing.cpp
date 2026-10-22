// The bot's outing (outing.hpp).
//
// ge:summary outing_from, the two legs' lines and the outing's pilot
// ge:keywords outing outing_from to_lift_line from_top_line outing-pilot
// ge:skill sim
// ge:test tests/fall_line_lift_ride_test.cpp
#include "fall_line/sim/outing.hpp"

#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/mapgen/lift_line.hpp"

namespace fall_line::sim {

namespace ml = ge::mountain::lifts;
namespace mg = ge::mountain::mapgen;
namespace core = ge::core;

namespace {

// Lays a line through points on the snow: every `kEvery` m between them, its arc, its height on
// the ground and its heading; a point under half a step from the last is dropped.
class LineLayer {
 public:
  static constexpr double kEvery = 2;
  explicit LineLayer(const mg::Level& level) : level_(level) {}

  void to(double x, double z, double width) {
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

  mg::Track done() {
    auto& pts = t_.points;
    for (size_t i = 0; i < pts.size(); i++) {
      const size_t a = i == 0 ? 0 : i - 1;
      const size_t b = i + 1 >= pts.size() ? pts.size() - 1 : i + 1;
      if (a != b) pts[i].heading = core::libm::atan2(pts[b].x - pts[a].x, pts[b].z - pts[a].z);
    }
    t_.length = pts.empty() ? 0 : pts.back().s;
    return std::move(t_);
  }

 private:
  void push(double x, double z, double s, double width) {
    t_.points.push_back(mg::TrackPoint{x, z, level_.ground_at(x, z), s, 0, width});
  }
  const mg::Level& level_;
  mg::Track t_;
};

// A point in the lift's line frame, in the world.
mg::PlanPoint on_line(const ml::LiftPlan& p, double u, double v) {
  return {p.bottom.x + p.dx * u + p.dz * v, p.bottom.z + p.dz * u - p.dx * v};
}

}  // namespace

Outing outing_from(const mg::Level& level, const std::vector<ml::LiftPlan>& lifts, int32_t first) {
  Outing o;
  o.first = first;
  if (!level.resort || first < 0) return o;
  const mg::Track chain = run_chain(level, first);
  if (chain.points.empty()) return o;
  const mg::TrackPoint foot = chain.points.back();
  double best = outing::kLiftReach;
  for (const ml::LiftPlan& p : lifts) {
    const double d = core::hypot(p.bottom.x - foot.x, p.bottom.z - foot.z);
    const auto& ramps = level.resort->lifts[static_cast<size_t>(p.lift)].ramps;
    if (d < best && !ramps.empty()) {
      best = d;
      o.lift = p.lift;
    }
  }
  if (o.lift < 0) return o;
  for (const mg::SummitRamp& r : level.resort->lifts[static_cast<size_t>(o.lift)].ramps) {
    const int32_t k = run_index(level, r.run);
    if (k < 0) continue;
    if (o.next < 0 || (o.next == first && k != first)) o.next = k;
  }
  if (o.next < 0) o.lift = -1;
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
