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

}  // namespace

std::vector<r::MeshSource> forest_meshes() { return ge::mountain::app::tree_meshes(); }

std::vector<float> forest_materials() { return ge::mountain::app::tree_materials(); }

std::vector<r::Instance> tree_instances(const ge::mountain::mapgen::Level& level) {
  std::vector<r::Instance> out;
  out.reserve(level.trees.size());
  for (size_t i = 0; i < level.trees.size(); ++i) {
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
    m.lods = {r::skinned_source(model.meshes[0], ge::skier::pose_reach())};
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

}  // namespace fall_line::app
