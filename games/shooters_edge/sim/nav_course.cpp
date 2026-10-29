// The nav course's brushes and model (nav_course.hpp describes the map).
//
// ge:summary The nav course's brushes: platform, stairs, crate, pit and its steps, two ramps
// ge:keywords build_nav_course ramp_brush brushes
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#include "shooters_edge/sim/nav_course.hpp"

#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::sim {

sh::Brush ramp_brush(float x0, float x1, float y0, float y1, float rise) {
  sh::Brush b = box_brush({x0, y0, 0}, {x1, y1, rise});
  // The slope: z <= rise * (x - x0) / (x1 - x0), as the plane n . p <= dist.
  sh::Vec3f n{-rise / (x1 - x0), 0, 1};
  sh::normalize(n);
  b.planes.push_back({n, n.x * x0, 3});
  return b;
}

void build_nav_course(NavCourse& c) {
  c.brushes = {
      box_brush({-528, -272, -160}, {0, 272, 0}),      // the floor west of the pit
      box_brush({96, -272, -160}, {528, 272, 0}),      // ... east of it
      box_brush({0, -272, -160}, {96, 272, -96}),      // the pit's floor
      box_brush({-528, -272, 0}, {-512, 272, 256}),    // the west wall
      box_brush({512, -272, 0}, {528, 272, 256}),      // the east wall
      box_brush({-512, -272, -96}, {512, -256, 256}),  // the south wall
      box_brush({-512, 256, -96}, {512, 272, 256}),    // the north wall
      box_brush({-512, -256, 0}, {-384, 256, 64}),     // the west platform
      box_brush({-384, -256, 0}, {-352, -128, 48}),    // its stairs
      box_brush({-352, -256, 0}, {-320, -128, 32}),
      box_brush({-320, -256, 0}, {-288, -128, 16}),
      box_brush({-192, 64, 0}, {-128, 128, 36}),  // the crate
      box_brush({0, 96, -96}, {96, 128, -80}),    // the pit's steps, up to the north wall
      box_brush({0, 128, -96}, {96, 160, -64}),
      box_brush({0, 160, -96}, {96, 192, -48}),
      box_brush({0, 192, -96}, {96, 224, -32}),
      box_brush({0, 224, -96}, {96, 256, -16}),
      ramp_brush(256, 384, -256, -128, 64),        // the ramp
      box_brush({384, -256, 0}, {512, -128, 64}),  // the east ledge
      ramp_brush(256, 384, 128, 256, 160),         // the steep ramp
      box_brush({384, 128, 0}, {512, 256, 160}),   // the high ledge past it
  };
  c.world = sh::build_brush_model(c.brushes);
}

}  // namespace shooters_edge::sim
