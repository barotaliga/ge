// The forest's and the figures' meshes, materials and placement.
//
// ge:summary Fall Line's forest and figure meshes, materials and instance placement
// ge:keywords fall-line forest figures meshes materials placement
// ge:skill render
#include "fall_line/app/scene.hpp"

#include <algorithm>
#include <cmath>

namespace fall_line::app {

namespace {

namespace r = ge::render;
using ge::mountain::mapgen::TreeKind;

// A LOD's switch, px of the sphere's radius on screen.
constexpr float kTreeLodPx = 14.0f;
constexpr float kFigureLodPx = 10.0f;

// The forest's meshes and their first material (a vertex's part adds 0 for the trunk, 1 for the
// crown).
enum ForestMesh : uint8_t { kConifer, kBroadleaf, kSnag };
constexpr uint32_t kForestMaterial[] = {0, 2, 4};

r::MeshData conifer(bool fine) {
  r::MeshData m;
  if (fine) {
    r::add_cylinder(m, -0.05f, 0.2f, 0.07f, 0.05f, 6, 0);
    r::add_cylinder(m, 0.12f, 0.66f, 1.0f, 0.38f, 9, 1);
    r::add_cylinder(m, 0.46f, 1.0f, 0.72f, 0.0f, 9, 1);
  } else {
    r::add_cylinder(m, 0.12f, 1.0f, 1.0f, 0.0f, 5, 1);
  }
  return m;
}

r::MeshData broadleaf(bool fine) {
  r::MeshData m;
  const float centre[3] = {0, 0.62f, 0}, radii[3] = {1.0f, 0.4f, 1.0f};
  if (fine) {
    r::add_cylinder(m, -0.05f, 0.5f, 0.08f, 0.05f, 6, 0);
    r::add_ellipsoid(m, centre, radii, 4, 7, 1);
  } else {
    r::add_ellipsoid(m, centre, radii, 2, 4, 1);
  }
  return m;
}

r::MeshData snag(bool fine) {
  r::MeshData m;
  r::add_cylinder(m, -0.05f, 1.0f, 0.08f, 0.03f, fine ? 6 : 3, 0);
  return m;
}

bool broadleaved(TreeKind k) {
  switch (k) {
    case TreeKind::birch:
    case TreeKind::aspen:
    case TreeKind::rowan:
    case TreeKind::alder:
    case TreeKind::willow:
    case TreeKind::beech:
    case TreeKind::maple:
    case TreeKind::ash:
      return true;
    default:
      return false;
  }
}

// A turn about +y as a quaternion (x, y, z, w).
void yaw(float angle, float out[4]) {
  out[0] = 0;
  out[1] = std::sin(angle * 0.5f);
  out[2] = 0;
  out[3] = std::cos(angle * 0.5f);
}

// The jackets, racer by racer (linear rgb): red, blue, yellow, green, then again.
constexpr float kJackets[][3] = {
    {0.55f, 0.04f, 0.03f}, {0.03f, 0.12f, 0.50f}, {0.60f, 0.42f, 0.02f}, {0.04f, 0.30f, 0.06f}};

}  // namespace

std::vector<r::MeshSource> forest_meshes() {
  std::vector<r::MeshSource> out(3);
  out[kConifer].lods = {{conifer(true), kTreeLodPx}, {conifer(false), 0}};
  out[kBroadleaf].lods = {{broadleaf(true), kTreeLodPx}, {broadleaf(false), 0}};
  out[kSnag].lods = {{snag(true), kTreeLodPx}, {snag(false), 0}};
  return out;
}

std::vector<float> forest_materials() {
  return {
      0.07f,  0.045f, 0.03f,   // 0: a conifer's trunk
      0.05f,  0.085f, 0.06f,   // 1: its crown
      0.12f,  0.11f,  0.095f,  // 2: a broadleaf's trunk
      0.095f, 0.08f,  0.065f,  // 3: its bare crown
      0.10f,  0.09f,  0.08f,   // 4: a snag
  };
}

std::vector<r::Instance> tree_instances(const ge::mountain::mapgen::Level& level) {
  std::vector<r::Instance> out;
  out.reserve(level.trees.size());
  for (size_t i = 0; i < level.trees.size(); ++i) {
    const auto& t = level.trees[i];
    r::Instance in;
    in.position[0] = t.x;
    in.position[1] = t.y;
    in.position[2] = t.z;
    // A turn of its own, so the faceted crowns do not all face one way.
    yaw(static_cast<float>(std::fmod(static_cast<double>(i) * 2.399963, 6.283185)), in.rotation);
    const uint32_t mesh = t.kind == TreeKind::snag ? kSnag
                          : broadleaved(t.kind)    ? kBroadleaf
                                                   : kConifer;
    const float crown = mesh == kSnag ? static_cast<float>(t.radius / 0.08)
                                      : static_cast<float>(std::max(t.crown, 0.3));
    in.scale[0] = in.scale[2] = crown;
    in.scale[1] = static_cast<float>(std::max(t.height, 0.5));
    in.mesh = mesh;
    in.material = kForestMaterial[mesh];
    out.push_back(in);
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
