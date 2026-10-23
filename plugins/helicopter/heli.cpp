// The helicopter's step (heli.hpp).
//
// ge:summary step_heli: the rotor, the air, the disc, the fuselage, the pedals, the skids, the
// strike ge:keywords helicopter step_heli fly thrust_most induced-flow lie landed-hard strike crash
// ge:keywords edge-wind turned-back stand_heli heli_hash
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#include "helicopter/heli.hpp"

#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "helicopter/pilot.hpp"
#include "helicopter/spec.hpp"

namespace ge::helicopter {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
namespace lm = core::libm;
namespace F = spec::flight;
using core::Vec3;

constexpr double kG = 9.81;

// An angle kept to (-pi, pi].
double wrap(double a) { return core::angle_diff(0, a); }

// THE EDGE'S WIND and how deep into its band a machine at (x, z), `agl` m over the snow, is.
struct Edge {
  double depth = 0;
  double wx = 0;
  double wz = 0;
};

// The band along the map's edge a machine is met over, m: the narrow one at a hover, wider by the
// seconds it takes to come round at the speed it is carried out at.
double band_of(const mg::Level& level, const HeliState& h) {
  const double half = level.size / 2;
  const double home = lm::atan2(half - h.x, half - h.z);
  const double out = -(h.vx * lm::sin(home) + h.vz * lm::cos(home));
  return spec::edge::kBand + spec::edge::kTurn * num::max(0.0, out);
}

Edge edge_at(const mg::Level& level, const HeliState& h) {
  const double band = band_of(level, h);
  const double size = level.size;
  const auto into = [band](double lo, double hi) {
    return num::max(0.0, (band - lo) / band) - num::max(0.0, (band - hi) / band);
  };
  const double ax = core::clamp(into(h.x, size - h.x), -2.0, 2.0);
  const double az = core::clamp(into(h.z, size - h.z), -2.0, 2.0);
  const double high =
      core::clamp((h.agl - spec::edge::kLow) / (spec::edge::kHigh - spec::edge::kLow), 0.0, 1.0);
  Edge e;
  e.depth = num::max(std::fabs(ax), std::fabs(az)) * high;
  e.wx = spec::edge::kWind * ax * std::fabs(ax) * high;
  e.wz = spec::edge::kWind * az * std::fabs(az) * high;
  return e;
}

// How the snow lies under the skids: the pitch and roll it stands at, and the steeper of the two.
struct Lie {
  double pitch = 0, roll = 0, slope = 0;
};

Lie lie_of(const mg::Level& level, const HeliState& h) {
  const double fx = lm::sin(h.heading);
  const double fz = lm::cos(h.heading);
  constexpr double kL = (spec::skid::kFront - spec::skid::kBack) / 2;
  constexpr double kW = spec::skid::kTrack / 2;
  Lie l;
  l.pitch = lm::atan2(
      level.ground_at(h.x + fx * kL, h.z + fz * kL) - level.ground_at(h.x - fx * kL, h.z - fz * kL),
      2 * kL);
  l.roll = lm::atan2(
      level.ground_at(h.x - fz * kW, h.z + fx * kW) - level.ground_at(h.x + fz * kW, h.z - fx * kW),
      2 * kW);
  l.slope = num::max(std::fabs(l.pitch), std::fabs(l.roll));
  return l;
}

// A touchdown the skids do not ride: too fast down or along, the airframe stood too far off the
// snow's lean, or snow too steep to sit on.
bool landed_hard(const mg::Level& level, const HeliState& h, double sink, double slide) {
  const Lie lie = lie_of(level, h);
  const double off = num::max(std::fabs(core::angle_diff(lie.pitch, h.pitch)),
                              std::fabs(core::angle_diff(lie.roll, h.roll)));
  return sink > spec::crash::kSink || slide > spec::crash::kSlide || off > spec::crash::kTilt ||
         lie.slope > spec::crash::kSlope;
}

// THE CRASH: its rotor stopped where it struck; struck in the air (a crown, the rotor's tips) it
// falls the rest of the way as a wreck (`step_heli`), never put on the snow in one step; how
// hard, reported.
void crash(HeliState& h, const mg::Level& level, HeliCrash kind, HeliEvents& ev) {
  const double speed = core::hypot3(h.vx, h.vy, h.vz);
  // How fast it was coming down when the snow stopped it: struck on a crown or by the rotor's
  // tips, it falls the rest of the way first.
  const double ground = level.ground_at(h.x, h.z);
  const double fall = num::max(0.0, h.y - ground);
  const double down = num::max(0.0, -h.vy);
  const double sink = std::sqrt(down * down + 2 * kG * fall);
  ev.crash = kind;
  ev.speed = speed;
  ev.fall = sink;
  ev.stop = core::unrotate(heli_quat(h), Vec3{0, -sink, 0});
  h.crashed = kind;
  h.vx = h.vz = 0;
  h.vy = -down;
  h.yaw_rate = h.pitch_rate = h.roll_rate = 0;
  h.disc.pitch_rate = h.disc.roll_rate = 0;
  h.grounded = fall <= 0;
  if (h.grounded) {
    h.y = ground;
    h.vy = 0;
  }
  h.spool = 0;
  h.thrust = 0;
  h.collective = 0;
  h.controls = {};
}

// ONE STEP OF FLIGHT on `c`. False when the skids' touchdown crashed it.
bool fly(HeliState& h, const HeliControls& c, const HeliLoad& load, const HeliWorld& world,
         double dt, HeliEvents& ev) {
  const mg::Level& level = *world.level;
  const double m = heli_mass(load);
  const core::Quat q = heli_quat(h);
  const Vec3 right = core::rotate(q, Vec3{1, 0, 0});
  const Vec3 fwd = core::rotate(q, Vec3{0, 0, 1});
  const Vec3 up = core::rotate(q, Vec3{0, 1, 0});
  const Vec3 thrust_up = core::rotate(disc_quat(h), Vec3{0, 1, 0});
  const Vec3 hub = heli_point(h, Vec3{0, spec::rotor::kHub, spec::rotor::kAt});
  h.agl = hub.y - level.ground_at(hub.x, hub.z);
  // THE AIR it flies through: the caller's wind and the edge's.
  const Edge edge = edge_at(level, h);
  const double ax = h.vx - world.air_x - edge.wx;
  const double ay = h.vy;
  const double az = h.vz - world.air_z - edge.wz;
  const double af = ax * fwd.x + ay * fwd.y + az * fwd.z;
  const double as = ax * right.x + ay * right.y + az * right.z;
  const double au = ax * up.x + ay * up.y + az * up.z;
  constexpr double kHalf = 0.5 * spec::kDensity;
  const double df = -kHalf * spec::drag::kFront * std::fabs(af) * af;
  const double ds = -kHalf * spec::drag::kSide * std::fabs(as) * as;
  const double du = -kHalf * spec::drag::kPlan * std::fabs(au) * au;
  double fx = fwd.x * df + right.x * ds + up.x * du;
  double fy = fwd.y * df + right.y * ds + up.y * du;
  double fz = fwd.z * df + right.z * ds + up.z * du;
  // THE COLLECTIVE: its share of what the rotor gives at its speed squared, answered in
  // `kLag` s. The air through the disc in the disc's frame: `vn` along its thrust (climbing
  // positive), what the blades meet.
  const double along = core::hypot(ax, az);
  const double vn = ax * thrust_up.x + ay * thrust_up.y + az * thrust_up.z;
  const double most = thrust_most(along, num::max(-20.0, ay), h.agl, vn) * h.spool * h.spool;
  const double want = core::clamp(c.collective, 0.0, 1.0) * most;
  const double thrust =
      core::clamp(h.thrust + (want - h.thrust) * (1 - lm::exp(-dt / F::kLag)), 0.0, most);
  h.thrust = thrust;
  h.collective = most > 0 ? thrust / most : 0;
  fx += thrust_up.x * thrust;
  fy += thrust_up.y * thrust - m * kG;
  fz += thrust_up.z * thrust;
  h.vx += fx / m * dt;
  h.vy += fy / m * dt;
  h.vz += fz / m * dt;
  // ON THE SNOW, a machine sat on its skids stands as the snow lies, its disc with it; light on
  // them it turns on its pedals.
  const bool light = h.grounded && thrust > 0.6 * m * kG;
  const bool sat = h.grounded && thrust < 0.5 * m * kG;
  Lie lie;
  if (h.grounded) lie = lie_of(level, h);
  // THE DISC, flown by the cyclic against its damping and its flapback. Nothing stops it: held
  // over it carries on past the vertical and round.
  HeliDisc& d = h.disc;
  const double spool2 = h.spool * h.spool;
  const double cp = core::clamp(c.pitch, -1.0, 1.0);
  const double cr = core::clamp(c.roll, -1.0, 1.0);
  const double cpedal = core::clamp(c.pedal, -1.0, 1.0);
  if (sat) {
    d.pitch = lie.pitch;
    d.roll = lie.roll;
    d.pitch_rate = d.roll_rate = 0;
  } else {
    d.pitch_rate +=
        (-F::kCyclicPitch * spool2 * cp - F::kDampingPitch * d.pitch_rate + F::kFlapback * af) * dt;
    d.roll_rate +=
        (F::kCyclicRoll * spool2 * cr - F::kDampingRoll * d.roll_rate - F::kFlapback * as) * dt;
    d.pitch = wrap(d.pitch + d.pitch_rate * dt);
    d.roll = wrap(d.roll + d.roll_rate * dt);
  }
  // THE FUSELAGE swung under the disc, hanging off the centre line by the load's weight (its
  // share of the mass, over the hub's height over the centre of gravity).
  constexpr double kW = F::kHangFrequency;
  constexpr double kZ2 = 2 * F::kHangDamping * kW;
  const double hang =
      load.mass > 0 ? lm::atan2(load.mass / m * load.x, spec::rotor::kHub - spec::kCog) : 0.0;
  if (h.grounded && !light) {
    h.pitch = lie.pitch;
    h.roll = lie.roll;
    h.pitch_rate = h.roll_rate = 0;
  } else {
    // Swung the short way round to the disc, so it follows it over the top.
    h.pitch_rate += (kW * kW * core::angle_diff(h.pitch, d.pitch) - kZ2 * h.pitch_rate) * dt;
    h.roll_rate += (kW * kW * core::angle_diff(h.roll, d.roll + hang) - kZ2 * h.roll_rate) * dt;
    h.pitch = wrap(h.pitch + h.pitch_rate * dt);
    h.roll = wrap(h.roll + h.roll_rate * dt);
  }
  // THE PEDALS, the torque, the fin.
  if (h.grounded && !light) {
    h.yaw_rate = 0;
  } else {
    const double torque = F::kTorque * (thrust / (m * kG) - 1);
    const double vane = spec::drag::kVane * as * num::min(40.0, std::fabs(af) + std::fabs(as));
    h.yaw_rate += (F::kPedal * spool2 * cpedal - F::kYawDamping * h.yaw_rate - torque + vane) * dt;
  }
  h.heading = wrap(h.heading + h.yaw_rate * dt);
  h.x += h.vx * dt;
  h.y += h.vy * dt;
  h.z += h.vz * dt;
  // THE SKIDS ON THE SNOW.
  const double under = level.ground_at(h.x, h.z);
  const bool was = h.grounded;
  if (h.y <= under) {
    const double sink = -h.vy;
    const double slide = core::hypot(h.vx, h.vz);
    h.y = under;
    if (!was && landed_hard(level, h, sink, slide)) {
      crash(h, level, HeliCrash::landing, ev);
      return false;
    }
    h.vy = num::max(0.0, h.vy);
    // Skids on snow under a machine sat on them: held where they stand.
    const double held = lm::exp(-6 * dt);
    h.vx *= held;
    h.vz *= held;
    h.grounded = true;
    if (!was) {
      h.yaw_rate *= 0.2;
      ev.landed = true;
      ev.sink = sink;
    }
  } else if (h.y > under + 0.05) {
    h.grounded = false;
    if (was) ev.lifted = true;
  }
  return true;
}

// THE ROTOR AND THE AIRFRAME AGAINST THE MOUNTAIN: the disc's rim and the airframe's extremes
// against the snow, and the disc against every crown it sweeps.
void strike(HeliState& h, const HeliWorld& world, HeliEvents& ev) {
  const mg::Level& level = *world.level;
  const core::Quat q = heli_quat(h);
  constexpr double kR = spec::rotor::kRadius;
  const Vec3 hub = heli_point(h, Vec3{0, spec::rotor::kHub, spec::rotor::kAt});
  for (int i = 0; i < 12; i++) {
    const double a = i / 12.0 * core::kTau;
    const Vec3 tip = core::rotate(q, Vec3{lm::sin(a) * kR, 0, lm::cos(a) * kR});
    if (hub.y + tip.y < level.ground_at(hub.x + tip.x, hub.z + tip.z) + spec::crash::kClear) {
      crash(h, level, HeliCrash::rotor, ev);
      return;
    }
  }
  for (const spec::body::Point& p : spec::body::kStrike) {
    const Vec3 w = heli_point(h, Vec3{p.x, p.y, p.z});
    if (w.y < level.ground_at(w.x, w.z) - 0.05) {
      crash(h, level, HeliCrash::airframe, ev);
      return;
    }
  }
  if (!world.trees) return;
  bool hit = false;
  skier::for_each_tree_near(*world.trees, hub.x, hub.z, [&](int32_t i) {
    if (hit) return;
    const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
    const double top = t.y + t.height;
    if (hub.y > top) return;
    const double dist = core::hypot(t.x - hub.x, t.z - hub.z);
    // The crown's spread at the disc's height: widest low, a point at the top, a conifer's cone.
    const double spread = t.crown * num::min(1.0, (top - hub.y) / num::max(1.0, t.height * 0.8));
    if (dist < kR + spread * 0.7 || (dist < t.radius + 1.2 && h.y < top)) hit = true;
  });
  if (hit) crash(h, level, HeliCrash::tree, ev);
}

}  // namespace

const char* heli_crash_name(HeliCrash c) {
  switch (c) {
    case HeliCrash::none:
      return "none";
    case HeliCrash::landing:
      return "landing";
    case HeliCrash::rotor:
      return "rotor";
    case HeliCrash::airframe:
      return "airframe";
    case HeliCrash::tree:
      return "tree";
  }
  return "?";
}

core::Quat heli_quat(const HeliState& h) { return core::from_euler(h.heading, h.pitch, h.roll); }

core::Quat disc_quat(const HeliState& h) {
  return core::from_euler(h.heading, h.disc.pitch, h.disc.roll);
}

Vec3 heli_point(const HeliState& h, Vec3 p) {
  const Vec3 w = core::rotate(heli_quat(h), p);
  return Vec3{h.x + w.x, h.y + w.y, h.z + w.z};
}

double heli_mass(const HeliLoad& load) { return spec::kMass + load.mass; }

double thrust_most(double along, double vc, double over, double through) {
  constexpr double kRho = spec::kDensity;
  constexpr double kA = spec::rotor::kArea;
  const double avail = num::max(
      spec::kPower * 0.25, spec::kPower - 0.5 * kRho * spec::kFlatPlate * along * along * along);
  double t = spec::kMass * kG * 1.3;
  for (int k = 0; k < 6; k++) {
    const double vh2 = t / (2 * kRho * kA);
    // Below a descent of v_h the momentum stream reverses (the vortex ring): the relation is held
    // at its edge.
    const double c = num::max(vc, -std::sqrt(vh2));
    double vi = std::sqrt(vh2);
    for (int j = 0; j < 4; j++) vi = vh2 / num::max(0.5, core::hypot(along, c + vi));
    const double need = t * num::max(0.5, c + vi);
    t *= lm::pow(avail / need, 2.0 / 3.0);
  }
  // The blades' own inflow: the same relation, through the disc's way along its thrust.
  const double vh = std::sqrt(t / (2 * kRho * kA));
  const double n = num::max(through, -vh);
  double vb = vh;
  for (int j = 0; j < 6; j++) {
    vb = 0.5 * (vb + vh * vh / num::max(0.5 * vh, core::hypot(along, n + vb)));
  }
  t = num::min(t, num::max(0.0, spec::blades::kLift * (spec::blades::kPitchMost / 3 -
                                                       (n + vb) / spec::rotor::kTip / 2)));
  constexpr double kR = spec::rotor::kRadius;
  const double z = num::max(over, kR / 4 * 1.05);
  const double r = kR / (4 * z);
  const double ground = num::min(spec::kGroundMost, 1 / (1 - r * r));
  return t * ground;
}

void step_heli(HeliState& h, const HeliControls& in, const HeliLoad& load, double spool_to,
               const HeliWorld& world, double dt, HeliEvents& ev) {
  ev = HeliEvents{};
  if (h.crashed != HeliCrash::none) {
    // THE WRECK falls to the snow under it and lies there.
    if (h.grounded) return;
    h.vy -= kG * dt;
    h.y += h.vy * dt;
    const double ground = world.level->ground_at(h.x, h.z);
    if (h.y <= ground) {
      h.y = ground;
      h.vy = 0;
      h.grounded = true;
    }
    return;
  }
  h.spool = core::clamp(h.spool + num::sign(spool_to - h.spool) * F::kSpool * dt, 0.0, 1.0);
  h.controls = in;
  if (!fly(h, in, load, world, dt, ev)) return;
  h.rotor = num::mod(h.rotor + spec::rotor::kOmega * h.spool * dt, core::kTau);
  h.tail_rotor = num::mod(h.tail_rotor + spec::tail::kOmega * h.spool * dt, core::kTau);
  if (!h.grounded || h.spool > 0) strike(h, world, ev);
}

HeliState stand_heli(const mg::Level& level, double x, double z, double heading) {
  HeliState h;
  h.x = x;
  h.z = z;
  h.y = level.ground_at(x, z);
  h.heading = heading;
  const Lie lie = lie_of(level, h);
  h.pitch = h.disc.pitch = lie.pitch;
  h.roll = h.disc.roll = lie.roll;
  const Vec3 hub = heli_point(h, Vec3{0, spec::rotor::kHub, spec::rotor::kAt});
  h.agl = hub.y - level.ground_at(hub.x, hub.z);
  return h;
}

HeliControls heli_turned_back(const HeliState& h, const HeliWorld& world, const HeliLoad& load,
                              const HeliControls& c) {
  if (h.grounded) return c;
  const mg::Level& level = *world.level;
  const Edge edge = edge_at(level, h);
  if (edge.depth <= 0) return c;
  const double w = core::clamp(edge.depth * 3, 0.0, 1.0);
  HeliAim home;
  home.x = level.size / 2;
  home.z = level.size / 2;
  home.height = num::max(h.agl, spec::edge::kHigh);
  const HeliControls b = heli_pilot(h, world, load, home);
  const auto mix = [w](double x, double y) { return x + (y - x) * w; };
  return HeliControls{mix(c.collective, b.collective), mix(c.pitch, b.pitch), mix(c.roll, b.roll),
                      mix(c.pedal, b.pedal)};
}

uint64_t heli_hash(const HeliState& s, uint64_t h) {
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
                         s.heading,
                         s.pitch,
                         s.roll,
                         s.yaw_rate,
                         s.pitch_rate,
                         s.roll_rate,
                         s.disc.pitch,
                         s.disc.roll,
                         s.disc.pitch_rate,
                         s.disc.roll_rate,
                         s.controls.collective,
                         s.controls.pitch,
                         s.controls.roll,
                         s.controls.pedal,
                         s.spool,
                         s.rotor,
                         s.tail_rotor,
                         s.thrust,
                         s.collective,
                         s.agl}) {
    fold(v);
  }
  fold(s.grounded ? 1 : 0);
  fold(static_cast<double>(s.crashed));
  return h;
}

}  // namespace ge::helicopter
