// The forest's and the figures' meshes, materials and placement.
//
// ge:summary Fall Line's forest and figure meshes, materials and instance placement
// ge:keywords fall-line forest figures meshes materials placement
// ge:skill render
#include "fall_line/app/scene.hpp"

#include <algorithm>
#include <cmath>

#include "mountain/app/trees.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// A LOD's switch, px of the sphere's radius on screen.
constexpr float kFigureLodPx = 10.0f;

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

std::vector<r::MeshSource> figure_meshes() {
  std::vector<r::MeshSource> out(3);
  r::MeshData box;
  const float zero[3] = {0, 0, 0}, half[3] = {0.5f, 0.5f, 0.5f};
  r::add_box(box, zero, half, 0);
  out[static_cast<size_t>(ge::skier::FigureShape::box)].lods = {{box, 0}};
  r::MeshData cyl_fine, cyl_coarse;
  r::add_cylinder(cyl_fine, -0.5f, 0.5f, 0.5f, 0.5f, 10, 0);
  r::add_cylinder(cyl_coarse, -0.5f, 0.5f, 0.5f, 0.5f, 5, 0);
  out[static_cast<size_t>(ge::skier::FigureShape::cylinder)].lods = {{cyl_fine, kFigureLodPx},
                                                                     {cyl_coarse, 0}};
  r::MeshData ball_fine, ball_coarse;
  r::add_ellipsoid(ball_fine, zero, half, 6, 10, 0);
  r::add_ellipsoid(ball_coarse, zero, half, 3, 5, 0);
  out[static_cast<size_t>(ge::skier::FigureShape::ball)].lods = {{ball_fine, kFigureLodPx},
                                                                 {ball_coarse, 0}};
  return out;
}

std::vector<float> figure_materials() {
  std::vector<float> out;
  for (int k = 0; k < kMaxFigures; ++k) {
    const float* jacket = kJackets[k % 4];
    out.insert(out.end(), {
                              0.50f, 0.50f, 0.52f,              // ski
                              0.02f, 0.02f, 0.025f,             // boot
                              0.015f, 0.02f, 0.04f,             // legs
                              jacket[0], jacket[1], jacket[2],  // jacket
                              0.70f, 0.70f, 0.70f,              // helmet
                              0.25f, 0.25f, 0.27f,              // pole
                          });
  }
  return out;
}

void figure_instances(const ge::skier::SkierState& skier, const ge::mountain::mapgen::Level& level,
                      int racer, std::span<r::Instance, ge::skier::kFigurePieces> out) {
  std::array<ge::skier::PiecePose, ge::skier::kFigurePieces> pose;
  pose_figure(skier, ge::skier::World{&level, nullptr}, pose);
  const auto& pieces = ge::skier::figure_pieces();
  for (int i = 0; i < ge::skier::kFigurePieces; ++i) {
    const auto& p = pose[static_cast<size_t>(i)];
    r::Instance& in = out[static_cast<size_t>(i)];
    in.position[0] = p.centre.x;
    in.position[1] = p.centre.y;
    in.position[2] = p.centre.z;
    in.rotation[0] = static_cast<float>(p.rotation.x);
    in.rotation[1] = static_cast<float>(p.rotation.y);
    in.rotation[2] = static_cast<float>(p.rotation.z);
    in.rotation[3] = static_cast<float>(p.rotation.w);
    for (int k = 0; k < 3; ++k) in.scale[k] = static_cast<float>(p.size[k]);
    in.mesh = static_cast<uint32_t>(pieces[static_cast<size_t>(i)].shape);
    in.material = static_cast<uint32_t>((racer % kMaxFigures) * ge::skier::kFigureParts) +
                  static_cast<uint32_t>(pieces[static_cast<size_t>(i)].part);
  }
}

}  // namespace fall_line::app
