// THE OWN LIGHTNING BEAM FOLLOWS THE AIM (cl_fakeshaft): traced again from the eye along the
// view's direction (1), the server's (0) or a share of the way between; it stops on the map's
// walls and on a living body in the way, never on the player's own; nothing without a map.
//
// ge:summary Tests: the own lightning beam traced along the aim, the share, walls and bodies
// ge:keywords test own_beam fakeshaft cl_fakeshaft lightning beam aim
// ge:skill render shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/own_beam.hpp"

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

// The fixture halls as a loaded map.
std::unique_ptr<cl::LoadedMap> halls() {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error), error);
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bytes, "halls", error);
  CHECK_MSG(map != nullptr, error);
  return map;
}

sh::Vec3f unit(sh::Vec3f a, sh::Vec3f b) {
  const float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
  const float n = std::sqrt(x * x + y * y + z * z);
  return {x / n, y / n, z / n};
}

// The halls' spot (128,192), the eye 22 units over the feet.
constexpr sh::Vec3f kEye = {128, 192, 46};

}  // namespace

TEST(the_beam_follows_the_view_the_server_or_a_share_between) {
  const auto map = halls();
  if (!map) return;
  // The server's line due east; the view turned 30 degrees north of it.
  const sh::Vec3f server_from = {128, 192, 40}, server_to = {528, 192, 40};
  const float c = std::cos(0.5236f), s = std::sin(0.5236f);
  const sh::Vec3f view = {c, s, 0};
  sh::Vec3f from, to, n;
  CHECK(cl::own_beam(map.get(), kEye, view, server_from, server_to, 1, {}, 0, from, to, n));
  CHECK(from.z == kEye.z - cl::kOwnBeamDropU);  // rooted under the eye as the server roots it
  sh::Vec3f d = unit(from, to);
  CHECK(std::abs(d.x - c) < 1e-3f && std::abs(d.y - s) < 1e-3f);
  CHECK(std::hypot(to.x - from.x, to.y - from.y) <= cl::kOwnBeamReachU + 0.01f);
  // 0: the server's line, from the eye.
  CHECK(cl::own_beam(map.get(), kEye, view, server_from, server_to, 0, {}, 0, from, to, n));
  d = unit(from, to);
  CHECK(std::abs(d.x - 1) < 1e-3f && std::abs(d.y) < 1e-3f);
  // A half: between the two, toward neither end.
  CHECK(cl::own_beam(map.get(), kEye, view, server_from, server_to, 0.5, {}, 0, from, to, n));
  d = unit(from, to);
  CHECK(d.y > 0.1f && d.y < s - 0.1f);
}

TEST(a_wall_or_a_living_body_stops_it_and_its_normal_faces_back) {
  const auto map = halls();
  if (!map) return;
  const sh::Vec3f view = {1, 0, 0};
  sh::Vec3f from, to, n;
  CHECK(cl::own_beam(map.get(), kEye, view, kEye, {kEye.x + 1, kEye.y, kEye.z}, 1, {}, 0, from, to,
                     n));
  const float wall = to.x;
  CHECK(wall > from.x);
  CHECK(n.x < -0.9f);  // the far wall's face, or with none in reach the line's own way back
  // A living body 64 units ahead stops it at its box's near side; the player's own (entity 1)
  // and a dead one never do.
  std::vector<cl::Body> bodies(3);
  bodies[0] = {1, {kEye.x + 8, kEye.y, kEye.z - 22}, true};
  bodies[1] = {2, {kEye.x + 40, kEye.y, kEye.z - 22}, false};
  bodies[2] = {3, {kEye.x + 64, kEye.y, kEye.z - 22}, true};
  CHECK(wall - from.x > 100);  // the hall is long enough for the bodies
  CHECK(cl::own_beam(map.get(), kEye, view, kEye, {kEye.x + 1, kEye.y, kEye.z}, 1, bodies, 1, from,
                     to, n));
  CHECK(std::abs(to.x - (kEye.x + 64 - cl::kBodyHalfU)) < 0.01f);
  CHECK(n.x == -1);
}

TEST(nothing_without_a_map) {
  sh::Vec3f from, to, n;
  CHECK(!cl::own_beam(nullptr, kEye, {1, 0, 0}, kEye, {kEye.x + 1, kEye.y, kEye.z}, 1, {}, 0, from,
                      to, n));
}

TEST(a_sweep_from_the_floor_onto_a_wall_finds_the_edge_between) {
  const auto map = halls();
  if (!map) return;
  // A wall in reach: the first of the four ways along the floor that stops inside it.
  const sh::Vec3f ways[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
  const sh::Vec3f from = {kEye.x, kEye.y, kEye.z - cl::kOwnBeamDropU};
  sh::Vec3f way{}, to, n;
  bool found = false;
  for (const sh::Vec3f& w : ways) {
    CHECK(cl::beam_trace(map.get(), from, w, {}, 0, to, n));
    if (std::hypot(to.x - from.x, to.y - from.y) < cl::kOwnBeamReachU - 1) {
      way = w;
      found = true;
      break;
    }
  }
  CHECK(found);
  if (!found) return;
  const sh::Vec3f down = {way.x, way.y, -0.4f};  // onto the floor before the wall
  sh::Vec3f at[2], en[2];
  CHECK(cl::beam_edge(map.get(), from, down, way, {}, 0, at, en) == 2);
  CHECK(en[0].z > 0.9f);            // the last point on the floor
  CHECK(std::abs(en[1].z) < 0.1f);  // the first on the wall
  const float gap = std::hypot(at[1].x - at[0].x, at[1].y - at[0].y, at[1].z - at[0].z);
  CHECK(gap < 2 * cl::kEdgeU);  // either side of the corner
  // On one face: no edge.
  CHECK(cl::beam_edge(map.get(), from, {way.x, way.y, -0.5f}, down, {}, 0, at, en) == 0);
}
