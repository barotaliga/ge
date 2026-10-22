// The test model's builder: the tube's rings and blended weights, the fin's faces, the joints
// and the two clips.
//
// ge:summary Test model definitions: the tube, the fin, the chain, the wave and twist clips
// ge:keywords assets test-model tube fin chain clip
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#include "assets/test_model.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::assets {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kSides = 6;
constexpr int kRings = 11;  // every 0.1 m from 0 to 1 m
constexpr float kRadius = 0.06f;

// A rotation of `angle` about the unit axis (x, y, z).
void axis_angle(double x, double y, double z, double angle, float q[4]) {
  const double s = core::libm::sin(angle / 2);
  q[0] = static_cast<float>(x * s);
  q[1] = static_cast<float>(y * s);
  q[2] = static_cast<float>(z * s);
  q[3] = static_cast<float>(core::libm::cos(angle / 2));
}

void vertex(Mesh& m, const float p[3], const float n[3], const uint8_t j[4], const float w[4],
            uint8_t part) {
  m.positions.insert(m.positions.end(), p, p + 3);
  m.normals.insert(m.normals.end(), n, n + 3);
  m.joints.insert(m.joints.end(), j, j + 4);
  m.weights.insert(m.weights.end(), w, w + 4);
  m.parts.push_back(part);
}

// A ring at height y's share of each joint: whole below and above the blends, blended across
// 0.1 m either side of the elbow (0.4) and the wrist (0.8).
void ring_weights(float y, uint8_t j[4], float w[4]) {
  auto blend = [y](float y0) { return std::clamp((y - (y0 - 0.1f)) / 0.2f, 0.0f, 1.0f); };
  const float e = blend(0.4f), r = blend(0.8f);
  j[0] = 0, j[1] = 1, j[2] = 2, j[3] = 0;
  w[0] = 1 - e;
  w[1] = e * (1 - r);
  w[2] = e * r;
  w[3] = 0;
}

// The fin: a box round `c` of half sizes `h`, every face wound outward, bound whole to `joint`.
void add_fin(Mesh& m, const float c[3], const float h[3], uint8_t joint, uint8_t part) {
  // Each face: its normal axis and sign, and two axes u, v with u x v the normal.
  struct Face {
    int n, u, v;
    float sign;
  };
  const Face faces[6] = {{0, 1, 2, 1},  {0, 2, 1, -1}, {1, 2, 0, 1},
                         {1, 0, 2, -1}, {2, 0, 1, 1},  {2, 1, 0, -1}};
  const uint8_t j[4] = {joint, 0, 0, 0};
  const float w[4] = {1, 0, 0, 0};
  for (const Face& f : faces) {
    float n[3] = {0, 0, 0};
    n[f.n] = f.sign;
    const auto base = m.vertex_count();
    const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const auto& uv : corners) {
      float p[3] = {c[0], c[1], c[2]};
      p[f.n] += f.sign * h[f.n];
      p[f.u] += uv[0] * h[f.u];
      p[f.v] += uv[1] * h[f.v];
      vertex(m, p, n, j, w, part);
    }
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

}  // namespace

Model make_test_model() {
  Model model;
  const char* names[4] = {"root", "elbow", "wrist", "fin"};
  const float heights[4] = {0, 0.4f, 0.4f, 0.1f};
  for (int j = 0; j < 4; ++j) {
    Joint joint;
    joint.name = names[j];
    joint.parent = j - 1;
    joint.rest.t[1] = heights[j];
    model.joints.push_back(joint);
  }
  float y = 0;
  for (Joint& joint : model.joints) {
    y += joint.rest.t[1];
    joint.inverse_bind.m[7] = -y;  // the joint's rest is a lift of y: its inverse lowers by it
  }
  model.materials = {Material{"skin", {0.8f, 0.45f, 0.3f}}, Material{"fin", {0.1f, 0.3f, 0.8f}}};

  Mesh limb;
  limb.name = "limb";
  for (int r = 0; r < kRings; ++r) {
    const float ry = 0.1f * static_cast<float>(r);
    uint8_t j[4];
    float w[4];
    ring_weights(ry, j, w);
    for (int k = 0; k < kSides; ++k) {
      const double a = 2 * kPi * k / kSides;
      const auto c = static_cast<float>(core::libm::cos(a)),
                 s = static_cast<float>(core::libm::sin(a));
      const float p[3] = {kRadius * c, ry, kRadius * s}, n[3] = {c, 0, s};
      vertex(limb, p, n, j, w, 0);
    }
  }
  // Each quad between ring r and r + 1 as two triangles whose fronts face out of the tube.
  for (uint32_t r = 0; r + 1 < kRings; ++r) {
    for (uint32_t k = 0; k < kSides; ++k) {
      const uint32_t k1 = (k + 1) % kSides;
      const uint32_t a = r * kSides + k, b = (r + 1) * kSides + k;
      const uint32_t c = r * kSides + k1, d = (r + 1) * kSides + k1;
      limb.indices.insert(limb.indices.end(), {a, b, c, b, d, c});
    }
  }
  const float fin_c[3] = {0.1f, 0.9f, 0}, fin_h[3] = {0.08f, 0.05f, 0.01f};
  add_fin(limb, fin_c, fin_h, 3, 1);
  model.meshes.push_back(std::move(limb));

  // `wave`: the elbow bends to the right (toward +x: a turn about -z) and back; the wrist with it.
  Clip wave;
  wave.name = "wave";
  wave.frames = static_cast<uint32_t>(kClipRateHz) + 1;
  for (uint32_t f = 0; f < wave.frames; ++f) {
    const double t = f / double{kClipRateHz};
    const double s = core::libm::sin(kPi * t);
    for (int j = 0; j < 4; ++j) {
      Trs trs = model.joints[j].rest;
      if (j == 1) axis_angle(0, 0, -1, kPi / 3 * s, trs.r);
      if (j == 2) axis_angle(0, 0, -1, kPi / 4 * s * s, trs.r);
      wave.samples.push_back(trs);
    }
  }
  model.clips.push_back(std::move(wave));

  // `twist`: the root turns a quarter round about +y (clockwise from above).
  Clip twist;
  twist.name = "twist";
  twist.frames = static_cast<uint32_t>(kClipRateHz / 2) + 1;
  for (uint32_t f = 0; f < twist.frames; ++f) {
    const double share = f / double(twist.frames - 1);
    for (int j = 0; j < 4; ++j) {
      Trs trs = model.joints[j].rest;
      if (j == 0) axis_angle(0, 1, 0, kPi / 2 * share, trs.r);
      twist.samples.push_back(trs);
    }
  }
  model.clips.push_back(std::move(twist));
  return model;
}

}  // namespace ge::assets
