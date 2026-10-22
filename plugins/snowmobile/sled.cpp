// The snowmobile's step around its body (sled.hpp): the trunks, the map's edge, the flight's
// report and what throws the rider; standing it on the snow; the digest.
//
// ge:summary step_sled: the body, the trunks, the edge, the landing judged, the rider thrown
// ge:keywords snowmobile step_sled trunks capsule bounds landing judge thrown wall rolled
// ge:keywords stand_sled boards_at sled_hash thrown_name
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#include "snowmobile/sled.hpp"

#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "skier/tuning.hpp"
#include "snowmobile/body.hpp"

namespace ge::snowmobile {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
namespace lm = core::libm;
namespace K = spec::crash;
using core::Vec3;

// THE TRUNKS against the machine's plan, a capsule down its length: pushed out, the closing speed
// taken off and a little back, the blow off its spine turning it about its CoG. Returns the
// hardest closing speed met, m/s, and the trunk.
double sled_trees(SledState& s, const SledWorld& world, int32_t& tree) {
  const mg::Level& level = *world.level;
  constexpr double kHalfL = spec::kLength / 2;
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  double worst = 0;
  skier::for_each_tree_near(*world.trees, s.x, s.z, [&](int32_t i) {
    const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
    if (s.y < t.y - 1 || s.y > t.y + t.height) return;
    // The nearest point on the machine's spine to the trunk.
    const double u = core::clamp((t.x - s.x) * fx + (t.z - s.z) * fz, -kHalfL, kHalfL);
    const double dx = s.x + fx * u - t.x;
    const double dz = s.z + fz * u - t.z;
    const double d = core::hypot(dx, dz);
    const double reach = spec::kHalfWidth + t.radius;
    if (d >= reach) return;
    const double nx = d > 1e-6 ? dx / d : -fx;
    const double nz = d > 1e-6 ? dz / d : -fz;
    const double vn = s.vx * nx + s.vz * nz;
    s.x += nx * (reach - d);
    s.z += nz * (reach - d);
    if (vn >= 0) return;
    s.vx -= 1.25 * vn * nx;
    s.vz -= 1.25 * vn * nz;
    s.wy += u * (nx * fz - nz * fx) * -vn * 0.25;
    if (-vn > worst) {
      worst = -vn;
      tree = i;
    }
  });
  return worst;
}

// The map's edge: the machine held inside it.
void sled_bounds(SledState& s, const mg::Level& level) {
  const double lo = skier::tuning::wipeout::kEdgeMargin;
  const double hi = level.size - lo;
  if (s.x < lo) {
    s.x = lo;
    s.vx = num::max(0.0, s.vx);
  } else if (s.x > hi) {
    s.x = hi;
    s.vx = num::min(0.0, s.vx);
  }
  if (s.z < lo) {
    s.z = lo;
    s.vz = num::max(0.0, s.vz);
  } else if (s.z > hi) {
    s.z = hi;
    s.vz = num::min(0.0, s.vz);
  }
}

// THE LANDING JUDGED, the step a flight ends: `into` the speed it came down into the snow along
// its normal, and `off` how far past what the rider rides out it is (1 throws him): the larger of
// what the drop asks of the suspension and the snow (the groomer gives little, powder a deal) and
// how far off its belt it came down (square is nothing, on its side or its nose all of it, given
// a landing worth the name).
void judge_landing(const SledState& s, const mg::Level& level, const Vec3& v0, double& into,
                   double& off) {
  mg::Vec3 n{0, 1, 0};
  level.normal_at(s.x, s.z, n);
  into = num::max(0.0, -(v0.x * n.x + v0.y * n.y + v0.z * n.z));
  const double soft = 1 - s.packed;
  const double most = K::kLanding + (K::kLandingPowder - K::kLanding) * soft;
  const Vec3 up = core::rotate(s.q, Vec3{0, 1, 0});
  const double square = up.x * n.x + up.y * n.y + up.z * n.z;
  const double tilt = into > K::kTiltFrom ? lm::acos(core::clamp(square, -1, 1)) / K::kTilt : 0;
  off = num::max(into / most, tilt);
}

}  // namespace

const char* thrown_name(Thrown t) {
  switch (t) {
    case Thrown::none:
      return "none";
    case Thrown::tree:
      return "tree";
    case Thrown::rolled:
      return "rolled";
    case Thrown::landing:
      return "landing";
    case Thrown::wall:
      return "wall";
  }
  return "none";
}

void step_sled(SledState& s, const SledControls& in, double rider, bool ridden,
               const SledWorld& world, double dt, SledEvents& events) {
  events = SledEvents{};
  const mg::Level& level = *world.level;
  if (s.hit_wait > 0) s.hit_wait -= dt;
  // Left to itself: the controls let go and the parking brake on, so it sits on a slope rather
  // than freewheeling down it.
  SledControls ctl = in;
  if (!ridden) ctl = SledControls{0, 1, 0, 0};
  const double air0 = s.air_time;
  const Vec3 v0{s.vx, s.vy, s.vz};
  const double fx0 = lm::sin(s.heading);
  const double fz0 = lm::cos(s.heading);
  const double way0 = v0.x * fx0 + v0.z * fz0;
  const BodyReport r = ride_body(s, ctl, sled_mass(rider), rider, world, dt);
  double off = 0;
  if (r.landed > 0) judge_landing(s, level, v0, events.into, off);
  if (s.airborne && s.air_time >= spec::air::kCounts && air0 < spec::air::kCounts) {
    events.took_off = true;
  }
  if (r.landed >= spec::air::kCounts) {
    events.landed = true;
    events.flight_s = r.landed;
    events.off = off;
  }
  int32_t tree = -1;
  const double hit = world.trees ? sled_trees(s, world, tree) : 0;
  if (hit > K::kHitSay && s.hit_wait <= 0) {
    s.hit_wait = 0.5;
    events.hit = hit;
    events.tree = tree;
  }
  sled_bounds(s, level);
  // PAST SAVING: on its side or its back on the snow, or looped.
  const Vec3 up = core::rotate(s.q, Vec3{0, 1, 0});
  s.over_for = up.y < lm::cos(K::kOver) ? s.over_for + dt : 0;
  if (!ridden) return;
  // INTO A WALL: the way it was going stopped dead in a step. Nothing on snow stops it so.
  const double stopped = way0 - (s.vx * fx0 + s.vz * fz0);
  if (hit > K::kTree) {
    events.thrown = Thrown::tree;
  } else if (r.landed > 0 && off > 1) {
    events.thrown = Thrown::landing;
  } else if (stopped > K::kWall) {
    events.thrown = Thrown::wall;
  } else if (s.over_for > K::kOverFor) {
    events.thrown = Thrown::rolled;
  }
}

SledState stand_sled(const mg::Level& level, double x, double z, double heading) {
  SledState s;
  const double fx = lm::sin(heading);
  const double fz = lm::cos(heading);
  constexpr double kL = 1.2;
  constexpr double kW = 0.45;
  const double pitch = lm::atan2(
      level.ground_at(x + fx * kL, z + fz * kL) - level.ground_at(x - fx * kL, z - fz * kL),
      2 * kL);
  const double roll = lm::atan2(
      level.ground_at(x - fz * kW, z + fx * kW) - level.ground_at(x + fz * kW, z - fx * kW),
      2 * kW);
  s.x = x;
  s.z = z;
  s.y = level.ground_at(x, z) + spec::kCogHeight / lm::cos(pitch);
  s.q = core::from_euler(heading, pitch, roll);
  s.packed = level.packed_at(x, z);
  derive_sled(s);
  return s;
}

Vec3 boards_at(const SledState& s) {
  const Vec3 p = core::rotate(
      s.q, Vec3{s.rider_right * 0.5, spec::kBoards.y, spec::kBoards.z - s.rider_aft * 0.5});
  return {s.x + p.x, s.y + p.y, s.z + p.z};
}

uint64_t sled_hash(const SledState& s, uint64_t h) {
  const auto fold = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  for (const double v : {s.x,
                         s.y,
                         s.z,
                         s.vx,
                         s.vy,
                         s.vz,
                         s.q.x,
                         s.q.y,
                         s.q.z,
                         s.q.w,
                         s.wx,
                         s.wy,
                         s.wz,
                         s.ski_angle,
                         s.rider_right,
                         s.rider_aft,
                         s.roll_aim,
                         s.rpm,
                         s.tread_speed,
                         s.controls.throttle,
                         s.controls.brake,
                         s.controls.lean,
                         s.air_time,
                         s.over_for}) {
    fold(v);
  }
  for (size_t i = 0; i < s.sinks.size(); i++) {
    fold(s.sinks[i]);
    fold(s.comps[i]);
  }
  return h;
}

}  // namespace ge::snowmobile
