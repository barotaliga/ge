// engine/render/far_land.hpp: the kinds are named once each and their water stays under the
// world; the land never rises into the site's cylinder and the ranges stand well over it far
// off; the snow line follows the valley's temperature by the lapse rate; the mesh is the grid
// it says, its normals face up, its horizons (the water's too) are elevations a ridge throws, and
// the same site
// builds the same bits; a vertex is the shader's 48 bytes.
//
// ge:summary Test: the far land's kinds, keep-out, snow line, mesh, horizons and determinism
// ge:keywords test far-land kinds keep-out snowline lapse horizon build_far_land far_height
// ge:skill render testing
#include <cmath>
#include <cstring>
#include <set>
#include <string>

#include "check.hpp"
#include "render/far_land.hpp"

namespace gr = ge::render;

namespace {

gr::FarSite site() {
  gr::FarSite s;
  s.x = 12;
  s.z = -30;
  s.radius_m = 60;
  s.floor_y = -5;
  s.seed = 7;
  return s;
}

}  // namespace

TEST(kinds_are_named_once_with_their_water_under_the_hill) {
  std::set<std::string> names;
  int alien = 0;
  for (const gr::FarLandKind& k : gr::far_land_kinds()) {
    CHECK(names.insert(k.name).second);
    CHECK(gr::find_far_land(k.name) == &k);
    CHECK(k.water < k.hill_drop);
    CHECK(k.peak > k.hill_drop);
    CHECK(k.clarity_km > 0 && k.span > 0);
    alien += k.alien ? 1 : 0;
  }
  CHECK(names.size() == 11);
  CHECK(alien == 3);
  CHECK(gr::find_far_land("nowhere") == nullptr);
}

TEST(the_land_stays_under_the_world_and_rises_far_off) {
  const gr::FarSite s = site();
  for (const gr::FarLandKind& k : gr::far_land_kinds()) {
    // Inside the circle (and at its edge) the land is under the floor.
    for (int a = 0; a < 16; ++a) {
      for (double r : {0.0, 20.0, 59.0, 60.0}) {
        const double h = 2 * 3.14159265358979 * a / 16;
        CHECK(gr::far_height(k, s, s.x + r * std::sin(h), s.z + r * std::cos(h)) < s.floor_y);
      }
    }
    // Some point of the ranges stands far over the floor.
    double best = -1e30;
    for (int a = 0; a < 64; ++a) {
      const double h = 2 * 3.14159265358979 * a / 64;
      for (int q = 8; q < 24; ++q) {
        const double r = 0.25 * q * k.near_m;
        best = std::max(best, gr::far_height(k, s, s.x + r * std::sin(h), s.z + r * std::cos(h)));
      }
    }
    CHECK(best > s.floor_y + 0.3 * k.peak);
  }
}

TEST(the_snow_line_follows_the_valley_temperature) {
  const gr::FarLandKind& alps = *gr::find_far_land("alps");
  gr::FarMesh m;
  m.valley_y = 100;
  CHECK(std::fabs(gr::far_snow_line(alps, m, 0) - (100 + alps.temp_c / gr::kLapse)) < 1e-9);
  // Ten degrees colder brings it about 1540 m down.
  CHECK(std::fabs(gr::far_snow_line(alps, m, 0) - gr::far_snow_line(alps, m, -10) -
                  10 / gr::kLapse) < 1e-9);
  const gr::FarLandKind& winter = *gr::find_far_land("winter");
  CHECK(gr::far_snow_line(winter, m, 0) < m.valley_y);  // the whole land under snow
}

TEST(the_mesh_is_its_grid_faces_up_and_builds_the_same_bits) {
  static_assert(sizeof(gr::FarVertex) == 48);
  const gr::FarLandKind& k = *gr::find_far_land("alps");
  const gr::FarSite s = site();
  const gr::FarMesh a = gr::build_far_land(k, s, 64, 40);
  CHECK(a.bearings == 64 && a.rings == 40);
  CHECK(a.land_indices == 64u * 39u * 6u);
  CHECK(a.vertices.size() == 64u * 40u + 256u * 64u);  // the land and the water's sheet
  CHECK(a.indices.size() == a.land_indices + 256u * 63u * 6u);
  CHECK(a.water_y < s.floor_y && a.water_y > a.valley_y);
  CHECK(a.high_y > s.floor_y + 1000);
  for (size_t i = 0; i < size_t{64} * 40; ++i) {
    const gr::FarVertex& v = a.vertices[i];
    CHECK(v.n[1] > 0);
    CHECK(v.n[3] >= 0 && v.n[3] <= 1);
    CHECK(v.pos[3] >= 0 && v.pos[3] <= 1);
  }
  const gr::FarMesh b = gr::build_far_land(k, s, 64, 40);
  CHECK(std::memcmp(a.vertices.data(), b.vertices.data(),
                    a.vertices.size() * sizeof(gr::FarVertex)) == 0);
  CHECK(a.indices == b.indices);
}

TEST(a_valley_under_the_ranges_sees_a_raised_horizon) {
  const gr::FarLandKind& k = *gr::find_far_land("alps");
  const gr::FarMesh m = gr::build_far_land(k, site(), 64, 60);
  // On the hill's top the ranges stand up round the eye in every bearing; somewhere they throw
  // a horizon of several degrees, and nothing is over a right angle.
  double most = 0;
  for (int b = 0; b < gr::kFarBearings; ++b) {
    const double a = gr::far_horizon(m.vertices[0], b);
    CHECK(a >= 0 && a <= 1.5708);
    most = std::max(most, a);
  }
  CHECK(most > 0.02);
  CHECK(m.vertices[0].n[3] < 1);
  // The water keeps its horizons too (the glade is hidden behind a ridge): some of it under the
  // ranges sees a raised one.
  double water_most = 0;
  for (size_t i = size_t{64} * 60; i < m.vertices.size(); ++i) {
    CHECK(m.vertices[i].pos[3] < 0);
    for (int b = 0; b < gr::kFarBearings; ++b)
      water_most = std::max(water_most, gr::far_horizon(m.vertices[i], b));
  }
  CHECK(water_most > 0.02);
}
