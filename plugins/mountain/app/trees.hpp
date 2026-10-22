// THE TREES AS DRAWN: every tree of a snow mountain built in code, a mesh for each FORM and
// VARIANT at three LEVELS OF DETAIL, for a game to draw as the forest's instance family
// (render/instances.hpp). A mesh is unit-sized: the ground at y 0, the top at y 1, the crown 1
// across in radius at its widest, so an instance scales it by its tree's crown (x, z) and height
// (y) and nothing else is per tree.
//
// THE FORMS (`TreeForm`), each a different read from a distance:
//   conifer    a stack of drooping skirts on a trunk that runs up into them (spruce, fir, the
//              black spruce, hemlock, juniper)
//   pine       a bare trunk under separate flat pads of needles (the pines)
//   larch      in winter a bare trunk and whorls of thin arms
//   broadleaf  a trunk forking into stems, limbs off them and sprays of twigs off the limbs, bare
//   snag       a dead trunk, broken at the top, a few stubs of arms
// A form has `kTreeVariants` variants (how many tiers, how far they droop, how the pads and
// limbs lie, how much snow they hold), picked per tree from its place (`tree_variant`), so no
// two neighbours stand the same.
//
// CHUNKY AND FACETED, on purpose: a few broad tiers or pads a tree, every ring's radius jittered
// a little, flat-shaded faces (render/mesh.hpp), a wood that reads as solid at a fraction of the
// triangles a fine one would cost.
//
// THE SNOW is in the mesh, by the way each face looks: a face turned up past the variant's
// threshold carries snow, one a little steeper is frosted, one turned down is the shaded green
// under a lip. So every tier is white on its upper face and dark under its lip, every pad has a
// cap, every limb a white line along its top. The faces' PARTS (`TreePart`) pick the material:
// an instance's material is its kind's first (`tree_material`), and the kind's table
// (`tree_materials`) colours each part, so a kind's needles, bark and twigs are its own.
//
// THE LEVELS OF DETAIL are the same tree cut lighter, so a tree handed from one to the next keeps
// its silhouette: FULL at the lens, MID in the middle distance, FAR a sketch (a cone or two, one
// pad, a haze of twigs). `kTreeLodPixels` are the switches, px of the bounding sphere's radius.
//
// ge:summary The trees as drawn: procedural forms, variants and LODs, snow by facing, materials
// ge:keywords trees forest tree-mesh conifer spruce pine larch broadleaf snag variant lod
// ge:keywords snow-load frost tiers skirts pads limbs twigs faceted tree_meshes tree_instance
// ge:keywords tree_form tree_variant tree_materials tree_material tree-parts
// ge:skill mountain render
// ge:test tests/mountain_trees_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "render/instances.hpp"
#include "render/mesh.hpp"

namespace ge::mountain::app {

enum class TreeForm : uint8_t { conifer, pine, larch, broadleaf, snag };
inline constexpr int kTreeForms = 5;
inline constexpr int kTreeVariants = 4;
inline constexpr int kTreeLods = 3;
// The LODs' switches: FULL while the sphere's radius is at least this many px, MID at least the
// next, FAR below.
inline constexpr float kTreeLodPixels[kTreeLods] = {36.0f, 9.0f, 0.0f};

// A face's part: an instance's material plus this picks its colour.
enum TreePart : uint8_t {
  kBark,         // the trunk, the stems and limbs
  kNeedle,       // needles in the light
  kNeedleShade,  // needles under a lip, out of it
  kSnow,         // snow lying on an upper face
  kFrost,        // needles dusted, a face too steep to hold a cap
  kTwig,         // a bare crown's twigs, and a larch's
};
inline constexpr int kTreeParts = 6;
inline constexpr int kTreeKinds = static_cast<int>(mapgen::TreeKind::ash) + 1;

TreeForm tree_form(mapgen::TreeKind kind);
// The variant a tree at (x, z) stands as, 0 .. kTreeVariants - 1.
int tree_variant(double x, double z);
// One form's variant at one LOD (0 full, 1 mid, 2 far), unit-sized.
render::MeshData tree_mesh(TreeForm form, int variant, int lod);
// Every form's variants, form-major, each with its three LODs at kTreeLodPixels: the forest
// family's mesh table.
std::vector<render::MeshSource> tree_meshes();
inline uint32_t tree_mesh_index(TreeForm form, int variant) {
  return static_cast<uint32_t>(form) * kTreeVariants + static_cast<uint32_t>(variant);
}
// Every kind's parts, linear rgb, three floats a part, kind-major: the family's material table.
std::vector<float> tree_materials();
inline uint32_t tree_material(mapgen::TreeKind kind) {
  return static_cast<uint32_t>(kind) * kTreeParts;
}
// The instance a tree stands as: at its foot, turned by `turn` (radians about +y), its mesh
// scaled to its crown and height (a snag, which has no crown, to its girth).
render::Instance tree_instance(const mapgen::TreeDef& tree, float turn);

}  // namespace ge::mountain::app
