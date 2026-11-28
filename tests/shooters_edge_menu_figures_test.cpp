// The player page's figures (games/shooters_edge/menu/figures.hpp): Quake II's figures and skins
// read off its packs (a skin counts only with its icon), the skin setting written by the page's
// rows, and the page refused with a word when the packs hold no figure.
//
// ge:summary Tests the player page's figures and skins read off Quake II's packs and the skin row
// ge:keywords test player-setup figures skins player_figures split_look skin-setting
// ge:skill ui testing
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/figures.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

namespace sm = shooters_edge::menu;
using namespace ge::ui;

namespace {

// A console with the player's name; the skin is bound by the test that reads it.
struct World {
  ge::console::Console con;
};

const Item* item(const Menu& m, std::string_view page, std::string_view id) {
  const Page* p = m.page(page);
  return p ? p->find(id) : nullptr;
}

}  // namespace

TEST(q2_player_page_lists_figures_and_writes_the_skin) {
  // Figures as Quake II's packs hold them: a skin counts only with its icon beside it.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {"players/cyborg/tris.md2", {1}},     {"players/cyborg/oni911.pcx", {1}},
      {"players/cyborg/oni911_i.pcx", {1}}, {"players/female/tris.md2", {1}},
      {"players/female/athena.pcx", {1}},   {"players/female/athena_i.pcx", {1}},
      {"players/male/tris.md2", {1}},       {"players/male/grunt.pcx", {1}},
      {"players/male/grunt_i.pcx", {1}},    {"players/male/viper.pcx", {1}},
      {"players/male/viper_i.pcx", {1}},    {"players/male/noicon.pcx", {1}},
      {"players/empty/tris.md2", {1}},      {"players/empty/skin.pcx", {1}}};
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &pak, &error), error);
  ge::vfs::SearchPath sp;
  CHECK_MSG(ge::vfs::add_archive(&sp, "pak0.pak", pak, "baseq2", &error), error);
  const std::vector<sm::Figure> figures = sm::player_figures(sp);
  CHECK(figures.size() == 3);
  CHECK(figures[0].model == "male" && figures[1].model == "female" && figures[2].model == "cyborg");
  CHECK(figures[0].skins == std::vector<std::string>({"grunt", "viper"}));
  CHECK(sm::split_look("female/athena").second == "athena");
  CHECK(sm::split_look("nothing").first == "male");

  World w;
  std::string skin = "male/viper";
  w.con.cvars().bind(
      {.name = "skin", .fallback = "male/viper", .kind = ge::console::CvarKind::text}, &skin);
  w.con.cvars().add({.name = "name", .fallback = "player"});
  sm::Host host{.console = &w.con, .table = &sm::q2_menu(), .files = &sp};
  Menu m = sm::build_menu(sm::q2_menu());
  sm::refresh(m, host);
  CHECK(item(m, "setup", "model")->choices.size() == 3);
  CHECK(item(m, "setup", "figskin")
            ->choices[static_cast<size_t>(item(m, "setup", "figskin")->choice)] == "viper");
  m.open("setup");
  CHECK(m.focus("model"));
  MenuInput right;
  right.right = true;
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  CHECK(skin == "female/athena");  // a new model at its first skin
}

// Quake II's player page with no figure in the packs: said, and the page does not open.
TEST(q2_player_page_without_figures_does_not_open) {
  ge::vfs::SearchPath sp;
  World w;
  sm::Host host{.console = &w.con, .table = &sm::q2_menu(), .files = &sp};
  Menu m = sm::build_menu(sm::q2_menu());
  sm::refresh(m, host);
  m.open("multi");
  std::vector<MenuEvent> events;
  m.open("setup", &events);
  for (const MenuEvent& e : events) sm::apply_event(e, m, host);
  CHECK(m.top().id == "multi");
  CHECK(!w.con.output().empty() && w.con.output().back() == "No player models found.");
}
