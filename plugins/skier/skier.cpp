// The skier's step (skier.hpp has the model, in order). Every transcendental is core/libm's and
// every vector is plain doubles, so a step is the same bits on every compiler.
//
// ge:summary step_skier's body: contact impulses, grip, yaw, flight, landing, trunks, wipeout
// ge:keywords skier step_skier contact impulse grip yaw landing takeoff wipeout trees
// ge:keywords stand_skier skier_hash
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#include "skier/skier.hpp"

#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "skier/limits.hpp"
#include "skier/snow.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

namespace {

using mountain::mapgen::Level;
using mountain::mapgen::Vec3;

double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The skis' way laid on the snow's plane: the heading's forward with its part along the normal
// taken out, unit length. Straight down the normal (never on a skiable face) falls back to flat.
Vec3 forward_on(double heading, const Vec3& n) {
  const Vec3 f{core::libm::sin(heading), 0, core::libm::cos(heading)};
  const double fn = dot(f, n);
  Vec3 t{f.x - fn * n.x, f.y - fn * n.y, f.z - fn * n.z};
  const double len = core::hypot3(t.x, t.y, t.z);
  if (len <= 1e-9) return f;
  return Vec3{t.x / len, t.y / len, t.z / len};
}

// n x a: the right of `a` on the plane of `n` (a right-handed y-up frame).
Vec3 right_of(const Vec3& n, const Vec3& a) {
  return Vec3{n.y * a.z - n.z * a.y, n.z * a.x - n.x * a.z, n.x * a.y - n.y * a.x};
}

void wipe_out(SkierState& s, SkierEvents& ev, Wipeout cause) {
  put_down(s, cause);
  ev.wiped_out = true;
  ev.cause = cause;
}

}  // namespace

void put_down(SkierState& s, Wipeout cause) {
  s.down = true;
  s.cause = cause;
  s.down_time = 0;
  s.jump_held = 0;
  s.thrown.active = false;
}

Over over_of(Wipeout cause) {
  switch (cause) {
    case Wipeout::tree:  // the trunk stops the skis and he goes on over them, away from it
      return Over{1, 0.25, 1.8};
    case Wipeout::landing:  // the legs could not hold it: sat down back and to a side
      return Over{-0.45, 0.8, 0.2};
    case Wipeout::across:  // the edge bites and stops the slide: flung over it
      return Over{1, 0.2, 1.2};
    case Wipeout::hurt:  // mortally hurt on his feet: he crumples onto a side
      return Over{0.1, 1, 0};
    case Wipeout::none:
      break;
  }
  return Over{0.1, 1, 0.3};
}

namespace {

// THE GROUND after the move: back onto the support below it (a landing if he was flying), off it
// when it falls away past the legs' reach. Returns nothing; writes the state and the events.
void meet_ground(SkierState& s, const Level& level, double dt, SkierEvents& ev) {
  namespace a = tuning::air;
  const double support = level.ground_at(s.x, s.z) - s.sink;
  const double gap = s.y - support;
  if (gap <= 0) {
    s.y = support;
    if (s.on_snow) return;
    Vec3 n;
    level.normal_at(s.x, s.z, n);
    const double vn = s.vx * n.x + s.vy * n.y + s.vz * n.z;
    double load = 1;
    if (vn < 0) {
      s.vx -= vn * n.x;
      s.vy -= vn * n.y;
      s.vz -= vn * n.z;
      load += vn * vn / (2 * a::kStroke * tuning::kGravity);
    }
    const double flight = s.air_time;
    s.on_snow = true;
    s.air_time = 0;
    s.air_peak = 0;
    if (s.down) return;
    if (flight >= a::kFlightMin) {
      ev.landed = true;
      ev.landing_g = load;
      ev.flight_s = flight;
    }
    const double plan = core::hypot(s.vx, s.vz);
    double off = 0;
    if (plan > a::kAcrossSpeed) {
      off = core::angle_diff(s.heading, core::libm::atan2(s.vx, s.vz));
      off = off < 0 ? -off : off;
      if (off > core::kPi / 2) off = core::kPi - off;  // landed switch is landed
    }
    ev.landing_off = off;
    if (load > a::kHarshG) {
      wipe_out(s, ev, Wipeout::landing);
    } else if (off > a::kAcross) {
      wipe_out(s, ev, Wipeout::across);
    }
    return;
  }
  if (s.on_snow) {
    if (gap > a::kLeaveGap) {
      s.on_snow = false;
      s.air_time = 0;
      s.air_peak = gap;
    }
    return;
  }
  const double before = s.air_time;
  s.air_time += dt;
  if (gap > s.air_peak) s.air_peak = gap;
  if (!s.down && before < a::kFlightMin && s.air_time >= a::kFlightMin) ev.took_off = true;
}

// THE TRUNKS: pushed back out of any trunk he overlaps; met fast, a wipeout.
void meet_trees(SkierState& s, const World& world, SkierEvents& ev) {
  namespace w = tuning::wipeout;
  const Level& level = *world.level;
  const int32_t i = tree_overlap(*world.trees, level, s.x, s.z, w::kBodyRadius);
  if (i < 0) return;
  const auto& t = level.trees[static_cast<size_t>(i)];
  if (s.y > t.y + t.height) return;  // flying over its crown
  double dx = s.x - t.x;
  double dz = s.z - t.z;
  double d = core::hypot(dx, dz);
  if (d <= 1e-9) {
    dx = -core::libm::sin(s.heading);
    dz = -core::libm::cos(s.heading);
    d = 1;
  }
  const double nx = dx / d;
  const double nz = dz / d;
  const double reach = t.radius + w::kBodyRadius;
  s.x = t.x + nx * reach;
  s.z = t.z + nz * reach;
  const double closing = -(s.vx * nx + s.vz * nz);
  if (closing <= 0) return;
  s.vx += closing * nx;
  s.vz += closing * nz;
  if (s.down) return;
  ev.tree = i;
  ev.closing = closing;
  if (closing > w::kTreeSpeed) {
    // The trunk stops the body; what was going past it is mostly lost in the fall.
    s.vx *= 0.3;
    s.vz *= 0.3;
    wipe_out(s, ev, Wipeout::tree);
  } else {
    ev.glanced = true;
  }
}

// The world's edge holds him inside it.
void meet_edge(SkierState& s, double size) {
  const double lo = tuning::wipeout::kEdgeMargin;
  const double hi = size - lo;
  if (s.x < lo) {
    s.x = lo;
    if (s.vx < 0) s.vx = 0;
  } else if (s.x > hi) {
    s.x = hi;
    if (s.vx > 0) s.vx = 0;
  }
  if (s.z < lo) {
    s.z = lo;
    if (s.vz < 0) s.vz = 0;
  } else if (s.z > hi) {
    s.z = hi;
    if (s.vz > 0) s.vz = 0;
  }
}

// THROW HIM off his skis, going at (vx, vy, vz): over onto the side away from the trunk that
// threw him, or otherwise the side he was leaning toward.
void throw_down(SkierState& s, const World& world, int32_t tree, double vx, double vy, double vz,
                double dt) {
  const double flat = core::hypot(vx, vz);
  const double heading = flat > 1 ? core::libm::atan2(vx, vz) : s.heading;
  double side = s.lean >= 0 ? 1 : -1;
  if (tree >= 0) {
    const auto& t = world.level->trees[static_cast<size_t>(tree)];
    const double across =
        (t.x - s.x) * core::libm::cos(heading) - (t.z - s.z) * core::libm::sin(heading);
    side = across > 0 ? -1 : 1;
  }
  s.thrown =
      throw_of(s.x, s.y, s.z, heading, s.lean, vx, vy, vz, s.yaw_rate, side, over_of(s.cause), dt);
}

// A STEP OF THE THROWN BODY, the state following its centre of mass.
void step_thrown(SkierState& s, const World& world, double dt) {
  namespace rd = tuning::ragdoll;
  Ragdoll& b = s.thrown;
  step_ragdoll(b, *world.level, world.trees, dt);
  s.x = b.x;
  s.z = b.z;
  s.vx = b.vx;
  s.vy = b.vy;
  s.vz = b.vz;
  s.speed = core::hypot3(s.vx, s.vy, s.vz);
  s.on_snow = b.touching;
  s.packed = core::clamp(world.level->packed_at(s.x, s.z), 0, 1);
  s.y = b.touching ? world.level->ground_at(s.x, s.z) - s.sink : b.y - rd::kStand;
  if (core::hypot(s.vx, s.vz) > 0.5) s.heading = core::libm::atan2(s.vx, s.vz);
  s.yaw_rate = 0;
  s.edge = 0;
  s.tuck = 0;
  s.lean = 0;
  s.g_lat = 0;
  s.air_time = b.aloft;
  s.down_time = b.still;
}

}  // namespace

const char* wipeout_name(Wipeout w) {
  switch (w) {
    case Wipeout::none:
      return "none";
    case Wipeout::tree:
      return "tree";
    case Wipeout::landing:
      return "landing";
    case Wipeout::across:
      return "across";
    case Wipeout::hurt:
      return "hurt";
  }
  return "none";
}

void step_skier(SkierState& s, const SkierInput& in, const World& world, const SkiSpec& spec,
                double dt, SkierEvents& ev) {
  namespace sn = tuning::snow;
  namespace st = tuning::steer;
  namespace dr = tuning::drive;
  namespace jp = tuning::jump;
  namespace w = tuning::wipeout;
  ev = SkierEvents{};
  const Level& level = *world.level;
  const double g = tuning::kGravity;
  // Down and thrown, the step is the thrown body's.
  if (s.down && s.thrown.active) {
    step_thrown(s, world, dt);
    return;
  }

  // 1. The snow under him.
  s.packed = core::clamp(level.packed_at(s.x, s.z), 0, 1);
  const double v0 = core::hypot3(s.vx, s.vy, s.vz);
  s.sink += (sink_target(s.packed, v0) - s.sink) * core::num::min(1.0, dt / sn::kSinkLag);
  Vec3 n;
  level.normal_at(s.x, s.z, n);

  // 2. The controls; a body down has none.
  const double steer = s.down ? 0 : core::clamp(in.steer, -1, 1);
  const double tuck = s.down ? 0 : core::clamp(in.tuck, 0, 1);
  const double brake = s.down ? 0 : core::clamp(in.brake, 0, 1);
  s.tuck = core::approach(s.tuck, tuck, dr::kCrouchRate * dt);
  s.edge = core::approach(s.edge, steer * edge_most(spec, v0, s.tuck), st::kEdgeRate * dt);

  // 3. The free forces: gravity, the air, his own push.
  const double cda =
      s.down ? spec.cda_upright : core::lerp(spec.cda_upright, spec.cda_tuck, s.tuck);
  const double drag = 0.5 * tuning::kAirDensity * cda * v0 / spec.mass * dt;
  s.vx -= drag * s.vx;
  s.vy -= drag * s.vy + g * dt;
  s.vz -= drag * s.vz;
  const Vec3 ft = forward_on(s.heading, n);
  if (s.on_snow && !s.down && tuck > dr::kTuckPush && brake == 0) {
    const double along = s.vx * ft.x + s.vy * ft.y + s.vz * ft.z;
    const double share = 1 - core::smoothstep(dr::kFrom, dr::kTo, along);
    if (share > 0) {
      const double force =
          core::num::min(dr::kForceMost, dr::kPower / core::num::max(along, 0.5)) * share;
      const double dv = force / spec.mass * dt;
      s.vx += ft.x * dv;
      s.vy += ft.y * dv;
      s.vz += ft.z * dv;
    }
  }

  // 4. On the snow: the contact, its grip and its friction. 5. The yaw.
  if (s.on_snow) {
    const double vn = s.vx * n.x + s.vy * n.y + s.vz * n.z;
    double jn = 0;  // the normal impulse this step, per unit mass, m/s
    if (vn < 0) {
      s.vx -= vn * n.x;
      s.vy -= vn * n.y;
      s.vz -= vn * n.z;
      jn = -vn;
    }
    const Vec3 lt = right_of(n, ft);
    const double vf = s.vx * ft.x + s.vy * ft.y + s.vz * ft.z;
    const double vl = s.vx * lt.x + s.vy * lt.y + s.vz * lt.z;
    const double vnn = s.vx * n.x + s.vy * n.y + s.vz * n.z;
    const double load = spec.mass * jn / dt;  // N
    double vf2;
    double vl2;
    if (s.down) {
      // A body on the snow: one friction, every way along the snow's plane.
      const double slide = core::hypot(vf, vl);
      const double cut = w::kBodyFriction * jn;
      const double keep = slide <= cut ? 0 : (slide - cut) / slide;
      vf2 = vf * keep;
      vl2 = vl * keep;
      s.g_lat = 0;
    } else {
      const double grip =
          side_grip(spec, s.packed, s.edge) * (1 - (1 - tuning::grip::kBrakeHold) * brake);
      const double cap = grip * jn;
      vl2 = vl - core::clamp(vl, -cap, cap);
      s.g_lat = (vl2 - vl) / (dt * g);
      const double slow = (base_friction(s.packed) + brake * brake_grip(s.packed)) * jn +
                          powder_drag(s.packed, s.sink, v0, load) / spec.mass * dt;
      const double mag = vf < 0 ? -vf : vf;
      vf2 = mag <= slow ? 0 : vf - core::num::sign(vf) * slow;

      const double ef = vf2 < 0 ? -vf2 : vf2;
      const double hold = st::kCarveHold * cap / dt / core::num::max(ef, 1.0);
      const double carve =
          core::num::sign(s.edge) * core::num::min(ef * carve_curvature(spec, s.edge), hold);
      const double pivot = steer * core::lerp(st::kPivotSlow, st::kPivotFast,
                                              core::clamp(v0 / st::kPivotFade, 0, 1));
      const double slip = (vl2 == 0 && vf2 == 0) ? 0 : core::libm::atan2(vl2, ef);
      const double track = st::kAlign * slip * core::smoothstep(0, st::kAlignSpeed, v0);
      const double want = (vf2 < 0 ? -carve : carve) + pivot + track;
      s.yaw_rate += (want - s.yaw_rate) * core::num::min(1.0, dt / st::kYawLag);
    }
    s.vx = ft.x * vf2 + lt.x * vl2 + n.x * vnn;
    s.vy = ft.y * vf2 + lt.y * vl2 + n.y * vnn;
    s.vz = ft.z * vf2 + lt.z * vl2 + n.z * vnn;
    if (!s.down) {
      if (in.jump) {
        s.jump_held += dt;
      } else if (s.jump_held > 0) {
        const double pop =
            core::lerp(jp::kPopMin, jp::kPopMax, core::num::min(1.0, s.jump_held / jp::kPopFull));
        s.vx += n.x * pop;
        s.vy += n.y * pop;
        s.vz += n.z * pop;
        s.jump_held = 0;
      }
    }
  } else {
    // 6. In the air.
    s.g_lat = 0;
    s.jump_held = 0;
    s.yaw_rate += (steer * tuning::air::kSpin - s.yaw_rate) * core::num::min(1.0, dt / st::kYawLag);
  }
  if (s.down) s.yaw_rate += (0 - s.yaw_rate) * core::num::min(1.0, dt / st::kYawLag);
  s.heading = core::angle_diff(0, s.heading + s.yaw_rate * dt);
  const double balance = core::clamp(core::libm::atan(s.g_lat), -st::kLeanMost, st::kLeanMost);
  s.lean += (balance - s.lean) * core::num::min(1.0, dt / st::kLeanLag);

  // 7. The move, the world's edge, the ground. 8. The trunks.
  const double before_x = s.vx;
  const double before_y = s.vy;
  const double before_z = s.vz;
  s.x += s.vx * dt;
  s.y += s.vy * dt;
  s.z += s.vz * dt;
  meet_edge(s, level.size);
  meet_ground(s, level, dt, ev);
  meet_trees(s, world, ev);

  s.speed = core::hypot3(s.vx, s.vy, s.vz);
  if (s.down) {
    s.tuck = 0;
    s.edge = 0;
    s.down_time = 0;
    // Thrown at the velocity he had before the blow that threw him (a trunk's stop is the body's
    // to meet), or at the one he has now when he was put down from outside.
    if (ev.wiped_out) {
      throw_down(s, world, ev.tree, before_x, before_y, before_z, dt);
    } else {
      throw_down(s, world, -1, s.vx, s.vy, s.vz, dt);
    }
  }
}

SkierState stand_skier(const World& world, double x, double z, double heading, double speed,
                       double lift) {
  const Level& level = *world.level;
  SkierState s;
  s.x = x;
  s.z = z;
  s.heading = core::angle_diff(0, heading);
  s.packed = core::clamp(level.packed_at(x, z), 0, 1);
  s.sink = sink_target(s.packed, speed);
  s.y = level.ground_at(x, z) - s.sink + lift;
  Vec3 n;
  level.normal_at(x, z, n);
  const Vec3 ft = lift > 0 ? Vec3{core::libm::sin(heading), 0, core::libm::cos(heading)}
                           : forward_on(heading, n);
  s.vx = ft.x * speed;
  s.vy = ft.y * speed;
  s.vz = ft.z * speed;
  s.speed = speed;
  s.on_snow = lift <= 0;
  s.air_peak = lift > 0 ? lift : 0;
  return s;
}

uint64_t skier_hash(const SkierState& s, uint64_t h) {
  const double nums[] = {s.x,         s.y,        s.z,        s.vx,       s.vy,
                         s.vz,        s.heading,  s.yaw_rate, s.edge,     s.tuck,
                         s.lean,      s.sink,     s.packed,   s.speed,    s.g_lat,
                         s.jump_held, s.air_time, s.air_peak, s.down_time};
  h = ragdoll_hash(s.thrown, h);
  for (double v : nums) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  }
  const uint8_t flags = static_cast<uint8_t>((s.on_snow ? 1 : 0) | (s.down ? 2 : 0) |
                                             (static_cast<uint8_t>(s.cause) << 2));
  h ^= flags;
  h *= 0x100000001b3ull;
  return h;
}

}  // namespace ge::skier
