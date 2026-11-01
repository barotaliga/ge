// plugins/mountain/app/ice_look.hpp: the sheet covers the lake to its shore, clipped to it, lies
// flat a hand's breadth over the water and faces up, and carries both the snow and the swept
// patches (or, on ground filled flat, the swept patches alone); thawing, it stands off the shore
// over a moat; young ice is dark and glossy, old ice under snow bright and matte with its swept
// patches still glass, and thawing ice grey slush and dark rotten glass, wet.
//
// ge:summary Test: a frozen lake as drawn: the sheet's cover and its look by thickness
// ge:keywords test lake-ice ice_mesh ice_surfaces swept snow-on-ice black-ice
// ge:skill mountain testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "mountain/app/ice_look.hpp"

namespace {

namespace mapp = ge::mountain::app;

// The sheet's area, m^2.
double area_of(const ge::render::MeshData& m) {
  double a = 0;
  for (size_t v = 0; v + 2 < m.parts.size(); v += 3) {
    const float* p = &m.positions[v * 3];
    const double ux = p[3] - p[0], uz = p[5] - p[2], wx = p[6] - p[0], wz = p[8] - p[2];
    a += 0.5 * std::fabs(ux * wz - uz * wx);
  }
  return a;
}

std::vector<ge::water::Point> round_lake(double cx, double cz, double r) {
  std::vector<ge::water::Point> out;
  for (int i = 0; i < 64; ++i) {
    const double a = 6.283185307179586 * i / 64;
    out.push_back({cx + r * std::cos(a), cz + r * std::sin(a)});
  }
  return out;
}

}  // namespace

TEST(the_sheet_covers_the_lake_and_faces_up) {
  const auto shore = round_lake(500, 700, 90);
  const ge::render::MeshData m = mapp::ice_mesh(shore, 1234, 38);
  CHECK(m.vertex_count() > 0);
  CHECK(m.vertex_count() % 3 == 0);
  // Clipped to the shore: its area the lake's (the 64-sided shore's), nothing past the shore.
  const double lake = 0.5 * 64 * 90 * 90 * std::sin(6.283185307179586 / 64);
  CHECK(area_of(m) > 0.995 * lake && area_of(m) < 1.005 * lake);
  int snow = 0, bare = 0;
  for (size_t v = 0; v < m.parts.size(); ++v) {
    const double x = m.positions[v * 3] - 500, z = m.positions[v * 3 + 2] - 700;
    CHECK(std::sqrt(x * x + z * z) < 90 + 0.01);
    CHECK(std::fabs(m.positions[v * 3 + 1] - (1234 + mapp::kIceLift)) < 1e-3);
    CHECK(m.normals[v * 3 + 1] > 0.999f);
    (m.parts[v] == mapp::kIceSnow ? snow : bare)++;
  }
  CHECK(snow > bare && bare > 0);
  // The same seed, the same patches; no shore, no sheet.
  CHECK(mapp::ice_mesh(shore, 1234, 38).parts == m.parts);
  CHECK(mapp::ice_mesh({}, 0, 1).vertex_count() == 0);
  // On ground filled flat, the swept patches alone, more of them.
  const ge::render::MeshData swept = mapp::ice_mesh(shore, 1234, 38, true);
  const double swept_area = area_of(swept);
  CHECK(swept_area > bare / 6.0 * mapp::kIceCell * mapp::kIceCell);
  CHECK(swept_area < 0.8 * lake);
  for (const uint8_t part : swept.parts) CHECK(part == mapp::kIceBare);
}

TEST(thawing_the_sheet_stands_off_the_shore_over_a_moat) {
  const auto shore = round_lake(500, 700, 90);
  const ge::render::MeshData m = mapp::ice_mesh(shore, 1234, 38, false, 5);
  CHECK(m.vertex_count() > 0);
  for (size_t v = 0; v < m.parts.size(); ++v) {
    const double x = m.positions[v * 3] - 500, z = m.positions[v * 3 + 2] - 700;
    CHECK(std::sqrt(x * x + z * z) < 90 - 5 + 0.05);
  }
  // The moat's water: the sheet short of the lake by the ring 5 m wide, within the cells' cut.
  const double ring = 3.14159265 * (90 * 90 - 85 * 85);
  const double lake = 0.5 * 64 * 90 * 90 * std::sin(6.283185307179586 / 64);
  const double a = area_of(m);
  CHECK(a < lake - 0.9 * ring && a > lake - 1.1 * ring);
}

TEST(young_ice_is_dark_and_old_ice_lies_under_snow) {
  const auto young = mapp::ice_surfaces(0.02), old = mapp::ice_surfaces(0.3);
  CHECK(young.size() == mapp::kIceParts && old.size() == mapp::kIceParts);
  // Young: both parts dark glass.
  for (const ge::render::Surface& s : young) {
    CHECK(s.albedo[1] < 0.05f && s.rough < 0.1f && s.spec > 0);
  }
  // Old: the snow bright and matte, the swept patches still glass, darker than the snow.
  CHECK(old[mapp::kIceSnow].albedo[1] > 0.8f && old[mapp::kIceSnow].rough == 1.0f);
  CHECK(old[mapp::kIceSnow].spec == 0);
  CHECK(old[mapp::kIceBare].albedo[1] < 0.3f && old[mapp::kIceBare].spec > 0);
  CHECK(old[mapp::kIceBare].rough < 0.3f);
  // Thawing: the snow on it grey wet slush with a sheen, the glass dark rotten ice, glossy.
  const auto rotten = mapp::ice_surfaces(0.3, 1);
  CHECK(rotten[mapp::kIceSnow].albedo[1] < 0.75f && rotten[mapp::kIceSnow].albedo[1] > 0.3f);
  CHECK(rotten[mapp::kIceSnow].rough < 0.6f && rotten[mapp::kIceSnow].spec > 0);
  CHECK(rotten[mapp::kIceBare].albedo[1] < old[mapp::kIceBare].albedo[1]);
  CHECK(rotten[mapp::kIceBare].rough < 0.1f);
  // No thaw, no change.
  CHECK(mapp::ice_surfaces(0.3, 0)[mapp::kIceSnow].albedo[1] == old[mapp::kIceSnow].albedo[1]);
}
