// engine/ui's draw list and CPU raster: a box covers its pixels with an antialiased edge, rounds
// its corners, draws its border, blends by its alpha, samples an image, keeps to its clip, and
// text lands at its baseline; images fit and blur as stated.
//
// ge:summary Tests the 2D draw list and CPU raster: boxes, corners, borders, clip, images, text
// ge:keywords test ui raster draw-list box rounded border clip blend image fit blur text
// ge:skill ui testing
#include <cmath>

#include "check.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

using namespace ge::ui;

namespace {

const uint8_t* px(const Image& img, int x, int y) {
  return img.rgba.data() + (static_cast<size_t>(y) * img.width + x) * 4;
}

}  // namespace

TEST(a_box_fills_inside_and_antialiases_its_edge) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  dl.rect({10, 10, 20.5f, 20}, hex(0xffffff));
  render(dl, img);
  CHECK(px(img, 15, 15)[0] == 255);
  CHECK(px(img, 5, 5)[0] == 0);
  CHECK(px(img, 9, 15)[0] == 0);
  const int edge = px(img, 30, 15)[0];  // half covered
  CHECK(edge > 100 && edge < 160);
}

TEST(rounded_corners_leave_the_corner_pixel_out) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  dl.rect({0, 0, 40, 40}, hex(0xffffff), 12);
  render(dl, img);
  CHECK(px(img, 0, 0)[0] < 10);
  CHECK(px(img, 20, 0)[0] == 255);
  CHECK(px(img, 20, 20)[0] == 255);
}

TEST(a_border_draws_its_band_inside_the_edge) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  Box b;
  b.rect = {0, 0, 40, 40};
  b.fill = solid(hex(0x0000ff));
  b.border = 4;
  b.border_color = hex(0xff0000);
  dl.box(b);
  render(dl, img);
  CHECK(px(img, 1, 20)[0] == 255 && px(img, 1, 20)[2] == 0);
  CHECK(px(img, 20, 20)[0] == 0 && px(img, 20, 20)[2] == 255);
}

TEST(alpha_blends_over_what_is_there) {
  Image img = make_image(4, 4, hex(0xffffff));
  DrawList dl;
  dl.begin(4, 4);
  dl.rect({0, 0, 4, 4}, hex(0x000000, 0.5f));
  render(dl, img);
  CHECK(px(img, 1, 1)[0] == 128 && px(img, 1, 1)[3] == 255);
  // Over a transparent target the colour is kept and the alpha is the box's.
  Image clear = make_image(4, 4, {0, 0, 0, 0});
  dl.begin(4, 4);
  dl.rect({0, 0, 4, 4}, hex(0xff0000, 0.5f));
  render(dl, clear);
  CHECK(px(clear, 1, 1)[0] == 255 && px(clear, 1, 1)[3] == 128);
}

TEST(the_clip_keeps_a_box_inside_it) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  dl.push_clip({0, 0, 20, 40});
  dl.rect({0, 0, 40, 40}, hex(0xffffff));
  dl.pop_clip();
  render(dl, img);
  CHECK(px(img, 10, 10)[0] == 255);
  CHECK(px(img, 30, 10)[0] == 0);
}

TEST(an_image_is_sampled_over_its_box_and_tinted_by_the_fill) {
  Image src = make_image(2, 1, hex(0xff0000));
  src.rgba[4] = 0, src.rgba[6] = 255;  // the right pixel blue
  Image img = make_image(20, 10, hex(0x000000));
  DrawList dl;
  dl.begin(20, 10);
  Box b;
  b.rect = {0, 0, 20, 10};
  b.image = &src;
  b.uv = {0, 0, 2, 1};
  b.fill = solid(hex(0xffffff));
  dl.box(b);
  render(dl, img);
  CHECK(px(img, 1, 5)[0] == 255 && px(img, 1, 5)[2] == 0);
  CHECK(px(img, 18, 5)[0] == 0 && px(img, 18, 5)[2] == 255);
}

TEST(cover_crops_and_contain_letterboxes) {
  const Image wide = make_image(200, 100, hex(0));
  const Rect c = fit_uv(wide, 100, 100, Fit::cover);
  CHECK(c.w == 100 && c.h == 100 && c.x == 50);
  const Rect left = fit_uv(wide, 100, 100, Fit::cover, 0, 0.5f);
  CHECK(left.x == 0);
  const Rect z = fit_uv(wide, 100, 100, Fit::cover, 0.5f, 0.5f, 2);
  CHECK(z.w == 50 && z.x == 75);
  const Rect box = contain_rect(wide, {0, 0, 100, 100});
  CHECK(box.w == 100 && box.h == 50 && box.y == 25);
}

TEST(blur_spreads_a_dot_and_keeps_a_flat_field) {
  Image dot = make_image(21, 21, hex(0x000000));
  dot.rgba[(10 * 21 + 10) * size_t{4}] = 255;
  const Image b = blurred(dot, 6);
  CHECK(px(b, 10, 10)[0] < 255 && px(b, 10, 10)[0] > 0);
  CHECK(px(b, 12, 10)[0] > 0);
  const Image flat = blurred(make_image(9, 9, hex(0x808080)), 4);
  CHECK(px(flat, 0, 0)[0] == 0x80 && px(flat, 8, 8)[0] == 0x80);
}

TEST(text_lands_on_its_baseline_and_aligns) {
  Image img = make_image(200, 60, hex(0x000000));
  DrawList dl;
  dl.begin(200, 60);
  const float w = dl.text(ui_font(), 30, 100, 40, "MENU", hex(0xffffff), Align::center);
  CHECK(w > 30);
  render(dl, img);
  int top = 60, bottom = 0, left = 200, right = 0;
  for (int y = 0; y < 60; y++)
    for (int x = 0; x < 200; x++)
      if (px(img, x, y)[0] > 128) {
        top = std::min(top, y), bottom = std::max(bottom, y);
        left = std::min(left, x), right = std::max(right, x);
      }
  CHECK(bottom <= 40 && bottom >= 37);  // capitals sit on the baseline
  CHECK(top >= 40 - 30 && top < 25);
  CHECK(std::abs((left + right) / 2 - 100) <= 2);  // centred
}
