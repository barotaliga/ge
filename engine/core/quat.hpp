// A UNIT QUATERNION as a vehicle's orientation, and the operations a rigid body needs on it.
// Port of oss-game-framework `src/core/quat.ts`, bit for bit (sin/cos/atan2/asin through
// core::libm). Parity: tests/core_quat_test.cpp against tests/golden/core_quat.txt.
//
// CONVENTIONS, stated once. A quaternion rotates BODY axes into WORLD axes: body x is the
// vehicle's right, y its up, z its forward; world x is east, z north, y up. Every rotation is
// right-handed about the axis it names, so the game's heading (0 along +z, clockwise seen from
// above) is a positive rotation about +y: it carries +z onto (sin h, 0, cos h). The Euler
// angles are quoted as a rider reads them: `pitch` is NOSE UP positive (a negative rotation
// about the right axis) and `roll` is RIGHT SIDE DOWN positive (a negative rotation about the
// forward axis). from_euler and to_euler own that sign flip; body-frame angular velocities
// stay right-handed, so a nose-up pitch rate is a negative wx.
#pragma once

#include "core/js.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::core {

struct Quat {
  double x, y, z, w;
};

struct Vec3 {
  double x, y, z;
};

struct Euler {
  double heading;  // rad, 0 = +z, clockwise from above
  double pitch;    // rad, nose up positive
  double roll;     // rad, right side down positive
};

inline Quat identity() { return Quat{0.0, 0.0, 0.0, 1.0}; }

// Hamilton product a * b: rotate by b first, then by a.
inline Quat multiply(const Quat& a, const Quat& b) {
  return Quat{
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
  };
}

// Renormalise in place and return q. Integration drifts the length by a part in a million a
// step, which after a minute at 120 Hz is a visible skew. JS `hypot4(...) || 1`: a zero or NaN
// length divides by 1.
inline Quat& normalize(Quat& q) {
  double n = hypot4(q.x, q.y, q.z, q.w);
  if (n == 0.0 || n != n) n = 1.0;
  q.x /= n;
  q.y /= n;
  q.z /= n;
  q.w /= n;
  return q;
}

// The rotation by `angle` rad (right-handed) about a unit axis.
inline Quat from_axis_angle(double ax, double ay, double az, double angle) {
  const double h = angle * 0.5;
  const double s = libm::sin(h);
  return Quat{ax * s, ay * s, az * s, libm::cos(h)};
}

// Rotate a body-frame vector into the world frame: v + 2w(u x v) + 2u x (u x v), u = (x, y, z).
inline Vec3 rotate(const Quat& q, const Vec3& v) {
  const double ux = q.x;
  const double uy = q.y;
  const double uz = q.z;
  const double cx = uy * v.z - uz * v.y;
  const double cy = uz * v.x - ux * v.z;
  const double cz = ux * v.y - uy * v.x;
  const double ccx = uy * cz - uz * cy;
  const double ccy = uz * cx - ux * cz;
  const double ccz = ux * cy - uy * cx;
  return Vec3{v.x + 2.0 * (q.w * cx + ccx), v.y + 2.0 * (q.w * cy + ccy),
              v.z + 2.0 * (q.w * cz + ccz)};
}

// Rotate a world-frame vector into the body frame (the inverse rotation).
inline Vec3 unrotate(const Quat& q, const Vec3& v) {
  return rotate(Quat{-q.x, -q.y, -q.z, q.w}, v);
}

// Advance the orientation by a BODY-frame angular velocity (rad/s) over dt (s):
// q * exp(w dt / 2), exact for a rate held constant over the step. Post-multiplied because the
// rate is in the body's own axes. A rate under 1e-9 rad/s returns q unchanged (and unnormalised).
inline Quat integrate(const Quat& q, double wx, double wy, double wz, double dt) {
  const double mag = hypot3(wx, wy, wz);
  if (mag < 1e-9) return q;
  const double angle = mag * dt;
  const Quat d = from_axis_angle(wx / mag, wy / mag, wz / mag, angle);
  Quat out = multiply(q, d);
  return normalize(out);
}

// The orientation with this heading, pitch and roll (see the header): yaw first, then pitch
// about the yawed right axis, then roll about the resulting forward axis.
inline Quat from_euler(double heading, double pitch, double roll) {
  const Quat qy = from_axis_angle(0.0, 1.0, 0.0, heading);
  const Quat qx = from_axis_angle(1.0, 0.0, 0.0, -pitch);
  const Quat qz = from_axis_angle(0.0, 0.0, 1.0, -roll);
  Quat out = multiply(multiply(qy, qx), qz);
  return normalize(out);
}

// The three angles from_euler was built from, read back off the rotated axes. At a pitch of
// exactly +-90 degrees heading and roll share an axis and their split is arbitrary (gimbal);
// nothing downstream steers by it.
inline Euler to_euler(const Quat& q) {
  const Vec3 f = rotate(q, Vec3{0.0, 0.0, 1.0});
  const double heading = libm::atan2(f.x, f.z);
  const double pitch = libm::asin(js::max(-1.0, js::min(1.0, f.y)));
  // Strip yaw and pitch back off; what is left is a pure rotation about the forward axis,
  // whose angle is read off its vector part exactly.
  const Quat yp = from_euler(heading, pitch, 0.0);
  const Quat rest = multiply(Quat{-yp.x, -yp.y, -yp.z, yp.w}, q);
  double roll = -2.0 * libm::atan2(rest.z, rest.w);
  if (roll > kPi) roll -= 2.0 * kPi;
  if (roll <= -kPi) roll += 2.0 * kPi;
  return Euler{heading, pitch, roll};
}

}  // namespace ge::core
