// The affine algebra a model is posed with, and the lookups by name.
//
// ge:summary Model definitions: TRS to affine, compose, invert, transform, find by name
// ge:keywords assets affine compose invert matrix trs find_joint find_mesh find_clip
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#include "assets/model.hpp"

namespace ge::assets {

Affine to_affine(const Trs& trs) {
  const float x = trs.r[0], y = trs.r[1], z = trs.r[2], w = trs.r[3];
  // The rotation matrix of a unit quaternion, its columns scaled by s.
  const float r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
                      2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                      2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
  Affine a;
  for (size_t row = 0; row < 3; ++row) {
    for (size_t col = 0; col < 3; ++col) a.m[row * 4 + col] = r[row * 3 + col] * trs.s[col];
    a.m[row * 4 + 3] = trs.t[row];
  }
  return a;
}

Affine compose(const Affine& a, const Affine& b) {
  Affine c;
  for (size_t row = 0; row < 3; ++row) {
    const float* ar = &a.m[row * 4];
    for (size_t col = 0; col < 4; ++col) {
      float v = ar[0] * b.m[col] + ar[1] * b.m[4 + col] + ar[2] * b.m[8 + col];
      if (col == 3) v += ar[3];
      c.m[row * 4 + col] = v;
    }
  }
  return c;
}

Affine invert(const Affine& a) {
  const float* m = a.m;
  // The 3x3 half's cofactors, transposed: its adjugate.
  const float c00 = m[5] * m[10] - m[6] * m[9];
  const float c01 = m[2] * m[9] - m[1] * m[10];
  const float c02 = m[1] * m[6] - m[2] * m[5];
  const float c10 = m[6] * m[8] - m[4] * m[10];
  const float c11 = m[0] * m[10] - m[2] * m[8];
  const float c12 = m[2] * m[4] - m[0] * m[6];
  const float c20 = m[4] * m[9] - m[5] * m[8];
  const float c21 = m[1] * m[8] - m[0] * m[9];
  const float c22 = m[0] * m[5] - m[1] * m[4];
  const float det = m[0] * c00 + m[1] * c10 + m[2] * c20;
  if (det == 0.0f) return Affine{};
  const float k = 1.0f / det;
  Affine inv;
  const float r[9] = {c00 * k, c01 * k, c02 * k, c10 * k, c11 * k,
                      c12 * k, c20 * k, c21 * k, c22 * k};
  for (size_t row = 0; row < 3; ++row) {
    for (size_t col = 0; col < 3; ++col) inv.m[row * 4 + col] = r[row * 3 + col];
    inv.m[row * 4 + 3] = -(r[row * 3] * m[3] + r[row * 3 + 1] * m[7] + r[row * 3 + 2] * m[11]);
  }
  return inv;
}

void transform_point(const Affine& a, const float p[3], float out[3]) {
  for (size_t row = 0; row < 3; ++row) {
    const float* r = &a.m[row * 4];
    out[row] = r[0] * p[0] + r[1] * p[1] + r[2] * p[2] + r[3];
  }
}

void transform_vector(const Affine& a, const float v[3], float out[3]) {
  for (size_t row = 0; row < 3; ++row) {
    const float* r = &a.m[row * 4];
    out[row] = r[0] * v[0] + r[1] * v[1] + r[2] * v[2];
  }
}

namespace {

template <class T>
int find_named(const std::vector<T>& items, std::string_view name) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

int find_joint(const Model& model, std::string_view name) { return find_named(model.joints, name); }
int find_mesh(const Model& model, std::string_view name) { return find_named(model.meshes, name); }
int find_clip(const Model& model, std::string_view name) { return find_named(model.clips, name); }

}  // namespace ge::assets
