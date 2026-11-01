// The held model's conversions: a glTF posed into morph frames, the mount that turns, scales and
// moves it into the hand, the bounds and the blend between two frames.
//
// ge:summary The held model's conversions: glTF to frames, the grip mount, bounds, frame blend
// ge:keywords viewmodel held-model fit_mount held_from_model pose_frames held_bounds
// ge:keywords find_muzzle muzzle_at muzzle-tip
// ge:skill render assets
// ge:test tests/viewmodel_held_test.cpp
#include "viewmodel/held.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

#include "assets/pose.hpp"

namespace ge::viewmodel {

void held_bounds(const HeldModel& model, float min[3], float max[3], int frame) {
  for (int k = 0; k < 3; ++k) min[k] = max[k] = 0;
  bool first = true;
  const bool one = frame >= 0 && static_cast<size_t>(frame) < model.frames.size();
  for (size_t fi = one ? size_t(frame) : 0; fi < (one ? size_t(frame) + 1 : model.frames.size());
       ++fi) {
    const HeldFrame& f = model.frames[fi];
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

void fit_mount(HeldModel& model, const Mount& mount, const HeldModel* measured_on) {
  if (mount.muzzle == mount.up || mount.muzzle < 0 || mount.muzzle > 2 || mount.up < 0 ||
      mount.up > 2)
    return;
  const AxisMap a = axis_map(mount);
  for (HeldFrame& f : model.frames) {
    turn(f.positions, a);
    turn(f.normals, a);
  }
  float lo[3], hi[3];
  if (measured_on) {
    HeldModel m = *measured_on;
    for (HeldFrame& f : m.frames) {
      turn(f.positions, a);
      turn(f.normals, a);
    }
    held_bounds(m, lo, hi, mount.bounds_frame);
  } else {
    held_bounds(model, lo, hi, mount.bounds_frame);
  }
  const double length = hi[2] - lo[2];
  const double s = mount.length_m > 0 && length > 0 ? mount.length_m / length : mount.scale;
  double grip[3];
  for (int k = 0; k < 3; ++k)
    grip[k] = mount.grip_at_origin ? 0.0 : lo[k] + (hi[k] - lo[k]) * mount.grip[k];
  for (HeldFrame& f : model.frames)
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3)
      for (int k = 0; k < 3; ++k)
        f.positions[i + k] = static_cast<float>((f.positions[i + k] - grip[k]) * s);
  if (model.muzzle_vertices.empty()) find_muzzle(model);
}

void find_muzzle(HeldModel& model, double share) {
  model.muzzle_vertices.clear();
  model.muzzle_dir[0] = model.muzzle_dir[1] = 0;
  model.muzzle_dir[2] = 1;
  if (model.frames.empty()) return;
  const std::vector<float>& p = model.frames[0].positions;
  // The rest frame's own vertices, less the model's own flash (it may sit anywhere at rest).
  std::vector<uint8_t> use(p.size() / 3, 1);
  for (size_t t = 0; t < model.flame.size() && t * 3 + 2 < model.indices.size(); ++t) {
    if (model.flame[t] == 0) continue;
    for (size_t k = 0; k < 3; ++k) use[model.indices[t * 3 + k]] = 0;
  }
  float lo = 0, hi = 0;
  bool first = true;
  for (size_t v = 0; v < use.size(); ++v) {
    if (!use[v]) continue;
    const float z = p[v * 3 + 2];
    if (first || z < lo) lo = z;
    if (first || z > hi) hi = z;
    first = false;
  }
  if (first) return;
  const double front = hi - (double(hi) - lo) * share;
  // The front-most slice; nearest the long axis through the slice's own middle first.
  double mid[2] = {0, 0};
  std::vector<uint32_t> slice;
  for (size_t v = 0; v < use.size(); ++v) {
    if (!use[v] || p[v * 3 + 2] < front) continue;
    slice.push_back(static_cast<uint32_t>(v));
    mid[0] += p[v * 3];
    mid[1] += p[v * 3 + 1];
  }
  mid[0] /= static_cast<double>(slice.size());
  mid[1] /= static_cast<double>(slice.size());
  std::vector<std::pair<double, uint32_t>> near;
  near.reserve(slice.size());
  for (uint32_t v : slice) {
    const double dx = p[size_t{v} * 3] - mid[0], dy = p[size_t{v} * 3 + 1] - mid[1];
    near.emplace_back(dx * dx + dy * dy, v);
  }
  std::sort(near.begin(), near.end());  // ties by index: the same pick every time
  for (size_t i = 0; i < near.size() && i < 16; ++i)
    model.muzzle_vertices.push_back(near[i].second);
}

int barrel_of_frame(const HeldModel& model, int frame) {
  if (frame < 0 || static_cast<size_t>(frame) >= model.frame_barrel.size()) return -1;
  const int b = model.frame_barrel[size_t(frame)];
  return b >= 0 && static_cast<size_t>(b) < model.barrels.size() ? b : -1;
}

void muzzle_at(const HeldModel& model, int a, int b, double t, float out[3], int barrel) {
  out[0] = out[1] = out[2] = 0;
  const std::vector<uint32_t>& tip = barrel >= 0 &&
                                             static_cast<size_t>(barrel) < model.barrels.size() &&
                                             !model.barrels[size_t(barrel)].empty()
                                         ? model.barrels[size_t(barrel)]
                                         : model.muzzle_vertices;
  if (model.frames.empty() || tip.empty()) return;
  const int last = static_cast<int>(model.frames.size()) - 1;
  a = a < 0 ? 0 : a > last ? last : a;
  b = b < 0 ? 0 : b > last ? last : b;
  const std::vector<float>& pa = model.frames[size_t(a)].positions;
  const std::vector<float>& pb = model.frames[size_t(b)].positions;
  const double u = t < 0 ? 0 : t > 1 ? 1 : t;
  double sum[3] = {0, 0, 0};
  for (uint32_t v : tip) {
    for (size_t k = 0; k < 3; ++k) {
      const double x = pa[size_t{v} * 3 + k], y = pb[size_t{v} * 3 + k];
      sum[k] += x + (y - x) * u;
    }
  }
  const auto n = static_cast<double>(tip.size());
  for (size_t k = 0; k < 3; ++k) out[k] = static_cast<float>(sum[k] / n);
}

namespace {

// A model loaded with its textures kept (assets::LoadOptions::keep_textures): the first textured
// material's picture is the skin, every vertex reads it at its coordinates, and the material's
// glow picture (the file's emissive texture) marks the texels that light themselves where it is
// at least half bright. A model with no kept picture keeps its colours.
void skin_from_kept(const assets::Model& model, HeldModel& out) {
  const assets::Material* textured = nullptr;
  for (const assets::Material& m : model.materials) {
    if (m.texture >= 0 && static_cast<size_t>(m.texture) < model.images.size()) {
      textured = &m;
      break;
    }
  }
  if (!textured) return;
  for (const assets::Mesh& mesh : model.meshes)
    if (mesh.uv.size() != size_t{mesh.vertex_count()} * 2) return;
  out.skin.image = model.images[static_cast<size_t>(textured->texture)];
  for (const assets::Mesh& mesh : model.meshes)
    out.uv.insert(out.uv.end(), mesh.uv.begin(), mesh.uv.end());
  out.colour.clear();
  if (textured->glow < 0 || static_cast<size_t>(textured->glow) >= model.images.size()) return;
  const assets::Image& glow = model.images[static_cast<size_t>(textured->glow)];
  const assets::Image& img = out.skin.image;
  out.skin.glow.assign(size_t(img.width) * size_t(img.height), 0);
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const uint8_t* g =
          assets::texel(glow, (static_cast<float>(x) + 0.5f) / static_cast<float>(img.width),
                        (static_cast<float>(y) + 0.5f) / static_cast<float>(img.height));
      if (std::max({g[0], g[1], g[2]}) >= 128) out.skin.glow[size_t(y) * img.width + x] = 1;
    }
  }
}

}  // namespace

namespace {

// One frame of `model`: clip `clip` at `t_s`, or the rest pose when clip < 0.
HeldFrame posed_frame(const assets::Model& model, int clip, double t_s, std::string name) {
  const size_t joints = model.joints.size();
  std::vector<assets::Trs> local(joints);
  std::vector<assets::Affine> global(joints), palette(joints);
  if (clip >= 0)
    assets::sample_clip(model, model.clips[size_t(clip)], t_s, false, local);
  else
    assets::rest_pose(model, local);
  assets::pose_globals(model, local, global);
  assets::skin_palette(model, global, palette);
  HeldFrame hf;
  hf.name = std::move(name);
  size_t n = 0;
  for (const assets::Mesh& mesh : model.meshes) n += size_t{mesh.vertex_count()} * 3;
  hf.positions.resize(n);
  hf.normals.resize(n);
  size_t at = 0;
  for (const assets::Mesh& mesh : model.meshes) {
    const size_t m = size_t{mesh.vertex_count()} * 3;
    assets::skin_mesh(mesh, palette, std::span<float>(hf.positions.data() + at, m),
                      std::span<float>(hf.normals.data() + at, m));
    at += m;
  }
  return hf;
}

// The shared half: the triangles and each vertex's colour (or the kept skin), meshes one after
// another.
HeldModel held_surface(const assets::Model& model) {
  HeldModel out;
  uint32_t base = 0;
  for (const assets::Mesh& mesh : model.meshes) {
    for (uint32_t i : mesh.indices) out.indices.push_back(base + i);
    for (uint8_t p : mesh.parts) {
      const float* rgb = p < model.materials.size() ? model.materials[p].rgb : nullptr;
      for (int k = 0; k < 3; ++k) out.colour.push_back(rgb ? rgb[k] : 0.6f);
    }
    base += mesh.vertex_count();
  }
  skin_from_kept(model, out);
  return out;
}

}  // namespace

HeldModel held_from_model(const assets::Model& model, int clip, int frames) {
  if (frames < 1) frames = 1;
  if (clip < 0 || clip >= static_cast<int>(model.clips.size())) frames = 1;
  HeldModel out = held_surface(model);
  for (int f = 0; f < frames; ++f) {
    if (clip >= 0 && frames > 1) {
      const assets::Clip& c = model.clips[size_t(clip)];
      out.frames.push_back(posed_frame(model, clip, c.duration_s() * f / (frames - 1),
                                       c.name + "." + std::to_string(f)));
    } else {
      out.frames.push_back(posed_frame(model, -1, 0, "rest"));
    }
  }
  return out;
}

HeldModel held_with_clips(const assets::Model& model, double fps) {
  HeldModel out = held_surface(model);
  out.frames.push_back(posed_frame(model, -1, 0, "rest"));
  if (fps <= 0) return out;
  for (size_t c = 0; c < model.clips.size(); ++c) {
    const assets::Clip& clip = model.clips[c];
    HeldClip hc;
    hc.name = clip.name;
    hc.first = static_cast<int>(out.frames.size());
    hc.fps = fps;
    const double len = clip.duration_s();
    hc.count = static_cast<int>(len * fps + 0.999) + 1;
    for (int f = 0; f < hc.count; ++f) {
      const double t = f + 1 == hc.count ? len : f / fps;
      out.frames.push_back(
          posed_frame(model, static_cast<int>(c), t, clip.name + "." + std::to_string(f)));
    }
    out.clips.push_back(std::move(hc));
  }
  return out;
}

bool mark_hands(HeldModel& model, const assets::Model& source, std::string_view marker) {
  model.hand.assign(model.indices.size() / 3, 0);
  size_t tri = 0;
  bool any = false;
  for (const assets::Mesh& mesh : source.meshes) {
    const size_t n = mesh.indices.size() / 3;
    const bool hand = !marker.empty() && mesh.name.find(marker) != std::string::npos;
    for (size_t i = 0; i < n && tri < model.hand.size(); ++i, ++tri) model.hand[tri] = hand ? 1 : 0;
    any = any || (hand && n > 0);
  }
  if (!any) model.hand.clear();
  return any;
}

void hidden_triangles(const HeldModel& model, bool flame, bool hands, std::vector<uint8_t>& out) {
  out.clear();
  const bool f = flame && !model.flame.empty(), h = hands && !model.hand.empty();
  if (!f && !h) return;
  const size_t tris = model.indices.size() / 3;
  out.assign(tris, 0);
  for (size_t t = 0; t < tris; ++t) {
    const bool fl = f && t < model.flame.size() && model.flame[t] != 0;
    const bool hd = h && t < model.hand.size() && model.hand[t] != 0;
    out[t] = fl || hd ? 1 : 0;
  }
}

int find_held_clip(const HeldModel& model, std::string_view name) {
  for (size_t i = 0; i < model.clips.size(); ++i)
    if (model.clips[i].name == name) return static_cast<int>(i);
  return -1;
}

void held_clip_frames(const HeldModel& model, int clip, double t_s, bool loop, int& a, int& b,
                      double& t) {
  a = b = 0;
  t = 0;
  if (clip < 0 || static_cast<size_t>(clip) >= model.clips.size()) return;
  const HeldClip& c = model.clips[size_t(clip)];
  if (c.count <= 0) return;
  if (c.count == 1) {
    a = b = c.first;
    return;
  }
  double at = t_s * c.fps;
  const double last = c.count - 1;
  if (loop) {
    at -= last * static_cast<double>(static_cast<long long>(at / last));
    if (at < 0) at += last;
  } else {
    at = at < 0 ? 0 : at > last ? last : at;
  }
  const int i = static_cast<int>(at);
  a = c.first + (i < c.count - 1 ? i : c.count - 1);
  b = c.first + (i + 1 < c.count ? i + 1 : c.count - 1);
  t = at - i;
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
