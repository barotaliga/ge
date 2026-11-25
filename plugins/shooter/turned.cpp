// A brush model turned by its entity's angles: the frame, the turn in and out, the reach
// (turned.hpp).
//
// ge:summary A turned brush model's frame, its turn in and out and its reach
// ge:keywords turned-brush to_model to_world turned_reach
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

}  // namespace ge::shooter
