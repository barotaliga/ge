// THE CLASSIC MOVE'S FLOAT VECTORS: the view's axes off the angles (sines of the radians rounded
// to float), the length's square root in double rounded to float, normalize.
//
// ge:summary Tests: the classic move's float vectors and the view's axes
// ge:keywords test vec3f angle-vectors length normalize float32
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/vec.hpp"

namespace sh = ge::shooter;

TEST(axes_at_the_cardinal_yaws) {
  const sh::ViewAxes a = sh::angle_vectors({0, 0, 0});
  CHECK(a.forward == (sh::Vec3f{1, 0, -0.0f}));
  CHECK(a.right.x == 0 && a.right.y == -1 && a.right.z == 0);
  CHECK(a.up.z == 1);
  // Facing -x: the sine of pi rounded to float is -8.74e-8, not zero: the recordings' drift.
  const sh::ViewAxes b = sh::angle_vectors({0, 180, 0});
  CHECK(b.forward.x == -1);
  CHECK(b.forward.y == -8.74227766e-08f);
  // Pitch is down-positive.
  const sh::ViewAxes c = sh::angle_vectors({90, 0, 0});
  CHECK(c.forward.z == -1);
}

TEST(axes_are_orthonormal_within_float) {
  for (int k = -24; k <= 24; k++) {
    const sh::ViewAxes a = sh::angle_vectors({-30, static_cast<float>(k) * 7.5f, 10});
    CHECK(sh::length(a.forward) > 0.99999f && sh::length(a.forward) < 1.00001f);
    CHECK(sh::dot(a.forward, a.right) < 1e-6f && sh::dot(a.forward, a.right) > -1e-6f);
    CHECK(sh::dot(a.up, a.right) < 1e-6f && sh::dot(a.up, a.right) > -1e-6f);
  }
}

TEST(length_and_normalize) {
  CHECK(sh::length({3, 4, 0}) == 5);
  sh::Vec3f v{0, 0, 0};
  CHECK(sh::normalize(v) == 0);
  CHECK(v == (sh::Vec3f{0, 0, 0}));
  v = {0, 10, 0};
  CHECK(sh::normalize(v) == 10);
  CHECK(v == (sh::Vec3f{0, 1, 0}));
  // The root is taken in double: sqrt(2) rounded once to float.
  CHECK(sh::length({1, 1, 0}) == 1.41421354f);
}
