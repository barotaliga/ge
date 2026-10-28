// The held model's conversions: a glTF posed into morph frames, the mount that turns, scales and
// moves it into the hand, the bounds and the blend between two frames.
//
// ge:summary The held model's conversions: glTF to frames, the grip mount, bounds, frame blend
// ge:keywords viewmodel held-model fit_mount held_from_model pose_frames held_bounds
// ge:skill render assets
// ge:test tests/viewmodel_held_test.cpp
#include "viewmodel/held.hpp"

#include <cmath>
#include <span>

#include "assets/pose.hpp"

namespace ge::viewmodel {

void held_bounds(const HeldModel& model, float min[3], float max[3]) {
  for (int k = 0; k < 3; ++k) min[k] = max[k] = 0;
  bool first = true;
  for (const HeldFrame& f : model.frames) {
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3) {
      for (int k = 0; k < 3; ++k) {
        const float v = f.positions[i + k];
        if (first || v < min[k]) min[k] = v;
        if (first || v > max[k]) max[k] = v;
      }
      first = false;
    }
  }
}

namespace {

// The held frame's axis `out` (0 x, 1 y, 2 z) read off the model's axes: which model axis feeds
// it, and with which sign.
struct AxisMap {
  int from[3];
  float sign[3];
};

AxisMap axis_map(const Mount& m) {
  AxisMap a{{2, 1, 0}, {1, 1, 1}};
  a.from[2] = m.muzzle;
  a.sign[2] = m.muzzle_negative ? -1.0f : 1.0f;
  a.from[1] = m.up;
  a.sign[1] = m.up_negative ? -1.0f : 1.0f;
  // The right is the third axis, signed so the frame keeps its handedness: right = up x forward
  // in the engine's frame (x = y cross z), the same cross product of the model axes they came from.
  const int r = 3 - m.muzzle - m.up;
  a.from[0] = r;
  // The model's unit axes e_up and e_fwd: (e_up x e_fwd) is +-e_r, by the cyclic order.
  const bool cyclic = (m.up + 1) % 3 == m.muzzle;  // e_up x e_fwd = +e_r when (up, fwd, r) cycle
  a.sign[0] = (cyclic ? 1.0f : -1.0f) * a.sign[1] * a.sign[2];
  return a;
}

void turn(std::vector<float>& v, const AxisMap& a) {
  for (size_t i = 0; i + 2 < v.size(); i += 3) {
    const float in[3] = {v[i], v[i + 1], v[i + 2]};
    for (int k = 0; k < 3; ++k) v[i + k] = in[a.from[k]] * a.sign[k];
  }
}

}  // namespace

void fit_mount(HeldModel& model, const Mount& mount) {
  if (mount.muzzle == mount.up || mount.muzzle < 0 || mount.muzzle > 2 || mount.up < 0 ||
      mount.up > 2)
    return;
  const AxisMap a = axis_map(mount);
  for (HeldFrame& f : model.frames) {
    turn(f.positions, a);
    turn(f.normals, a);
  }
  float lo[3], hi[3];
  held_bounds(model, lo, hi);
  const double length = hi[2] - lo[2];
  const double s = mount.length_m > 0 && length > 0 ? mount.length_m / length : mount.scale;
  double grip[3];
  for (int k = 0; k < 3; ++k) grip[k] = lo[k] + (hi[k] - lo[k]) * mount.grip[k];
  for (HeldFrame& f : model.frames)
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3)
      for (int k = 0; k < 3; ++k)
        f.positions[i + k] = static_cast<float>((f.positions[i + k] - grip[k]) * s);
}

HeldModel held_from_model(const assets::Model& model, int clip, int frames) {
  HeldModel out;
  if (frames < 1) frames = 1;
  if (clip < 0 || clip >= static_cast<int>(model.clips.size())) frames = 1;
  const size_t joints = model.joints.size();
  std::vector<assets::Trs> local(joints);
  std::vector<assets::Affine> global(joints), palette(joints);

  // The shared half: the triangles and each vertex's colour, meshes one after another.
  uint32_t base = 0;
  for (const assets::Mesh& mesh : model.meshes) {
    for (uint32_t i : mesh.indices) out.indices.push_back(base + i);
    for (uint8_t p : mesh.parts) {
      const float* rgb = p < model.materials.size() ? model.materials[p].rgb : nullptr;
      for (int k = 0; k < 3; ++k) out.colour.push_back(rgb ? rgb[k] : 0.6f);
    }
    base += mesh.vertex_count();
  }

  for (int f = 0; f < frames; ++f) {
    HeldFrame hf;
    if (clip >= 0 && frames > 1) {
      const assets::Clip& c = model.clips[clip];
      const double t = c.duration_s() * f / (frames - 1);
      assets::sample_clip(model, c, t, false, local);
      hf.name = c.name + "." + std::to_string(f);
    } else {
      assets::rest_pose(model, local);
      hf.name = "rest";
    }
    assets::pose_globals(model, local, global);
    assets::skin_palette(model, global, palette);
    hf.positions.resize(size_t{base} * 3);
    hf.normals.resize(size_t{base} * 3);
    size_t at = 0;
    for (const assets::Mesh& mesh : model.meshes) {
      const size_t n = size_t{mesh.vertex_count()} * 3;
      assets::skin_mesh(mesh, palette, std::span<float>(hf.positions.data() + at, n),
                        std::span<float>(hf.normals.data() + at, n));
      at += n;
    }
    out.frames.push_back(std::move(hf));
  }
  return out;
}

void pose_frames(const HeldModel& model, int a, int b, double t, std::vector<float>& positions,
                 std::vector<float>& normals) {
  const int last = static_cast<int>(model.frames.size()) - 1;
  a = a < 0 ? 0 : a > last ? last : a;
  b = b < 0 ? 0 : b > last ? last : b;
  const HeldFrame& fa = model.frames[a];
  const HeldFrame& fb = model.frames[b];
  const float u = static_cast<float>(t < 0 ? 0 : t > 1 ? 1 : t);
  const size_t n = fa.positions.size();
  positions.resize(n);
  normals.resize(n);
  for (size_t i = 0; i < n; ++i)
    positions[i] = fa.positions[i] + (fb.positions[i] - fa.positions[i]) * u;
  for (size_t i = 0; i + 2 < n; i += 3) {
    float v[3];
    for (int k = 0; k < 3; ++k)
      v[k] = fa.normals[i + k] + (fb.normals[i + k] - fa.normals[i + k]) * u;
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    for (int k = 0; k < 3; ++k) normals[i + k] = v[k] * inv;
  }
}

}  // namespace ge::viewmodel
