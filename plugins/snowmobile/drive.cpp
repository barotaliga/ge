// The snowmobile's drive (drive.hpp).
//
// ge:summary The engine's power curve, the CVT's hold and the belt's step
// ge:keywords snowmobile drive power_share drive_force step_belt step_rpm
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#include "snowmobile/drive.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "snowmobile/spec.hpp"

namespace ge::snowmobile {

namespace {
constexpr double kRpmToRad = 2 * core::kPi / 60;
}  // namespace

double power_share(double rpm) {
  using namespace spec;
  if (rpm <= kPeakRpm) {
    const double x = core::clamp((rpm - kIdleRpm) / (kPeakRpm - kIdleRpm), 0, 1);
    return 0.12 + 0.88 * (1 - core::libm::pow(1 - x, kCurve));
  }
  const double x = core::clamp((rpm - kPeakRpm) / (kMaxRpm - kPeakRpm), 0, 1.5);
  return 1 - 0.25 * x * x;
}

double rpm_at_top(double tread_speed) { return tread_speed / spec::kGearTop * spec::kMaxRpm; }

double max_drive_force() {
  using namespace spec;
  const double torque = kPowerKw * 1000 / (kPeakRpm * kRpmToRad);
  const double rpm_per_mps = kMaxRpm * kGearSpan / kGearTop;
  return torque * kDriveline * rpm_per_mps * kRpmToRad;
}

double drive_force(double rpm, double throttle, double tread_speed) {
  using namespace spec;
  const double top = rpm_at_top(tread_speed);
  if (throttle <= 0.01 || rpm < kEngageRpm * 0.95) {
    return top > kEngageRpm ? -kEngineBrake * tread_speed : 0;
  }
  const double power = kPowerKw * 1000 * power_share(rpm) * throttle * kDriveline;
  double force =
      core::num::min(power / core::num::max(tread_speed, kLaunchFloor), max_drive_force());
  // THE LIMITER: the fuel cut over the last two per cent to the redline.
  force *= core::clamp((kMaxRpm * 1.02 - top) / (kMaxRpm * 0.02), 0, 1);
  return force;
}

double rpm_goal(double throttle, double tread_speed) {
  using namespace spec;
  const double top = rpm_at_top(tread_speed);
  if (throttle <= 0.01) return top > kEngageRpm ? top : kIdleRpm;
  const double asked = kEngageRpm + (kPeakRpm - kEngageRpm) * core::clamp(throttle, 0, 1);
  return core::num::min(core::num::max(asked, top), kMaxRpm * 1.03);
}

double step_belt(double tread_speed, double rpm, double throttle, double brake, double ground,
                 double dt, double floor) {
  using namespace spec;
  const double v = tread_speed;
  const double loss = kBeltLossLin * v + kBeltLossQuad * v * v;
  const double net = drive_force(rpm, throttle, v) - ground - loss;
  double next = v + net / kBeltMass * dt;
  const double clamp_dv = kBrakeForce * brake * dt / kBeltMass;
  if (next > 0) next = core::num::max(core::num::min(next, floor), next - clamp_dv);
  return core::num::max(0.0, next);
}

double step_rpm(double rpm, double throttle, double tread_speed, double dt) {
  const double goal = rpm_goal(throttle, tread_speed);
  return rpm + (goal - rpm) * core::num::min(1.0, spec::kRpmRate * dt);
}

}  // namespace ge::snowmobile
