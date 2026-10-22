// plugins/mountain/app/snow.hpp: the snow's ground map. Rock comes in with the slope (none where
// snow holds, all past the bare slope, rising between); the packed share is the map's own; the
// piste's direction is its doubled angle, full on the centre line and faded to nought (128, 128)
// at its edge and off it, so a piste run either way along one line reads the same.
//
// ge:summary Tests for the snow material's ground map: rock by slope, packed share, direction
// ge:keywords test snow ground-map rock_share bake_snow_ground doubled-angle piste-direction
// ge:skill mountain testing
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "mountain/app/snow.hpp"
#include "mountain/mapgen/types.hpp"

namespace {

namespace app = ge::mountain::app;
using ge::mountain::mapgen::Level;

constexpr double kPi = 3.14159265358979323846;

// A flat 64 m square at 2 m a cell, packed in its left half, with a straight piste 20 m wide up
// its middle at `heading`.
Level flat_map(double heading) {
  Level level;
  level.cell = 2;
  level.size = 64;
  level.ground = ge::core::create_heightfield(0, 0, 2, 33, 33);
  level.packed = ge::core::create_heightfield(0, 0, 2, 33, 33);
  for (int r = 0; r < 33; ++r) {
    for (int c = 0; c < 16; ++c) level.packed.data[static_cast<size_t>(r * 33 + c)] = 1;
  }
  for (int i = -20; i <= 20; ++i) {
    const double d = i * 2.0;
    level.track.points.push_back(
        {32 + d * std::sin(heading), 32 + d * std::cos(heading), 0, d + 40, heading, 20});
  }
  return level;
}

uint8_t texel(const std::vector<uint8_t>& map, int c, int r, int k) {
  return map[static_cast<size_t>(r * 33 + c) * 4 + static_cast<size_t>(k)];
}

TEST(rock_rises_with_the_slope) {
  CHECK(app::rock_share(0) == 0);
  CHECK(app::rock_share(1.0) == 0);
  CHECK(app::rock_share(1.6) == 1);
  CHECK(app::rock_share(3) == 1);
  CHECK(app::rock_share(1.2) > 0 && app::rock_share(1.2) < app::rock_share(1.4));
}

TEST(the_map_carries_the_packed_share_and_the_rock) {
  Level level = flat_map(0);
  // A cliff along the last columns: 4 m up a 2 m cell.
  for (int r = 0; r < 33; ++r) {
    for (int c = 30; c < 33; ++c)
      level.ground.data[static_cast<size_t>(r * 33 + c)] = 4.0f * static_cast<float>(c - 29);
  }
  const std::vector<uint8_t> map = app::bake_snow_ground(level);
  CHECK(map.size() == size_t{33} * 33 * 4);
  CHECK(texel(map, 2, 10, 0) == 255);   // packed
  CHECK(texel(map, 24, 10, 0) == 0);    // loose
  CHECK(texel(map, 10, 10, 1) == 0);    // flat: snow holds
  CHECK(texel(map, 31, 10, 1) == 255);  // the cliff: bare
}

TEST(the_direction_is_the_doubled_angle_faded_at_the_edge) {
  // Down +z (heading 0): the doubled angle's (1, 0), full on the centre line.
  const std::vector<uint8_t> along_z = app::bake_snow_ground(flat_map(0));
  CHECK(texel(along_z, 16, 16, 2) == 255);
  CHECK(texel(along_z, 16, 16, 3) == 128);
  // At the edge (10 m off) and off the piste, nought.
  CHECK(texel(along_z, 21, 16, 2) == 128);
  CHECK(texel(along_z, 28, 16, 2) == 128 && texel(along_z, 28, 16, 3) == 128);
  // Halfway in (6 m off, 4 m inside the edge): full again; 8 m off, half.
  CHECK(texel(along_z, 19, 16, 2) == 255);
  CHECK(std::abs(int{texel(along_z, 20, 16, 2)} - 191) <= 1);
  // Down +x (heading pi/2): (-1, 0); and up -z the same line reads as down +z.
  const std::vector<uint8_t> along_x = app::bake_snow_ground(flat_map(0.5 * kPi));
  CHECK(texel(along_x, 16, 16, 2) == 0);
  const std::vector<uint8_t> back = app::bake_snow_ground(flat_map(kPi));
  CHECK(texel(back, 16, 16, 2) == 255);
}

}  // namespace
