// The fly between the map and the eye (fly.hpp).
//
// ge:summary The fly's pose, the map's share and the demo's rate as functions of its progress
// ge:keywords overview fly-down dive tilt-up catch-up
// ge:skill map-design camera
#include "overview/fly.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::overview {

namespace {

namespace lm = ge::core::libm;
using ge::core::kPi;

constexpr double kFall = 3.0;   // how much faster the height drops at the top than at the ground
constexpr double kStart = 1.6;  // the dive's slow start: progress to this power before the fall

// The share of the height above the eye left at progress `e` (1 at the top, 0 at the eye): eased
// in log space, so the ground grows at an even rate until it slows to land.
double height_left(double e) {
  const double end = lm::exp(-kFall);
  return (lm::exp(-kFall * e) - end) / (1 - end);
}

}  // namespace

FlyPose top_pose(const double lo[3], const double hi[3], double fov_y, double aspect) {
  FlyPose t;
  const double w = hi[0] - lo[0], d = hi[2] - lo[2];
  const double half_tan = lm::tan(fov_y / 2);
  // The distance at which the level's depth (z) fits the height and its width (x) the width.
  const double need = std::max(d / 2, w / 2 / std::max(0.1, aspect)) / std::max(1e-3, half_tan);
  t.eye[0] = (lo[0] + hi[0]) / 2;
  t.eye[2] = (lo[2] + hi[2]) / 2;
  t.eye[1] = hi[1] + need * 1.04;  // a little room round it
  t.heading = 0;
  t.pitch = -kPi / 2;
  return t;
}

FlyPose top_pose_onto(const double lo[3], const double hi[3], double fov_y, double width,
                      double height, double shift_y, const double box_px[4]) {
  FlyPose t;
  const double span = std::max(1e-3, hi[0] - lo[0]);
  const double px_per_m = std::max(1e-6, (box_px[2] - box_px[0]) / span);
  const double focal = height / 2 / std::max(1e-3, lm::tan(fov_y / 2));
  const double plane = lo[1] + (hi[1] - lo[1]) / 3;
  const double dist = focal / px_per_m;
  // The box's middle off the lens's middle, px, back to metres on the plane: right is +x, down
  // the screen is -z (north up).
  const double dx = (box_px[0] + box_px[2]) / 2 - width / 2;
  const double dy = (box_px[1] + box_px[3]) / 2 - (height / 2 - shift_y * height / 2);
  t.eye[0] = (lo[0] + hi[0]) / 2 - dx / px_per_m;
  t.eye[2] = (lo[2] + hi[2]) / 2 + dy / px_per_m;
  t.eye[1] = plane + dist;
  t.heading = 0;
  t.pitch = -kPi / 2;
  return t;
}

void fly_plan_box(const FlyPose& pose, const double lo[3], const double hi[3], double plane_y,
                  double fov_y, double width, double height, double shift_y, double box_px[4]) {
  const double focal = height / 2 / std::max(1e-3, lm::tan(fov_y / 2));
  const double px_per_m = focal / std::max(1e-3, pose.eye[1] - plane_y);
  const double cx = width / 2 + ((lo[0] + hi[0]) / 2 - pose.eye[0]) * px_per_m;
  const double cy =
      height / 2 - shift_y * height / 2 - ((lo[2] + hi[2]) / 2 - pose.eye[2]) * px_per_m;
  const double hw = (hi[0] - lo[0]) / 2 * px_per_m, hd = (hi[2] - lo[2]) / 2 * px_per_m;
  box_px[0] = cx - hw;
  box_px[1] = cy - hd;
  box_px[2] = cx + hw;
  box_px[3] = cy + hd;
}

void fly_start(Fly& f, FlyWay way) {
  if (way == FlyWay::none) return;
  f.way = way;
  // A new dive starts from wherever the last way left off; the catch-up still owed carries on.
}

void fly_advance(Fly& f, double dt, const FlyTimes& times) {
  if (dt <= 0) return;
  if (f.way == FlyWay::down) {
    f.owed_s += dt;  // the demo is held while it dives
    f.p += dt / std::max(1e-3, times.dive_s);
  } else if (f.way == FlyWay::up) {
    f.p -= dt / std::max(1e-3, times.dive_s);
  }
  if (f.way != FlyWay::none && (f.p >= 1 || f.p <= 0)) {
    f.p = ge::core::clamp(f.p, 0, 1);
    f.way = FlyWay::none;
    if (f.owed_s > 0) {
      // Rate 1 + (R - 1)(1 - u)^2 over the length L gives back (R - 1) L / 3: L = 3 owed / (R - 1).
      f.catch_len = 3 * f.owed_s / std::max(0.1, times.catch_rate - 1);
      f.catch_t = 0;
      f.owed_s = 0;
    }
    return;
  }
  if (f.way == FlyWay::none && f.catch_t >= 0) {
    f.catch_t += dt;
    if (f.catch_t >= f.catch_len) f.catch_t = -1;
  }
}

bool fly_running(const Fly& f) { return f.way != FlyWay::none || f.catch_t >= 0; }

FlyPose fly_pose(const Fly& f, const FlyTimes& times, const FlyPose& top, const FlyPose& eye) {
  const double p = ge::core::clamp(f.p, 0, 1);
  if (p >= 1) return eye;
  FlyPose o;
  // Over the player early, so the last of the dive falls straight onto him.
  const double over = ge::core::smoothstep(0, 0.65, p);
  o.eye[0] = ge::core::lerp(top.eye[0], eye.eye[0], over);
  o.eye[2] = ge::core::lerp(top.eye[2], eye.eye[2], over);
  // A slow start (the map still whole while it fades), then the fall.
  o.eye[1] = eye.eye[1] + (top.eye[1] - eye.eye[1]) * height_left(lm::pow(p, kStart));
  // North up while the map shows, so the map and the level stay one picture; then his heading.
  o.heading = top.heading + ge::core::angle_diff(top.heading, eye.heading) *
                                ge::core::smoothstep(times.blend_to, 0.95, p);
  const double tilt_from = 1 - std::min(1.0, times.tilt_s / std::max(1e-3, times.dive_s));
  o.pitch = ge::core::lerp(top.pitch, eye.pitch, ge::core::smoothstep(tilt_from, 1, p));
  return o;
}

double fly_map_share(const Fly& f, const FlyTimes& times) {
  return 1 - ge::core::smoothstep(times.blend_from,
                                  std::max(times.blend_from + 1e-3, times.blend_to), f.p);
}

double fly_demo_rate(const Fly& f, const FlyTimes& times) {
  if (f.way == FlyWay::down) return 0;
  if (f.catch_t >= 0 && f.catch_len > 0) {
    const double u = std::min(1.0, f.catch_t / f.catch_len);
    return 1 + (times.catch_rate - 1) * (1 - u) * (1 - u);
  }
  return 1;
}

bool fly_shows_world(const Fly& f) { return f.p > 0; }

}  // namespace ge::overview
