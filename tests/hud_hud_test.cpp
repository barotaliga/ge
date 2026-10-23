// engine/hud: the frame's scale and safe area, the anchors and the stack, the widgets' measures,
// the charset type's ink on the raster, the feed's life and the layout check.
//
// ge:summary Tests the HUD kit: frame, anchors, stack, widgets, charset type, feed, layout check
// ge:keywords test hud frame anchor stack readout meter pips feed blink layout-problems charset
// ge:skill ui testing
#include <cmath>

#include "check.hpp"
#include "hud/feed.hpp"
#include "hud/hud.hpp"
#include "ui/raster.hpp"

using namespace ge::hud;
namespace ui = ge::ui;

namespace {

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// The share of the image's pixels whose red differs from the backdrop's.
double inked(const ui::Image& img, uint8_t bg_r) {
  size_t n = 0;
  for (size_t i = 0; i < img.rgba.size(); i += 4) n += img.rgba[i] != bg_r;
  return static_cast<double>(n) / static_cast<double>(img.rgba.size() / 4);
}

}  // namespace

TEST(the_frame_scales_by_the_short_side_and_clamps) {
  const Style st;
  CHECK(near(frame_for(1280, 720, st).scale, 1));
  CHECK(near(frame_for(2560, 1440, st).scale, 2));
  CHECK(near(frame_for(390, 844, st).scale, st.min_scale));
  CHECK(frame_for(390, 844, st).portrait);
}

TEST(the_safe_area_takes_the_insets_and_the_margin) {
  Style st;
  st.margin = 10;
  const Frame f = frame_for(1280, 720, st, Insets{40, 0, 0, 30});
  CHECK(near(f.safe.x, 50) && near(f.safe.y, 10));
  CHECK(near(f.safe.right(), 1270) && near(f.safe.bottom(), 680));
}

TEST(anchors_put_a_box_in_its_corner_of_the_safe_area) {
  Style st;
  st.margin = 0;
  const Frame f = frame_for(1000, 500, st);
  const ui::Rect tl = place(f, Anchor::top_left, 100, 50);
  const ui::Rect br = place(f, Anchor::bottom_right, 100, 50);
  const ui::Rect c = place(f, Anchor::center, 100, 50);
  CHECK(near(tl.x, 0) && near(tl.y, 0));
  CHECK(near(br.right(), 1000) && near(br.bottom(), 500));
  CHECK(near(c.x, 450) && near(c.y, 225));
}

TEST(a_stack_goes_down_from_the_top_and_up_from_the_bottom) {
  Style st;
  st.margin = 0;
  const Frame f = frame_for(1000, 500, st);
  Stack down(f, Anchor::top_left, 10);
  const ui::Rect a = down.next(50, 20), b = down.next(80, 30);
  CHECK(near(b.y, a.bottom() + 10));
  CHECK(near(down.used(), 60));
  Stack up(f, Anchor::bottom_right, 10);
  const ui::Rect c = up.next(50, 20), d = up.next(80, 30);
  CHECK(near(c.bottom(), 500) && near(d.bottom(), c.y - 10) && near(d.right(), 1000));
}

TEST(a_charset_run_advances_half_its_size_a_character) {
  const Face face = Face::bitmap(ge::charset::FontId::mono);
  CHECK(near(text_width(face, 40, "ABCD"), 80));
  CHECK(near(text_width(face, 40, "ABCD", 2), 88));
  CHECK(near(ascent(face, 100) + descent(face, 100), 100));
}

TEST(a_charset_run_puts_ink_on_the_raster_and_a_space_none) {
  ui::DrawList dl;
  dl.begin(120, 60);
  draw_text(dl, Face::bitmap(ge::charset::FontId::rounded), 40, 10, 40, "72", {1, 1, 1, 1});
  ui::Image img = ui::make_image(120, 60, ui::hex(0x000000));
  ui::render(dl, img);
  CHECK(inked(img, 0) > 0.03);
  ui::DrawList blank;
  blank.begin(120, 60);
  draw_text(blank, Face::bitmap(ge::charset::FontId::rounded), 40, 10, 40, "   ", {1, 1, 1, 1});
  CHECK(blank.size() == 0);
}

TEST(widgets_measure_bigger_on_a_bigger_screen) {
  const Style st;
  const Readout r{"38 @ 12,-40", "SEED", st.ink};
  const Size small = measure(frame_for(1280, 720, st), st, r);
  const Size big = measure(frame_for(2560, 1440, st), st, r);
  CHECK(near(big.w, small.w * 2, 0.5f) && near(big.h, small.h * 2, 0.5f));
  Pips p;
  p.count = 5, p.size = 40, p.spacing = 0;
  CHECK(near(measure(frame_for(1280, 720, st), p).w, 100));
}

TEST(a_centred_meter_fills_from_the_middle) {
  const Style st;
  const Frame f = frame_for(1280, 720, st);
  ui::DrawList dl;
  dl.begin(200, 20);
  Meter m;
  m.centred = true, m.value = -0.5f, m.w = 200, m.h = 10;
  draw(dl, f, st, m, {0, 5, 200, 10});
  // shadow, track, the fill, the centre mark: the fill runs from 50 to 100.
  bool found = false;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && near(c.box.rect.x, 50) && near(c.box.rect.w, 50))
      found = true;
  CHECK(found);
}

TEST(blink_is_on_for_the_first_half_of_each_period) {
  CHECK(blink(0.0, 2));
  CHECK(blink(0.2, 2));
  CHECK(!blink(0.3, 2));
  CHECK(blink(0.51, 2));
}

TEST(a_feed_line_holds_fades_and_is_forgotten) {
  Feed feed(3, 2, 1, 0.2);
  feed.push("A", {1, 1, 1, 1}, 0);
  CHECK(near(static_cast<float>(feed.strength(0, 1.5)), 1));
  CHECK(near(static_cast<float>(feed.strength(0, 2.5)), 0.5f));
  CHECK(near(static_cast<float>(feed.risen(0, 0.1)), 0.75f));
  feed.expire(2.9);
  CHECK(feed.lines().size() == 1);
  feed.expire(3.1);
  CHECK(feed.lines().empty());
}

TEST(a_full_feed_drops_its_oldest) {
  Feed feed(2);
  feed.push("A", {1, 1, 1, 1}, 0);
  feed.push("B", {1, 1, 1, 1}, 1);
  feed.push("C", {1, 1, 1, 1}, 2);
  CHECK(feed.lines().size() == 2 && feed.lines().front().text == "B");
}

TEST(the_layout_check_names_overlaps_and_boxes_off_the_screen) {
  const Style st;
  const Frame f = frame_for(640, 360, st);
  const std::vector<Placed> ok{{"a", {0, 0, 10, 10}}, {"b", {10, 0, 10, 10}}};
  CHECK(layout_problems(f, ok).empty());
  const std::vector<Placed> bad{
      {"a", {0, 0, 20, 20}}, {"b", {10, 10, 20, 20}}, {"c", {630, 0, 20, 10}}};
  const auto p = layout_problems(f, bad);
  CHECK(p.size() == 2);
  CHECK(p[0] == "c leaves the screen");
  CHECK(p[1] == "a overlaps b");
}
