// engine/hud/layout: the HUD as data. Elements stack at their anchors in table order; a hidden
// one closes its gap; an offset, an anchor or a scale set through the element's cvars moves that
// element alone; the corners' rule and the `under` rule keep the centre clear; a fade reaches
// every command an element drew; `seta` archives a place and `hud_reset` puts it back.
//
// ge:summary Tests the HUD layout: stacking, hiding, the hud_ cvars, clearance, fade, hud_reset
// ge:keywords test hud layout element cvars hud_reset seta archive anchor offset scale alpha
// ge:keywords under-corners stack hide
// ge:skill ui testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/layout.hpp"

using namespace ge::hud;
namespace ui = ge::ui;
namespace con = ge::console;

namespace {

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// Three readouts top left, one top right, a big number top centre under the corners, a banner
// kept under it.
struct Three {
  Layout l;
  int a, b, c, right, mid, banner;
  Three() {
    a = l.add({.id = "a", .kind = Kind::readout});
    b = l.add({.id = "b", .kind = Kind::readout});
    c = l.add({.id = "c", .kind = Kind::readout});
    right = l.add({.id = "right", .kind = Kind::readout, .anchor = Anchor::top_right});
    mid = l.add({.id = "mid", .kind = Kind::big, .anchor = Anchor::top, .under_corners = true});
    banner =
        l.add({.id = "banner", .kind = Kind::banner, .anchor = Anchor::center, .under = "mid"});
    for (const int i : {a, b, c, right}) {
      Content& k = l.content(i);
      k.on = true;
      k.value = "A LONG VALUE " + std::to_string(i);
      k.caption = "CAPTION";
    }
  }
};

ui::Rect box_of(const std::vector<Placed>& placed, const std::string& id) {
  for (const Placed& p : placed)
    if (p.name == id) return p.rect;
  return {-1, -1, 0, 0};
}

std::vector<Placed> draw(Three& t, int w = 1280, int h = 720) {
  const Style st;
  ui::DrawList dl;
  dl.begin(w, h);
  return t.l.draw(dl, t.l.frame(w, h, st), st);
}

}  // namespace

TEST(anchor_words_round_trip_and_an_unknown_word_is_none) {
  for (int i = 0; i < 9; ++i) {
    const Anchor a = static_cast<Anchor>(i);
    CHECK(anchor_from(anchor_name(a)) == a);
  }
  CHECK(!anchor_from("middle").has_value());
}

TEST(elements_at_one_anchor_stack_and_a_hidden_one_closes_its_gap) {
  Three t;
  const auto shown = draw(t);
  const ui::Rect a = box_of(shown, "a"), b = box_of(shown, "b"), c = box_of(shown, "c");
  CHECK(b.y > a.bottom() && c.y > b.bottom());
  t.l.content(t.b).on = false;
  const auto hidden = draw(t);
  CHECK(box_of(hidden, "b").w == 0);
  CHECK(near(box_of(hidden, "c").y, b.y));
}

TEST(a_cvar_moves_only_the_element_it_names) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  const auto before = draw(t);
  console.execute("hud_b_x 0.1; hud_right_anchor bottom_right");
  const auto after = draw(t);
  const Frame f = t.l.frame(1280, 720, Style{});
  CHECK(near(box_of(after, "b").x, box_of(before, "b").x + 0.1f * f.safe.w));
  CHECK(near(box_of(after, "b").y, box_of(before, "b").y));
  CHECK(near(box_of(after, "right").bottom(), f.safe.bottom()));
  for (const char* id : {"a", "c"}) {
    CHECK(near(box_of(after, id).x, box_of(before, id).x));
    CHECK(near(box_of(after, id).y, box_of(before, id).y));
  }
  console.execute("hud_c_show 0");
  CHECK(box_of(draw(t), "c").w == 0);
}

TEST(an_unknown_anchor_word_keeps_the_games_anchor) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  const auto before = draw(t);
  console.execute("hud_a_anchor somewhere");
  const auto after = draw(t);
  CHECK(near(box_of(after, "a").x, box_of(before, "a").x));
}

TEST(a_scale_sizes_the_element_and_the_whole_hud_scales_every_one) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  const ui::Rect a0 = box_of(draw(t), "a"), r0 = box_of(draw(t), "right");
  console.execute("hud_a_scale 2");
  const auto one = draw(t);
  CHECK(near(box_of(one, "a").w, a0.w * 2, 0.5f));
  CHECK(near(box_of(one, "right").w, r0.w));
  console.execute("hud_a_scale 1; hud_scale 1.5");
  const auto all = draw(t);
  CHECK(near(box_of(all, "a").w, a0.w * 1.5f, 0.5f));
  CHECK(near(box_of(all, "right").w, r0.w * 1.5f, 0.5f));
}

TEST(the_centre_drops_under_the_corners_only_where_it_meets_them) {
  Three t;
  Content& m = t.l.content(t.mid);
  m.on = true;
  m.value = "1.23";
  m.unit = "S";
  // Wide: the corners' columns leave the top centre clear.
  const auto wide = draw(t, 1280, 720);
  const Frame fw = t.l.frame(1280, 720, Style{});
  CHECK(near(box_of(wide, "mid").y, fw.safe.y));
  // Narrow: the number would sit on the columns, so it goes under the taller one.
  const auto narrow = draw(t, 390, 844);
  CHECK(box_of(narrow, "mid").y > box_of(narrow, "c").bottom());
  CHECK(layout_problems(t.l.frame(390, 844, Style{}), narrow).empty());
}

TEST(an_element_stays_under_the_one_it_names) {
  Three t;
  Content& m = t.l.content(t.mid);
  m.on = true;
  m.value = "1.23";
  Content& b = t.l.content(t.banner);
  b.on = true;
  b.value = "STUCK";
  b.caption = "PRESS R";
  con::Console console;
  add_hud_cvars(console, t.l);
  console.execute("hud_banner_y -0.5");  // up into the top centre
  const auto placed = draw(t);
  CHECK(box_of(placed, "banner").y >= box_of(placed, "mid").bottom());
  t.l.content(t.mid).on = false;
  const auto alone = draw(t);
  CHECK(box_of(alone, "banner").y < box_of(placed, "banner").y);
}

TEST(an_alpha_fades_every_command_the_element_drew) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  console.execute("hud_a_alpha 0.5");
  for (const int i : {t.b, t.c, t.right}) t.l.content(i).on = false;
  const Style st;
  ui::DrawList dl;
  dl.begin(1280, 720);
  t.l.draw(dl, t.l.frame(1280, 720, st), st);
  CHECK(dl.size() > 0);
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::text) CHECK(c.text.color.a <= 0.5f);
}

TEST(seta_archives_a_place_and_hud_reset_puts_it_back) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  CHECK(console.save_text().find("hud_a_x") == std::string::npos);
  console.execute("seta hud_a_x 0.25; seta hud_b_y 0.1");
  CHECK(console.save_text().find("seta hud_a_x \"0.25\"") != std::string::npos);
  CHECK(t.l[static_cast<size_t>(t.a)].place.x == 0.25);
  console.execute("hud_reset a");
  CHECK(t.l[static_cast<size_t>(t.a)].place.x == 0);
  CHECK(console.save_text().find("hud_a_x") == std::string::npos);
  CHECK(t.l[static_cast<size_t>(t.b)].place.y == 0.1);
  console.execute("hud_reset");
  CHECK(t.l[static_cast<size_t>(t.b)].place.y == 0);
  console.clear_problems();
  console.execute("hud_reset nothing");
  CHECK(!console.problems().empty());
}

TEST(a_saved_layout_comes_back_the_same_in_a_fresh_start) {
  Three t;
  con::Console console;
  add_hud_cvars(console, t.l);
  console.execute(
      "seta hud_b_x 0.1; seta hud_right_anchor bottom; seta hud_a_scale 1.5; seta hud_c_show 0; "
      "seta hud_alpha 0.8");
  const auto before = draw(t);
  const std::string saved = console.save_text();
  Three fresh;
  con::Console again;
  add_hud_cvars(again, fresh.l);
  again.execute(saved);
  const auto after = draw(fresh);
  CHECK(before.size() == after.size());
  for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
    CHECK(before[i].name == after[i].name);
    const ui::Rect a = before[i].rect, b = after[i].rect;
    CHECK(a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h);
  }
  CHECK(fresh.l.alpha == 0.8);
}
