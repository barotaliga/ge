// Tests: the multiplayer scoreboard (games/shooters_edge/menu/scoreboard.hpp) over the menu's
// fixture pictures (classic_art.hpp's `fixture_files`): the order (frags, ties in slot order,
// spectators last), the teams summed and sorted, the colour's palette index, the players' board's
// picture, header, rule, rows and columns where porting/shooters-edge/scoreboard.md puts them, the
// brackets round the player's own frags, the large game's closer rows kept once on, the teams'
// board before the players', the scorebar's clock, and every board in the engine's type when the
// packs carry no pictures.
//
// ge:summary Tests: the scoreboard's order, teams, colours, layout, large game and scorebar
// ge:keywords test scoreboard score_order team_rows colour_index ranking header rule frags
// ge:keywords brackets spectator large-game teamplay scorebar fixture
// ge:skill ui testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/scoreboard.hpp"
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

// The glyph boxes drawn, with the glyph each one is (its cell in the sheet).
struct Glyph {
  ui::Rect at;
  int code;
};
std::vector<Glyph> glyphs_of(const ui::DrawList& dl, const mn::ClassicArt& art) {
  std::vector<Glyph> out;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && c.box.image == &art.glyphs)
      out.push_back(
          {c.box.rect, static_cast<int>(c.box.uv.y / 8) * 16 + static_cast<int>(c.box.uv.x / 8)});
  return out;
}
// The glyphs on row `y`, left to right, as text (a code below 32 as '#'; the second set's
// blank left out).
std::string row_text(const std::vector<Glyph>& g, float y) {
  std::vector<Glyph> row;
  for (const Glyph& k : g)
    if (k.at.y == y && k.code != (' ' | 128)) row.push_back(k);  // the second set's blank
  std::stable_sort(row.begin(), row.end(),
                   [](const Glyph& a, const Glyph& b) { return a.at.x < b.at.x; });
  std::string s;
  for (const Glyph& k : row) s += k.code < 32 ? '#' : static_cast<char>(k.code & 127);
  return s;
}
const Glyph* at(const std::vector<Glyph>& g, float x, float y) {
  for (const Glyph& k : g)
    if (k.at.x == x && k.at.y == y) return &k;
  return nullptr;
}

mn::Scoreboard board() {
  mn::Scoreboard b;
  b.rows = {{.name = "anna", .frags = 3, .ping = 25, .minutes = 4, .top = 4, .bottom = 13},
            {.name = "watcher", .frags = 50, .ping = 40, .spectator = true},
            {.name = "me", .frags = 12, .ping = 12, .loss = 30, .minutes = 9, .self = true},
            {.name = "bo", .frags = 12, .ping = 1200, .minutes = 2}};
  b.minutes = 4;
  b.seconds = 12;
  return b;
}

}  // namespace

TEST(the_order_is_frags_then_slot_with_spectators_last) {
  CHECK(mn::score_order(board()) == (std::vector<size_t>{2, 3, 0, 1}));
}

TEST(a_colour_fills_with_its_ramps_middle_clamped) {
  CHECK(mn::colour_index(0) == 8);
  CHECK(mn::colour_index(4) == 72);
  CHECK(mn::colour_index(13) == 216);
  CHECK(mn::colour_index(15) == 216);
  CHECK(mn::colour_index(-2) == 8);
}

TEST(teams_are_summed_and_sorted_by_frags) {
  mn::Scoreboard b;
  b.teamplay = true;
  b.rows = {{.name = "a", .frags = 5, .ping = 20, .team = "blue"},
            {.name = "b", .frags = 9, .ping = 60, .team = "red", .self = true},
            {.name = "c", .frags = 3, .ping = 40, .team = "blue"},
            {.name = "d", .frags = 99, .team = "red", .spectator = true},
            {.name = "e", .frags = 1, .ping = 30}};
  const std::vector<mn::TeamRow> t = mn::team_rows(b);
  CHECK(t.size() == 2);
  CHECK(t[0].team == "red" && t[0].frags == 9 && t[0].players == 1 && t[0].self);
  CHECK(t[1].team == "blue" && t[1].frags == 8 && t[1].players == 2 && !t[1].self);
  CHECK(t[1].low == 20 && t[1].mean == 30 && t[1].high == 40);
}

TEST(the_players_board_is_laid_out_as_the_notes_say) {
  const mn::ClassicArt art = fixture_art();
  CHECK(art.find("ranking") != nullptr);
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::BoardView view;
  mn::draw_scoreboard(dl, art, 320, 200, board(), view);
  // The picture centred on the 320 column at the top.
  int pictures = 0;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && c.box.image == art.find("ranking")) {
      pictures++;
      CHECK(c.box.rect.y == 0 && c.box.rect.x == 160 - c.box.rect.w / 2);
    }
  CHECK(pictures == 1);
  const std::vector<Glyph> g = glyphs_of(dl, art);
  CHECK(row_text(g, 24) == "pingpltimefragsname");
  CHECK(at(g, 16, 24) != nullptr && at(g, 16, 24)->code == 'p');
  CHECK(at(g, 16, 32)->code == 29 && at(g, 24, 32)->code == 30 && at(g, 40, 32)->code == 31);
  CHECK(row_text(g, 32).size() == 4 + 2 + 4 + 5 + 16);
  // The first row (y 40) is the player: the ping, the loss in the second set (over 25%), the
  // minutes, the frags bracketed in the box, the name.
  CHECK(row_text(g, 40) ==
        "12"
        "30"
        "9"
        "#12#"
        "me");
  CHECK(at(g, 32 + 16 + 8, 40)->code == ('3' | 128));
  CHECK(at(g, 16 + 104, 40)->code == 16 && at(g, 16 + 136, 40)->code == 17);
  CHECK(at(g, 16 + 112, 40) == nullptr);  // " 12": the space is not drawn
  CHECK(at(g, 16 + 120, 40)->code == '1' && at(g, 16 + 152, 40)->code == 'm');
  // A tie in slot order, the ping over 999 shown as 999; the spectator last, his row in words.
  CHECK(row_text(g, 50) ==
        "99902"
        "12"
        "bo");
  CHECK(row_text(g, 60) ==
        "2504"
        "3"
        "anna");
  // The text over the loss's last cells, as the original draws it.
  CHECK(row_text(g, 70) == "40(0spectator)watcher");
  CHECK(at(g, 16 + 40, 70)->code == '(');
  // The colour box of anna: her top colour over her bottom colour, 40 wide.
  int fills = 0;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::box && c.box.image == nullptr && c.box.rect.x == 120 &&
        (c.box.rect.y == 60 || c.box.rect.y == 64)) {
      fills++;
      CHECK(c.box.rect.w == 40 && c.box.rect.h == 4);
    }
  CHECK(fills == 2);
  CHECK(!view.large);
}

TEST(a_full_board_turns_to_the_large_game_and_stays) {
  const mn::ClassicArt art = fixture_art();
  mn::Scoreboard b;
  for (int i = 0; i < 20; i++) b.rows.push_back({.name = "p" + std::to_string(i), .frags = 20 - i});
  mn::BoardView view;
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::draw_scoreboard(dl, art, 320, 200, b, view);
  CHECK(view.large);  // 16 rows reached y 190
  CHECK(row_text(glyphs_of(dl, art), 190).find("p15") != std::string::npos);
  dl.begin(320, 200);
  mn::draw_scoreboard(dl, art, 320, 200, b, view);
  const std::vector<Glyph> g = glyphs_of(dl, art);
  CHECK(row_text(g, 48).find("p1") != std::string::npos);  // 8 px apart now
  CHECK(row_text(g, 184).find("p18") != std::string::npos);
  b.rows.resize(2);
  dl.begin(320, 200);
  mn::draw_scoreboard(dl, art, 320, 200, b, view);
  CHECK(view.large);
}

TEST(with_teamplay_the_teams_come_first_then_the_players_with_their_team) {
  const mn::ClassicArt art = fixture_art();
  mn::Scoreboard b;
  b.teamplay = true;
  b.rows = {{.name = "a", .frags = 5, .ping = 20, .team = "blue"},
            {.name = "me", .frags = 9, .ping = 60, .team = "red", .self = true}};
  mn::BoardView view;
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::draw_team_scoreboard(dl, art, 320, 200, b, view);
  const std::vector<Glyph> g = glyphs_of(dl, art);
  CHECK(row_text(g, 24) == "low/avg/highteamtotalplayers");
  CHECK(at(g, 36, 24)->code == 'l');
  CHECK(row_text(g, 40) == "60/60/60#red#91");
  CHECK(at(g, 36 + 96, 40)->code == 16 && at(g, 36 + 104, 40)->code == 'r');
  CHECK(row_text(g, 48) == "20/20/20blue51");
  // A blank line, then the players' header at y 64, its rows from 80, the team at +152.
  CHECK(row_text(g, 64) == "pingpltimefragsteamname");
  CHECK(at(g, 4, 64)->code == 'p');
  CHECK(at(g, 4 + 152, 80)->code == 'r' && at(g, 4 + 192, 80)->code == 'm');
}

TEST(the_scorebar_shows_the_maps_clock) {
  const mn::ClassicArt art = fixture_art();
  ui::DrawList dl;
  dl.begin(320, 200);
  mn::draw_scorebar(dl, art, 320, 200, board());
  const std::vector<Glyph> g = glyphs_of(dl, art);
  CHECK(row_text(g, 180) == "Time:4:12");
  CHECK(at(g, 184, 180)->code == 'T');
}

TEST(without_the_packs_pictures_the_board_is_drawn_in_the_engines_type) {
  const mn::ClassicArt none;
  ui::DrawList dl;
  dl.begin(640, 400);
  mn::BoardView view;
  mn::draw_scoreboard(dl, none, 640, 400, board(), view);
  int texts = 0;
  for (const ui::Cmd& c : dl.cmds()) texts += c.kind == ui::CmdKind::text ? 1 : 0;
  CHECK(texts > 10);
  CHECK(glyphs_of(dl, none).empty());
}
