// THE 2D LAYER ON THE CPU: a draw list (draw.hpp) painted into an image, command by command,
// back to front, each pixel blended over what is there. It is how the labs picture a menu, a
// HUD or a console on the GPU-less container, how the tests check the layer to the pixel, and
// the reference the GPU pass (M22b) is held to. It is plain float arithmetic with no
// transcendental, so its pixels are the same on every CI row and feed a lab's digest.
//
// A box's edge is its rounded rectangle's signed distance at the pixel centre (in the box's own
// frame when it is turned) over the feather (1 px: antialiased; more: soft), its border the band
// `border` px inside that edge. A segment is a capsule's distance, an arc an annular sector's (a
// ring's inside the sweep, its ends' outside it), a convex polygon the largest of its edges'
// distances. Text is each glyph's coverage mask (font.hpp) times the run's colour.
//
// ge:summary The 2D layer's CPU raster: a draw list painted into an RGBA8 image
// ge:keywords ui raster cpu software render draw-list rounded-rect sdf antialias feather blend
// ge:keywords over text glyph reference headless
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#pragma once

#include "ui/draw.hpp"
#include "ui/image.hpp"

namespace ge::ui {

// Paints `list` over `target` (which keeps what it holds under the first command).
void render(const DrawList& list, Image& target);

// `src` over `dst` for one pixel, straight alpha: the blend every command uses.
void blend_over(uint8_t* dst, Color src);

// The signed distances (px, negative inside) the raster covers an arc and a convex polygon by;
// the GPU pass (render/ui_pass.wgsl) works the same ones.
float arc_distance(const Arc& a, float px, float py);
float polygon_distance(const PolyPoint* pts, uint32_t n, float px, float py);

}  // namespace ge::ui
