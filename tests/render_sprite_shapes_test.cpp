// engine/render/sprite_shapes.hpp: a flare is brightest in its middle, falls off outward and is
// gone at its edge; a bubble is nearly clear in its middle and opaque nowhere but its rim and its
// highlight, which is up and to the left, and nothing outside its ball; both are their size, at
// least 8, with the origin in the middle, the same texels every time.
//
// ge:summary Tests for the made sprite frames: the flare's glow and the bubble's rim
// ge:keywords test render sprite flare bubble procedural
// ge:skill render testing
#include "check.hpp"
#include "render/sprite_shapes.hpp"

namespace {

namespace r = ge::render;

int alpha(const r::SpriteFrame& f, uint32_t x, uint32_t y) {
  return f.rgba[(size_t{y} * f.width + x) * 4 + 3];
}

}  // namespace

TEST(a_flare_glows_in_its_middle_and_fades_to_nothing) {
  const r::SpriteFrame f = r::flare_frame(64);
  CHECK(f.width == 64 && f.height == 64 && f.origin[0] == 32 && f.origin[1] == 32);
  CHECK(alpha(f, 32, 32) > 200);
  CHECK(alpha(f, 32, 32) > alpha(f, 44, 32) && alpha(f, 44, 32) > alpha(f, 56, 32));
  CHECK(alpha(f, 0, 0) == 0 && alpha(f, 63, 32) == 0);
  CHECK(r::flare_frame(64).rgba == f.rgba);
  CHECK(r::flare_frame(2).width == 8);
}

TEST(a_bubble_is_clear_inside_bright_at_its_rim_and_lit_up_left) {
  const r::SpriteFrame b = r::bubble_frame(32);
  CHECK(b.width == 32 && b.origin[0] == 16);
  CHECK(alpha(b, 16, 16) < 40);                         // nearly clear in the middle
  CHECK(alpha(b, 16, 3) > alpha(b, 16, 16));            // the rim brighter
  CHECK(alpha(b, 0, 0) == 0 && alpha(b, 31, 31) == 0);  // nothing outside the ball
  // The highlight: up and to the left of the middle.
  int best = 0;
  uint32_t bx = 0, by = 0;
  for (uint32_t y = 4; y < 28; ++y)
    for (uint32_t x = 4; x < 28; ++x)
      if (alpha(b, x, y) > best && (x - 16) * (x - 16) + (y - 16) * (y - 16) < 64) {
        best = alpha(b, x, y);
        bx = x, by = y;
      }
  CHECK(bx < 16 && by < 16);
  CHECK(r::bubble_frame(32).rgba == b.rgba);
}
