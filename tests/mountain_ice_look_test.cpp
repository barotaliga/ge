// plugins/mountain/app/ice_look.hpp: the sheet covers the lake inside its shore and no further
// than a cell past it, lies flat a hand's breadth over the water and faces up, and carries both
// the snow and the swept patches; young ice is dark and glossy, old ice under snow bright and
// matte with its swept patches still glass.
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
  // Its area: the lake's, give or take the ragged cells at the shore.
  const double area = m.vertex_count() / 6.0 * mapp::kIceCell * mapp::kIceCell;
  const double lake = 3.14159265 * 90 * 90;
  CHECK(area > lake && area < 1.15 * lake);
  int snow = 0, bare = 0;
  for (size_t v = 0; v < m.parts.size(); ++v) {
    const double x = m.positions[v * 3] - 500, z = m.positions[v * 3 + 2] - 700;
    CHECK(std::sqrt(x * x + z * z) < 90 + 2 * mapp::kIceCell);
    CHECK(std::fabs(m.positions[v * 3 + 1] - (1234 + mapp::kIceLift)) < 1e-3);
    CHECK(m.normals[v * 3 + 1] > 0.999f);
    (m.parts[v] == mapp::kIceSnow ? snow : bare)++;
  }
  CHECK(snow > bare && bare > 0);
  // The same seed, the same patches; no shore, no sheet.
  CHECK(mapp::ice_mesh(shore, 1234, 38).parts == m.parts);
  CHECK(mapp::ice_mesh({}, 0, 1).vertex_count() == 0);
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
}
