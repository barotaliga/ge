// Fall Line's directors: the replay's film camera and the trackside broadcast.
//
// ge:summary The film camera's lenses planted off the cut and the path, its pure poses, trackside
// ge:keywords camera film replay director lens plant trackside broadcast
// ge:skill camera replay
// ge:test tests/fall_line_film_test.cpp
#include "fall_line/camera/film.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/query.hpp"

namespace fall_line::camera {

namespace cam = ge::camera;
namespace core = ge::core;

namespace {

// Each planted lens's place round its key point and its frame. The key is the shot's mark,
// moved `key_s` along the run.
struct LensRow {
  double key_s;
  cam::PlantSpec spec;
};

LensRow row_of(FilmLens lens) {
  cam::PlantSpec s;
  switch (lens) {
    case FilmLens::side:  // beside the lip, a little ahead of it, low: the flight crosses frame
      s.across_m = 16, s.along_m = 6, s.height_m = 1.4, s.hold_m = 4.5;
      return {0.4, s};
    case FilmLens::front:  // down the line ahead, a little off it: he runs at the lens
      s.across_m = 6, s.along_m = 30, s.height_m = 1.6, s.hold_m = 3.5, s.least_m = 3;
      return {0, s};
    case FilmLens::aerial:  // high and wide over the finish
      s.across_m = 22, s.along_m = -6, s.height_m = 24, s.hold_m = 12;
      return {0, s};
    case FilmLens::impact:  // low and near, just past the fall, waiting for it
      s.across_m = 8, s.along_m = 7, s.height_m = 1.0, s.hold_m = 3.2, s.least_m = 3;
      return {0, s};
    case FilmLens::chase:
      break;
  }
  return {0, s};
}

constexpr double kChaseBack = 6.5, kChaseUp = 2.2, kChaseAim = 0.9, kChaseFov = 0.9;
constexpr double kChaseLook = 0.4;   // s either side of the step the travel is read across
constexpr double kChaseFloor = 0.6;  // m over the snow at the least
constexpr double kAimHeight = 0.9;   // over his feet

// The chase at path point i: behind the smoothed path along its travel, aimed at it. Pure.
cam::Pose chase_at(const FilmPlan& plan, const cam::World& world, size_t i, double hz) {
  const auto look = static_cast<size_t>(kChaseLook * hz);
  const size_t a = i > look ? i - look : 0;
  const cam::Vec3 m = cam::smoothed_at(plan.path, i, plan.half_steps);
  const cam::Vec3 p0 = cam::smoothed_at(plan.path, a, plan.half_steps);
  const cam::Vec3 p1 = cam::smoothed_at(plan.path, i + look, plan.half_steps);
  double fx = p1.x - p0.x, fz = p1.z - p0.z;
  const double len = core::hypot(fx, fz);
  if (len > 1e-6) {
    fx /= len;
    fz /= len;
  } else {
    fx = 0;
    fz = 1;
  }
  cam::Vec3 eye{m.x - fx * kChaseBack, m.y + kChaseUp, m.z - fz * kChaseBack};
  const double floor = cam::ground_at(world, eye.x, eye.z) + kChaseFloor;
  if (eye.y < floor) eye.y = floor;
  return cam::aim_pose(eye, {m.x, m.y + kChaseAim, m.z}, 0, kChaseFov);
}

cam::Pose pose_at(const FilmPlan& plan, const cam::World& world, const FilmShot& shot, size_t i,
                  double hz) {
  if (shot.lens == FilmLens::chase || shot.plant.candidate < 0) {
    return chase_at(plan, world, i, hz);
  }
  return cam::planted_at(shot.plant.row, plan.path, i, plan.half_steps);
}

}  // namespace

FilmLens film_lens_named(const std::string& name) {
  if (name == "side") return FilmLens::side;
  if (name == "front") return FilmLens::front;
  if (name == "aerial") return FilmLens::aerial;
  if (name == "impact") return FilmLens::impact;
  return FilmLens::chase;
}

const char* film_lens_name(FilmLens lens) {
  switch (lens) {
    case FilmLens::side:
      return "side";
    case FilmLens::front:
      return "front";
    case FilmLens::aerial:
      return "aerial";
    case FilmLens::impact:
      return "impact";
    case FilmLens::chase:
      break;
  }
  return "chase";
}

cam::PathPoint path_point(const ge::skier::SkierState& s) {
  return {{s.x, s.y, s.z}, {s.vx, s.vy, s.vz}};
}

FilmPlan plan_film(const ge::replay::Cut& cut, std::span<const ge::replay::Mark> marks,
                   std::vector<cam::PathPoint> path, const cam::World& world, double hz) {
  FilmPlan plan;
  plan.path = std::move(path);
  plan.hz = hz;
  plan.half_steps = static_cast<size_t>(kFilmWindowS * 0.5 * hz);
  if (plan.path.empty()) return plan;
  const auto last = static_cast<int64_t>(plan.path.size()) - 1;
  for (const ge::replay::Shot& s : cut.shots) {
    FilmShot f;
    f.lens = film_lens_named(s.lens);
    f.from = static_cast<int64_t>(
        core::clamp(static_cast<double>(s.from), 0, static_cast<double>(last)));
    f.to = static_cast<int64_t>(core::clamp(static_cast<double>(s.to), static_cast<double>(f.from),
                                            static_cast<double>(last)));
    f.mark = s.mark < marks.size() ? marks[s.mark].step : f.from;
    if (f.lens != FilmLens::chase) {
      const LensRow r = row_of(f.lens);
      cam::PlantSpec spec = r.spec;
      const auto key =
          static_cast<int64_t>(core::clamp(static_cast<double>(f.mark) + r.key_s * hz,
                                           static_cast<double>(f.from), static_cast<double>(f.to)));
      spec.key = static_cast<size_t>(key - f.from);
      spec.aim_height_m = kAimHeight;
      const std::span<const cam::PathPoint> stretch(plan.path.data() + f.from,
                                                    static_cast<size_t>(f.to - f.from + 1));
      f.plant = cam::plant_beside(stretch, spec, world);
    }
    plan.shots.push_back(f);
  }
  return plan;
}

cam::Pose film_pose(const FilmPlan& plan, const cam::World& world, size_t shot, double step) {
  if (plan.path.empty() || shot >= plan.shots.size()) return {};
  const FilmShot& s = plan.shots[shot];
  const double hz = plan.hz;
  const double at = core::clamp(step, static_cast<double>(s.from), static_cast<double>(s.to));
  const auto i0 = static_cast<size_t>(at);
  const size_t i1 = i0 + 1 < plan.path.size() ? i0 + 1 : i0;
  const double t = at - static_cast<double>(i0);
  const cam::Pose a = pose_at(plan, world, s, i0, hz);
  if (t <= 0 || i1 == i0) return a;
  return cam::blend_pose(a, pose_at(plan, world, s, i1, hz), t);
}

std::vector<cam::PlantedRow> trackside_lenses(const ge::mountain::mapgen::Level& level,
                                              const cam::World& world) {
  std::vector<cam::PlantedRow> out;
  const auto& track = level.track.points;
  if (track.size() < 2) return out;
  // The piste as a path a skier runs at 15 m/s, for the planting's travel and sight.
  std::vector<cam::PathPoint> path;
  path.reserve(track.size());
  for (size_t i = 0; i < track.size(); ++i) {
    const size_t j = i + 1 < track.size() ? i + 1 : i;
    const size_t k = j == i ? i - 1 : i;
    const double dx = track[j].x - track[k].x, dz = track[j].z - track[k].z;
    const double len = core::hypot(dx, dz);
    const double v = len > 1e-9 ? 15 / len : 0;
    path.push_back({{track[i].x, track[i].y, track[i].z}, {dx * v, 0, dz * v}});
  }
  const double length = track.back().s;
  cam::PlantSpec spec;
  spec.along_m = 22, spec.height_m = 6, spec.hold_m = 5;
  spec.aim_height_m = kAimHeight, spec.least_m = 8, spec.every = 2;
  size_t from = 0;
  for (int n = 0; (n + 0.5) * kTracksideEvery < length; ++n) {
    const double s = (n + 0.5) * kTracksideEvery;
    while (from + 1 < track.size() && track[from].s < s - kTracksideEvery * 0.5) ++from;
    size_t key = from;
    while (key + 1 < track.size() && track[key].s < s) ++key;
    size_t to = key;
    while (to + 1 < track.size() && track[to].s < s + kTracksideEvery * 0.5) ++to;
    cam::PlantSpec at = spec;
    at.key = key - from;
    at.across_m = track[key].width * 0.5 + kTracksideOff;  // off the piste, not on it
    // Every lens on the right of the line, so a cut between two keeps the direction of travel
    // on screen (the 180-degree rule); a place on the right that sees too little of its
    // stretch, or that stands on the piste or across it, is left out.
    const std::span<const cam::PathPoint> stretch(path.data() + from, to - from + 1);
    at.sides = 1;
    const cam::Planting p = cam::plant_beside(stretch, at, world);
    if (p.seen < kTracksideSeen) continue;
    // A candidate turned round a bend can end on the piste or across it: right and off it only.
    const auto hit =
        ge::mountain::mapgen::nearest_track_point(level.track, p.row.eye.x, p.row.eye.z);
    if (hit.lateral < track[static_cast<size_t>(hit.index)].width * 0.5) continue;
    if (p.candidate >= 0) out.push_back(p.row);
  }
  return out;
}

cam::Pose broadcast_pose(Broadcast& b, const ge::mountain::mapgen::Level& level,
                         const cam::Subject& s, const cam::World& world, double dt) {
  cam::Vec3 line{0, 0, 0};
  if (level.track.points.size() >= 2) {
    const auto hit = ge::mountain::mapgen::nearest_track_point(level.track, s.pos.x, s.pos.z);
    const double h = level.track.points[static_cast<size_t>(hit.index)].heading;
    line = {core::libm::sin(h), 0, core::libm::cos(h)};
  }
  const int live = cam::direct_broadcast(b.lenses, s, world, b.rule, b.state, dt, line);
  if (live < 0) return {};
  if (b.state.cut) b.aim.fresh = true;
  return cam::frame_planted(b.lenses[static_cast<size_t>(live)], s, b.aim, dt);
}

}  // namespace fall_line::camera
