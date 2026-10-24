// engine/ui's draw list and CPU raster: a box covers its pixels with an antialiased edge, rounds
// its corners, draws its border, blends by its alpha, samples an image, keeps to its clip, and
// text lands at its baseline; images fit and blur as stated; a box turns about its pivot and
// nine-slices its image; a line is a capsule, an arc covers its sweep with flat or round ends,
// and a convex polygon fills inside its edges in either winding.
//
// ge:summary Tests the 2D draw list and CPU raster: boxes, corners, borders, clip, images, text
// ge:keywords test ui raster draw-list box rounded border clip blend image fit blur text
// ge:keywords rotation pivot nine-slice line arc polygon
// ge:skill ui testing
#include <cmath>
#include <utility>

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

TEST(a_turned_box_lies_on_its_side) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  Box b;
  b.rect = {15, 5, 10, 30};
  b.rotation = 1.5707963f;  // a quarter turn about its centre
  dl.box(b);
  render(dl, img);
  CHECK(px(img, 30, 20)[0] == 255);
  CHECK(px(img, 8, 20)[0] == 255);
  CHECK(px(img, 20, 8)[0] == 0);
  CHECK(px(img, 20, 31)[0] == 0);
}

TEST(a_pivot_turns_the_box_about_it) {
  // A needle 4 px wide and 20 long standing on (22, 30), turned a quarter clockwise about its
  // foot: it points right from there.
  Image img = make_image(50, 40, hex(0x000000));
  DrawList dl;
  dl.begin(50, 40);
  Box b;
  b.rect = {20, 10, 4, 20};
  b.pivot = Vec2{2, 20};
  b.rotation = 1.5707963f;
  dl.box(b);
  render(dl, img);
  CHECK(px(img, 38, 30)[0] == 255);
  CHECK(px(img, 24, 29)[0] == 255);
  CHECK(px(img, 21, 15)[0] == 0);
  CHECK(px(img, 18, 30)[0] == 0);
}

TEST(a_nine_sliced_box_keeps_its_corners_and_stretches_the_rest) {
  // A 9x9 frame: 3 px red corners, green edges, a blue middle.
  Image src = make_image(9, 9, hex(0x0000ff));
  for (int y = 0; y < 9; ++y) {
    for (int x = 0; x < 9; ++x) {
      const bool ex = x < 3 || x >= 6, ey = y < 3 || y >= 6;
      if (!ex && !ey) continue;
      uint8_t* p = src.rgba.data() + (static_cast<size_t>(y) * 9 + x) * 4;
      p[0] = ex && ey ? 255 : 0, p[1] = ex && ey ? 0 : 255, p[2] = 0;
    }
  }
  Image img = make_image(60, 30, hex(0x000000));
  DrawList dl;
  dl.begin(60, 30);
  Box b;
  b.rect = {0, 0, 60, 30};
  b.image = &src;
  b.uv = {0, 0, 9, 9};
  b.slice = {3, 3, 3, 3};
  b.slice_to = {6, 6, 6, 6};
  dl.box(b);
  render(dl, img);
  CHECK(px(img, 1, 1)[0] == 255 && px(img, 1, 1)[1] == 0);
  CHECK(px(img, 58, 28)[0] == 255 && px(img, 58, 28)[1] == 0);
  CHECK(px(img, 30, 1)[1] == 255 && px(img, 30, 1)[0] == 0);
  CHECK(px(img, 1, 15)[1] == 255 && px(img, 1, 15)[0] == 0);
  CHECK(px(img, 30, 15)[2] == 255 && px(img, 30, 15)[1] == 0);
}

TEST(a_line_is_a_capsule_with_round_caps) {
  Image img = make_image(40, 40, hex(0x000000));
  DrawList dl;
  dl.begin(40, 40);
  dl.line({10, 20.5f}, {30, 20.5f}, 6, hex(0xffffff));
  render(dl, img);
  CHECK(px(img, 20, 20)[0] == 255);
  CHECK(px(img, 20, 22)[0] == 255);
  CHECK(px(img, 20, 25)[0] == 0);
  CHECK(px(img, 8, 20)[0] == 255);  // inside the cap
  CHECK(px(img, 5, 20)[0] == 0);    // past it
  CHECK(px(img, 7, 23)[0] < 128);   // the cap's rounded corner
}

TEST(an_arc_covers_its_sweep_and_its_ends_are_flat_or_round) {
  auto ring = [](bool round, float to) {
    Image img = make_image(40, 40, hex(0x000000));
    DrawList dl;
    dl.begin(40, 40);
    dl.arc({20, 20}, 12, 4, 0, to, solid(hex(0xffffff)), round);
    render(dl, img);
    return img;
  };
  const Image flat = ring(false, 1.5707963f);  // twelve to three o'clock
  CHECK(px(flat, 28, 11)[0] == 255);           // the middle of the sweep
  CHECK(px(flat, 31, 19)[0] == 255);           // three o'clock
  CHECK(px(flat, 19, 31)[0] == 0);             // six o'clock: outside the sweep
  CHECK(px(flat, 20, 20)[0] == 0);             // the centre
  CHECK(px(flat, 18, 7)[0] == 0);              // just before twelve: the flat end
  const Image round = ring(true, 1.5707963f);
  CHECK(px(round, 18, 7)[0] > 200);  // the round end reaches back past twelve
  const Image whole = ring(false, 6.2831853f);
  CHECK(px(whole, 19, 31)[0] == 255);
  CHECK(px(whole, 20, 20)[0] == 0);
}

TEST(a_convex_polygon_fills_inside_its_edges_in_either_winding) {
  auto draw = [](bool reverse) {
    Image img = make_image(40, 40, hex(0x000000));
    DrawList dl;
    dl.begin(40, 40);
    Vec2 tri[3] = {{10, 10}, {30, 10}, {20, 30}};
    if (reverse) std::swap(tri[0], tri[2]);
    dl.polygon(tri, solid(hex(0xffffff)));
    render(dl, img);
    return img;
  };
  const Image a = draw(false), b = draw(true);
  CHECK(px(a, 20, 15)[0] == 255);
  CHECK(px(a, 12, 25)[0] == 0);
  CHECK(px(a, 20, 5)[0] == 0);
  CHECK(a.rgba == b.rgba);
}

TEST(a_polygon_past_the_most_points_or_with_no_area_draws_nothing) {
  DrawList dl;
  dl.begin(40, 40);
  Vec2 many[kMaxPolygon + 1];
  for (int i = 0; i <= kMaxPolygon; ++i)
    many[i] = {static_cast<float>(i), static_cast<float>(i * i)};
  dl.polygon(many, solid(hex(0xffffff)));
  const Vec2 flat[3] = {{0, 0}, {10, 10}, {20, 20}};
  dl.polygon(flat, solid(hex(0xffffff)));
  CHECK(dl.size() == 0);
}
