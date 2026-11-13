// Where a line through a map stops (games/shooters_edge/client/map_line.hpp): in the first
// family's fixture hall and in a box room of the sequel's family, a line up meets the ceiling and a
// short line inside meets nothing.
//
// ge:summary Tests the line through a map's world: the ceiling met in both classic families
// ge:keywords test shooters-edge map-line line_end look-trace loc-there fixture brush-world
// ge:skill shooter testing
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/map_line.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using shooters_edge::client::line_end;

// The ceiling over `in`, met by a line straight up, then a short line that meets nothing.
void check_room(const std::vector<uint8_t>& bytes, sh::Vec3f in, float ceiling_lo,
                float ceiling_hi) {
  sv::BspMap map;
  sv::MapSpec spec;
  std::string error;
  CHECK_MSG(sv::read_bsp_map(bytes, "room", &map, &spec, error), error);
  const sh::Vec3f up = line_end(map, in, {in.x, in.y, in.z + 8192});
  CHECK_MSG(up.z >= ceiling_lo && up.z <= ceiling_hi, "the ceiling at " + std::to_string(up.z));
  const sh::Vec3f near = line_end(map, in, {in.x, in.y, in.z + 16});
  CHECK(near.z == in.z + 16);
}

}  // namespace

TEST(a_line_up_from_the_first_family_s_hall_meets_its_ceiling) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error), error);
  check_room(bytes, {256, 192, 64}, 65, 1024);
}

TEST(a_line_up_from_a_room_of_the_sequel_s_family_meets_its_ceiling) {
  // A room 512 wide inside, floor at 0, ceiling at 256: six box brushes round it.
  br::BrushWorld w;
  w.models.emplace_back();
  const float boxes[6][6] = {{-16, -16, -16, 528, 528, 0}, {-16, -16, 256, 528, 528, 272},
                             {-16, -16, 0, 0, 528, 256},   {512, -16, 0, 528, 528, 256},
                             {0, -16, 0, 512, 0, 256},     {0, 512, 0, 512, 528, 256}};
  for (const auto& b : boxes) w.models[0].push_back(br::box_brush(b, b + 3));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  check_room(bytes, {256, 256, 64}, 255, 256);
}
