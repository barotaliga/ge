// The snowmobile's rigid body (body.hpp). Every force is summed in the world frame, the torques
// taken about the centre of gravity and turned into the body frame, and the whole integrated
// once, semi-implicitly.
//
// ge:summary One body step: probes, grip, drive, rider, air, roll, yaw, hull, integration
// ge:keywords snowmobile ride_body probe spring grip combined-slip cohesion carve roll tip
// ge:keywords balance yaw-hold air nose-set hull impulse derive_sled
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#include "snowmobile/body.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "skier/tuning.hpp"
#include "snowmobile/drive.hpp"

namespace ge::snowmobile {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
namespace lm = core::libm;
using core::clamp;
using core::Vec3;

Vec3 cross(double ax, double ay, double az, double bx, double by, double bz) {
  return {ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx};
}

double dot(const Vec3& a, const mg::Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// How deep a footprint sits below the untouched surface at `speed` m/s, m: the skier's sink at
// `scale` times his depth, planing `plane` times later (skier/snow.hpp's `sink_target`).
double sink_target(double packed, double speed, double scale) {
  namespace sn = skier::tuning::snow;
  const double r = speed / (sn::kPlaneSpeed * spec::snow::kPlane);
  return sn::kPowderSink * scale * lm::exp(-(r * r)) * (1 - packed) + sn::kPackedSink * packed;
}

// The deepest anything sits in the snow at rest, m: what a hull is shoved down to.
double powder_floor(double packed, double scale) {
  namespace sn = skier::tuning::snow;
  return sn::kPowderSink * num::max(1.0, scale) * (1 - packed) + sn::kPackedSink * packed;
}

// What the snow costs a footprint of `width` m cutting fresh snow, sunk `sink` m under `load` N
// at `speed` m/s along its line: the rolling resistance, the plough and the powder's compaction
// (`compact` times a ski's), N.
double snow_drag(double packed, double sink, double width, double load, double speed,
                 double compact) {
  namespace sn = skier::tuning::snow;
  const double v = std::fabs(speed);
  const double crr = sn::kCrrPacked * packed + sn::kCrrPowder * (1 - packed);
  const double plough = spec::snow::kPlough * width * sink * v * v * (1 - packed);
  const double powder = sn::kPowderDrag * compact * load * v * (1 - packed);
  return crr * load + plough + powder;
}

// The pitch of the snow the machine will come down on, along its heading, plus `air::kSetNose`:
// where its flight meets the ground, read off its fall. Rad, nose up positive.
double landing_pitch(const SledState& s, const mg::Level& level) {
  const double g = spec::kGravity;
  const double high = num::max(0.0, s.y - spec::kCogHeight - level.ground_at(s.x, s.z));
  const double t = (s.vy + std::sqrt(s.vy * s.vy + 2 * g * high)) / g;
  mg::Vec3 under{0, 1, 0};
  level.normal_at(s.x + s.vx * t, s.z + s.vz * t, under);
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  const double rise = -(under.x * fx + under.z * fz) / num::max(0.2, under.y);
  return lm::atan(rise) + spec::air::kSetNose;
}

// THE HULL ON THE SNOW: every hull point under the snow's floor (deep snow does not hold a hull
// up, it is shoved aside down to what the belt presses) pushed out with an impulse along the
// snow's normal and its slide rubbed by the plastic's friction. Returns the hardest closing
// speed met, m/s.
double hull_contacts(SledState& s, double m, const Vec3& inertia, const mg::Level& level) {
  double worst = 0;
  for (const Vec3& h : spec::kHull) {
    const Vec3 r = core::rotate(s.q, h);
    const double px = s.x + r.x;
    const double py = s.y + r.y;
    const double pz = s.z + r.z;
    const double packed = level.packed_at(px, pz);
    const double floor = level.ground_at(px, pz) - powder_floor(packed, spec::snow::kTreadSink);
    const double pen = floor - py;
    if (pen <= 0) continue;
    mg::Vec3 n{0, 1, 0};
    level.normal_at(px, pz, n);
    const Vec3 w = core::rotate(s.q, Vec3{s.wx, s.wy, s.wz});
    const Vec3 wv = cross(w.x, w.y, w.z, r.x, r.y, r.z);
    const double vx = s.vx + wv.x;
    const double vy = s.vy + wv.y;
    const double vz = s.vz + wv.z;
    const double vn = vx * n.x + vy * n.y + vz * n.z;
    // Out of the snow by the depth it went in.
    s.x += n.x * pen * n.y;
    s.y += pen * n.y * n.y;
    s.z += n.z * pen * n.y;
    if (vn >= 0) continue;
    worst = num::max(worst, -vn);
    // The impulse along the normal through this point: -(1 + e) vn over the effective mass of a
    // rigid body struck there.
    const Vec3 rn = core::unrotate(s.q, cross(r.x, r.y, r.z, n.x, n.y, n.z));
    const double eff =
        1 / m + rn.x * rn.x / inertia.x + rn.y * rn.y / inertia.y + rn.z * rn.z / inertia.z;
    const double j = -(1 + spec::kHullBounce) * vn / eff;
    // The slide along the snow, rubbed: friction up to the normal impulse.
    const double tx = vx - vn * n.x;
    const double ty = vy - vn * n.y;
    const double tz = vz - vn * n.z;
    const double ts = core::hypot3(tx, ty, tz);
    const double jt = ts > 1e-4 ? num::min(spec::kHullFriction * j, ts / eff) / ts : 0;
    const double jx = n.x * j - tx * jt;
    const double jy = n.y * j - ty * jt;
    const double jz = n.z * j - tz * jt;
    s.vx += jx / m;
    s.vy += jy / m;
    s.vz += jz / m;
    const Vec3 l = core::unrotate(s.q, cross(r.x, r.y, r.z, jx, jy, jz));
    s.wx += l.x / inertia.x;
    s.wy += l.y / inertia.y;
    s.wz += l.z / inertia.z;
  }
  return worst;
}

}  // namespace

double sled_lock_at(double speed) {
  return spec::steer::kSkiLock / (1 + std::fabs(speed) / spec::steer::kFadeSpeed);
}

BodyReport ride_body(SledState& s, const SledControls& in, double mass, double rider,
                     const SledWorld& world, double dt) {
  namespace G = spec::grip;
  namespace R = spec::roll;
  namespace A = spec::air;
  namespace S = spec::steer;
  const mg::Level& level = *world.level;
  const double m = mass;
  const Vec3 inertia = sled_inertia(m);
  const double g = spec::kGravity;
  SledControls& k = s.controls;

  // THE CONTROLS, through their lags.
  k.throttle = core::approach(k.throttle, clamp(in.throttle, 0, 1), spec::kThrottleRate * dt);
  k.brake = core::approach(k.brake, clamp(in.brake, 0, 1), spec::kThrottleRate * dt);
  k.steer = clamp(in.steer, -1, 1);
  k.lean = core::approach(k.lean, clamp(in.lean, -1, 1), dt / spec::kRiderLag);
  const double speed0 = core::hypot3(s.vx, s.vy, s.vz);
  s.ski_angle = core::approach(s.ski_angle, k.steer * sled_lock_at(speed0), S::kRate * dt);
  // THE HANG FOLLOWS THE TURN: on the groomer as far as the bend asks (a cornering load over
  // 0.4 g is all of it), in powder where the bars send it at any pace: there his weight is how
  // the sled is carved.
  const double bend = s.way * s.way * std::fabs(lm::tan(s.ski_angle)) / S::kBase;
  const double across = s.packed * clamp(bend / (0.4 * g), 0, 1) + (1 - s.packed);
  const double lag = num::min(1.0, dt / spec::kRiderLag);
  s.rider_right += (k.steer * spec::kRiderReach * across - s.rider_right) * lag;
  s.rider_aft += (k.lean * spec::kAftReach - s.rider_aft) * lag;

  // THE FRAME.
  const core::Quat q = s.q;
  const Vec3 up = core::rotate(q, Vec3{0, 1, 0});
  const Vec3 fwd = core::rotate(q, Vec3{0, 0, 1});
  const Vec3 right = core::rotate(q, Vec3{1, 0, 0});
  const Vec3 w = core::rotate(q, Vec3{s.wx, s.wy, s.wz});

  double fx = 0;
  double fy = -m * g;
  double fz = 0;
  Vec3 torque{0, 0, 0};
  const auto push = [&](double px, double py, double pz, double ffx, double ffy, double ffz) {
    fx += ffx;
    fy += ffy;
    fz += ffz;
    const Vec3 t = cross(px - s.x, py - s.y, pz - s.z, ffx, ffy, ffz);
    torque.x += t.x;
    torque.y += t.y;
    torque.z += t.z;
  };

  mg::Vec3 normal{0, 1, 0};
  level.normal_at(s.x, s.z, normal);
  const double roll_rel = lm::asin(clamp(-dot(right, normal), -1, 1));

  // THE SUSPENSION AND THE GRIP, probe by probe.
  int32_t touching = 0;
  double belt_reaction = 0;
  double load_sum = 0;
  double packed_load = 0;
  double impact = 0;
  double tread_comp = 0;
  int32_t tread_n = 0;
  double slip_sum = 0;
  double tread_sink = 0;
  s.ski_comp = {0, 0};
  const Vec3 ski_dir = core::rotate(q, Vec3{lm::sin(s.ski_angle), 0, lm::cos(s.ski_angle)});
  const double weight = m * g;
  const double slip_ref =
      s.tread_speed == 0 && k.brake > 0.5 ? G::kSlipRef * G::kLockedRef : G::kSlipRef;
  for (size_t i = 0; i < spec::kProbeTable.size(); i++) {
    const spec::Probe& p = spec::kProbeTable[i];
    SledContact& contact = s.contacts[i];
    const double rest = p.share * weight;
    contact.touching = false;
    contact.load = 0;
    const Vec3 a = core::rotate(q, Vec3{p.bx, p.by, p.bz});
    const double ax = s.x + a.x;
    const double ay = s.y + a.y;
    const double az = s.z + a.z;
    const double dy = -up.y;
    if (dy > -spec::kProbeMinDown) {
      s.comps[i] = 0;
      continue;
    }
    const double dx = -up.x;
    const double dz = -up.z;
    const double packed = level.packed_at(ax, az);
    const double scale = p.tread ? spec::snow::kTreadSink : spec::snow::kSkiSink;
    const double target = sink_target(packed, speed0, scale);
    s.sinks[i] += (target - s.sinks[i]) * num::min(1.0, dt / skier::tuning::snow::kSinkLag);
    const double sink = s.sinks[i];
    if (p.tread) tread_sink += sink;
    // Where the strut meets the support, along the body's down axis off the ground's height
    // under the attachment (the strut is short; a vertical guess along it is near enough).
    const double support = level.ground_at(ax, az) - sink;
    const double t = (ay - support) / -dy;
    const double cx = ax + dx * t;
    const double cz = az + dz * t;
    const double cy = ay + dy * t;
    const double comp = p.susp.travel - t;
    if (comp <= 0) {
      s.comps[i] = 0;
      continue;
    }
    if (p.tread) {
      tread_comp += comp;
      tread_n++;
    } else {
      s.ski_comp[p.side < 0 ? 0 : 1] = comp;
    }
    level.normal_at(cx, cz, normal);
    const Vec3 wv = cross(w.x, w.y, w.z, cx - s.x, cy - s.y, cz - s.z);
    const double pvx = s.vx + wv.x;
    const double pvy = s.vy + wv.y;
    const double pvz = s.vz + wv.z;
    const double was = s.comps[i];
    const double rate = was > 0
                            ? (comp - was) / dt
                            : num::max(0.0, -(pvx * normal.x + pvy * normal.y + pvz * normal.z)) /
                                  num::max(0.3, dot(up, normal));
    s.comps[i] = comp;
    const double zone =
        clamp((comp / p.susp.travel - (1 - spec::kBottomZone)) / spec::kBottomZone, 0, 1);
    const double damp = rate > 0 ? p.susp.bump * (1 + spec::kBottomDamp * zone) : p.susp.rebound;
    double spring = p.susp.rate * comp + damp * rate;
    if (comp > p.susp.travel) {
      spring += spec::kStopRate * p.susp.rate * (comp - p.susp.travel) *
                    (rate > 0 ? 1 : spec::kStopRelease) +
                spec::kStopDamp * p.susp.bump * num::max(0.0, rate);
    }
    if (spring <= 0) continue;
    touching++;
    const double vn = pvx * normal.x + pvy * normal.y + pvz * normal.z;
    impact = num::max(impact, -vn);
    const double tilt = dot(up, normal);
    const double load = num::min(spring / num::max(spec::kTiltMin, tilt), spec::kMaxLoad * rest);
    push(ax, ay, az, normal.x * load, normal.y * load, normal.z * load);
    contact.touching = true;
    contact.load = load;
    load_sum += load;
    packed_load += load * packed;

    // The tangent frame along this probe's line of travel.
    const Vec3& dir = p.tread ? fwd : ski_dir;
    const double dn = dot(dir, normal);
    double tx = dir.x - dn * normal.x;
    double ty = dir.y - dn * normal.y;
    double tz = dir.z - dn * normal.z;
    double tl = core::hypot3(tx, ty, tz);
    if (tl == 0) tl = 1;
    tx /= tl;
    ty /= tl;
    tz /= tl;
    const Vec3 side = cross(normal.x, normal.y, normal.z, tx, ty, tz);
    const double vf = pvx * tx + pvy * ty + pvz * tz;
    const double vl = pvx * side.x + pvy * side.y + pvz * side.z;
    double along = 0;
    double across_f = 0;
    if (p.tread) {
      // COMBINED SLIP: the belt slipping along its length and the snow sliding across it spend
      // one grip budget (the shear saturating with how far it is sheared).
      const double drive = G::kTreadPacked * packed + G::kTreadPowder * (1 - packed);
      const double hold = G::kSidePacked * packed + G::kSidePowder * (1 - packed);
      const double slip = s.tread_speed - vf;
      const double sx = slip / slip_ref;
      const double sy = vl / G::kSideRef;
      const double sheared = core::hypot(sx, sy);
      const double share = sheared > 1e-6 ? lm::tanh(sheared) / sheared : 1;
      // ...and the snow's own COHESION sheared by the paddles over the belt's area, in powder.
      const double bite = drive * load + G::kCohesion / 6 * (1 - packed);
      const double f = bite * sx * share;
      belt_reaction += f;
      slip_sum += slip;
      along += f;
      across_f -= hold * num::min(load, G::kSideLoad * rest) * sy * share;
      // THE CARVE: a belt rolled onto its edge in powder bites toward its low side, once there
      // is way on to carve with.
      across_f += load * R::kCarve * (1 - packed) * lm::sin(roll_rel) *
                  clamp(std::fabs(vf) / R::kCarveSpeed, 0, 1);
    } else {
      const double hold = G::kSkiPacked * packed + G::kSkiPowder * (1 - packed);
      across_f -= hold * num::min(load, G::kSideLoad * rest) * lm::tanh(vl / G::kSideRef);
    }
    // Only the belt's FRONT cuts fresh snow: the middle and the rear run in its trench.
    const double cuts = !p.tread || p.station == spec::Station::front ? p.width : 0;
    const double drag = snow_drag(packed, sink, cuts, load, vf, p.tread ? spec::snow::kDrag : 1);
    along -= drag * lm::tanh(vf / spec::snow::kDragFade);
    push(cx, cy, cz, tx * along + side.x * across_f, ty * along + side.y * across_f,
         tz * along + side.z * across_f);
    contact.x = cx;
    contact.z = cz;
    contact.y = level.ground_at(cx, cz);
    contact.sink = sink;
  }
  tread_sink /= 6;
  s.tread_comp = tread_n > 0 ? tread_comp / tread_n : 0;
  s.packed = load_sum > 0 ? packed_load / load_sum : level.packed_at(s.x, s.z);
  const bool grounded = touching > 0;

  // THE BELT AND THE ENGINE.
  const double floor = grounded ? num::max(0.0, s.way - spec::kBrakeSlip) : 0;
  if (s.running) {
    s.tread_speed = step_belt(s.tread_speed, s.rpm, k.throttle, k.brake, belt_reaction, dt, floor);
    s.rpm = step_rpm(s.rpm, k.throttle, s.tread_speed, dt);
  } else {
    // Shut off: no drive, the belt run along by the snow alone, the crank winding down.
    s.tread_speed = step_belt(s.tread_speed, 0, 0, k.brake, belt_reaction, dt, floor);
    s.rpm = num::max(0.0, s.rpm - 4000 * dt);
  }
  s.slip = grounded ? slip_sum / 6 : 0;

  // THE RIDER'S WEIGHT, off-centre.
  const Vec3 shift = core::rotate(q, Vec3{s.rider_right, 0, -s.rider_aft});
  torque.x += shift.z * rider * g;
  torque.z += -shift.x * rider * g;

  // THE AIR.
  const double ax = s.vx - world.air_x;
  const double az = s.vz - world.air_z;
  const double drag = 0.5 * spec::kAirDensity * spec::kCdA * core::hypot3(ax, s.vy, az);
  fx -= drag * ax;
  fy -= drag * s.vy;
  fz -= drag * az;

  // INTO THE BODY FRAME, with the rider's own torques.
  Vec3 tb = core::unrotate(q, torque);
  if (grounded) {
    const double packed = s.packed;
    // The lean the rider takes it to, rolled from one edge to the other no faster than a rider
    // rolls a sled under him.
    const double aim_was = s.roll_aim;
    s.roll_aim = core::approach(
        s.roll_aim, k.steer * (R::kPacked * packed + R::kPowder * (1 - packed)), R::kLeanRate * dt);
    // The roll rate that keeps up with it (a roll toward the right side down is a negative wz):
    // the damping checks only what is past it.
    const double aim_rate = -(s.roll_aim - aim_was) / dt;
    const double hold = clamp((1.3 - std::fabs(roll_rel)) / 0.4, 0, 1);
    // RIDE IT LIKE A BIKE: in loose snow the buried skis hold less and the soft side gives (a
    // sled rolled off the snow's plane sinks on its low side and rolls further), most of it down
    // in the snow, part of it planing on top; none on the groomer, none at a crawl.
    namespace sn = skier::tuning::snow;
    const double rest_tread = sn::kPowderSink * spec::snow::kTreadSink;
    const double loose =
        R::kLoose * (1 - packed) *
        (R::kDeepPlaning +
         (1 - R::kDeepPlaning) * clamp(tread_sink / num::max(1e-6, rest_tread), 0, 1));
    // THE TIP: how far it leans off the way its weight and the turn's pull hang together (a sled
    // leant into a turn as a bicycle is, is upright to it), against the angle its CoG passes over
    // the low edge of what it stands on, the skis' stance narrowing to the belt as the low ski
    // sinks, and a little past it for the rider hung off the high side.
    const double pull_x = w.y * s.vz;
    const double pull_z = -w.y * s.vx;
    const double hang_l = core::hypot3(pull_x, g, pull_z);
    const double lean =
        lm::asin(clamp(-(right.x * pull_x + right.y * g + right.z * pull_z) / hang_l, -1, 1));
    const double edge =
        spec::kSkiStance / 2 + (spec::kTreadWidth / 2 - spec::kSkiStance / 2) * loose;
    const double tip_at = lm::atan(edge / spec::kCogHeight) + R::kHang;
    const double upright = clamp((tip_at + R::kTipBand - std::fabs(lean)) / R::kTipBand, 0, 1);
    const double firm = (1 - R::kDeepHold * loose) * upright;
    const double moving = clamp((speed0 - 1.5) / 2.5, 0, 1);
    if (loose > 0 && moving > 0) {
      // ...and the rider BALANCES it: the bars countered and his weight hung off the high side
      // take up the soft side's lever as far as they reach; it goes over only past that.
      const double give =
          R::kDeepTip * loose * moving * m * g * spec::kCogHeight * lm::sin(roll_rel) * hold;
      const double reach =
          R::kBalance * (R::kBalanceCrawl +
                         (1 - R::kBalanceCrawl) * clamp(std::fabs(s.way) / R::kBalanceSpeed, 0, 1));
      tb.z -= num::sign(give) * num::max(0.0, std::fabs(give) - reach);
    }
    // The righting and its damping, each held to the most the rider and the springs have.
    tb.z += (clamp(R::kStiff * (roll_rel - s.roll_aim), -R::kMost, R::kMost) -
             clamp(R::kDamp * (s.wz - aim_rate), -R::kMost, R::kMost)) *
            hold * firm;
    // THE YAW HELD (the arcade's hand): toward the rate the skis ask, no faster than the grip
    // turns the way, and the nose to the way it goes.
    const double way = s.way;
    const double flat = core::hypot(s.vx, s.vz);
    const double grip = (G::kSkiPacked * packed + S::kReachPowder * (1 - packed)) * g;
    const double reach = std::fabs(way) > 1 ? grip * S::kPathShare / std::fabs(way) : 0;
    const double asked = clamp(way * lm::tan(s.ski_angle) / S::kBase, -reach, reach);
    const double slide =
        flat > S::kSlipFrom && way > 0 ? core::angle_diff(lm::atan2(s.vx, s.vz), s.heading) : 0;
    if (flat > 1) {
      const double scrub = S::kScrub * packed * m * std::fabs(asked * way);
      fx -= scrub * s.vx / flat;
      fz -= scrub * s.vz / flat;
    }
    tb.y += clamp(-S::kYawHold * (s.wy - asked) - S::kSlipHold * slide, -S::kYawHoldMax,
                  S::kYawHoldMax);
  } else {
    // IN THE AIR: the lean pitches it, the belt's gyro lifts the nose on the throttle and drops
    // it on the brake, the bars give a little yaw, and the rider levels the roll with his body.
    tb.x += -A::kLean * k.lean - A::kThrottle * k.throttle + A::kBrake * k.brake;
    tb.y += A::kSteer * k.steer;
    const double reach = clamp((1.2 - std::fabs(s.roll)) / 0.3, 0, 1);
    tb.z +=
        clamp(A::kRollLevel * s.roll - A::kRollDamp * s.wz, -A::kRollMost, A::kRollMost) * reach;
    // THE NOSE SET FOR THE LANDING (the arcade's hand, `assist`): pitched toward the snow it will
    // come down on, a little nose high, let go the moment he leans or brakes.
    const double set = world.assist * (1 - num::max(std::fabs(k.lean), k.brake)) * reach;
    if (set > 0) {
      const double err = landing_pitch(s, level) - s.pitch;
      tb.x += clamp(-A::kSetStiff * err - A::kSetDamp * s.wx, -A::kSetMost, A::kSetMost) * set;
    }
    tb.x -= A::kDamping * s.wx;
    tb.y -= A::kDamping * s.wy;
    tb.z -= A::kDamping * s.wz;
  }
  // Euler's equations with a diagonal inertia: tau - w x Iw.
  const double gx = (inertia.z - inertia.y) * s.wy * s.wz;
  const double gy = (inertia.x - inertia.z) * s.wz * s.wx;
  const double gz = (inertia.y - inertia.x) * s.wx * s.wy;
  s.wx += (tb.x - gx) / inertia.x * dt;
  s.wy += (tb.y - gy) / inertia.y * dt;
  s.wz += (tb.z - gz) / inertia.z * dt;
  const double spin = core::hypot3(s.wx, s.wy, s.wz);
  if (spin > spec::kMaxSpin) {
    s.wx *= spec::kMaxSpin / spin;
    s.wy *= spec::kMaxSpin / spin;
    s.wz *= spec::kMaxSpin / spin;
  }

  // INTEGRATE: the velocities, then the hull's impulses, then the position.
  s.vx += fx / m * dt;
  s.vy += fy / m * dt;
  s.vz += fz / m * dt;
  const double hull = hull_contacts(s, m, inertia, level);
  impact = num::max(impact, hull);
  s.x += s.vx * dt;
  s.y += s.vy * dt;
  s.z += s.vz * dt;
  s.q = core::integrate(s.q, s.wx, s.wy, s.wz, dt);

  // AIR AND LANDING.
  BodyReport report;
  report.impact = impact;
  if (!grounded && hull == 0) {
    if (!s.airborne) {
      s.airborne = true;
      s.air_time = 0;
    }
    s.air_time += dt;
  } else if (s.airborne) {
    report.landed = s.air_time;
    s.airborne = false;
    s.air_time = 0;
  }
  derive_sled(s);
  return report;
}

void derive_sled(SledState& s) {
  const core::Euler e = core::to_euler(s.q);
  s.heading = e.heading;
  s.pitch = e.pitch;
  s.roll = e.roll;
  s.speed = core::hypot3(s.vx, s.vy, s.vz);
  const Vec3 f = core::rotate(s.q, Vec3{0, 0, 1});
  double fl = core::hypot(f.x, f.z);
  if (fl == 0) fl = 1;
  s.way = (s.vx * f.x + s.vz * f.z) / fl;
}

}  // namespace ge::snowmobile
