// THE FLOORS OF A MAP PICTURE (the `map-overview` lab): a version 29 map drawn straight down on the
// CPU from its own faces: every face that looks up, its texture under its lightmap, the highest in
// each column; water, slime and lava over what lies under them. Then shaded and brought down to the
// picture: the void a blueprint with a grid of 128 units, a soft shade where a wall or a higher
// floor stands over a floor, a low sun's shadows from the north-west, the lip of every drop lit,
// the walls' outline. `Frame` maps the map's units (x, y; z up) to the picture's pixels.
//
// ge:summary The map picture's floors: a map's up-facing faces lit and shaded from above on the CPU
// ge:keywords map-floors top-down plan-view floor-plan lightmap ambient-shade sun-shadow blueprint
// ge:keywords map-overview
// ge:skill map-design labs
#pragma once

#include <cstdint>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "ui/image.hpp"

namespace shooters_edge::livemap {

struct Frame {
  double lo[3] = {}, hi[3] = {};  // map units, z up
  double scale = 1;               // px a unit
  int pad = 48;
  int width = 0, height = 0;
  float px(double x) const { return float(pad + (x - lo[0]) * scale); }
  float py(double y) const { return float(pad + (hi[1] - y) * scale); }
};

constexpr float kNone = -1e9f;

struct Floors {
  int w = 0, h = 0, ss = 2;
  std::vector<float> top;   // the highest floor's height, kNone where there is none
  std::vector<float> rgb;   // its colour, 0..1
  std::vector<float> wet;   // a liquid's surface height over it, kNone where none
  std::vector<float> wrgb;  // the liquid's colour
  std::vector<uint8_t> wkind;
};

double wrap(double v, double n);  // v mod n, in [0, n)
// Every face that looks up drawn straight down (`ss` samples a pixel each way).
Floors draw_floors(const ge::brush::app::Scene& scene, const Frame& fr, int ss);
// The floors shaded and brought down to the picture.
ge::ui::Image shade_floors(const Floors& f, const Frame& fr);

}  // namespace shooters_edge::livemap
