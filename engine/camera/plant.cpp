// Planted lenses where the path is known, and the broadcast's cuts between them.
//
// ge:summary Planting a lens by a known path, its pure aim, and the broadcast's cut rules
// ge:keywords camera planted trackside broadcast replay sight plant cut
// ge:skill camera
// ge:test tests/camera_plant_test.cpp
#include "camera/plant.hpp"

#include <array>
#include <cmath>

#include "camera/follow.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {

constexpr int kObstacles = 16;

// The direction of travel in plan at a path point: its velocity, or the way to the next point
// when it stands still; (0, 1) when neither says.
void travel_at(std::span<const PathPoint> path, size_t i, double& fx, double& fz) {
  fx = path[i].vel.x;
  fz = path[i].vel.z;
  double len = core::hypot(fx, fz);
  if (len < 0.1) {
    const size_t j = i + 1 < path.size() ? i + 1 : i;
    const size_t k = j == i && i > 0 ? i - 1 : i;
    fx = path[j].pos.x - path[k].pos.x;
    fz = path[j].pos.z - path[k].pos.z;
    len = core::hypot(fx, fz);
  }
  if (len < 1e-9) {
    fx = 0;
    fz = 1;
    return;
  }
  fx /= len;
  fz /= len;
}

double fov_for(double hold_m, double range, double fallback, double lo, double hi) {
  if (hold_m <= 0 || range <= 0) return fallback;
  return core::clamp(2.0 * libm::atan(hold_m * 0.5 / range), lo, hi);
}

}  // namespace

double sight_share(const World& w, const Vec3& from, const Vec3& to, double step_m) {
  const double dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
  const double len = core::hypot3(dx, dy, dz);
  if (len < 1e-9) return 1;
  const double step = step_m > 0.05 ? step_m : 0.05;
  const auto n = static_cast<int>(len / step);
  std::array<Obstacle, kObstacles> near{};
  for (int i = 1; i < n; ++i) {
    const double t = static_cast<double>(i) * step / len;
    const Vec3 p{from.x + dx * t, from.y + dy * t, from.z + dz * t};
    if (p.y < ground_at(w, p.x, p.z) - 0.05) return t;
    const int k = obstacles_near(w, p.x, p.z, 0, near.data(), kObstacles);
    for (int o = 0; o < k; ++o) {
      const Obstacle& b = near[static_cast<size_t>(o)];
      if (p.y < b.y0 || p.y > b.y1) continue;
      if (core::hypot(p.x - b.x, p.z - b.z) < b.radius) return t;
    }
  }
  return clear_share(w, from, to);
}

Planting plant_beside(std::span<const PathPoint> path, const PlantSpec& spec, const World& w) {
  Planting best;
  if (path.empty()) return best;
  const size_t key = spec.key < path.size() ? spec.key : path.size() - 1;
  double fx = 0, fz = 0;
  travel_at(path, key, fx, fz);
  // Right of the travel: heading 0 = +z, clockwise from above, so right is (fz, -fx).
  const double rx = fz, rz = -fx;
  const Vec3 at = path[key].pos;
  constexpr std::array<double, 3> kRings{1.0, 0.75, 1.35};
  constexpr std::array<double, 5> kTurns{0, 0.44, -0.44, 0.87, -0.87};  // rad: 0, ±25°, ±50°
  std::array<double, 2> sides{1, -1};
  int side_count = 2;
  if (spec.sides == 1) side_count = 1;
  if (spec.sides == -1) {
    sides[0] = -1;
    side_count = 1;
  }
  const int every = spec.every > 0 ? spec.every : 1;
  double best_score = -1;
  int candidate = 0;
  for (const double ring : kRings) {
    for (const double turn : kTurns) {
      for (int si = 0; si < side_count; ++si, ++candidate) {
        // The spec's offset in the key's frame, turned about the key point and scaled.
        const double across = spec.across_m * sides[static_cast<size_t>(si)] * ring;
        const double along = spec.along_m * ring;
        const double c = libm::cos(turn), s = libm::sin(turn);
        const double a2 = across * c - along * s, l2 = across * s + along * c;
        Vec3 eye{at.x + rx * a2 + fx * l2, 0, at.z + rz * a2 + fz * l2};
        const double g = ground_at(w, eye.x, eye.z);
        if (!(g > -1e300)) continue;
        eye.y = g + spec.height_m;
        bool near_path = false;
        int tested = 0, seen = 0;
        for (size_t i = 0; i < path.size(); i += static_cast<size_t>(every)) {
          const Vec3& p = path[i].pos;
          if (core::hypot(p.x - eye.x, p.z - eye.z) < spec.least_m) {
            near_path = true;
            break;
          }
          ++tested;
          const Vec3 aim{p.x, p.y + spec.aim_height_m, p.z};
          if (sight_share(w, eye, aim) >= 0.999) ++seen;
        }
        if (near_path || tested == 0) continue;
        double score = static_cast<double>(seen) / static_cast<double>(tested);
        const double raw = score;
        if (score >= spec.seen_enough) score = 1;
        if (score > best_score) {
          best_score = score;
          best.seen = raw;
          best.candidate = candidate;
          best.row.eye = eye;
        }
      }
    }
  }
  if (best.candidate < 0) return best;
  best.row.aim_height_m = spec.aim_height_m;
  best.row.hold_m = spec.hold_m;
  best.row.fov_y = spec.fov_y;
  best.row.fov_min = spec.fov_min;
  best.row.fov_max = spec.fov_max;
  return best;
}

Vec3 smoothed_at(std::span<const PathPoint> path, size_t i, size_t half_steps) {
  if (path.empty()) return {0, 0, 0};
  const size_t n = path.size();
  i = i < n ? i : n - 1;
  double x = 0, y = 0, z = 0;
  const auto h = static_cast<long long>(half_steps);
  for (long long k = -h; k <= h; ++k) {
    long long j = static_cast<long long>(i) + k;
    j = j < 0 ? 0 : (j >= static_cast<long long>(n) ? static_cast<long long>(n) - 1 : j);
    const Vec3& p = path[static_cast<size_t>(j)].pos;
    x += p.x;
    y += p.y;
    z += p.z;
  }
  const double c = static_cast<double>(2 * h + 1);
  return {x / c, y / c, z / c};
}

Pose planted_at(const PlantedRow& row, std::span<const PathPoint> path, size_t i,
                size_t half_steps) {
  const Vec3 m = smoothed_at(path, i, half_steps);
  const Vec3 aim{m.x, m.y + row.aim_height_m, m.z};
  const double range = core::hypot3(aim.x - row.eye.x, aim.y - row.eye.y, aim.z - row.eye.z);
  return aim_pose(row.eye, aim, 0, fov_for(row.hold_m, range, row.fov_y, row.fov_min, row.fov_max));
}

int direct_broadcast(std::span<const PlantedRow> lenses, const Subject& s, const World& w,
                     const BroadcastRule& rule, BroadcastState& st, double dt, const Vec3& line) {
  st.cut = false;
  const int n = static_cast<int>(lenses.size());
  if (n == 0) return -1;
  const Vec3 aim{s.pos.x, s.pos.y + rule.aim_height_m, s.pos.z};
  // The line of travel: the velocity's plan direction, eased so one turn does not swing it.
  const double given = core::hypot(line.x, line.z);
  const double sp = core::hypot(s.vel.x, s.vel.z);
  if (given > 1e-9) {
    st.line_x = line.x / given;
    st.line_z = line.z / given;
  } else if (sp > 1e-6) {
    const double ux = s.vel.x / sp, uz = s.vel.z / sp;
    const bool fresh = st.line_x == 0 && st.line_z == 0;
    st.line_x = fresh ? ux : damp(st.line_x, ux, rule.line_halflife_s, dt);
    st.line_z = fresh ? uz : damp(st.line_z, uz, rule.line_halflife_s, dt);
  }
  const double vx = s.vel.x, vz = s.vel.z;
  const double lx = st.line_x, lz = st.line_z;
  struct Look {
    double range = 0;
    bool sees = false, toward = false;
    double side = 0;
  };
  auto look = [&](int k) {
    const Vec3& e = lenses[static_cast<size_t>(k)].eye;
    Look l;
    const double dx = e.x - s.pos.x, dz = e.z - s.pos.z;
    l.range = core::hypot3(aim.x - e.x, aim.y - e.y, aim.z - e.z);
    l.toward = dx * vx + dz * vz > 0;
    const double across = lx * dz - lz * dx, plan = core::hypot(dx, dz);
    // Which side of the line the lens stands on; 0 looking nearly along it (a neutral shot).
    l.side = std::fabs(across) <= rule.neutral * plan ? 0 : across > 0 ? 1 : -1;
    l.sees = l.range <= rule.range_m && sight_share(w, e, aim) >= rule.sight;
    return l;
  };
  // The best lens other than `skip` that can be live: running toward it, nearest first, then
  // receding ones nearest first; among those near in range, one not across from `keep` wins.
  auto pick = [&](int skip, double keep) {
    int best = -1;
    Look bl;
    for (int k = 0; k < n; ++k) {
      if (k == skip) continue;
      const Look l = look(k);
      if (!l.sees) continue;
      bool better = best < 0;
      if (!better) {
        if (l.toward != bl.toward) {
          better = l.toward;
        } else {
          const bool near = l.range < bl.range * 1.5 && bl.range < l.range * 1.5;
          if (keep != 0 && near && (l.side != -keep) != (bl.side != -keep)) {
            better = l.side != -keep;
          } else {
            better = l.range < bl.range;
          }
        }
      }
      if (better) {
        best = k;
        bl = l;
      }
    }
    return best;
  };
  st.held_s += dt;
  if (st.live < 0 || st.live >= n) {
    st.live = pick(-1, 0);
    if (st.live < 0) st.live = 0;
    st.held_s = 0;
    st.lost_s = 0;
    st.cut = true;
    return st.live;
  }
  const Look live = look(st.live);
  st.lost_s = live.sees ? 0 : st.lost_s + dt;
  const bool must = st.lost_s >= rule.lost_s || live.range > rule.range_m;
  const bool want = st.held_s >= rule.min_hold_s && !live.toward && live.range > rule.behind_m;
  if (!must && !want) return st.live;
  const int next = pick(st.live, live.side);
  if (next < 0) return st.live;
  const Look nl = look(next);
  // A planned cut waits for a lens the subject runs toward; a forced one takes what sees.
  if (!must && !nl.toward) return st.live;
  st.crossed += nl.side * live.side < 0 ? 1 : 0;
  st.cuts++;
  st.live = next;
  st.held_s = 0;
  st.lost_s = 0;
  st.cut = true;
  return st.live;
}

}  // namespace ge::camera
