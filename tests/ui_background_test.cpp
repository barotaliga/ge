// engine/ui's backdrops: each start draws the layers it names, unit coordinates span the area,
// the drift eases out and back over its period, and a scene layer shows the frame handed in.
//
// ge:summary Tests backdrops: colour, gradient, image, scene, scrim, vignette, the drift
// ge:keywords test ui background backdrop gradient scrim vignette drift scene image
// ge:skill ui testing
#include <cmath>

#include "check.hpp"
#include "ui/background.hpp"
#include "ui/raster.hpp"

using namespace ge::ui;

namespace {

Image draw(const Background& bg, int w, int h, double t = 0, const Image* scene = nullptr) {
  Image img = make_image(w, h, hex(0x000000));
  DrawList dl;
  dl.begin(w, h);
  draw_background(dl, bg, {0, 0, float(w), float(h)}, t, scene);
  render(dl, img);
  return img;
}

int red(const Image& img, int x, int y) {
  return img.rgba[(static_cast<size_t>(y) * img.width + x) * 4];
}

}  // namespace

TEST(a_gradient_spans_the_area_whatever_its_shape) {
  const Background bg = Background::gradient(180, hex(0x000000), hex(0xff0000));
  for (auto [w, h] : {std::pair{64, 32}, std::pair{32, 64}}) {
    const Image img = draw(bg, w, h);
    CHECK(red(img, w / 2, 0) < 10);
    CHECK(red(img, w / 2, h - 1) > 245);
    CHECK(std::abs(red(img, w / 2, h / 2) - 128) < 8);
  }
}

TEST(a_scrim_darkens_its_side_only) {
  Background bg = Background::color(hex(0xffffff));
  bg.scrim(Side::left, 1, 0.5f);
  const Image img = draw(bg, 100, 10);
  CHECK(red(img, 0, 5) < 10);
  CHECK(red(img, 80, 5) == 255);
}

TEST(a_vignette_darkens_the_corners_not_the_middle) {
  Background bg = Background::color(hex(0xffffff));
  bg.vignette(0.8f);
  const Image img = draw(bg, 160, 90);
  CHECK(red(img, 80, 45) == 255);
  CHECK(red(img, 0, 0) < 140);
}

TEST(the_drift_eases_out_and_back_once_a_period) {
  Drift d;
  d.period_s = 10;
  d.zoom_to = 1.2f;
  d.focus_to_x = 0.3f;
  CHECK(framing_at(d, 0).zoom == 1);
  CHECK(std::fabs(framing_at(d, 5).zoom - 1.2f) < 1e-5f);
  CHECK(std::fabs(framing_at(d, 5).focus_x - 0.3f) < 1e-5f);
  CHECK(std::fabs(framing_at(d, 10).zoom - 1) < 1e-5f);
  CHECK(framing_at(d, 2.5).zoom > 1.05f && framing_at(d, 2.5).zoom < 1.15f);
  CHECK(framing_at(Drift{}, 3).zoom == 1);
}

TEST(an_image_layer_covers_and_a_scene_shows_the_frame_handed_in) {
  const Image pic = make_image(40, 10, hex(0x00ff00));
  const Image a = draw(Background::image(pic), 20, 20);
  CHECK(a.rgba[(10 * 20 + 10) * size_t{4} + 1] == 255);
  const Image frame = make_image(8, 8, hex(0x0000ff));
  Background scene = Background::scene();
  scene.tint(hex(0xffffff, 0.5f));
  const Image b = draw(scene, 20, 20, 0, &frame);
  CHECK(std::abs(b.rgba[(10 * 20 + 10) * size_t{4} + 2] - 128) <= 1);
  const Image none = draw(Background::scene(), 20, 20);  // no frame: nothing drawn
  CHECK(none.rgba[(10 * 20 + 10) * size_t{4} + 2] == 0);
}
