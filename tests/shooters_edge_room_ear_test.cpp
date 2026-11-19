// The room round the ear on a classic map (games/shooters_edge/client/room_ear.hpp): a ray finds
// the hall's ceiling and leaves through the yard's sky, and the hall rings longer than the yard
// open to the sky, whose outdoor echo it keeps.
//
// ge:summary Tests of room acoustics on a classic map: bsp_ray and bsp_room
// ge:keywords ce_acoustics room-sound bsp_ray bsp_room test
// ge:skill audio shooter
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/room_ear.hpp"

namespace br = ge::brush;
namespace cl = shooters_edge::client;

namespace {

br::Map weather_map() {
  std::vector<uint8_t> bytes;
  std::string error;
  br::Map m;
  CHECK_MSG(br::write_bsp(br::weather_fixture(), br::Family::v38, &bytes, &error), error);
  CHECK_MSG(br::read_bsp(bytes, &m, &error), error);
  return m;
}

// Map units (x, y, z up) as engine metres (x, y up, z).
void metres(const double u[3], double out[3]) {
  out[0] = u[0] * br::kUnitM;
  out[1] = u[2] * br::kUnitM;
  out[2] = u[1] * br::kUnitM;
}

}  // namespace

TEST(a_ray_meets_the_halls_ceiling_and_leaves_through_the_yards_sky) {
  const br::Map m = weather_map();
  const double hall_u[3] = {320, -200, 32}, yard_u[3] = {320, 800, 32};
  double hall[3], yard[3];
  metres(hall_u, hall);
  metres(yard_u, yard);
  const double up[3] = {0, 1, 0};
  const ge::audio::RayHit ceiling = cl::bsp_ray(m, hall, up, 60);
  CHECK(ceiling.hit);
  CHECK(ceiling.distance_m > 0.5 && ceiling.distance_m < 20);
  CHECK(!cl::bsp_ray(m, yard, up, 60).hit);
}

TEST(the_hall_rings_longer_than_the_open_yard) {
  const br::Map m = weather_map();
  const double hall_u[3] = {320, -200, 32}, yard_u[3] = {320, 800, 32};
  double hall[3], yard[3];
  metres(hall_u, hall);
  metres(yard_u, yard);
  const double right[3] = {1, 0, 0};
  const ge::audio::RoomSound in = cl::bsp_room(m, hall, right);
  const ge::audio::RoomSound out = cl::bsp_room(m, yard, right);
  CHECK(in.decay_s > 1.2 * out.decay_s);
  CHECK(in.taps > 0);
  CHECK(in.echo < out.echo);
}
