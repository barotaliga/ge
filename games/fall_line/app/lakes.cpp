// The map's laid lakes drawn (lakes.hpp).
//
// ge:summary show_lakes: the laid lakes' water and ice and the open streams added to a frame
// ge:keywords lakes tarn show_lakes freeze_water show_streams mapped-stream
// ge:skill render water
#include "fall_line/app/lakes.hpp"

#include <vector>

#include "core/heightfield.hpp"
#include "mountain/air/air.hpp"
#include "mountain/tarn/brook.hpp"
#include "mountain/tarn/tarn.hpp"
#include "render/water_stream.hpp"
#include "water/world.hpp"

namespace fall_line::app {

namespace {

double bed_height(const void* context, double x, double z) {
  return ge::core::sample_field(static_cast<const ge::mountain::mapgen::Level*>(context)->ground, x,
                                z);
}

// THE STREAMS: every open one a channel down its laid line (water/world.hpp's `add_river`), its
// ribbon built once (render/water_stream.hpp) a row every `kStreamRow` m; a frozen one is snow.
constexpr double kStreamRow = 2;
constexpr double kBrookFlow = 0.4;  // m^3/s
constexpr double kRiverFlow = 8;    // m^3/s

void show_streams(ge::gpu::Device& device, GameFrame& f) {
  const ge::mountain::mapgen::Level& level = *f.level;
  ge::water::World world(ge::mountain::air::field_of(level));
  std::vector<ge::water::ChannelPoint> line;
  for (const ge::mountain::mapgen::MapStream& s : level.streams) {
    if (s.frozen || s.points.size() < 2) continue;
    line.clear();
    for (const ge::mountain::mapgen::StreamPoint& p : s.points)
      line.push_back({p.x, p.z, p.level, p.width, p.depth});
    ge::water::add_river(world, line, s.river ? kRiverFlow : kBrookFlow,
                         {.look = "glacial", .temperature = 2}, {bed_height, &level});
  }
  if (world.bodies.empty()) return;
  ge::render::StreamMesh mesh;
  ge::render::build_water_streams(world, 0, mesh, kStreamRow);
  ge::render::prepare_water_streams(device, f.water, mesh, world);
}

}  // namespace

void show_lakes(ge::gpu::Device& device, GameFrame& f) {
  const ge::mountain::mapgen::Level& level = *f.level;
  for (const ge::mountain::mapgen::MapLake& lake : level.lakes) {
    ge::water::World world(ge::mountain::air::field_of(level));
    const std::vector<ge::water::Point> shore = ge::mountain::tarn::shore_of(lake);
    ge::water::add_lake(world, shore, lake.level, {.temperature = ge::mountain::tarn::kTarnWater},
                        {bed_height, &level});
    const int body = add_water(device, f, world.bodies[0]);
    // Ice that bears a skier lies on ground filled flat (lake.depth 0): the map's snow is on it,
    // and its thaw is its look alone (no water under it to open a moat on).
    const bool filled = lake.depth == 0;
    freeze_water(device, f, body, world.bodies[0], lake.ice, filled,
                 filled ? 0 : ge::mountain::tarn::moat_of(lake), lake.thaw);
  }
  show_streams(device, f);
}

}  // namespace fall_line::app
