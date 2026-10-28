// The held model (engine/viewmodel/held.hpp): a glTF model becomes frames with a colour a vertex,
// the mount turns a model so its muzzle is +z and its up +y without mirroring it (a triangle's
// front stays outside), scales it to its length and puts the grip at the origin, and two frames
// blend with unit normals.
//
// ge:summary Tests: the held model's conversion from a model, the grip mount, the frame blend
// ge:keywords test viewmodel held-model mount grip muzzle fit_mount pose_frames
// ge:skill render testing
// ge:test tests/viewmodel_held_test.cpp
#include <cmath>

#include "assets/test_model.hpp"
#include "check.hpp"
#include "viewmodel/held.hpp"

namespace vm = ge::viewmodel;

namespace {

// A long thin box along +x (the muzzle), one triangle a face would do; a single tetrahedron-ish
// triangle with a known front is enough to watch the winding.
vm::HeldModel stick() {
  vm::HeldModel m;
  vm::HeldFrame f;
  f.name = "rest";
  // A triangle in the x-y plane, front toward -z by (b - a) x (c - a): a=(0,0,0) b=(0,1,0)
  // c=(2,0,0): (0,1,0) x (2,0,0) = (0,0,-2).
  f.positions = {0, 0, 0, 0, 1, 0, 2, 0, 0};
  f.normals = {0, 0, -1, 0, 0, -1, 0, 0, -1};
  m.frames.push_back(f);
  m.indices = {0, 1, 2};
  m.colour = {1, 0, 0, 1, 0, 0, 1, 0, 0};
  return m;
}

void front(const vm::HeldFrame& f, float out[3]) {
  const float* p = f.positions.data();
  const float e1[3] = {p[3] - p[0], p[4] - p[1], p[5] - p[2]};
  const float e2[3] = {p[6] - p[0], p[7] - p[1], p[8] - p[2]};
  out[0] = e1[1] * e2[2] - e1[2] * e2[1];
  out[1] = e1[2] * e2[0] - e1[0] * e2[2];
  out[2] = e1[0] * e2[1] - e1[1] * e2[0];
}

}  // namespace

TEST(a_model_becomes_a_coloured_frame) {
  const ge::assets::Model model = ge::assets::make_test_model();
  const vm::HeldModel h = vm::held_from_model(model);
  CHECK(h.frames.size() == 1);
  CHECK(h.vertex_count() > 0);
  CHECK(h.colour.size() == size_t{h.vertex_count()} * 3);
  CHECK(h.indices.size() % 3 == 0);
  for (uint32_t i : h.indices) CHECK(i < h.vertex_count());
}

TEST(the_mount_points_the_muzzle_forward_and_keeps_the_front) {
  vm::HeldModel m = stick();
  vm::Mount mount;
  mount.muzzle = 0;  // the model's +x is its barrel
  mount.up = 1;
  mount.length_m = 0.5;
  mount.grip[0] = 0.5;
  mount.grip[1] = 0;
  mount.grip[2] = 0;
  vm::fit_mount(m, mount);
  float lo[3], hi[3];
  vm::held_bounds(m, lo, hi);
  CHECK(std::fabs((hi[2] - lo[2]) - 0.5f) < 1e-6f);  // its length along +z
  CHECK(std::fabs(lo[2]) < 1e-6f);                   // the back at the grip
  CHECK(std::fabs(lo[1]) < 1e-6f);
  // The front turned with the normals: the stored normal and the winding agree.
  float f[3];
  front(m.frames[0], f);
  const float* n = m.frames[0].normals.data();
  CHECK(f[0] * n[0] + f[1] * n[1] + f[2] * n[2] > 0);
}

TEST(a_backward_muzzle_is_turned_round) {
  vm::HeldModel m = stick();
  vm::Mount mount;
  mount.muzzle = 0;
  mount.muzzle_negative = true;
  vm::fit_mount(m, mount);
  // The tip that was at +x = 2 is now the furthest back.
  const float* p = m.frames[0].positions.data();
  CHECK(p[8] < p[2]);
  float f[3];
  front(m.frames[0], f);
  const float* n = m.frames[0].normals.data();
  CHECK(f[0] * n[0] + f[1] * n[1] + f[2] * n[2] > 0);
}

TEST(two_frames_blend_with_unit_normals) {
  vm::HeldModel m = stick();
  vm::HeldFrame b = m.frames[0];
  for (float& v : b.positions) v += 1;
  b.normals = {0, 1, 0, 0, 1, 0, 0, 1, 0};
  m.frames.push_back(b);
  std::vector<float> p, n;
  vm::pose_frames(m, 0, 1, 0.5, p, n);
  CHECK(std::fabs(p[0] - 0.5f) < 1e-6f);
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  CHECK(std::fabs(len - 1) < 1e-5f);
}
