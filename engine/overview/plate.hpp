// THE PICTURE OF A LEVEL FROM ABOVE, for any game's spectators (TV, a recording, a map's preview):
// a level's up-facing floors drawn straight down (`Floors`, filled by the game's map reader, such
// as plugins/brush/app/overview_floors.hpp for the classic map families), then shaded and brought
// down to a picture on the CPU (`shade_floors`): the void a blueprint with a grid of 128 units, a
// soft shade where a wall or a higher floor stands over a floor, a low sun's shadows from the
// north-west, the lip of every drop lit, the walls' outline. `Frame` maps the level's units (x, y;
// z up) to the picture's pixels.
//
// THE PICTURE A GAME KEEPS (`Picture`): the shaded floors, the liquids as a layer of their own (so
// what lies under a surface is drawn under it), each pixel's highest floor (so what lies under a
// bridge reads as covered), and a thinned list of the liquids' pixels the glints play on.
//
// ge:summary A level's floors from above shaded on the CPU, its liquids a layer, its roofs kept
// ge:keywords map-floors top-down plan-view floor-plan ambient-shade sun-shadow blueprint
// ge:keywords overview live-map water-layer covered roof liquid-tint map-overview
// ge:skill map-design ui
// ge:test tests/overview_plate_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ui/draw.hpp"
#include "ui/image.hpp"

namespace ge::overview {

struct Frame {
  double lo[3] = {}, hi[3] = {};  // level units, z up
  double scale = 1;               // px a unit
  int pad = 48;
  int width = 0, height = 0;
  float px(double x) const { return float(pad + (x - lo[0]) * scale); }
  float py(double y) const { return float(pad + (hi[1] - y) * scale); }
};

constexpr float kNone = -1e9f;

// A liquid's kind, as `Floors::wkind` holds it (0: none).
enum Liquid : uint8_t { kWater = 2, kSlime = 3, kLava = 4 };

struct Floors {
  int w = 0, h = 0, ss = 2;
  std::vector<float> top;      // the highest floor's height, kNone where there is none
  std::vector<float> rgb;      // its colour, 0..1
  std::vector<float> wet;      // a liquid's surface height over it, kNone where none
  std::vector<float> wrgb;     // the liquid's colour
  std::vector<uint8_t> wkind;  // its `Liquid`
};

// `floors` at the frame's size times `ss` each way, empty.
Floors empty_floors(const Frame& fr, int ss);

double wrap(double v, double n);  // v mod n, in [0, n)

// The liquids as a layer of their own, over what lies under their surface: its picture (the
// liquid's colour and how much it hides, the plate's size) and each pixel's surface height
// (kNone: no liquid on top).
struct WaterLayer {
  ge::ui::Image image;
  std::vector<float> height;
};

// The floors shaded and brought down to the picture. With `layer` the liquids are left out of
// the picture (the floor under them only dimmed) and drawn into `layer` instead, so the plate,
// then the layer over it, looks as the picture without one does.
ge::ui::Image shade_floors(const Floors& f, const Frame& fr, WaterLayer* layer = nullptr);

struct Picture {
  Frame frame;                                 // units to the picture's px
  ge::ui::Image image;                         // the floors, shaded (the liquids left to `liquid`)
  ge::ui::Color void_colour;                   // the blueprint round the floors
  std::vector<std::array<uint16_t, 2>> water;  // liquid pixels on top, thinned (where glints play)
  std::vector<uint8_t> water_kind;             // the `Liquid` of each
  WaterLayer liquid;        // the liquids as a layer over what lies under their surface
  std::vector<float> roof;  // each pixel's highest floor (units, kNone: none)

  // The liquid's surface height over the point (units), kNone where there is none on top.
  float liquid_surface(double x, double y) const;
  // How a figure at the point shows: white and whole in the air; under a liquid's surface tinted
  // by it and partly hidden (`a` its opacity).
  ge::ui::Color liquid_tint(double x, double y, double z) const;
  // Whether a floor stands `cover_units` or more over the point: a bridge or a ledge over it.
  bool covered(double x, double y, double z, double cover_units) const;
};

// The frame for a level's bounds (`lo`, `hi`, units) at `max_px` on its long side.
Frame frame_for(const double lo[3], const double hi[3], int max_px);
// `out` from `floors` drawn in `fr`: the shaded picture, the liquid layer, the roofs, the glints'
// pixels (one in every 3 x 3 where the liquid is the top surface).
void picture_from_floors(const Floors& floors, const Frame& fr, Picture* out);
// The picture fitted in `area`, centred, its proportions kept.
ge::ui::Rect fit_rect(const Picture& p, ge::ui::Rect area);

}  // namespace ge::overview
