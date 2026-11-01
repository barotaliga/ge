// The map's laid lakes drawn (lakes.hpp).
//
// ge:summary show_lakes: the laid lakes' water and ice added to a frame
// ge:keywords lakes tarn show_lakes freeze_water
// ge:skill render water
#include "fall_line/app/lakes.hpp"

#include <vector>

#include "core/heightfield.hpp"
#include "mountain/air/air.hpp"
#include "mountain/tarn/tarn.hpp"
#include "water/world.hpp"

namespace fall_line::app {

namespace {

constexpr double kTarnWater = 4;  // C: the water under a mountain tarn's ice, or just thawed

double bed_height(const void* context, double x, double z) {
  return ge::core::sample_field(static_cast<const ge::mountain::mapgen::Level*>(context)->ground, x,
                                z);
}

}  // namespace

void show_lakes(ge::gpu::Device& device, GameFrame& f) {
  const ge::mountain::mapgen::Level& level = *f.level;
  for (const ge::mountain::mapgen::MapLake& lake : level.lakes) {
    ge::water::World world(ge::mountain::air::field_of(level));
    const std::vector<ge::water::Point> shore = ge::mountain::tarn::shore_of(lake);
    ge::water::add_lake(world, shore, lake.level, {.temperature = kTarnWater},
                        {bed_height, &level});
    const int body = add_water(device, f, world.bodies[0]);
    // Ice that bears a skier lies on ground filled flat (lake.depth 0): the map's snow is on it.
    freeze_water(device, f, body, world.bodies[0], lake.ice, lake.depth == 0);
  }
}

}  // namespace fall_line::app
