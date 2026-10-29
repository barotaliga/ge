// plugins/mountain/app/building_look.hpp: every building kind has a mesh standing on its floor
// at its measures (the roof out over its eaves, the church's spire at its height) with a smaller
// far LOD; every roof wears the snow; the plinth is a unit box under the origin; the looks are a
// set each and every building wears one; a building is turned to its heading over a plinth down
// to its lowest snow.
//
// ge:summary Tests for the buildings as drawn: meshes and LODs, the plinth, the looks, instances
// ge:keywords test village building_look building_mesh building_instances plinth materials lod
// ge:skill mountain testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "mountain/app/building_look.hpp"

namespace {

namespace mapp = ge::mountain::app;
namespace vg = ge::mountain::village;

struct Box {
  float lo[3] = {1e9f, 1e9f, 1e9f};
  float hi[3] = {-1e9f, -1e9f, -1e9f};
};
Box box_of(const ge::render::MeshData& m) {
  Box b;
  for (size_t v = 0; v + 2 < m.positions.size(); v += 3) {
    for (int a = 0; a < 3; ++a) {
      b.lo[a] = std::fmin(b.lo[a], m.positions[v + static_cast<size_t>(a)]);
      b.hi[a] = std::fmax(b.hi[a], m.positions[v + static_cast<size_t>(a)]);
    }
  }
  return b;
}

}  // namespace

TEST(every_kind_stands_at_its_measures_with_a_smaller_far_lod) {
  for (int k = 0; k < vg::kBuildingKinds; ++k) {
    const auto kind = static_cast<vg::BuildingKind>(k);
    const ge::render::MeshData near = mapp::building_mesh(kind, 0);
    const ge::render::MeshData far = mapp::building_mesh(kind, 1);
    CHECK_MSG(near.vertex_count() > 0 && far.vertex_count() > 0, vg::building_kind_name(kind));
    CHECK(far.vertex_count() < near.vertex_count());
    CHECK(near.positions.size() == size_t{near.vertex_count()} * 3);
    bool snow = false;
    for (const uint8_t p : near.parts) {
      CHECK(p < mapp::kBuildingParts);
      snow = snow || p == mapp::kSnow;
    }
    CHECK_MSG(snow, std::string(vg::building_kind_name(kind)) + " wears no snow on its roof");
    for (size_t v = 0; v + 2 < near.normals.size(); v += 3) {
      const float n = std::hypot(near.normals[v], near.normals[v + 1], near.normals[v + 2]);
      CHECK(std::fabs(n - 1) < 1e-4f);
    }
    // On its floor at its middle: its walls its footprint, out no further than the eaves, a
    // terrace, a balcony or an awning reach; as tall as its ridge (the church its spire).
    const vg::BuildingSize s = vg::building_size(kind);
    const Box b = box_of(near);
    const float reach = 3.2f;
    CHECK(std::fabs(b.lo[1]) < 0.05f);
    CHECK(b.hi[0] - b.lo[0] >= static_cast<float>(s.width) - 0.01f);
    CHECK(b.hi[2] - b.lo[2] >= static_cast<float>(s.depth) - 0.01f);
    CHECK(b.hi[0] - b.lo[0] <= static_cast<float>(s.width) + 2 * reach);
    CHECK(b.hi[2] - b.lo[2] <= static_cast<float>(s.depth) + 2 * reach);
    CHECK(b.lo[2] >= -static_cast<float>(s.depth) / 2 - reach);
    const double top = kind == vg::BuildingKind::church ? vg::buildings::kSpire : s.ridge + 0.6;
    CHECK_MSG(
        std::fabs(b.hi[1] - static_cast<float>(top)) < 0.7f,
        std::string(vg::building_kind_name(kind)) + " stands " + std::to_string(b.hi[1]) + " m");
  }
  const auto meshes = mapp::building_meshes();
  CHECK(meshes.size() == static_cast<size_t>(vg::kBuildingKinds) + 1);
  const Box plinth = box_of(meshes[mapp::kPlinthMesh].lods.front().mesh);
  CHECK(plinth.hi[1] == 0.0f && plinth.lo[1] == -1.0f);
  CHECK(plinth.lo[0] == -0.5f && plinth.hi[0] == 0.5f);
}

TEST(every_building_wears_a_look_and_stands_on_its_plinth) {
  const std::vector<float> looks = mapp::building_materials();
  CHECK(looks.size() == static_cast<size_t>(mapp::kBuildingLooks) * mapp::kBuildingParts * 3);
  for (const float c : looks) CHECK(c >= 0 && c <= 1);
  for (int k = 0; k < vg::kBuildingKinds; ++k) {
    for (int32_t id = 0; id < 40; ++id) {
      const auto kind = static_cast<vg::BuildingKind>(k);
      const vg::BuildingSize s = vg::building_size(kind);
      const vg::Building b{id,      kind,    100,     50,      200,  1.25,
                           s.width, s.depth, s.eaves, s.ridge, 47.5, 0};
      const uint32_t m = mapp::building_material(b);
      CHECK(m % mapp::kBuildingParts == 0);
      CHECK(m < static_cast<uint32_t>(mapp::kBuildingLooks * mapp::kBuildingParts));
      std::vector<ge::render::Instance> out;
      mapp::building_instances(b, out);
      CHECK(out.size() == 2);
      CHECK(out[0].mesh == static_cast<uint32_t>(k) && out[1].mesh == mapp::kPlinthMesh);
      CHECK(out[0].material == m && out[1].material == m);
      // Turned to its heading about y: +z goes to (sin, cos) of it.
      const float rot_y = out[0].rotation[1];
      const float rot_w = out[0].rotation[3];
      CHECK(std::fabs(2 * rot_y * rot_w - static_cast<float>(std::sin(1.25))) < 1e-5f);
      CHECK(std::fabs(rot_w * rot_w - rot_y * rot_y - static_cast<float>(std::cos(1.25))) < 1e-5f);
      CHECK(out[0].position[1] == 50 && out[1].position[1] == 50);
      // Down to its lowest snow and into it.
      CHECK(std::fabs(out[1].scale[1] - static_cast<float>(2.5 + mapp::kPlinthSink)) < 1e-5f);
      CHECK(out[1].scale[0] > static_cast<float>(s.width));
      CHECK(out[1].scale[2] > static_cast<float>(s.depth));
    }
  }
}
