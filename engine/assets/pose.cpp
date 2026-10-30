// Posing: the clip's blend, the chain to model space, the palette, CPU skinning, the reach.
//
// ge:summary Pose definitions: clip blend, global chain, palette, CPU skinning, reach box
// ge:keywords assets pose sample blend nlerp skin reach
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#include "assets/pose.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/libm/libm.hpp"

namespace ge::assets {

namespace {

// a + (b - a) * f on the shorter arc, normalised.
void nlerp(const float a[4], const float b[4], float f, float out[4]) {
  const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  const float sb = dot < 0 ? -1.0f : 1.0f;
  float len2 = 0;
  for (int k = 0; k < 4; ++k) {
    out[k] = a[k] + (sb * b[k] - a[k]) * f;
    len2 += out[k] * out[k];
  }
  const float inv = len2 > 0 ? 1.0f / std::sqrt(len2) : 0.0f;
  for (int k = 0; k < 4; ++k) out[k] *= inv;
}

// The two baked frames either side of time t_s (wrapped into the clip with `loop`, else held at
// its ends) and the blend between them. The clip has at least one frame.
float frames_at(const Clip& clip, double t_s, bool loop, uint32_t& f0, uint32_t& f1) {
  const double d = clip.duration_s();
  double t = t_s;
  if (loop && d > 0) {
    t = t - d * std::floor(t / d);
  }
  t = std::clamp(t, 0.0, d);
  const double frame = t * kClipRateHz;
  f0 = std::min(static_cast<uint32_t>(frame), clip.frames - 1);
  f1 = std::min(f0 + 1, clip.frames - 1);
  return static_cast<float>(frame - f0);
}

}  // namespace

void rest_pose(const Model& model, std::span<Trs> local) {
  const size_t n = std::min(local.size(), model.joints.size());
  for (size_t j = 0; j < n; ++j) local[j] = model.joints[j].rest;
}

void sample_clip(const Model& model, const Clip& clip, double t_s, bool loop,
                 std::span<Trs> local) {
  const size_t joints = model.joints.size();
  if (clip.frames == 0 || clip.samples.size() < size_t{clip.frames} * joints) {
    rest_pose(model, local);
    return;
  }
  uint32_t f0 = 0, f1 = 0;
  const float f = frames_at(clip, t_s, loop, f0, f1);
  const Trs* a = &clip.samples[size_t{f0} * joints];
  const Trs* b = &clip.samples[size_t{f1} * joints];
  const size_t n = std::min(local.size(), joints);
  for (size_t j = 0; j < n; ++j) {
    Trs& o = local[j];
    for (int k = 0; k < 3; ++k) {
      o.t[k] = a[j].t[k] + (b[j].t[k] - a[j].t[k]) * f;
      o.s[k] = a[j].s[k] + (b[j].s[k] - a[j].s[k]) * f;
    }
    nlerp(a[j].r, b[j].r, f, o.r);
  }
}

void sample_weights(const Model& model, const Clip* clip, size_t mesh, double t_s, bool loop,
                    std::span<float> weights) {
  if (mesh >= model.meshes.size()) return;
  const std::vector<float>& own = model.meshes[mesh].target_weights;
  const size_t n = std::min(weights.size(), own.size());
  const MorphTrack* track = nullptr;
  if (clip && clip->frames > 0) {
    for (const MorphTrack& m : clip->morphs) {
      if (m.mesh == static_cast<int32_t>(mesh) &&
          m.weights.size() >= size_t{clip->frames} * own.size()) {
        track = &m;
      }
    }
  }
  if (!track) {
    std::copy(own.begin(), own.begin() + static_cast<std::ptrdiff_t>(n), weights.begin());
    return;
  }
  uint32_t f0 = 0, f1 = 0;
  const float f = frames_at(*clip, t_s, loop, f0, f1);
  const float* a = &track->weights[size_t{f0} * own.size()];
  const float* b = &track->weights[size_t{f1} * own.size()];
  for (size_t k = 0; k < n; ++k) weights[k] = a[k] + (b[k] - a[k]) * f;
}

void morph_mesh(const Mesh& mesh, std::span<const float> weights, std::span<float> positions,
                std::span<float> normals) {
  const size_t n = std::min({mesh.positions.size(), positions.size(), normals.size()});
  std::copy(mesh.positions.begin(), mesh.positions.begin() + static_cast<std::ptrdiff_t>(n),
            positions.begin());
  std::copy(mesh.normals.begin(), mesh.normals.begin() + static_cast<std::ptrdiff_t>(n),
            normals.begin());
  for (size_t t = 0; t < mesh.targets.size() && t < weights.size(); ++t) {
    const float w = weights[t];
    if (w == 0) continue;
    const MorphTarget& target = mesh.targets[t];
    const size_t m = std::min({n, target.positions.size(), target.normals.size()});
    for (size_t i = 0; i < m; ++i) {
      positions[i] += w * target.positions[i];
      normals[i] += w * target.normals[i];
    }
  }
  for (size_t i = 0; i + 2 < n; i += 3) {
    const float len = std::sqrt(normals[i] * normals[i] + normals[i + 1] * normals[i + 1] +
                                normals[i + 2] * normals[i + 2]);
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    for (size_t k = 0; k < 3; ++k) normals[i + k] *= inv;
  }
}

void pose_globals(const Model& model, std::span<const Trs> local, std::span<Affine> global) {
  const size_t n = std::min({local.size(), global.size(), model.joints.size()});
  for (size_t j = 0; j < n; ++j) {
    const Affine l = to_affine(local[j]);
    const int32_t p = model.joints[j].parent;
    global[j] = p >= 0 ? compose(global[static_cast<size_t>(p)], l) : l;
  }
}

void skin_palette(const Model& model, std::span<const Affine> global, std::span<Affine> palette) {
  const size_t n = std::min({global.size(), palette.size(), model.joints.size()});
  for (size_t j = 0; j < n; ++j) palette[j] = compose(global[j], model.joints[j].inverse_bind);
}

void skin_mesh(const Mesh& mesh, std::span<const Affine> palette, std::span<float> positions,
               std::span<float> normals) {
  skin_mesh(mesh, palette, mesh.positions, mesh.normals, positions, normals);
}

void skin_mesh(const Mesh& mesh, std::span<const Affine> palette, std::span<const float> bind,
               std::span<const float> bind_normals, std::span<float> positions,
               std::span<float> normals) {
  const uint32_t n = std::min<uint32_t>(
      mesh.vertex_count(), static_cast<uint32_t>(std::min(bind.size(), bind_normals.size()) / 3));
  for (uint32_t v = 0; v < n; ++v) {
    // The weighted sum of the influences' matrices, then one transform.
    Affine m;
    for (float& e : m.m) e = 0;
    for (int k = 0; k < kJointsPerVertex; ++k) {
      const float w = mesh.weights[size_t{v} * kJointsPerVertex + k];
      if (w == 0) continue;
      const uint8_t j = mesh.joints[size_t{v} * kJointsPerVertex + k];
      if (j >= palette.size()) continue;
      for (int e = 0; e < 12; ++e) m.m[e] += w * palette[j].m[e];
    }
    float p[3], q[3];
    transform_point(m, &bind[size_t{v} * 3], p);
    transform_vector(m, &bind_normals[size_t{v} * 3], q);
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    for (int k = 0; k < 3; ++k) {
      if (size_t{v} * 3 + k < positions.size()) positions[size_t{v} * 3 + k] = p[k];
      if (size_t{v} * 3 + k < normals.size()) normals[size_t{v} * 3 + k] = q[k] * inv;
    }
  }
}

Box reach_box(const Model& model, const Mesh& mesh, float margin) {
  const size_t joints = model.joints.size();
  std::vector<Trs> local(joints);
  std::vector<Affine> global(joints), palette(joints);
  std::vector<float> pos(size_t{mesh.vertex_count()} * 3), nrm(pos.size());
  std::vector<float> bind(pos.size()), bind_n(pos.size()), weights(mesh.targets.size());
  size_t self = 0;
  while (self < model.meshes.size() && &model.meshes[self] != &mesh) ++self;
  Box box;
  bool first = true;
  auto add = [&](const Clip* clip, double t) {
    pose_globals(model, local, global);
    skin_palette(model, global, palette);
    sample_weights(model, clip, self, t, false, weights);
    morph_mesh(mesh, weights, bind, bind_n);
    skin_mesh(mesh, palette, bind, bind_n, pos, nrm);
    for (size_t i = 0; i + 2 < pos.size(); i += 3) {
      for (int k = 0; k < 3; ++k) {
        if (first || pos[i + k] < box.min[k]) box.min[k] = pos[i + k];
        if (first || pos[i + k] > box.max[k]) box.max[k] = pos[i + k];
      }
      first = false;
    }
  };
  rest_pose(model, local);
  add(nullptr, 0);
  for (const Clip& clip : model.clips) {
    for (uint32_t f = 0; f < clip.frames; ++f) {
      sample_clip(model, clip, f / double{kClipRateHz}, false, local);
      add(&clip, f / double{kClipRateHz});
    }
  }
  for (int k = 0; k < 3; ++k) {
    const float grow = (box.max[k] - box.min[k]) * margin;
    box.min[k] -= grow;
    box.max[k] += grow;
  }
  return box;
}

double turn_from_rest(const Joint& joint, const Trs& local, double out[3]) {
  // The turn between the two, conj(rest) * local, as 2 atan2(|xyz|, |w|): exact near zero, where
  // an arccosine of the dot product loses the small angles a jolt shows in.
  const double a[4] = {-joint.rest.r[0], -joint.rest.r[1], -joint.rest.r[2], joint.rest.r[3]};
  const double b[4] = {local.r[0], local.r[1], local.r[2], local.r[3]};
  const double x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  const double y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  const double z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  const double w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
  const double s = std::sqrt(x * x + y * y + z * z);
  const double angle = 2.0 * core::libm::atan2(s, std::fabs(w));
  const double k = s > 0 ? (w < 0 ? -angle : angle) / s : 0.0;
  out[0] = x * k;
  out[1] = y * k;
  out[2] = z * k;
  return angle;
}

}  // namespace ge::assets
