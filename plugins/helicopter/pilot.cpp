// The bot's hands on the helicopter (pilot.hpp).
//
// ge:summary heli_pilot and heli_hands: the way, the height ahead, the disc, the pedals, the lever
// ge:keywords helicopter heli_pilot heli_hands cascade terrain-following land settle
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#include "helicopter/pilot.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "helicopter/spec.hpp"

namespace ge::helicopter {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
namespace lm = core::libm;
namespace F = spec::flight;
namespace P = spec::pilot;
using core::Vec3;

constexpr double kG = 9.81;

}  // namespace

HeliControls heli_pilot(const HeliState& h, const HeliWorld& world, const HeliLoad& load,
                        const HeliAim& aim) {
  const mg::Level& level = *world.level;
  const double dx = aim.x - h.x;
  const double dz = aim.z - h.z;
  const double d = core::hypot(dx, dz);
  // THE HEIGHT over the snow along the way, and the climb to it: the highest snow it will be
  // over in the next `kAhead` s (never nearer than two readings). THE SPEED the way allows: every
  // point of it reached no sooner than the climb to clear it takes, and the aim no sooner than the
  // sink down to it takes (but never slower than `kDescend` for it, fast enough to sink freely: a
  // machine still high over the aim comes down over it in the hover).
  const double flat = core::hypot(h.vx, h.vz);
  const double near = num::max(2 * P::kLookStep, flat * P::kAhead);
  double ground = level.ground_at(h.x, h.z);
  double rise_most = P::kCruise;
  const double look = num::min(d, P::kLook);
  for (int k = 1; P::kLookStep * k <= look; k++) {
    const double s = P::kLookStep * k;
    const double g = level.ground_at(h.x + dx / d * s, h.z + dz / d * s);
    if (s <= near) ground = num::max(ground, g);
    const double need = g + aim.height - h.y;
    if (need > 0) rise_most = num::min(rise_most, P::kClimbShare * P::kClimb * s / need);
  }
  const double fall = h.y - level.ground_at(aim.x, aim.z) - aim.height;
  if (fall > 0) {
    rise_most = num::min(rise_most, num::max(P::kDescend, P::kClimbShare * P::kSink * d / fall));
  }
  // THE WAY THERE: the velocity it wants, slowed in time to stop at the aim, and near it in
  // proportion to the distance, so the hover settles rather than hunting across the spot.
  const double reach = aim.land ? 0.0 : 4.0;
  const double left = num::max(0.0, d - reach);
  const double speed = num::min(num::min(P::kCruise, rise_most),
                                num::min(std::sqrt(2 * P::kBrake * left), 0.4 * left));
  const double vxw = d > 0.5 ? dx / d * speed : 0.0;
  const double vzw = d > 0.5 ? dz / d * speed : 0.0;
  // THE PEDALS: the nose the way it is going (or held, at the hover).
  const double going = flat > 6 ? lm::atan2(h.vx, h.vz) : h.heading;
  const double yaw_to = d > 15 && !aim.land                  ? lm::atan2(dx, dz)
                        : aim.land && aim.has_face && d < 40 ? aim.face
                                                             : going;
  const double turn = core::clamp(core::angle_diff(h.heading, yaw_to) * 1.2, -0.6, 0.6);
  const double over = h.y - level.ground_at(h.x, h.z);
  const double sink_most = flat > 15 ? P::kSink : P::kSinkSlow;
  double climb = core::clamp(0.4 * (ground + aim.height - h.y), -sink_most, P::kClimb);
  if (aim.land && d < 5 && flat < 2.5) {
    climb = -core::clamp(over * 0.25, P::kSettle, P::kSinkSlow);
    if (h.grounded) return HeliControls{};
  }
  return heli_hands(h, world, load, vxw, vzw, turn, climb);
}

HeliControls heli_hands(const HeliState& h, const HeliWorld& world, const HeliLoad& load,
                        double vxw, double vzw, double turn, double climb) {
  const double tilt_most = lm::tan(P::kTilt) * kG;
  double axw = 0.8 * (vxw - h.vx);
  double azw = 0.8 * (vzw - h.vz);
  const double aw = core::hypot(axw, azw);
  if (aw > tilt_most) {
    axw *= tilt_most / aw;
    azw *= tilt_most / aw;
  }
  // ...as the disc's tilt in its own heading's frame, with the drag at speed leant into.
  const double fx = lm::sin(h.heading);
  const double fz = lm::cos(h.heading);
  const double air_f = (h.vx - world.air_x) * fx + (h.vz - world.air_z) * fz;
  const double air_s = (h.vx - world.air_x) * fz - (h.vz - world.air_z) * fx;
  const double m = heli_mass(load);
  const double drag_f = 0.5 * spec::kDensity * spec::drag::kFront * std::fabs(air_f) * air_f / m;
  const double drag_s = 0.5 * spec::kDensity * spec::drag::kSide * std::fabs(air_s) * air_s / m;
  const double a_f = axw * fx + azw * fz + drag_f;
  const double a_s = axw * fz - azw * fx + drag_s;
  const double pitch_to = core::clamp(-lm::atan2(a_f, kG), -P::kTilt * 1.3, P::kTilt * 1.3);
  const double roll_to = core::clamp(lm::atan2(a_s, kG), -P::kTilt * 1.3, P::kTilt * 1.3);
  // THE CYCLIC: the disc's own dynamics inverted, a damped spring onto the tilt wanted, its
  // damping and its flapback paid for.
  constexpr double kW2 = P::kAttitude * P::kAttitude;
  constexpr double kZ2 = 2 * P::kDamping * P::kAttitude;
  const double sp = h.disc.pitch_rate;
  const double sr = h.disc.roll_rate;
  const double want_p = kW2 * core::angle_diff(h.disc.pitch, pitch_to) - kZ2 * sp;
  const double want_r = kW2 * core::angle_diff(h.disc.roll, roll_to) - kZ2 * sr;
  const double spool2 = num::max(0.05, h.spool * h.spool);
  const double pitch = core::clamp(
      -(want_p + F::kDampingPitch * sp - F::kFlapback * air_f) / (F::kCyclicPitch * spool2), -1.0,
      1.0);
  const double roll = core::clamp(
      (want_r + F::kDampingRoll * sr + F::kFlapback * air_s) / (F::kCyclicRoll * spool2), -1.0,
      1.0);
  // THE PEDALS: the turn, the torque and the weathervane paid for.
  const double hover = m * kG;
  const double torque = F::kTorque * (h.thrust / hover - 1);
  const double vane =
      spec::drag::kVane * air_s * num::min(40.0, std::fabs(air_f) + std::fabs(air_s));
  const double pedal = core::clamp(
      (3 * (turn - h.yaw_rate) + F::kYawDamping * h.yaw_rate + torque - vane) / F::kPedal, -1.0,
      1.0);
  // THE COLLECTIVE: the thrust that climbs so, as a share of what the rotor gives.
  const Vec3 up = core::rotate(disc_quat(h), Vec3{0, 1, 0});
  const double want = m * (kG + P::kHold * (climb - h.vy)) / num::max(0.5, up.y);
  const double along = core::hypot(air_f, air_s);
  const double most = thrust_most(along, h.vy, h.agl, h.vy) * h.spool * h.spool;
  const double collective = most > 0 ? core::clamp(want / most, 0.0, 1.0) : 0.0;
  return HeliControls{collective, pitch, roll, pedal};
}

}  // namespace ge::helicopter
