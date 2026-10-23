// The skier's pilot (plugins/skier/pilot.hpp) on the hand-built slope, with no game around it:
// its plan eases under the kicker's speed up to the lip and never asks for more braking than it
// planned on; flown down the piste it stays on the snow and on the piste, takes the kicker at
// the planned speed and holds its lane; and it decides the same with its record on or off,
// writing its `line` and `speed` decisions every step.
//
// ge:summary Test: the skier's pilot on the slope: its plan, its run, its lane, its record
// ge:keywords test skier pilot plan pursuit lane kicker lip brake why decisions synthetic slope
// ge:skill skier bots testing
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "ai/why.hpp"
#include "check.hpp"
#include "mountain/mapgen/query.hpp"
#include "skier/pilot.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace {

namespace sk = ge::skier;
namespace mg = ge::mountain::mapgen;
constexpr double kDt = 1.0 / 120;
constexpr double kLip = sk::slope::kKickerLip - 10;  // the lip's arc down the slope's piste

// The slope's plan along its own piste, its one kicker's lip by its arc.
sk::PilotPlan slope_plan(const mg::Level& level) {
  sk::PilotPlan plan;
  plan.track = std::shared_ptr<const mg::Track>(std::shared_ptr<const mg::Track>{}, &level.track);
  const std::vector<double> lips{kLip};
  sk::lay_pilot_plan(level, sk::kReferenceSkis, lips, plan);
  return plan;
}

// What a run down the slope as `rider` saw: his speed at the lip, the furthest off the piste's
// line, the mean offset on the straight below the rollers, whether he stayed up.
struct Flight {
  double at_lip = 0;
  double widest = 0;
  double mean_offset = 0;
  bool up = true;
};

Flight fly(const mg::Level& level, const sk::PilotPlan& plan, const sk::PilotRider& rider) {
  const sk::TreeIndex trees = sk::build_tree_index(level);
  const sk::World world{&level, &trees};
  const mg::TrackPoint& start = level.track.points.front();
  sk::SkierState s = sk::stand_skier(world, start.x, start.z + 4, 0, 2);
  Flight f;
  double sum = 0;
  int n = 0;
  bool crossed = false;
  for (int i = 0; i < 120 * 90 && s.z < level.track.points.back().z - 30; i++) {
    sk::SkierEvents ev;
    sk::step_skier(s, sk::skier_pilot(s, sk::kReferenceSkis, plan, rider), world,
                   sk::kReferenceSkis, kDt, ev);
    const double off = s.x - sk::slope::kCentre;
    f.widest = std::fmax(f.widest, std::fabs(off));
    if (s.z > 280 && s.z < 360) {
      sum += off;
      n++;
    }
    if (!crossed && s.z >= sk::slope::kKickerLip - 1) {
      f.at_lip = s.speed;
      crossed = true;
    }
    f.up = f.up && !s.down;
  }
  f.mean_offset = n > 0 ? sum / n : NAN;
  return f;
}

}  // namespace

TEST(the_plan_eases_under_the_kicker_speed_to_the_lip_and_brakes_only_as_planned) {
  const mg::Level level = sk::synthetic_slope();
  const sk::PilotPlan plan = slope_plan(level);
  const auto& pts = level.track.points;
  CHECK(plan.speed.size() == pts.size() && plan.curvature.size() == pts.size());
  double before_lip = 0, open = 0;
  for (size_t i = 0; i + 1 < pts.size(); i++) {
    if (pts[i].s > kLip - sk::pilot::kKickerReach && pts[i].s <= kLip) {
      before_lip = std::fmax(before_lip, plan.speed[i]);
    }
    if (pts[i].s > kLip + 40) open = std::fmax(open, plan.speed[i]);
    const double ds = pts[i + 1].s - pts[i].s;
    const double most =
        std::sqrt(plan.speed[i + 1] * plan.speed[i + 1] + 2 * sk::pilot::kBrakeDecel * ds);
    CHECK_MSG(plan.speed[i] <= most + 1e-9, "point " + std::to_string(i) + " brakes past the plan");
  }
  CHECK(before_lip <= sk::pilot::kKickerSpeed);
  CHECK(open > sk::pilot::kKickerSpeed + 5);
}

TEST(flown_down_the_slope_it_stays_up_on_the_piste_and_takes_the_kicker_as_planned) {
  const mg::Level level = sk::synthetic_slope();
  const Flight f = fly(level, slope_plan(level), {});
  CHECK(f.up);
  CHECK_MSG(f.widest < 20 - sk::pilot::kLaneMargin, std::to_string(f.widest));
  CHECK_MSG(f.at_lip > 8 && f.at_lip < sk::pilot::kKickerSpeed + 2, std::to_string(f.at_lip));
  CHECK_MSG(std::fabs(f.mean_offset) < 1.5, std::to_string(f.mean_offset));
}

TEST(a_rider_right_of_the_line_holds_his_lane) {
  const mg::Level level = sk::synthetic_slope();
  const Flight f = fly(level, slope_plan(level), {6, 1});
  CHECK(f.up);
  CHECK_MSG(f.mean_offset > 4 && f.mean_offset < 8, std::to_string(f.mean_offset));
}

TEST(it_decides_the_same_recorded_or_not_and_records_its_line_and_speed) {
  const mg::Level level = sk::synthetic_slope();
  const sk::PilotPlan plan = slope_plan(level);
  const sk::TreeIndex trees = sk::build_tree_index(level);
  const sk::World world{&level, &trees};
  sk::SkierState s = sk::stand_skier(world, sk::slope::kCentre, 20, 0, 5);
  ge::ai::Why why(0);
  constexpr int kSteps = 1200;
  for (int i = 0; i < kSteps; i++) {
    const sk::SkierInput plain = sk::skier_pilot(s, sk::kReferenceSkis, plan, {2, 0.9});
    const sk::SkierInput traced =
        sk::skier_pilot(s, sk::kReferenceSkis, plan, {2, 0.9}, why.follow(0, i), 0.2);
    const sk::SkierInput late =
        sk::skier_pilot(s, sk::kReferenceSkis, plan, {2, 0.9}, nullptr, 0.2);
    CHECK(traced.steer == late.steer && traced.brake == late.brake && traced.tuck == late.tuck);
    sk::SkierEvents ev;
    sk::step_skier(s, plain, world, sk::kReferenceSkis, kDt, ev);
  }
  int line = 0, speed = 0;
  for (const ge::ai::Decision& d : why.decisions()) {
    line += std::string(d.name) == "line";
    speed += std::string(d.name) == "speed";
  }
  CHECK(line == kSteps && speed == kSteps);
}
