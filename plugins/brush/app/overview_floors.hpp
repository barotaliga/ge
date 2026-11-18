// A CLASSIC BSP MAP SEEN FROM ABOVE (engine/overview): the map's faces that look up drawn straight
// down into the overview's floors (`overview_floors`: each column's highest floor in its texture
// under its lightmap; water, slime and lava as a liquid's surface over what lies under it), the
// frame that fits the map (`overview_frame`), and a small map (a health or ammo box) as a mesh the
// overview draws as a figure (`overview_box`). Any game on these map families gets its overview
// from here; it adds only what its things mean.
//
// ge:summary A brush map's up-facing faces drawn straight down into the overview's floors
// ge:keywords overview map-floors top-down plan-view lightmap live-map box-model overview_floors
// ge:skill brush map-design
// ge:test tests/overview_plate_test.cpp
#pragma once

#include "brush/app/draw_data.hpp"
#include "overview/mesh_picture.hpp"
#include "overview/plate.hpp"

namespace ge::brush::app {

// Every face of `scene` that looks up drawn straight down (`ss` samples a pixel each way).
ge::overview::Floors overview_floors(const Scene& scene, const ge::overview::Frame& fr, int ss);
// The frame that fits `scene`'s bounds at `max_px` on its long side (map units: x, y; z up).
ge::overview::Frame overview_frame(const Scene& scene, int max_px);
// A small map's faces as one textured mesh (map units, z up), its atlas the texture.
ge::overview::Mesh overview_box(const Scene& scene);

}  // namespace ge::brush::app
