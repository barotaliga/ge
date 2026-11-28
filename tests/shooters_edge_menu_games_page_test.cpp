// SELECT GAME (games/shooters_edge/menu/games_page.hpp) in the classic look: both games and all
// their mission packs on Quake II's 240-line screen close up to end above the foot's line, and a
// list no spacing can fit is a problem the page says.
//
// ge:summary Tests SELECT GAME's classic list: every row above the foot, an overlong list said
// ge:keywords test select-game games-page classic foot mission-packs layout problems
// ge:skill ui testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/games_page.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace sm = shooters_edge::menu;
using namespace ge::ui;

// Quake II's SELECT GAME with both games and all their packs: the rows close up to end above
// the foot's line, and a list no spacing can fit is said.
TEST(classic_games_page_ends_above_its_foot) {
  sm::ClassicArt art;
  art.glyphs = make_image(128, 128, hex(0xffffff));
  art.pictures.emplace("m_main_game", make_image(99, 32, hex(0x2c2c2c)));
  CHECK(art.plates());
  std::vector<sm::GameRow> rows = {{.id = "q1", .install = "/a", .edition = "Registered"},
                                   {.id = "q1", .pack = "hipnotic", .pack_name = "Scourge"},
                                   {.id = "q1", .pack = "rogue", .pack_name = "Dissolution"},
                                   {.id = "q2", .install = "/b", .edition = "Full game"},
                                   {.id = "q2", .pack = "xatrix", .pack_name = "The Reckoning"},
                                   {.id = "q2", .pack = "rogue", .pack_name = "Ground Zero"}};
  const auto draw = [&](sm::GamesPage& page) {
    DrawList dl;
    dl.begin(1280, 720);
    page.draw_classic(dl, art, 1280, 720, 0);
    std::vector<float> ys;  // the glyphs' rows, the foot's the lowest
    for (const auto& c : dl.cmds())
      if (c.box.image == &art.glyphs) ys.push_back(c.box.rect.y);
    return ys;
  };
  sm::GamesPage page;
  page.set_games(rows);
  const std::vector<float> ys = draw(page);
  CHECK(page.problems().empty());
  const float foot = *std::max_element(ys.begin(), ys.end());
  for (const float y : ys) CHECK_MSG(y == foot || y + 8 * 3 <= foot, std::to_string(y));
  for (int i = 0; i < 12; i++) rows.push_back(rows[1]);
  page.set_games(rows);
  draw(page);
  CHECK(!page.problems().empty());
}
