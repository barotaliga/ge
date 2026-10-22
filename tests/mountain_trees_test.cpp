// plugins/mountain/app/trees.hpp: the trees as drawn. Every form's every variant builds at every
// LOD inside its unit box with whole triangles and unit normals, each LOD lighter than the one
// before, the loaded forms carry snow on their upper faces, every kind has a mesh and its parts'
// colours, and an instance stands its tree at its foot, scaled to its crown and height.
//
// ge:summary Tests for the trees as drawn: meshes in bounds, LODs lighter, snow, kinds, instances
// ge:keywords test trees tree-mesh lod variant snow-load tree_instance tree_materials tree_form
// ge:skill mountain testing
#include <cmath>
#include <cstddef>
#include <set>
#include <vector>

#include "check.hpp"
#include "mountain/app/trees.hpp"
#include "mountain/mapgen/types.hpp"

namespace {

namespace app = ge::mountain::app;
using ge::mountain::mapgen::TreeDef;
using ge::mountain::mapgen::TreeKind;

app::TreeForm form_at(int f) { return static_cast<app::TreeForm>(f); }

TEST(every_mesh_builds_inside_its_unit_box) {
  for (int f = 0; f < app::kTreeForms; ++f) {
    for (int v = 0; v < app::kTreeVariants; ++v) {
      for (int l = 0; l < app::kTreeLods; ++l) {
        const auto m = app::tree_mesh(form_at(f), v, l);
        CHECK(!m.indices.empty());
        CHECK(m.indices.size() % 3 == 0);
        CHECK(m.positions.size() == size_t{3} * m.vertex_count());
        CHECK(m.normals.size() == m.positions.size());
        for (uint32_t i : m.indices) CHECK(i < m.vertex_count());
        for (size_t k = 0; k < m.vertex_count(); ++k) {
          const float x = m.positions[k * 3], y = m.positions[k * 3 + 1],
                      z = m.positions[k * 3 + 2];
          // The crown 1 across in radius, a ring's jitter and a limb's tip a little past it; the
          // trunk's foot a little under the ground so no slope shows it standing on air.
          CHECK(std::sqrt(x * x + z * z) <= 1.15f);
          CHECK(y >= -0.06f && y <= 1.06f);
          const float nx = m.normals[k * 3], ny = m.normals[k * 3 + 1], nz = m.normals[k * 3 + 2];
          CHECK(std::fabs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1) < 1e-3f);
          CHECK(m.parts[k] < app::kTreeParts);
        }
      }
    }
  }
}

TEST(each_lod_is_lighter_than_the_one_before) {
  for (int f = 0; f < app::kTreeForms; ++f) {
    for (int v = 0; v < app::kTreeVariants; ++v) {
      size_t before = SIZE_MAX;
      for (int l = 0; l < app::kTreeLods; ++l) {
        const size_t tris = app::tree_mesh(form_at(f), v, l).indices.size() / 3;
        CHECK(tris < before);
        before = tris;
      }
    }
  }
}

TEST(the_loaded_forms_carry_snow_on_their_upper_faces) {
  for (auto form : {app::TreeForm::conifer, app::TreeForm::pine, app::TreeForm::larch}) {
    for (int v = 0; v < app::kTreeVariants; ++v) {
      const auto m = app::tree_mesh(form, v, 0);
      size_t snow = 0;
      for (size_t t = 0; t < m.indices.size(); t += 3) {
        const uint32_t k = m.indices[t];
        if (m.parts[k] != app::kSnow) continue;
        ++snow;
        CHECK(m.normals[size_t{k} * 3 + 1] > 0);  // snow lies on a face turned up
      }
      CHECK(snow > 0);
    }
  }
  // A conifer's tiers are dark under their lips.
  const auto c = app::tree_mesh(app::TreeForm::conifer, 0, 0);
  bool shade = false;
  for (uint8_t p : c.parts) shade = shade || p == app::kNeedleShade;
  CHECK(shade);
}

TEST(the_variants_of_a_form_differ) {
  for (int f = 0; f < app::kTreeForms - 1; ++f) {  // a snag's variants are one dead trunk
    std::set<std::vector<float>> seen;
    for (int v = 0; v < app::kTreeVariants; ++v)
      seen.insert(app::tree_mesh(form_at(f), v, 0).positions);
    CHECK(seen.size() == size_t{app::kTreeVariants});
  }
}

TEST(the_mesh_table_holds_every_form_and_variant_at_every_lod) {
  const auto table = app::tree_meshes();
  CHECK(table.size() == size_t{app::kTreeForms} * app::kTreeVariants);
  for (const auto& source : table) {
    CHECK(source.lods.size() == size_t{app::kTreeLods});
    for (int l = 0; l < app::kTreeLods; ++l) CHECK(source.lods[l].min_px == app::kTreeLodPixels[l]);
  }
  CHECK(app::tree_mesh_index(app::TreeForm::snag, app::kTreeVariants - 1) == table.size() - 1);
}

TEST(every_kind_has_a_form_and_its_colours) {
  const auto materials = app::tree_materials();
  CHECK(materials.size() == size_t{app::kTreeKinds} * app::kTreeParts * 3);
  for (float c : materials) CHECK(c >= 0 && c <= 1);
  for (int k = 0; k < app::kTreeKinds; ++k) {
    const auto kind = static_cast<TreeKind>(k);
    CHECK(static_cast<int>(app::tree_form(kind)) < app::kTreeForms);
    // Every kind's snow is the same white.
    const size_t snow = (size_t{app::tree_material(kind)} + app::kSnow) * 3;
    CHECK(materials[snow] == materials[size_t{app::kSnow} * 3]);
  }
  CHECK(app::tree_form(TreeKind::spruce) == app::TreeForm::conifer);
  CHECK(app::tree_form(TreeKind::pine) == app::TreeForm::pine);
  CHECK(app::tree_form(TreeKind::larch) == app::TreeForm::larch);
  CHECK(app::tree_form(TreeKind::birch) == app::TreeForm::broadleaf);
  CHECK(app::tree_form(TreeKind::snag) == app::TreeForm::snag);
}

TEST(an_instance_stands_its_tree_at_its_foot) {
  TreeDef t{12.5, -40.25, 803, 14, 0.2, 60, 3.0, TreeKind::fir, -1};
  const auto in = app::tree_instance(t, 1.0f);
  CHECK(in.position[0] == 12.5 && in.position[1] == 803 && in.position[2] == -40.25);
  CHECK(in.scale[0] == 3.0f && in.scale[2] == 3.0f && in.scale[1] == 14.0f);
  CHECK(std::fabs(in.rotation[1] * in.rotation[1] + in.rotation[3] * in.rotation[3] - 1) < 1e-6f);
  CHECK(in.mesh == app::tree_mesh_index(app::TreeForm::conifer, app::tree_variant(t.x, t.z)));
  CHECK(in.material == app::tree_material(TreeKind::fir));
  // The variant is the place's: the same spot always, and every variant somewhere in a stand.
  CHECK(app::tree_variant(t.x, t.z) == app::tree_variant(t.x, t.z));
  std::set<int> variants;
  for (int i = 0; i < 64; ++i) variants.insert(app::tree_variant(i * 3.1, i * 1.7));
  CHECK(variants.size() == size_t{app::kTreeVariants});
}

}  // namespace
