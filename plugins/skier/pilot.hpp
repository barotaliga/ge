// THE SKIER'S PILOT: what any bot on skis flies, a game's rivals, its crowd or its pursuers. It
// reads only what a skier could see (where he is, how fast, the line ahead) and plans on the
// skier's own limits (`limits.hpp`), never a ceiling of its own. A game lays the line (a piste, a
// chain of runs, a weave through gates) and says where its lips are; the pilot does the rest.
//
// - THE PLAN, once a line (`lay_pilot_plan`): the line's curvature at every point (along the
//   offset `PilotPlan::line` where the game laid one), and the fastest the pilot means to be
//   there: what the pair can carve round that bend (`carve_speed`) times `kCornerShare`, eased
//   under `kKickerSpeed` up to every lip, held under a slowing from a point on, then carried back
//   up the line so every slowing can be braked for (`kBrakeDecel` on the flat, less as the pitch
//   steepens).
// - THE LINE: pure pursuit of the skier's LANE (m right of the line, inside its edges by
//   `kLaneMargin`, and inside `PilotPlan::room` about an offset line) a speed-scaled look ahead,
//   further by the lead of late hands; the curvature to the point is turned into the edge it needs
//   (`edge_for_curvature`) and that into a steer.
// - THE SPEED: the brake past the plan's speed over the next stretch times his pace, the tuck
//   under it on a straight (which at a crawl is the skater's push).
// - ITS DECISIONS, recorded when it is handed a `ge::ai::Why` (the `bots` skill): `line` (the
//   aim, the lane, the bearing to it, the curvature and edge it asks, the steer) and `speed`
//   (brake, tuck or glide, each scored by how far his speed is from the plan's: the tuck barred
//   while he turns hard). It decides the same with the record or without.
//
// ge:summary The skier's pilot: a speed plan off the skier's limits, pure pursuit of a lane
// ge:keywords pilot bot ai skier_pilot lay_pilot_plan pure-pursuit lookahead speed-plan
// ge:keywords curvature corner kicker lip brake tuck lane pace lead why decisions
// ge:skill skier bots
// ge:test tests/skier_pilot_test.cpp
#pragma once

#include <cmath>
#include <memory>
#include <span>
#include <vector>

#include "ai/why.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"

namespace ge::skier {

namespace pilot {
// The share of the carve's speed the pilot takes a bend at.
inline constexpr double kCornerShare = 0.8;
// The speed it takes a lip at, m/s, and how far up the line from the lip it holds it, m.
inline constexpr double kKickerSpeed = 16;
inline constexpr double kKickerReach = 30;
// The braking it plans on, m/s^2, on the flat (it brakes harder, and the steep takes it back),
// and the least it ever counts on.
inline constexpr double kBrakeDecel = 5;
inline constexpr double kBrakeLeast = 0.8;
// The look ahead: `kLookTime` s at his speed plus `kLookLeast` m, at most `kLookMost` m.
inline constexpr double kLookTime = 0.5;
inline constexpr double kLookLeast = 7;
inline constexpr double kLookMost = 30;
// With late hands (`lead_s`), the look reaches `kLeadLook` times the way he covers while his hands
// catch up further, at no less than `kLeadLeast` m/s: a pursuit aimed as close as an instant
// hand's swings ever wider once its steer lands 0.2 s late, and a person who knows their hands
// are slow looks further down the hill.
inline constexpr double kLeadLook = 6;
inline constexpr double kLeadLeast = 8;
// How far ahead it reads the plan's speed, s at his speed plus m.
inline constexpr double kReadTime = 0.6;
inline constexpr double kReadLeast = 4;
// The least room a lane leaves between the line and the edge of the way, m.
inline constexpr double kLaneMargin = 2;
}  // namespace pilot

struct PilotPlan {
  // The line the plan is laid along. Every vector below has an entry for each of its points.
  std::shared_ptr<const ge::mountain::mapgen::Track> track;
  std::vector<double> curvature;  // 1/m at each point, signed (+ bending right)
  std::vector<double> speed;      // m/s, the fastest the pilot means to be at each point
  // An offset line the lane is held about, m right of the track at each point, and the half-room
  // a lane has about it; both empty where the lane is held about the track itself. A game lays
  // them (a weave through gates) before `lay_pilot_plan`.
  std::vector<double> line;
  std::vector<double> room;
};

// Lays `plan`'s curvature and speed along `plan.track` (and `plan.line` where set): `lips` the
// distances along the track of every kicker's lip, held under `slow` m/s from `slow_from` m on.
void lay_pilot_plan(const ge::mountain::mapgen::Level& level, const SkiSpec& spec,
                    std::span<const double> lips, PilotPlan& plan, double slow_from = INFINITY,
                    double slow = 0);

// Who flies the pilot: his lane, m right of the line, and his pace, the share of the plan's
// speed he skis at (1: the plan's own).
struct PilotRider {
  double lane = 0;
  double pace = 1;
};

// The skier's controls this step, `s` flying `plan` on `spec` as `rider`. With `why` it records
// its `line` and `speed` decisions there. `lead_s`: how late its hands press what it decides; it
// looks further ahead by it.
SkierInput skier_pilot(const SkierState& s, const SkiSpec& spec, const PilotPlan& plan,
                       const PilotRider& rider, ge::ai::Why* why = nullptr, double lead_s = 0);

}  // namespace ge::skier
