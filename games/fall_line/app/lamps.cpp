// Fall Line's lamps after dark: the headlamps and the eye's adaptation to the lamps.
//
// ge:summary The headlamp's spot and flood on each racer, the eye's lamp lux
// ge:keywords fall-line headlamp_lights headlamp_of eye_lamp_lux
// ge:skill render
// ge:test tests/fall_line_lamps_test.cpp
#include "fall_line/app/lamps.hpp"

#include <algorithm>
#include <cmath>

namespace fall_line::app {

namespace {

namespace r = ge::render;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The headlamp's light (5500 K) and its two beams' cones off the axis, degrees.
constexpr float kHeadlampRgb[3] = {1.0f, 0.89f, 0.80f};
constexpr float kSpotInner = 7, kSpotOuter = 13, kSpotRange = 60;
constexpr float kFloodInner = 30, kFloodOuter = 60, kFloodRange = 18;
// Where the lamp is strapped in the head's frame, m: over the eyes, proud of the helmet's shell.
constexpr ge::core::Vec3 kBrow = {0, 0.06, 0.17};
// Where the eye's points lie on the ground ahead along the view, m, and how far over the snow.
constexpr double kEyeAhead[] = {5, 10, 18, 30};
constexpr double kEyeOver = 0.1;
// The least lux a point counts for in the mean: past it, a dark point drags the eye no further.
constexpr double kEyeFloor = 0.05;
// The share of the mean the eye adapts to: under it, so the lit snow reads bright and the
// moonlit mountain beyond the lamps is still seen.
constexpr double kEyeKey = 0.4;

}  // namespace

Headlamp headlamp_of(const ge::skier::FigureFrame& frame, const ge::skier::RigPose& pose) {
  namespace core = ge::core;
  static const int head = ge::skier::rig_joint("head");
  const auto h = static_cast<size_t>(head);
  // In the figure's frame (+z forward, y up), then the world's.
  const core::Vec3 brow = core::rotate(pose.turn[h], kBrow);
  const core::Vec3 in = {pose.at[h].x + brow.x, pose.at[h].y + brow.y, pose.at[h].z + brow.z};
  const core::Vec3 world = core::rotate(frame.rot, in);
  const core::Vec3 ahead = core::rotate(
      frame.rot, core::rotate(pose.turn[h], core::Vec3{0, -std::sin(kHeadlampDip * kDeg),
                                                       std::cos(kHeadlampDip * kDeg)}));
  Headlamp lamp;
  lamp.at[0] = frame.at.x + world.x;
  lamp.at[1] = frame.at.y + world.y;
  lamp.at[2] = frame.at.z + world.z;
  lamp.dir[0] = static_cast<float>(ahead.x);
  lamp.dir[1] = static_cast<float>(ahead.y);
  lamp.dir[2] = static_cast<float>(ahead.z);
  return lamp;
}

void headlamp_lights(const Headlamp& lamp, uint32_t key, double lit, bool shadows,
                     std::vector<r::LocalLight>& out) {
  if (lit <= 0) return;
  r::LocalLight spot;
  std::copy(lamp.at, lamp.at + 3, spot.position);
  std::copy(lamp.dir, lamp.dir + 3, spot.direction);
  std::copy(kHeadlampRgb, kHeadlampRgb + 3, spot.colour);
  spot.radius_m = 0.02f;
  r::LocalLight flood = spot;
  spot.candela = static_cast<float>(kHeadlampCandela * lit);
  spot.inner_deg = kSpotInner;
  spot.outer_deg = kSpotOuter;
  spot.range_m = kSpotRange;
  spot.shadow = shadows ? key : 0;
  flood.candela = static_cast<float>(kHeadlampFlood * lit);
  flood.inner_deg = kFloodInner;
  flood.outer_deg = kFloodOuter;
  flood.range_m = kFloodRange;
  out.push_back(spot);
  out.push_back(flood);
}

double eye_lamp_lux(std::span<const r::LocalLight> lights, const r::FrameView& view,
                    const ge::mountain::mapgen::Level& level) {
  if (lights.empty()) return 0;
  const double fx = std::sin(view.heading), fz = std::cos(view.heading);
  double sum = 0, most = 0;
  for (const double d : kEyeAhead) {
    double at[3] = {view.eye[0] + fx * d, 0, view.eye[2] + fz * d};
    at[1] = level.ground_at(at[0], at[2]) + kEyeOver;
    const double lux = r::lamp_lux_at(lights, at, view.time_s);
    most = std::max(most, lux);
    sum += std::log(std::max(lux, kEyeFloor));
  }
  if (most <= 0) return 0;
  return kEyeKey * std::exp(sum / static_cast<double>(std::size(kEyeAhead)));
}

}  // namespace fall_line::app
