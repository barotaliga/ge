// engine/hud/spinner.hpp: the loading spinner sits in its corner, draws its ring, turns with the
// clock and puts its caption on the inner side.
//
// ge:summary Tests the loading spinner: its corner, its ink, its turn and its caption's side
// ge:keywords test hud spinner loading draw_spinner corner arc caption
// ge:skill ui testing
#include "hud/spinner.hpp"

#include <cstdint>

#include "check.hpp"
#include "ui/raster.hpp"

using namespace ge::hud;
namespace ui = ge::ui;

namespace {

constexpr int kW = 640, kH = 360;

ui::Image drawn(double t, std::string_view caption, Anchor corner = Anchor::bottom_right) {
  const Style st;
  const Frame f = frame_for(kW, kH, st);
  ui::DrawList dl;
  dl.begin(kW, kH);
  Spinner s;
  s.corner = corner;
  draw_spinner(dl, f, st, t, caption, 2.5, s);
  ui::Image img;
  img.width = kW;
  img.height = kH;
  img.rgba.assign(size_t(kW) * kH * 4, 0);
  for (size_t i = 3; i < img.rgba.size(); i += 4) img.rgba[i] = 255;
  ui::render(dl, img);
  return img;
}

// The pixels inked (any channel lit) in the box x0..x1, y0..y1.
size_t inked(const ui::Image& img, int x0, int y0, int x1, int y1) {
  size_t n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      const size_t i = (size_t(y) * kW + x) * 4;
      n += img.rgba[i] + img.rgba[i + 1] + img.rgba[i + 2] > 0;
    }
  return n;
}

}  // namespace

TEST(the_spinner_sits_in_its_corner) {
  const Style st;
  const Frame f = frame_for(kW, kH, st);
  ui::DrawList dl;
  dl.begin(kW, kH);
  const ui::Rect r = draw_spinner(dl, f, st, 0, "LOADING DM6", 1.0);
  CHECK(r.right() <= f.safe.right() + 0.5f && r.bottom() <= f.safe.bottom() + 0.5f);
  CHECK(r.x > kW / 2.0f && r.y > kH / 2.0f);
  const ui::Image img = drawn(0, "LOADING DM6");
  CHECK(inked(img, kW / 2, kH / 2, kW, kH) > 200);
  CHECK(inked(img, 0, 0, kW / 2, kH / 2) == 0);
}

TEST(the_arc_turns_with_the_clock) {
  const ui::Image a = drawn(0.0, "");
  const ui::Image b = drawn(0.3, "");
  size_t differ = 0;
  for (size_t i = 0; i < a.rgba.size(); ++i) differ += a.rgba[i] != b.rgba[i];
  CHECK(differ > 50);
  // A whole period later the picture is the one it was (the breath runs over two).
  const ui::Image c = drawn(2.2, "");
  size_t same = 0;
  for (size_t i = 0; i < a.rgba.size(); ++i) same += a.rgba[i] == c.rgba[i];
  CHECK(same + 8 >= a.rgba.size());
}

TEST(the_caption_is_on_the_inner_side) {
  // Bottom right: the ring at the far right, the words to its left; bottom left mirrors it.
  const ui::Image right = drawn(0, "LOADING");
  const ui::Image left = drawn(0, "LOADING", Anchor::bottom_left);
  CHECK(inked(right, kW / 2, kH / 2, kW, kH) > inked(right, 0, kH / 2, kW / 2, kH));
  CHECK(inked(left, 0, kH / 2, kW / 2, kH) > inked(left, kW / 2, kH / 2, kW, kH));
}
