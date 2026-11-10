// A skeletal model baked into a morph model's frames (morph/skeletal.hpp).
//
// ge:summary The skeletal bake: each clip posed at a steady rate, every pose a morph frame
// ge:keywords morph skeletal bake_skeletal pose skin_mesh frame-group atlas
// ge:skill morph assets
// ge:test tests/morph_skeletal_test.cpp
#include "morph/skeletal.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "assets/pose.hpp"

namespace ge::morph {
namespace {

// The engine's frame into the classic file's, in its units: (x, y, z) -> (z, -x, y) / metres.
void to_file(const float e[3], double metres, float out[3]) {
  out[0] = static_cast<float>(e[2] / metres);
  out[1] = static_cast<float>(-e[0] / metres);
  out[2] = static_cast<float>(e[1] / metres);
}

// Every mesh posed by `local`, in the file's frame, one position list for the whole model.
std::vector<float> posed(const assets::Model& m, std::span<const assets::Trs> local,
                         double metres) {
  std::vector<assets::Affine> global(m.joints.size()), palette(m.joints.size());
  assets::pose_globals(m, local, global);
  assets::skin_palette(m, global, palette);
  std::vector<float> out;
  for (const assets::Mesh& mesh : m.meshes) {
    std::vector<float> p(mesh.positions.size()), n(mesh.normals.size());
    assets::skin_mesh(mesh, palette, p, n);
    for (size_t v = 0; v * 3 < p.size(); ++v) {
      float f[3];
      to_file(&p[v * 3], metres, f);
      out.insert(out.end(), f, f + 3);
    }
  }
  return out;
}

Frame frame_of(std::vector<float> positions, std::string name, int group, float interval) {
  Frame f;
  f.name = std::move(name);
  f.group = group;
  f.interval = interval;
  f.min = {1e30f, 1e30f, 1e30f};
  f.max = {-1e30f, -1e30f, -1e30f};
  for (size_t v = 0; v * 3 < positions.size(); ++v)
    for (int k = 0; k < 3; ++k) {
      f.min[k] = std::min(f.min[k], positions[v * 3 + k]);
      f.max[k] = std::max(f.max[k], positions[v * 3 + k]);
    }
  f.positions = std::move(positions);
  return f;
}

}  // namespace

bool bake_skeletal(const assets::Model& in, std::string_view name, const BakeOptions& options,
                   Model* out, std::string* error) {
  *out = {};
  const auto fail = [&](const std::string& why) {
    *out = {};
    if (error != nullptr) *error = std::string(name) + ": " + why;
    return false;
  };
  if (in.meshes.empty() || in.joints.empty())
    return fail("no mesh or no joint to bake; next: export the model with its mesh and skeleton");
  if (!(options.fps > 0) || !(options.metres > 0))
    return fail(
        "the bake's rate and unit must be above 0; next: pass fps 20 and the read's metres");

  // The vertices: texture coordinates into their material's rect; the triangles, outside kept.
  uint32_t base = 0;
  for (const assets::Mesh& mesh : in.meshes) {
    for (uint32_t v = 0; v < mesh.vertex_count(); ++v) {
      float u = mesh.uv.size() >= (size_t{v} + 1) * 2 ? mesh.uv[size_t{v} * 2] : 0.0f;
      float t = mesh.uv.size() >= (size_t{v} + 1) * 2 ? mesh.uv[size_t{v} * 2 + 1] : 0.0f;
      const uint8_t part = mesh.parts[v];
      if (part < options.rects.size()) {
        const std::array<float, 4>& r = options.rects[part];
        u = r[0] + std::clamp(u, 0.0f, 1.0f) * (r[2] - r[0]);
        t = r[1] + std::clamp(t, 0.0f, 1.0f) * (r[3] - r[1]);
      }
      out->st.push_back(u);
      out->st.push_back(t);
      out->source_vertex.push_back(static_cast<int>(base + v));
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
      out->indices.insert(out->indices.end(), {base + mesh.indices[i], base + mesh.indices[i + 2],
                                               base + mesh.indices[i + 1]});
    base += mesh.vertex_count();
  }
  if (base == 0) return fail("its meshes have no vertices; next: export the model again");

  // The frames: a group a clip, or the rest pose alone.
  std::vector<assets::Trs> local(in.joints.size());
  if (in.clips.empty()) {
    assets::rest_pose(in, local);
    out->frames.push_back(frame_of(posed(in, local, options.metres), "rest", 0, 0));
  }
  for (size_t c = 0; c < in.clips.size(); ++c) {
    const assets::Clip& clip = in.clips[c];
    const double end = clip.duration_s();
    const double steps = std::round(end * options.fps);
    if (!(steps < kMaxBakedFrames) || out->frames.size() + size_t(steps) >= kMaxBakedFrames)
      return fail("more than " + std::to_string(kMaxBakedFrames) + " frames at " +
                  std::to_string(options.fps) + " a second; next: bake it at a lower rate");
    const auto count = static_cast<size_t>(steps) + 1;
    std::vector<std::vector<float>> poses;
    for (size_t k = 0; k < count; ++k) {
      assets::sample_clip(in, clip, std::min(end, static_cast<double>(k) / options.fps), false,
                          local);
      poses.push_back(posed(in, local, options.metres));
    }
    // A looping clip's last pose is its first: leave it off, so the group plays round evenly.
    if (poses.size() > 1) {
      float apart = 0;
      for (size_t i = 0; i < poses[0].size(); ++i)
        apart = std::max(apart, std::abs(poses.back()[i] - poses[0][i]));
      if (apart < 1e-4f) poses.pop_back();
    }
    const float interval = poses.size() > 1 ? static_cast<float>(1 / options.fps) : 0.0f;
    for (size_t k = 0; k < poses.size(); ++k)
      out->frames.push_back(frame_of(std::move(poses[k]), clip.name + std::to_string(k),
                                     static_cast<int>(c), interval));
  }

  // One stand-in skin; the reach for the radius.
  Skin skin;
  skin.width = skin.height = 8;
  skin.indices.assign(64, options.grey);
  out->skins.push_back(std::move(skin));
  for (const Frame& f : out->frames)
    for (size_t v = 0; v * 3 < f.positions.size(); ++v) {
      const float* p = &f.positions[v * 3];
      out->radius = std::max(out->radius, std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]));
    }
  return true;
}

}  // namespace ge::morph
