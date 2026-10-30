// engine/hud/pictures and the layout's picture kinds: a picture is its pixels times the unit
// over its density and samples nearest only at a whole scale; a digit field is right-aligned in
// its cells, the alternate sheet drawn when asked; a strip keeps an empty slot's place and sizes
// a cell by its picture when the strip gives none; a child hangs on its parent's point, moves in
// the parent's units and takes its scale; snap floors the unit and fit keeps a wide element on
// the screen; a ring fills a second lap past 1; a glyph sheet maps the figures to its own cells.
//
// ge:summary Tests the HUD's pictures: units, nearest, digits, strips, parents, snap, fit, rings
// ge:keywords test hud pictures picture digits strip cell parent attach snap fit ring nearest
// ge:keywords glyph-sheet density
// ge:skill ui testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/layout.hpp"
#include "hud/pictures.hpp"

using namespace ge::hud;
namespace ui = ge::ui;
namespace con = ge::console;

namespace {

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

ui::Image solid_image(int w, int h) { return ui::make_image(w, h, {1, 1, 1, 1}); }

const ui::Box* first_box(const ui::DrawList& dl) {
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && c.box.image != nullptr) return &c.box;
  return nullptr;
}

ui::Rect box_of(const std::vector<Placed>& placed, const std::string& id) {
  for (const Placed& p : placed)
    if (p.name == id) return p.rect;
  return {-1, -1, 0, 0};
}

}  // namespace

TEST(a_picture_is_its_pixels_times_the_unit_over_its_density) {
  const ui::Image img = solid_image(96, 64);
  CHECK(near(measure(Picture{&img, {1, 1, 1, 1}, 4}, 3).w, 72));
  CHECK(near(measure(Picture{&img, {1, 1, 1, 1}, 4}, 3).h, 48));
  ui::DrawList dl;
  dl.begin(200, 200);
  draw(dl, Picture{&img, {1, 1, 1, 1}, 1}, {0, 0, 288, 192}, 3);
  CHECK(first_box(dl) != nullptr && first_box(dl)->nearest);
  dl.begin(200, 200);
  draw(dl, Picture{&img, {1, 1, 1, 1}, 4}, {0, 0, 72, 48}, 3);  // 0.75 px a pixel: blended
  CHECK(first_box(dl) != nullptr && !first_box(dl)->nearest);
}

TEST(nearest_sampling_keeps_a_pixel_edge_hard) {
  ui::Image src = ui::make_image(2, 1, {0, 0, 0, 1});
  src.rgba[4] = src.rgba[5] = src.rgba[6] = 255;  // the right pixel white
  const ui::Color mid_nearest = ui::sample_nearest(src, 0.99f, 0.5f);
  const ui::Color mid_blend = ui::sample(src, 0.99f, 0.5f);
  CHECK(mid_nearest.r == 0);
  CHECK(mid_blend.r > 0.4f);
  CHECK(ui::sample_nearest(src, 1.01f, 0.5f).r == 1);
}

TEST(a_digit_field_is_right_aligned_and_draws_the_alternate_sheet_when_asked) {
  std::vector<ui::Image> glyphs(22, solid_image(24, 24));
  DigitSheet sheet;
  for (size_t k = 0; k < 11; ++k) {
    sheet.glyph[k] = &glyphs[k];
    sheet.alt[k] = &glyphs[11 + k];
  }
  const Digits d{&sheet, "7", 3, false, {1, 1, 1, 1}};
  CHECK(near(measure(d, 2).w, 3 * 24 * 2));
  ui::DrawList dl;
  dl.begin(400, 100);
  draw(dl, d, {10, 0, 144, 48}, 2);
  CHECK(dl.size() == 1);
  CHECK(near(dl.cmds()[0].box.rect.x, 10 + 2 * 48));  // the last cell
  CHECK(dl.cmds()[0].box.image == &glyphs[7]);
  dl.begin(400, 100);
  draw(dl, Digits{&sheet, "-5", 3, true, {1, 1, 1, 1}}, {0, 0, 144, 48}, 2);
  CHECK(dl.size() == 2 && dl.cmds()[0].box.image == &glyphs[16]);  // the alternate 5
  CHECK(dl.cmds()[1].box.image == &glyphs[21]);                    // the alternate minus
}

TEST(a_strip_keeps_an_empty_slots_place_and_sizes_a_cell_by_its_picture) {
  const ui::Image narrow = solid_image(24, 16), wide = solid_image(48, 16);
  std::vector<Cell> cells(3);
  cells[0].image = &narrow;
  cells[1].image = &narrow;
  cells[1].show = false;
  cells[2].image = &wide;
  StripShape shape;
  shape.cell_w = 0;
  const Strip strip{&cells};
  CHECK(near(measure(shape, strip, 2).w, (24 + 24 + 48) * 2));
  ui::DrawList dl;
  dl.begin(400, 100);
  draw(dl, shape, strip, {0, 0, 192, 32}, 2);
  CHECK(dl.size() == 2);
  CHECK(near(dl.cmds()[1].box.rect.x, 96));  // after the empty slot's 48 px
  CHECK(near(dl.cmds()[1].box.rect.w, 96));
}

TEST(a_child_hangs_on_its_parents_point_in_its_units_and_follows_a_player_cvar) {
  const ui::Image bar = solid_image(320, 24), icon = solid_image(24, 24);
  Layout l;
  ElementSpec spec{.id = "bar", .kind = Kind::picture, .anchor = Anchor::bottom};
  spec.unit = 3, spec.snap = true;
  const int b = l.add(spec);
  ElementSpec kid{.id = "icon", .kind = Kind::picture, .x = 112};
  kid.unit = 3, kid.snap = true, kid.parent = "bar";
  const int k = l.add(kid);
  l.content(b).on = true;
  l.content(b).image = &bar;
  l.content(k).on = true;
  l.content(k).image = &icon;
  const Style st;
  ui::DrawList dl;
  dl.begin(1280, 720);
  const Frame f = l.frame(1280, 720, st);
  auto placed = l.draw(dl, f, st);
  const ui::Rect rb = box_of(placed, "bar"), ri = box_of(placed, "icon");
  CHECK(near(rb.w, 960));  // 3 px an art pixel at 720 px
  CHECK(near(ri.x, rb.x + 112 * 3));
  CHECK(near(ri.y, rb.y));
  CHECK(near(ri.w, 72));
  con::Console console;
  add_hud_cvars(console, l);
  console.execute("hud_icon_x 0; hud_icon_anchor top_right; hud_icon_attach bottom_right");
  dl.begin(1280, 720);
  placed = l.draw(dl, f, st);
  CHECK(near(box_of(placed, "icon").right(), rb.right()));
  CHECK(near(box_of(placed, "icon").bottom(), rb.y));
  console.execute("hud_icon_parent \"\"; hud_icon_anchor top_left");  // stands alone
  dl.begin(1280, 720);
  placed = l.draw(dl, f, st);
  CHECK(near(box_of(placed, "icon").x, f.safe.x));
  console.execute("hud_reset icon");
  dl.begin(1280, 720);
  CHECK(near(box_of(l.draw(dl, f, st), "icon").x, rb.x + 112 * 3));
}

TEST(snap_floors_the_unit_and_fit_keeps_a_wide_element_on_the_screen) {
  const ui::Image bar = solid_image(320, 24);
  Layout l;
  ElementSpec spec{.id = "bar", .kind = Kind::picture, .anchor = Anchor::bottom};
  spec.unit = 3, spec.snap = true, spec.fit = 1;
  const int b = l.add(spec);
  l.content(b).on = true;
  l.content(b).image = &bar;
  const Style st;
  ui::DrawList dl;
  dl.begin(1920, 1080);
  l.draw(dl, l.frame(1920, 1080, st), st);
  CHECK(near(l[static_cast<size_t>(b)].drawn_unit, 4));  // 4.5 floored
  dl.begin(390, 844);
  const Frame narrow = l.frame(390, 844, st);
  const auto placed = l.draw(dl, narrow, st);
  CHECK(box_of(placed, "bar").w <= narrow.safe.w);
  CHECK(near(l[static_cast<size_t>(b)].drawn_unit, 1));
}

TEST(a_ring_fills_a_second_lap_past_one) {
  const Frame f = frame_for(1280, 720, Style{});
  ui::DrawList dl;
  dl.begin(1280, 720);
  draw(dl, f, RingShape{}, Ring{1.5f}, {0, 0, 140, 140});
  CHECK(dl.size() == 3);  // the track, the first lap, the second
  dl.begin(1280, 720);
  draw(dl, f, RingShape{}, Ring{0.5f}, {0, 0, 140, 140});
  CHECK(dl.size() == 2);
}

TEST(a_glyph_sheet_draws_a_cell_a_character_and_its_figures_from_their_cells) {
  const ui::Image sheet = solid_image(128, 128);
  const Face face = Face::glyph_sheet(sheet, 18);
  CHECK(near(text_width(face, 24, "123"), 72));
  ui::DrawList dl;
  dl.begin(200, 100);
  draw_text(dl, face, 24, 0, 30, "1A", {1, 1, 1, 1});
  CHECK(dl.size() == 2);
  CHECK(near(dl.cmds()[0].box.uv.x, (19 % 16) * 8.0f) && near(dl.cmds()[0].box.uv.y, 8));
  CHECK(near(dl.cmds()[1].box.uv.x, ('A' % 16) * 8.0f));
  CHECK(dl.cmds()[0].box.nearest);  // 3 px a sheet pixel
}
