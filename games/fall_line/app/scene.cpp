// The forest's and the skinned skiers' meshes, materials and placement.
//
// ge:summary Fall Line's forest and skinned skier meshes, materials and instance placement
// ge:keywords fall-line forest figures meshes materials placement skier-model embedded
// ge:skill render
#include "fall_line/app/scene.hpp"

#include <algorithm>
#include <cmath>

#include "mountain/app/trees.hpp"
#include "render/skinning.hpp"
#include "skier/app/figure.hpp"
#include "skier/app/rig_pose.hpp"

// The skier model's bytes, compiled in by ge_embed_binary (games/fall_line/CMakeLists.txt).
extern const unsigned char fall_line_skier_glb[];
extern const std::size_t fall_line_skier_glb_size;

namespace fall_line::app {

namespace {

namespace r = ge::render;

// The jackets, racer by racer (linear rgb): red, blue, yellow, green, then again.
constexpr float kJackets[][3] = {
    {0.55f, 0.04f, 0.03f}, {0.03f, 0.12f, 0.50f}, {0.60f, 0.42f, 0.02f}, {0.04f, 0.30f, 0.06f}};

// The crowd's jackets (linear rgb): bright and dark, as a ski area wears them.
constexpr float kCrowdJackets[][3] = {
    {0.55f, 0.04f, 0.03f},  {0.03f, 0.12f, 0.50f}, {0.60f, 0.42f, 0.02f}, {0.04f, 0.30f, 0.06f},
    {0.02f, 0.02f, 0.025f}, {0.45f, 0.45f, 0.47f}, {0.50f, 0.08f, 0.30f}, {0.02f, 0.30f, 0.36f},
    {0.70f, 0.22f, 0.02f},  {0.10f, 0.05f, 0.25f}};
constexpr uint32_t kJacketCount = sizeof kCrowdJackets / sizeof kCrowdJackets[0];

// Each body's height over the model's (the model is a grown man's), by sim/crowd_rules.hpp's
// bodies: man, woman, teen, child, old man, old woman, freerider, retro.
constexpr float kBodyScale[] = {1.0f, 0.94f, 0.92f, 0.68f, 0.97f, 0.91f, 1.02f, 0.98f};

}  // namespace

std::vector<r::MeshSource> forest_meshes() { return ge::mountain::app::tree_meshes(); }

std::vector<float> forest_materials() { return ge::mountain::app::tree_materials(); }

std::vector<r::Instance> tree_instances(const ge::mountain::mapgen::Level& level,
                                        std::span<const uint8_t> felled) {
  std::vector<r::Instance> out;
  out.reserve(level.trees.size());
  for (size_t i = 0; i < level.trees.size(); ++i) {
    if (i < felled.size() && felled[i] != 0) continue;
    // A turn of its own, so the faceted crowns do not all face one way.
    const auto turn = static_cast<float>(std::fmod(static_cast<double>(i) * 2.399963, 6.283185));
    out.push_back(ge::mountain::app::tree_instance(level.trees[i], turn));
  }
  return out;
}

const ge::assets::LoadResult& skier_model() {
  static const ge::assets::LoadResult k = ge::assets::load_gltf(
      std::span<const uint8_t>(fall_line_skier_glb, fall_line_skier_glb_size), "skier.glb");
  return k;
}

std::vector<r::MeshSource> figure_meshes(const ge::assets::Model& model) {
  std::vector<r::MeshSource> out(kMaxFigures);
  for (r::MeshSource& m : out) {
    m.lods = {r::skinned_source(model.meshes[skier_lods(model)[0]], ge::skier::pose_reach())};
  }
  return out;
}

std::vector<float> figure_materials(const ge::assets::Model& model) {
  std::vector<float> out;
  for (int k = 0; k < kMaxFigures; ++k) {
    for (const ge::assets::Material& m : model.materials) {
      const float* rgb = m.name == "jacket" ? kJackets[k % 4] : m.rgb;
      out.insert(out.end(), {rgb[0], rgb[1], rgb[2]});
    }
  }
  return out;
}

r::Instance figure_instance(const ge::skier::SkierState& skier,
                            const ge::mountain::mapgen::Level& level, int racer,
                            uint32_t materials_per_racer) {
  const ge::skier::FigureFrame f =
      ge::skier::figure_frame(skier, ge::skier::World{&level, nullptr});
  r::Instance in;
  in.position[0] = f.at.x;
  in.position[1] = f.at.y;
  in.position[2] = f.at.z;
  in.rotation[0] = static_cast<float>(f.rot.x);
  in.rotation[1] = static_cast<float>(f.rot.y);
  in.rotation[2] = static_cast<float>(f.rot.z);
  in.rotation[3] = static_cast<float>(f.rot.w);
  const auto k = static_cast<uint32_t>(racer % kMaxFigures);
  in.mesh = k;
  in.material = k * materials_per_racer;
  return in;
}

std::vector<uint32_t> skier_lods(const ge::assets::Model& model) {
  std::vector<uint32_t> out;
  for (const char* name : {"skier", "skier.lod1", "skier.lod2"}) {
    for (size_t k = 0; k < model.meshes.size(); ++k) {
      if (model.meshes[k].name == name) out.push_back(static_cast<uint32_t>(k));
    }
  }
  if (out.empty()) out.push_back(0);
  return out;
}

std::vector<r::MeshSource> crowd_meshes(const ge::assets::Model& model) {
  std::vector<r::MeshSource> out(1);
  const std::vector<uint32_t> lods = skier_lods(model);
  for (size_t n = 0; n < lods.size(); ++n) {
    r::LodSource lod = r::palette_source(model.meshes[lods[n]], ge::skier::pose_reach());
    if (n < std::size(kSkierLodPx)) lod.min_px = kSkierLodPx[n];
    out[0].lods.push_back(std::move(lod));
  }
  return out;
}

std::vector<float> crowd_materials(const ge::assets::Model& model) {
  std::vector<float> out;
  for (const auto& jacket : kCrowdJackets) {
    for (const ge::assets::Material& m : model.materials) {
      const float* rgb = m.name == "jacket" ? jacket : m.rgb;
      out.insert(out.end(), {rgb[0], rgb[1], rgb[2]});
    }
  }
  return out;
}

ge::skier::SkierState crowd_skier(const sim::Amateur& a) {
  ge::skier::SkierState s;
  s.x = a.x;
  s.y = a.y;
  s.z = a.z;
  s.vx = a.vx;
  s.vz = a.vz;
  s.heading = a.heading;
  s.lean = a.lean;
  s.edge = a.lean;
  s.tuck = a.crouch;
  s.speed = std::sqrt(a.vx * a.vx + a.vz * a.vz);
  s.on_snow = true;
  s.down = a.mode == sim::AmateurMode::down && !a.rising;
  return s;
}

bool crowd_drawn(const sim::Amateur& a) {
  return a.mode != sim::AmateurMode::ride && a.mode != sim::AmateurMode::unseen;
}

r::Instance crowd_instance(const sim::Amateur& a, const ge::mountain::mapgen::Level& level,
                           uint32_t materials_per_jacket) {
  const ge::skier::FigureFrame f =
      ge::skier::figure_frame(crowd_skier(a), ge::skier::World{&level, nullptr});
  r::Instance in;
  in.position[0] = f.at.x;
  in.position[1] = f.at.y;
  in.position[2] = f.at.z;
  in.rotation[0] = static_cast<float>(f.rot.x);
  in.rotation[1] = static_cast<float>(f.rot.y);
  in.rotation[2] = static_cast<float>(f.rot.z);
  in.rotation[3] = static_cast<float>(f.rot.w);
  const float k = crowd_drawn(a) ? kBodyScale[static_cast<size_t>(a.body) % 8] : 0.0f;
  in.scale[0] = in.scale[1] = in.scale[2] = k;
  // A jacket by his id, so a group is not all one colour.
  in.material = (static_cast<uint32_t>(a.id) * 7u % kJacketCount) * materials_per_jacket;
  return in;
}

void shadow_centres(std::span<const ge::skier::SkierState> racers,
                    std::span<const sim::Amateur> crowd, const double eye[3], int most,
                    std::vector<std::array<double, 3>>& out) {
  out.clear();
  const size_t cap = static_cast<size_t>(std::max(most, 0));
  for (const ge::skier::SkierState& s : racers) {
    if (out.size() == cap) return;
    out.push_back({s.x, s.y + 1.0, s.z});
  }
  // The nearest drawn amateurs, kept sorted by an insertion into a short list (no allocation).
  constexpr size_t kMost = 64;
  const size_t want = std::min(cap - out.size(), kMost);
  std::array<double, kMost> near_d{};
  std::array<size_t, kMost> near_i{};
  size_t kept = 0;
  for (size_t i = 0; i < crowd.size() && want > 0; ++i) {
    const sim::Amateur& a = crowd[i];
    if (!crowd_drawn(a)) continue;
    const double dx = a.x - eye[0], dy = a.y - eye[1], dz = a.z - eye[2];
    const double d = dx * dx + dy * dy + dz * dz;
    if (kept == want && d >= near_d[kept - 1]) continue;
    size_t at = kept < want ? kept++ : kept - 1;
    while (at > 0 && near_d[at - 1] > d) {
      near_d[at] = near_d[at - 1];
      near_i[at] = near_i[at - 1];
      --at;
    }
    near_d[at] = d;
    near_i[at] = i;
  }
  for (size_t k = 0; k < kept; ++k) {
    const sim::Amateur& a = crowd[near_i[k]];
    out.push_back({a.x, a.y + 1.0, a.z});
  }
}

}  // namespace fall_line::app
