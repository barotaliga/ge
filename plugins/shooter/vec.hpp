// THE CLASSIC MOVE'S VECTOR MATH, in 32-bit floats. The move keeps positions and velocities as
// floats, so every sum and product here is a float operation done in the order written (the
// build forbids contraction, so `a * b + c` rounds twice, as the recordings did). Two things
// leave float on purpose: a vector's length takes the square root of its float sum of squares
// in double and rounds the root to float (`length`, `normalize`), and the view's sines and
// cosines are taken in double, each rounded to float, off the angle in radians (degrees times
// 2 pi / 360 in double, rounded to float) (`angle_vectors`). The axes are the map format's: x and y
// across, z up.
//
// ge:summary The classic move's float vectors: dot, length, normalize, the view's axes
// ge:keywords vec3f float32 vector dot cross length normalize angle-vectors forward right up
// ge:keywords view-angles pitch yaw roll
// ge:skill shooter determinism
// ge:test tests/shooter_vec_test.cpp
#pragma once

#include <cmath>

namespace ge::shooter {

struct Vec3f {
  float x = 0, y = 0, z = 0;

  float& operator[](int i) { return i == 0 ? x : i == 1 ? y : z; }
  float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
};

inline Vec3f operator+(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator-(Vec3f a) { return {-a.x, -a.y, -a.z}; }
inline Vec3f operator*(Vec3f a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline bool operator==(Vec3f a, Vec3f b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// x*x + y*y + z*z in float, summed left to right.
inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f cross(Vec3f a, Vec3f b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// `a + b * s` per component: one float product, then one float sum.
inline Vec3f mad(Vec3f a, float s, Vec3f b) {
  return {a.x + s * b.x, a.y + s * b.y, a.z + s * b.z};
}

// The square root of a float sum of squares, taken in double and rounded to float.
inline float root_of(float squares) {
  return static_cast<float>(std::sqrt(static_cast<double>(squares)));
}

inline float length(Vec3f v) { return root_of(dot(v, v)); }

// Scales `v` to unit length in place by the float reciprocal of its length; returns the
// length. A zero vector stays zero.
inline float normalize(Vec3f& v) {
  const float len = length(v);
  if (len != 0) {
    const float inv = 1 / len;
    v = v * inv;
  }
  return len;
}

// The view's three axes from (pitch down, yaw counter-clockwise from +x, roll) in degrees.
struct ViewAxes {
  Vec3f forward, right, up;
};
ViewAxes angle_vectors(Vec3f angles_deg);

}  // namespace ge::shooter
