// A brush model turned by its entity's angles: the frame, the turn in and out, the reach and the
// rows it is drawn by (turned.hpp).
//
// ge:summary A turned brush model's frame, its turn in and out, its reach and its drawn rows
// ge:keywords turned-brush to_model to_world turned_reach turned_rows
// ge:skill shooter
// ge:test tests/shooter_turned_test.cpp
#include "shooter/turned.hpp"

#include <algorithm>

namespace ge::shooter {

Vec3f to_model(Vec3f d, Vec3f angles) {
  const ViewAxes a = angle_vectors(angles);
  return {dot(d, a.forward), -dot(d, a.right), dot(d, a.up)};
}

Vec3f to_world(Vec3f l, Vec3f angles) {
  const ViewAxes a = angle_vectors(angles);
  return a.forward * l.x - a.right * l.y + a.up * l.z;
}

float turned_reach(Vec3f mins, Vec3f maxs) {
  const auto far = [](float a, float b) { return std::max(a < 0 ? -a : a, b < 0 ? -b : b); };
  const float x = far(mins.x, maxs.x), y = far(mins.y, maxs.y), z = far(mins.z, maxs.z);
  return root_of(x * x + y * y + z * z);
}

void turned_rows(Vec3f angles, double rows[9]) {
  // The map's frame: columns forward, left, up. The engine's axes are the map's x, z, y, so the
  // engine's row i, column j is the map's row p(i), column p(j) with p = (0, 2, 1).
  const ViewAxes a = angle_vectors(angles);
  const Vec3f left = a.right * -1.0f;
  const float m[3][3] = {
      {a.forward.x, left.x, a.up.x}, {a.forward.y, left.y, a.up.y}, {a.forward.z, left.z, a.up.z}};
  static constexpr int kP[3] = {0, 2, 1};
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) rows[i * 3 + j] = double(m[kP[i]][kP[j]]);
  if (!turned(angles))
    for (int i = 0; i < 9; i++) rows[i] = i % 4 == 0 ? 1.0 : 0.0;  // exactly, not cos 0's float
}

}  // namespace ge::shooter
