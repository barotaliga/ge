// plugins/mountain/app/village_look.hpp: every vehicle kind has a mesh the size of its measures
// with a smaller far LOD; the streets' mesh covers each carriageway and sidewalk cell once, faces
// up, a hand's breadth over the snow; the materials are a set a paint; a vehicle is turned to its
// heading and hidden off the map; a frost whitens the cars and rimes their glass, a glaze darkens
// the carriageway to a gloss, and no cold leaves the matte colours bit for bit.
//
// ge:summary Tests for the village as drawn: vehicle meshes and LODs, the streets' mesh, materials
// ge:keywords test village village_look street_mesh vehicle_mesh vehicle_instance materials lod
// ge:keywords village_surfaces frost glaze cold_steps
// ge:skill mountain testing
#include <cmath>
#include <cstdint>

#include "check.hpp"
#include "mountain/app/village_look.hpp"
#include "render/surface.hpp"
#include "skier/synthetic.hpp"

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

TEST(every_kind_is_its_measures_with_a_smaller_far_lod) {
  for (int k = 0; k < mapp::kVehicleKinds; ++k) {
    const auto kind = static_cast<vg::VehicleKind>(k);
    const ge::render::MeshData near = mapp::vehicle_mesh(kind, 0);
    const ge::render::MeshData far = mapp::vehicle_mesh(kind, 1);
    CHECK(near.vertex_count() > 0 && far.vertex_count() > 0);
    CHECK(far.vertex_count() < near.vertex_count());
    CHECK(near.positions.size() == size_t{near.vertex_count()} * 3);
    for (uint8_t p : near.parts) CHECK(p < mapp::kCarriage);
    // On the snow at its middle, +z forward: as long as its kind and no wider than it with its
    // mirrors out.
    const vg::VehicleSize size = vg::vehicle_size(kind);
    const Box b = box_of(near);
    CHECK(std::fabs(b.lo[1]) < 0.05f);
    CHECK(std::fabs((b.hi[2] - b.lo[2]) - static_cast<float>(size.length)) < 0.3f);
    CHECK(b.hi[0] - b.lo[0] < static_cast<float>(size.width) + 0.6f);
    CHECK(std::fabs(b.hi[1] - static_cast<float>(size.height)) < 0.3f);
  }
  const auto meshes = mapp::vehicle_meshes();
  CHECK(meshes.size() == static_cast<size_t>(mapp::kVehicleKinds));
}

TEST(the_streets_cover_each_cell_once_face_up_over_the_snow) {
  const ge::mountain::mapgen::Level level = ge::skier::flat_strip(0.2, 1);
  vg::GroundMask mask;
  mask.x0 = 2000;
  mask.z0 = 1000;
  mask.cols = 23;
  mask.rows = 5;
  mask.data.assign(static_cast<size_t>(mask.cols) * static_cast<size_t>(mask.rows),
                   static_cast<uint8_t>(vg::Ground::garden));
  int64_t cells = 0;
  for (int32_t j = 0; j < mask.rows; ++j) {
    for (int32_t i = 0; i < mask.cols; ++i) {
      // A carriageway down the middle rows, sidewalks either side, a gap in the middle row.
      vg::Ground g = (j == 0 || j == 4) ? vg::Ground::street : vg::Ground::road;
      if (j == 2 && i == 11) g = vg::Ground::felled;
      if (g == vg::Ground::felled) continue;
      mask.data[static_cast<size_t>(j) * static_cast<size_t>(mask.cols) + static_cast<size_t>(i)] =
          static_cast<uint8_t>(g);
      ++cells;
    }
  }
  const ge::render::MeshData m = mapp::street_mesh(level, mask);
  CHECK(m.vertex_count() % 3 == 0);
  double area = 0;
  for (uint32_t t = 0; t < m.vertex_count(); t += 3) {
    const float* a = &m.positions[size_t{t} * 3];
    const float* b = a + 3;
    const float* c = a + 6;
    // Face up: the normal's y and the plan's winding.
    CHECK(m.normals[size_t{t} * 3 + 1] > 0.9f);
    const double cross = (static_cast<double>(b[2]) - a[2]) * (static_cast<double>(c[0]) - a[0]) -
                         (static_cast<double>(b[0]) - a[0]) * (static_cast<double>(c[2]) - a[2]);
    area += std::fabs(cross) / 2;
    for (const float* v : {a, b, c}) {
      const double over =
          v[1] - level.ground_at(mask.x0 + v[0], mask.z0 + v[2]) - mapp::kStreetLift;
      CHECK(std::fabs(over) < 1e-3);
      CHECK(v[0] >= 0 && v[0] <= static_cast<float>(mask.cols) && v[2] >= 0 &&
            v[2] <= static_cast<float>(mask.rows));
    }
    const uint8_t part = m.parts[t];
    CHECK(part == mapp::kCarriage || part == mapp::kWalk);
  }
  CHECK(std::fabs(area - static_cast<double>(cells)) < 1e-6);
  // No strip runs longer than kStreetRun between its heights.
  CHECK(m.vertex_count() / 6 >=
        static_cast<uint32_t>(static_cast<double>(cells) / mapp::kStreetRun));
  // Nothing of the village's: nothing drawn.
  vg::GroundMask none = mask;
  none.data.assign(none.data.size(), static_cast<uint8_t>(vg::Ground::none));
  CHECK(mapp::street_mesh(level, none).vertex_count() == 0);
}

TEST(the_materials_are_a_set_a_paint_and_the_bus_wears_its_livery) {
  const std::vector<float> rgb = mapp::village_materials();
  CHECK(rgb.size() == size_t{mapp::kPaints + 1} * mapp::kVillageParts * 3);
  for (float c : rgb) CHECK(c >= 0 && c <= 1);
  const uint32_t livery = static_cast<uint32_t>(mapp::kPaints * mapp::kVillageParts);
  CHECK(mapp::vehicle_material(3, vg::VehicleKind::bus) == livery);
  CHECK(mapp::vehicle_material(7, vg::VehicleKind::bus) == livery);
  bool mixed = false;
  for (int32_t id = 0; id < 40; ++id) {
    const uint32_t m = mapp::vehicle_material(id, vg::VehicleKind::hatch);
    CHECK(m % mapp::kVillageParts == 0 && m >= mapp::kVillageParts && m < livery);
    CHECK(m == mapp::vehicle_material(id, vg::VehicleKind::hatch));
    mixed = mixed || m != mapp::vehicle_material(0, vg::VehicleKind::hatch);
  }
  CHECK(mixed);
}

TEST(a_vehicle_is_turned_to_its_heading_and_hidden_off_the_map) {
  vg::VehiclePose pose;
  pose.shown = true;
  pose.kind = vg::VehicleKind::van;
  pose.x = 10;
  pose.y = 20;
  pose.z = 30;
  pose.heading = 3.14159265358979323846 / 2;
  const ge::render::Instance in = mapp::vehicle_instance(pose, 4);
  CHECK(in.mesh == static_cast<uint32_t>(vg::VehicleKind::van));
  CHECK(in.position[0] == 10 && in.position[1] == 20 && in.position[2] == 30);
  // A quarter turn about y: forward (+z) turned to +x, heading's way.
  CHECK(std::fabs(in.rotation[1] - std::sqrt(0.5f)) < 1e-6f);
  CHECK(std::fabs(in.rotation[3] - std::sqrt(0.5f)) < 1e-6f);
  CHECK(in.scale[0] == 1 && in.scale[1] == 1 && in.scale[2] == 1);
  pose.shown = false;
  const ge::render::Instance gone = mapp::vehicle_instance(pose, 4);
  CHECK(gone.scale[0] == 0 && gone.scale[1] == 0 && gone.scale[2] == 0);
  const vg::Parked car{2, vg::VehicleKind::suv, 5, 1, 2, 3, 0};
  const ge::render::Instance parked = mapp::parked_instance(car);
  CHECK(parked.mesh == static_cast<uint32_t>(vg::VehicleKind::suv));
  CHECK(parked.scale[0] == 1 && parked.position[2] == 3);
}

TEST(the_cold_whitens_the_cars_and_glazes_the_road) {
  const std::vector<float> rgb = mapp::village_materials();
  const std::vector<ge::render::Surface> bare = mapp::village_surfaces({});
  CHECK(bare.size() * 3 == rgb.size());
  // No cold: the matte colours, bit for bit.
  for (size_t i = 0; i < bare.size(); ++i) {
    const ge::render::Surface m = ge::render::matte_surface(&rgb[i * 3]);
    for (int c = 0; c < 3; ++c) CHECK_BITS(bare[i].albedo[c], m.albedo[c]);
    CHECK(bare[i].rough == m.rough && bare[i].spec == m.spec);
  }
  // A full frost: the glass rimed white, the paint paler, the tyres hardly.
  const std::vector<ge::render::Surface> frost = mapp::village_surfaces({1, 0});
  const size_t car = mapp::kVillageParts;  // the first paint's set
  CHECK(frost[car + mapp::kGlass].albedo[1] > 0.7f && bare[car + mapp::kGlass].albedo[1] < 0.05f);
  CHECK(frost[car + mapp::kPaint].albedo[1] > bare[car + mapp::kPaint].albedo[1]);
  CHECK(frost[car + mapp::kTyre].albedo[1] < 0.3f);
  // A full glaze: the carriageway darker and glossy, the sidewalk untouched.
  const std::vector<ge::render::Surface> glaze = mapp::village_surfaces({0, 1});
  CHECK(glaze[mapp::kCarriage].albedo[0] < 0.5f * bare[mapp::kCarriage].albedo[0]);
  CHECK(glaze[mapp::kCarriage].rough < 0.1f && glaze[mapp::kCarriage].spec > 0);
  CHECK(glaze[mapp::kWalk].rough == bare[mapp::kWalk].rough);
  // The steps: rounded to kColdStep, kept in 0..1.
  const mapp::VillageCold c = mapp::cold_steps({0.5 + 0.2 * mapp::kColdStep, 1.7});
  CHECK(c.frost == 0.5 && c.glaze == 1);
}
