// The frame rate on screen (engine/hud/frame_graph.hpp).
//
// ge:summary Tests the frame rate on screen: its corner, its modes, a missed refresh in red
// ge:keywords test hud fps scr_showfps frame-graph draw_frame_rate
// ge:skill ui testing
#include "hud/frame_graph.hpp"

#include "check.hpp"
#include "ui/raster.hpp"

using namespace ge::hud;
namespace ui = ge::ui;

namespace {

constexpr int kW = 640, kH = 360;

// Pixels clearly red (the style's `bad`): red over green by a margin.
size_t red_pixels(const ui::DrawList& dl, const Style& st) {
  ui::Image img;
  img.width = kW;
  img.height = kH;
  img.rgba.assign(size_t(kW) * kH * 4, 0);
  ui::render(dl, img);
  size_t n = 0;
  (void)st;
  for (size_t i = 0; i < img.rgba.size(); i += 4) n += img.rgba[i] > img.rgba[i + 1] + 60;
  return n;
}

}  // namespace

TEST(the_rate_sits_top_right_and_mode_zero_draws_nothing) {
  const Style st;
  const Frame f = frame_for(kW, kH, st);
  ge::loop::FrameTimes t;
  for (int i = 0; i < 200; i++) t.add(1.0 / 120);
  ui::DrawList dl;
  dl.begin(kW, kH);
  CHECK(draw_frame_rate(dl, f, st, t, 120, 0).w == 0);
  CHECK(dl.size() == 0);
  const ui::Rect one = draw_frame_rate(dl, f, st, t, 120, 1);
  CHECK(one.x > kW / 2.0f && one.y < kH / 2.0f);
  const ui::Rect two = draw_frame_rate(dl, f, st, t, 120, 2);
  CHECK(two.h > one.h);
}

TEST(a_missed_refresh_is_drawn_red) {
  const Style st;
  const Frame f = frame_for(kW, kH, st);
  ge::loop::FrameTimes even, missed;
  for (int i = 0; i < 120; i++) {
    even.add(1.0 / 120);
    missed.add(i % 10 == 0 ? 2.0 / 120 : 1.0 / 120);
  }
  ui::DrawList a, b;
  a.begin(kW, kH);
  b.begin(kW, kH);
  draw_frame_rate(a, f, st, even, 120, 2);
  draw_frame_rate(b, f, st, missed, 120, 2);
  const size_t ra = red_pixels(a, st), rb = red_pixels(b, st);
  CHECK(rb > ra + 20);
}
