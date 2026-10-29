// PLANTED LENSES WHERE THE PATH IS KNOWN, AND THE BROADCAST'S CUTS BETWEEN THEM.
//
// A replay knows where its subject goes, so a lens can be put where it will SEE the moment
// before it happens: beside the lip of a jump, ahead of a fall, high over a finish. This file is
// the generic half of that (docs/camera.md § Directors):
//
//   - `sight_share`: how much of the line from a lens to a point is open, marching it against
//     the world's ground, its obstacles (trunks: cylinders over a height band) and its `clear`
//     callback (posts, walls). The camera's own `clear` answers only for what a boom pulls in
//     for; a planted lens also needs the hill and the trunks between it and the subject.
//   - `plant_beside`: a planted lens for a stretch of known path. Candidates stand round the key
//     point (the lip, the fall) at the spec's distance across the travel, ahead and behind, and
//     at nearer and farther rings; each is scored by the share of the path it sees, and the best
//     wins (ties: the candidate nearest the spec's own offset). Nothing is drawn from a random
//     stream: the same path and world always plant the same lens.
//   - `smoothed_at` and `planted_at`: a planted lens's pose as a PURE function of the path and
//     the step. A live planted lens pans on a follower; a replay's has the whole path, so its aim
//     is the path averaged over a window round the step (the same smoothness, no state), and its
//     zoom holds the subject's size from that range. Any frame of a replay can then be drawn on
//     its own, as a video export needs.
//   - `direct_broadcast`: the live director's cut rules over a set of trackside lenses. The live
//     lens is kept while it sees the subject; once it has been held `min_hold_s` it hands over to
//     the best lens the subject is running TOWARD (nearest first) when the live one falls behind
//     or out of range, and at once when the live one has lost sight for `lost_s`. Among lenses
//     near in score it keeps the side of the subject's line the live lens stands on (the 180°
//     rule: a cut across the line flips the direction of travel on screen), and counts the cuts
//     that cross anyway. The line is the caller's (a course's direction where the subject is)
//     or the travel eased over `line_halflife_s`: the way he is going, not the swing of a turn. A
//     lens looking nearly along the line (him running at it or away, within `neutral`) is on
//     neither side: a NEUTRAL shot, and a cut to or from one crosses nothing.
//
// ge:summary Planted lenses placed by a known path, pure replay aims, and the broadcast's cuts
// ge:keywords camera planted trackside director broadcast replay film impact sight line-of-sight
// ge:keywords plant_beside sight_share planted_at smoothed_at direct_broadcast cut 180-degree-rule
// ge:skill camera
// ge:test tests/camera_plant_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "camera/lenses.hpp"
#include "camera/pose.hpp"
#include "camera/subject.hpp"
#include "camera/world.hpp"

namespace ge::camera {

// The subject at one step of a known path: where it is, its velocity, and the height over its
// position the lens aims at.
struct PathPoint {
  Vec3 pos{0, 0, 0};
  Vec3 vel{0, 0, 0};
};

// The share (0..1) of the line from `from` to `to` that is open before the ground, an obstacle
// or the world's `clear` closes it; marched every `step_m`.
double sight_share(const World& w, const Vec3& from, const Vec3& to, double step_m = 1.0);

struct PlantSpec {
  size_t key = 0;             // the path point the lens is planted for
  double across_m = 14;       // plan distance from the key point, across its travel
  double along_m = 0;         // along its travel: + ahead of the key, - behind it
  double height_m = 2;        // the lens over the ground where it stands
  double aim_height_m = 0.9;  // over the subject's position
  double hold_m = 3;          // the frame's height the subject is held to (0: fixed fov)
  double fov_y = 0.6;         // with hold_m 0, or before the zoom has a range
  double fov_min = 0.05, fov_max = 1.0;
  int sides = 2;              // 2: either side of the travel; 1: the right only; -1: the left
  double least_m = 4;         // the nearest a candidate may stand to any path point in plan
  int every = 4;              // the path points tested for sight: every `every`th
  double seen_enough = 0.98;  // a candidate seeing this share is as good as all of it
};

struct Planting {
  PlantedRow row{};
  double seen = 0;     // the share of the tested path points in clear sight
  int candidate = -1;  // which candidate won (-1: the path was empty)
};

Planting plant_beside(std::span<const PathPoint> path, const PlantSpec& spec, const World& w);

// The path's position averaged over `half_steps` either side of `i` (clamped to the path).
Vec3 smoothed_at(std::span<const PathPoint> path, size_t i, size_t half_steps);

// A planted lens's pose at path point `i`: aimed at the smoothed position plus the aim height,
// the fov holding `row.hold_m` of frame at that range. Pure.
Pose planted_at(const PlantedRow& row, std::span<const PathPoint> path, size_t i,
                size_t half_steps);

struct BroadcastRule {
  double min_hold_s = 2.5;       // the least a lens stays live before a planned cut
  double lost_s = 0.25;          // sight lost this long cuts at once
  double range_m = 140;          // a lens farther than this from the subject cannot be live
  double behind_m = 25;          // the live lens, receding and this far, hands over
  double sight = 0.999;          // the share of the line that must be open to see the subject
  double aim_height_m = 0.9;     // over the subject's position
  double line_halflife_s = 1.5;  // the line of travel the 180° rule reads, eased over a turn
  double neutral = 0.34;  // sin of the angle off the line inside which a lens is on neither side
};

struct BroadcastState {
  int live = -1;      // the live lens; -1 before the first frame
  double held_s = 0;  // since the last cut
  double lost_s = 0;  // since the live lens last saw the subject
  int cuts = 0;
  int crossed = 0;                // cuts from one side of the subject's line to the other
  bool cut = false;               // whether this frame cut
  double line_x = 0, line_z = 0;  // the line of travel, eased (zero before the first frame)
};

// The live lens this frame, after the rules. `lenses` are the planted lenses' rows. `line` is the
// line of action the 180° rule keeps to (a course's direction where the subject is); zero, the
// subject's travel eased over `rule.line_halflife_s`.
int direct_broadcast(std::span<const PlantedRow> lenses, const Subject& s, const World& w,
                     const BroadcastRule& rule, BroadcastState& st, double dt,
                     const Vec3& line = {0, 0, 0});

}  // namespace ge::camera
