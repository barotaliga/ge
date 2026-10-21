// engine/render/view.hpp: the camera's matrices put what is ahead in the middle of the frame,
// what is to the right on the right and what is above at the top; depth is reversed (1 at the
// near plane, falling toward 0 with distance); the inverse undoes the product; the frustum's
// planes keep what is in view and refuse what is behind or beside.
//
// ge:summary Tests for the camera: projection sides, reversed depth, inverse, frustum planes
// ge:keywords test render camera view projection reverse-z frustum inverse heading
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/view.hpp"

namespace {

using ge::render::camera_matrices;
using ge::render::FrameView;

// Clip coordinates of a camera-relative point, divided through.
void project(const ge::render::Mat4& m, double x, double y, double z, double ndc[3]) {
  const float in[4] = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1};
  float out[4];
  ge::render::transform(m, in, out);
  for (int i = 0; i < 3; ++i) ndc[i] = out[i] / out[3];
}

bool inside(const ge::render::CameraMatrices& c, double x, double y, double z) {
  for (const auto& p : c.planes) {
    if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0) return false;
  }
  return true;
}

}  // namespace

TEST(heading_zero_looks_along_plus_z_with_plus_x_on_the_right) {
  FrameView v;
  v.heading = 0;
  v.pitch = 0;
  const auto c = camera_matrices(v, 16.0 / 9.0);
  double n[3];
  project(c.view_proj, 0, 0, 10, n);
  CHECK(std::fabs(n[0]) < 1e-6 && std::fabs(n[1]) < 1e-6);
  project(c.view_proj, 1, 0, 10, n);
  CHECK(n[0] > 0.01);
  project(c.view_proj, 0, 1, 10, n);
  CHECK(n[1] > 0.01);
}

TEST(heading_turns_clockwise_from_above_toward_plus_x) {
  FrameView v;
  v.heading = 3.14159265358979 / 2;  // looking along +x
  const auto c = camera_matrices(v, 1.0);
  double n[3];
  project(c.view_proj, 10, 0, 0, n);
  CHECK(std::fabs(n[0]) < 1e-6);
  project(c.view_proj, 10, 0, -1, n);  // -z is to the right when facing +x
  CHECK(n[0] > 0.01);
}

TEST(depth_is_reversed_and_infinite) {
  FrameView v;
  v.near_m = 0.1;
  const auto c = camera_matrices(v, 1.0);
  double near_p[3], mid[3], far_p[3];
  project(c.view_proj, 0, 0, 0.1, near_p);
  project(c.view_proj, 0, 0, 10, mid);
  project(c.view_proj, 0, 0, 1e6, far_p);
  CHECK(std::fabs(near_p[2] - 1.0) < 1e-5);
  CHECK(mid[2] < near_p[2] && far_p[2] < mid[2] && far_p[2] > 0);
}

TEST(inverse_undoes_the_product) {
  FrameView v;
  v.heading = 0.7;
  v.pitch = -0.3;
  v.fov_y = 0.9;
  const auto c = camera_matrices(v, 1.5);
  const auto id = ge::render::mul(c.inv_view_proj, c.view_proj);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float want = col == row ? 1.0f : 0.0f;
      CHECK(std::fabs(id.m[col * 4 + row] - want) < 1e-4f);
    }
  }
}

TEST(frustum_planes_keep_what_is_in_view) {
  FrameView v;
  v.heading = 0;
  v.fov_y = 1.0;
  const auto c = camera_matrices(v, 1.0);
  CHECK(inside(c, 0, 0, 50));
  CHECK(!inside(c, 0, 0, -50));   // behind
  CHECK(!inside(c, 200, 0, 50));  // far to the right
  CHECK(!inside(c, 0, 0, 0.05));  // before the near plane
  CHECK(inside(c, 0, 0, 1e5));    // no far plane
}
