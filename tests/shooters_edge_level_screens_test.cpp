// Tests: the campaign's screens (games/shooters_edge/menu/level_screens.hpp) over the menu's
// fixture pictures (classic_art.hpp's `fixture_files`, painted in code in the pack's formats):
// the intermission's title, labels and big figures where the layout puts them; the finale's text
// typed a character at a time, each line centred by its whole width; the solo scorebar's rows,
// the level's statistics and the big figures' fields;
// and every screen drawn in the game's own modern look when the packs carry no pictures.
//
// ge:summary Tests: the intermission, the level's statistics, the solo scorebar, the typed finale
// ge:keywords test campaign intermission finale typed-text solo-scorebar level-stats big-figures
// ge:keywords fixture field_names padded
// ge:skill ui testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "ui/draw.hpp"

namespace {

namespace mn = shooters_edge::menu;
namespace ui = ge::ui;

mn::ClassicArt fixture_art() {
  const auto files = mn::fixture_files();
  const auto find = [&files](std::string_view p,
                             std::vector<uint8_t>&) -> std::span<const uint8_t> {
    const auto it = files.find(p);
    return it == files.end() ? std::span<const uint8_t>{} : std::span<const uint8_t>(it->second);
  };
  mn::ClassicArt art;
  std::string error;
  CHECK_MSG(mn::load_classic_art(find, &art, &error), error);
  return art;
}

// The boxes drawn with `img`, in order.
std::vector<ui::Rect> boxes_of(const ui::DrawList& dl, const ui::Image* img) {
  std::vector<ui::Rect> out;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && c.box.image == img && img != nullptr)
      out.push_back(c.box.rect);
  return out;
}

int texts(const ui::DrawList& dl) {
  int n = 0;
  for (const ui::Cmd& c : dl.cmds()) n += c.kind == ui::CmdKind::text ? 1 : 0;
  return n;
}

}  // namespace

TEST(numbers_are_the_big_figures_names) {
  CHECK(mn::figure_names(7) == std::vector<std::string>{"num_7"});
  CHECK(mn::figure_names(5, 2) == (std::vector<std::string>{"num_0", "num_5"}));
  CHECK(mn::figure_names(-12) == (std::vector<std::string>{"num_minus", "num_1", "num_2"}));
  CHECK(mn::figure_names(123, 2) == (std::vector<std::string>{"num_1", "num_2", "num_3"}));
}

TEST(the_intermission_shows_the_title_the_labels_and_the_figures) {
  const mn::ClassicArt art = fixture_art();
  for (const char* name : {"complete", "inter", "num_0", "num_colon", "num_slash"})
    CHECK_MSG(art.find(name) != nullptr, name);
  ui::DrawList dl;
  dl.begin(640, 400);  // scale 2, the screen at (0, 0)
  mn::draw_intermission(dl, art, 640, 400,
                        {.minutes = 1,
                         .seconds = 5,
                         .secrets = 1,
                         .total_secrets = 2,
                         .kills = 10,
                         .total_kills = 12});
  const std::vector<ui::Rect> complete = boxes_of(dl, art.find("complete"));
  CHECK(complete.size() == 1);
  CHECK(complete[0].x == 128 && complete[0].y == 48 && complete[0].w == 384);  // (64, 24)
  const std::vector<ui::Rect> inter = boxes_of(dl, art.find("inter"));
  CHECK(inter.size() == 1 && inter[0].x == 0 && inter[0].y == 112);  // (0, 56)
  // The minutes right-aligned in a field of three from x 160 on y 64, the colon at 234, the
  // seconds' digits at 246 and 266; 1/2 on y 104 and 10/12 on y 144, the slash at 232 and the
  // total's field from 240.
  const std::vector<ui::Rect> colon = boxes_of(dl, art.find("num_colon"));
  CHECK(colon.size() == 1 && colon[0].x == 2 * 234 && colon[0].y == 2 * 64);
  const std::vector<ui::Rect> five = boxes_of(dl, art.find("num_5"));
  CHECK(five.size() == 1 && five[0].x == 2 * 266 && five[0].y == 2 * 64);
  const std::vector<ui::Rect> slash = boxes_of(dl, art.find("num_slash"));
  CHECK(slash.size() == 2);
  CHECK(slash[0].x == 2 * 232 && slash[0].y == 2 * 104);
  CHECK(slash[1].x == 2 * 232 && slash[1].y == 2 * 144);
  const std::vector<ui::Rect> two = boxes_of(dl, art.find("num_2"));
  CHECK(two.size() == 2 && two[0].x == 2 * 288 && two[1].x == 2 * 288);  // 2 and 12's units
  CHECK(boxes_of(dl, art.find("num_1")).size() == 4);                    // 1:05, 1/2, 10/12 twice
  CHECK(boxes_of(dl, art.find("num_0")).size() == 2);
  CHECK(texts(dl) == 0);
}

TEST(the_finale_is_typed_and_each_line_centred_by_its_whole_width) {
  const mn::ClassicArt art = fixture_art();
  const auto glyphs = [&](size_t typed) {
    ui::DrawList dl;
    dl.begin(320, 200);
    mn::draw_level_text(dl, art, 320, 200, "AB\nCDEF", typed);
    return boxes_of(dl, &art.glyphs);
  };
  CHECK(glyphs(0).empty());
  CHECK(glyphs(2).size() == 2);
  CHECK(glyphs(3).size() == 2);  // the line's break
  const std::vector<ui::Rect> four = glyphs(4);
  CHECK(four.size() == 3);
  CHECK(four[0].x == 152);  // "AB" centred: (320 - 16) / 2
  CHECK(four[2].x == 144);  // "CDEF" centred by its whole width while one letter shows
  CHECK(four[2].y == four[0].y + 8);
  CHECK(glyphs(100).size() == 6);
}

TEST(the_solo_scorebar_takes_the_status_bars_place) {
  const mn::ClassicArt art = fixture_art();
  CHECK(art.find("scorebar") != nullptr);
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::draw_solo_scorebar(
      dl, art, 320, 200, "the Grey Gate",
      {.minutes = 2, .seconds = 7, .secrets = 0, .total_secrets = 3, .kills = 4, .total_kills = 9});
  const std::vector<ui::Rect> bar = boxes_of(dl, art.find("scorebar"));
  CHECK(bar.size() == 1 && bar[0].x == 0 && bar[0].y == 176);
  // "Monsters:  4 /  9" (12 glyphs), "Secrets :  0 /  3" (11), "Time :  2:07" (9) and the
  // title (11) without their spaces.
  const std::vector<ui::Rect> g = boxes_of(dl, &art.glyphs);
  CHECK_MSG(g.size() == 43, std::to_string(g.size()));
  for (const ui::Rect& r : g) CHECK(r.y >= 180 && r.y + r.h <= 196);
  CHECK(g[0].x == 8 && g[0].y == 180);  // M
  // The title centred on x 232 on the bar's second row: 13 characters from 232 - 52.
  CHECK(g[32].x == 180 && g[32].y == 188);
}

TEST(the_level_stats_are_the_intermissions_figures_under_the_maps_title) {
  const mn::ClassicArt art = fixture_art();
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::draw_level_stats(dl, art, 320, 200, "AB",
                       {.minutes = 0, .seconds = 9, .secrets = 1, .total_secrets = 1});
  CHECK(boxes_of(dl, art.find("complete")).empty());
  CHECK(boxes_of(dl, art.find("inter")).size() == 1);
  const std::vector<ui::Rect> g = boxes_of(dl, &art.glyphs);
  CHECK(g.size() == 2 && g[0].x == 152 && g[0].y == 32);
  const std::vector<ui::Rect> nine = boxes_of(dl, art.find("num_9"));
  CHECK(nine.size() == 1 && nine[0].x == 266 && nine[0].y == 64);
}

TEST(a_field_keeps_its_last_digits_and_is_right_aligned) {
  int skip = 0;
  CHECK(mn::field_names(7, 3, &skip) == std::vector<std::string>{"num_7"} && skip == 2);
  CHECK(mn::field_names(-5, 3, &skip) == (std::vector<std::string>{"num_minus", "num_5"}) &&
        skip == 1);
  CHECK(mn::field_names(1234, 3, &skip) == (std::vector<std::string>{"num_2", "num_3", "num_4"}) &&
        skip == 0);
  CHECK(mn::padded(5, 3) == "  5");
  CHECK(mn::padded(1000, 3, 3) == "100");
  CHECK(mn::padded(-100, 3, 3) == "-10");
}

TEST(without_the_packs_pictures_the_screens_are_drawn_in_the_engines_type) {
  const mn::ClassicArt none;
  ui::DrawList dl;
  dl.begin(640, 400);
  // The game's own look: the heading, then the three tiles' labels and values.
  mn::draw_intermission(dl, none, 640, 400, {.minutes = 0, .seconds = 9});
  CHECK(texts(dl) == 1 + 3 + 3);
  dl.begin(640, 400);
  mn::draw_level_text(dl, none, 640, 400, "AB", 2);
  CHECK(texts(dl) == 1);
  dl.begin(640, 400);
  mn::draw_solo_scorebar(dl, none, 640, 400, "x", {});
  CHECK(texts(dl) == 1 + 3 + 3);  // the title, the three labels and values
}
