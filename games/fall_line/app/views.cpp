// The named views' rigs and the map's day.
//
// ge:summary The named views' camera rigs and the map's day
// ge:keywords fall-line views camera rig sun
// ge:skill render
#include "fall_line/app/views.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/solar.hpp"
#include "mountain/mapgen/sun.hpp"

namespace fall_line::app {

namespace {

using ge::mountain::mapgen::Level;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

constexpr std::array<std::string_view, 6> kNames = {"start", "piste",  "lookback",
                                                    "vista", "aerial", "wood"};

// The piste's station nearest arc `s`.
const ge::mountain::mapgen::TrackPoint& station_at(const Level& level, double s) {
  const auto& p = level.track.points;
  size_t best = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    if (std::fabs(p[i].s - s) < std::fabs(p[best].s - s)) best = i;
  }
  return p[best];
}

// True when the ground at (x, z) sees the sun (unit, toward it) past the mountain: a march of
// 96 samples toward the sun, growing from 4 m to about 10 km, none of them over the ray.
bool sunlit(const Level& level, double x, double z, const double sun[3]) {
  const double flat = std::hypot(sun[0], sun[2]);
  if (sun[1] <= 0 || flat < 1e-6) return sun[1] > 0;
  const double rise = sun[1] / flat;
  const double y0 = level.ground_at(x, z) + 0.5;
  double d = 4;
  for (int i = 0; i < 96; ++i, d *= 1.085) {
    const double px = x + d * sun[0] / flat, pz = z + d * sun[2] / flat;
    if (level.ground_at(px, pz) > y0 + d * rise) return false;
  }
  return true;
}

// The wood the sun lights best: of the trees on a wood's sunward edge (at least eight more within
// 15 m, none toward the sun within 30 m), the one whose ground faces the sun the most and sees it
// past the mountain (`strict`; otherwise any wood's tree whose ground faces it most). Null when
// none qualifies.
const ge::mountain::mapgen::TreeDef* sunlit_wood(const Level& level, const double sun[3],
                                                 bool strict) {
  const auto& trees = level.trees;
  const ge::mountain::mapgen::TreeDef* best = nullptr;
  double best_score = 0;
  for (size_t i = 0; i < trees.size(); i += 5) {
    const auto& t = trees[i];
    ge::mountain::mapgen::Vec3 n{};
    level.normal_at(t.x, t.z, n);
    const double score = n.x * sun[0] + n.y * sun[1] + n.z * sun[2];
    if (score <= best_score) continue;
    int near = 0;
    bool shaded = false;  // a tree within 30 m toward the sun, 4 m either side of its ray
    const double flat = std::max(std::hypot(sun[0], sun[2]), 1e-6);
    for (const auto& u : trees) {
      const double dx = u.x - t.x, dz = u.z - t.z;
      if (std::fabs(dx) < 15 && std::fabs(dz) < 15) ++near;
      const double along = (dx * sun[0] + dz * sun[2]) / flat;
      const double side = (dx * sun[2] - dz * sun[0]) / flat;
      if (along > 1 && along < 30 && std::fabs(side) < 4) shaded = true;
    }
    if (near < 9 || (strict && (shaded || !sunlit(level, t.x, t.z, sun)))) continue;
    best = &t;
    best_score = score;
  }
  return best;
}

void place(const Level& level, double x, double z, double over_ground, ge::render::FrameView& v) {
  v.eye[0] = x;
  v.eye[2] = z;
  v.eye[1] = level.ground_at(x, z) + over_ground;
}

}  // namespace

std::span<const std::string_view> view_names() { return kNames; }

std::string_view view_caption(std::string_view name) {
  if (name == "start") return "From behind the start line, a skier's eye height, down the piste.";
  if (name == "piste") return "Halfway down the piste at a skier's eye height, looking down it.";
  if (name == "lookback") return "From the finish, looking back up the piste to the summit.";
  if (name == "vista") return "High over the summit, looking down the mountain to the valley.";
  if (name == "aerial") return "From high over a corner of the map, the whole mountain.";
  if (name == "wood") {
    return "Beside the wood the sun lights best, across the sun, looking at the trees' shadows "
           "on the snow (in the mountain's shade when a low sun lights no wood).";
  }
  return "";
}

void day_view(const Level& level, double hour, ge::render::FrameView& v) {
  const double h = hour < 0 ? level.sun.hour : hour;
  const ge::core::SunPlace sun = ge::core::sun_at(
      h, level.sun.latitude, ge::mountain::mapgen::declination_of(level.sun.day_of_year));
  v.sun_elevation = sun.elevation;
  v.sun_azimuth = sun.azimuth;
  v.sun_illuminance = 1.0;
  v.altitude_m = -level.mountain.sea;
  v.fov_y = 55 * kDeg;
  v.near_m = 0.1;
}

bool make_view(const Level& level, std::string_view name, ge::render::FrameView& v) {
  const auto& track = level.track;
  if (track.points.empty()) return false;
  const auto& m = level.mountain;
  if (name == "start") {
    const auto& p = station_at(level, 0);
    place(level, p.x - 4 * std::sin(p.heading), p.z - 4 * std::cos(p.heading), 1.8, v);
    v.heading = p.heading;
    v.pitch = -8 * kDeg;
  } else if (name == "piste") {
    const auto& p = station_at(level, track.length * 0.5);
    place(level, p.x, p.z, 1.7, v);
    v.heading = p.heading;
    v.pitch = -6 * kDeg;
  } else if (name == "lookback") {
    const auto& p = station_at(level, track.length);
    place(level, p.x, p.z, 2.0, v);
    v.heading = std::atan2(m.summit.x - p.x, m.summit.z - p.z);
    v.pitch = 4 * kDeg;
  } else if (name == "vista") {
    place(level, m.summit.x, m.summit.z, 0, v);
    v.eye[1] = m.summit.y + 150;
    v.heading = std::atan2(m.base.x - m.summit.x, m.base.z - m.summit.z);
    v.pitch = -18 * kDeg;
  } else if (name == "aerial") {
    const double c = level.size * 0.5;
    const double x = -0.15 * level.size, z = -0.15 * level.size;
    place(level, x, z, 0, v);
    v.eye[1] = m.summit.y + 600;
    v.heading = std::atan2(c - x, c - z);
    v.pitch = -std::atan2(v.eye[1] - m.base.y, std::hypot(c - x, c - z)) * 0.8;
  } else if (name == "wood") {
    // Across the sun from the wood, 70 m off and 30 m up, so the shadows fall sideways in the
    // picture; when the mountain shades every wood (a low sun), the wood facing the sun most, in
    // its shade; the start's view on a map with no wood.
    double sun[3];
    ge::render::sun_direction(v, sun);
    const auto* t = sunlit_wood(level, sun, true);
    if (t == nullptr) t = sunlit_wood(level, sun, false);  // the mountain shades every wood
    if (t == nullptr) return make_view(level, "start", v);
    const double across = std::atan2(sun[0], sun[2]) + 0.5 * kPi;
    place(level, t->x - 70 * std::sin(across), t->z - 70 * std::cos(across), 30, v);
    v.heading = across;
    v.pitch = -std::atan2(v.eye[1] - t->y, 70.0);
  } else {
    return false;
  }
  return true;
}

void chase_view(const Level& level, const ge::skier::SkierState& s, ge::render::FrameView& v,
                double back, double up) {
  const double flat = std::hypot(s.vx, s.vz);
  const double way = flat > 1.0 ? std::atan2(s.vx, s.vz) : s.heading;
  const double ex = s.x - back * std::sin(way), ez = s.z - back * std::cos(way);
  v.eye[0] = ex;
  v.eye[2] = ez;
  v.eye[1] = std::max(s.y + up, level.ground_at(ex, ez) + 1.2);
  const double tx = s.x - ex, tz = s.z - ez, ty = s.y + 1.0 - v.eye[1];
  v.heading = std::atan2(tx, tz);
  v.pitch = std::atan2(ty, std::hypot(tx, tz));
}

}  // namespace fall_line::app
