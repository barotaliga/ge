// Shooter's Edge's main menu (games/shooters_edge/menu): the classic pages and their rows; a row
// dimmed while the console knows nothing it needs and lit once it does; a setting read from its
// cvar and written by a console line; NEW GAME's START running the rules on the server and the
// map; ESCAPE opening the menu and going back out of it; the classic pictures read from a pack's
// files and refused without them; the classic layout's rows on the 320 x 200 screen; the modern
// and the classic looks drawing every page clean.
//
// ge:summary Tests the main menu: pages, dimming by the console, cvars, START, ESCAPE, both looks
// ge:keywords test menu main-menu dimmed cvar console new-game start escape classic modern layout
// ge:skill ui testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "ui/raster.hpp"

namespace sm = shooters_edge::menu;
using namespace ge::ui;

namespace {

// A console like the window's: the commands the menu's rows need, the mouse's cvars.
struct World {
  ge::console::Console con;
  std::vector<std::string> ran;
  double sensitivity = 0.066;
  bool invert = false;
  std::string hud = "classic";
  World() {
    for (const char* c : {"map", "sv", "quit", "toggleconsole"})
      con.add_command(c, "", [this, c](ge::console::Console&, const ge::console::Args& a) {
        ran.push_back(std::string(c) + (a.size() > 1 ? " " + a.rest(0) : ""));
      });
    con.cvars().bind({.name = "sensitivity", .fallback = "0.066"}, &sensitivity);
    con.cvars().bind({.name = "m_invert", .fallback = "0", .kind = ge::console::CvarKind::boolean},
                     &invert);
    con.cvars().bind(
        {.name = "hud_style", .fallback = "classic", .kind = ge::console::CvarKind::text}, &hud);
  }
};

const Item* item(const Menu& m, std::string_view page, std::string_view id) {
  const Page* p = m.page(page);
  return p ? p->find(id) : nullptr;
}

std::vector<MenuEvent> press(Menu& m, std::string_view id) {
  CHECK_MSG(m.focus(id), std::string(id) + " is not a stop on page " + m.top().id);
  MenuInput in;
  in.accept = true;
  return m.input(in);
}

sm::FindFile fixture_find(const std::map<std::string, std::vector<uint8_t>, std::less<>>& files) {
  return [&files](std::string_view path, std::vector<uint8_t>&) -> std::span<const uint8_t> {
    const auto it = files.find(path);
    if (it == files.end()) return {};
    return it->second;
  };
}

}  // namespace

TEST(menu_has_the_classic_pages_and_rows) {
  const Menu m = sm::build_menu();
  const Page& root = *m.page("main");
  CHECK(root.items.size() == 5);
  const char* rows[] = {"single", "multi", "options", "help", "quit"};
  for (size_t i = 0; i < 5; i++) CHECK(root.items[i].id == rows[i]);
  for (const char* p : {"single", "multi", "host", "setup", "options", "video", "help", "quit"})
    CHECK_MSG(m.page(p) != nullptr, std::string("no page ") + p);
  for (const Page* p : {m.page("single"), m.page("multi")})
    CHECK(p->items.size() == 4);  // three rows, as the classic strips, and BACK
  for (const sm::Entry& e : sm::entries())
    CHECK_MSG(item(m, e.page, e.id) != nullptr, std::string(e.page) + "." + std::string(e.id));
}

TEST(rows_dim_until_the_console_knows_what_they_need) {
  World w;
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  sm::refresh(m, host);
  CHECK(!item(m, "single", "new")->disabled);
  CHECK(item(m, "single", "load")->disabled);  // no `load` yet
  CHECK(item(m, "multi", "join")->disabled);   // no `connect` yet
  CHECK(item(m, "options", "volume")->disabled);
  CHECK(!item(m, "options", "sensitivity")->disabled);
  CHECK(!item(m, "options", "m_invert")->disabled);
  CHECK(item(m, "options", "classic")->disabled);  // no ce_ switches registered
  // The day a `load` command lands, LOAD lights up by itself.
  w.con.add_command("load", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::refresh(m, host);
  CHECK(!item(m, "single", "load")->disabled);
  // A server cvar the mod ignores stays dimmed although the server has it.
  ge::console::Cvars server(true);
  server.add({.name = "skill", .fallback = "1"});
  server.add({.name = "fraglimit", .fallback = "20"});
  host.server = &server;
  sm::refresh(m, host);
  CHECK(item(m, "host", "skill")->disabled);
  CHECK(!item(m, "host", "fraglimit")->disabled);
  CHECK(item(m, "host", "fraglimit")
            ->choices[static_cast<size_t>(item(m, "host", "fraglimit")->choice)] == "20");
}

TEST(a_setting_is_its_cvar_read_and_written) {
  World w;
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con};
  w.con.execute("m_invert 1");
  sm::refresh(m, host);
  CHECK(item(m, "options", "m_invert")->on);
  m.open("options");
  for (const MenuEvent& e : press(m, "m_invert")) sm::apply_event(e, m, host);
  CHECK(!w.invert);
  CHECK(m.focus("sensitivity"));
  MenuInput right;
  right.right = true;
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  CHECK(w.sensitivity > 0.066);
  sm::reset_defaults(host);
  CHECK(w.sensitivity == 0.066);
}

TEST(new_game_start_runs_the_rules_then_the_map) {
  World w;
  Menu m = sm::build_menu();
  ge::console::Cvars server(true);
  for (const char* c : {"deathmatch", "fraglimit", "timelimit", "teamplay"})
    server.add({.name = c, .fallback = "0"});
  sm::Host host{.console = &w.con, .server = &server, .maps = {"start", "dm1", "arena"}};
  sm::refresh(m, host);
  m.open("multi");
  for (const MenuEvent& e : press(m, "host")) sm::apply_event(e, m, host);
  CHECK(m.top().id == "host");
  CHECK(m.focus("map"));
  MenuInput right;
  right.right = true;
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  sm::Outcome out = sm::Outcome::none;
  for (const MenuEvent& e : press(m, "start")) out = sm::apply_event(e, m, host);
  CHECK(out == sm::Outcome::close);
  CHECK(std::find(w.ran.begin(), w.ran.end(), "sv fraglimit 0") != w.ran.end());
  CHECK(std::find(w.ran.begin(), w.ran.end(), "sv teamplay 0") == w.ran.end());  // dimmed
  CHECK(!w.ran.empty() && w.ran.back() == "map dm1");
}

TEST(escape_opens_the_menu_and_backs_out_of_it) {
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start"}};
  g.update({}, true, host, 0.016);
  CHECK(g.open());
  CHECK(!g.classic());                                     // no pictures: the modern look
  CHECK(!item(g.main(), "options", "classic")->disabled);  // the ce_ switches are registered
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);  // SINGLE PLAYER
  CHECK(g.main().top().id == "single");
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);
  CHECK(g.main().top().id == "main");
  g.update(back, false, host, 0.016);
  CHECK(!g.open());
  w.con.execute("menu_quit");
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.main().top().id == "quit");
  g.update(accept, false, host, 0.016);  // YES
  CHECK(!w.ran.empty() && w.ran.back() == "quit");
}

TEST(classic_pictures_are_read_from_the_packs_files) {
  const auto files = sm::fixture_files();
  sm::ClassicArt art;
  std::string error;
  CHECK_MSG(sm::load_classic_art(fixture_find(files), &art, &error), error);
  CHECK(art.loaded());
  CHECK(art.glyphs.width == 128 && art.glyphs.height == 128);
  for (std::string_view n : sm::classic_picture_names())
    CHECK_MSG(art.find(n) != nullptr, std::string(n));
  CHECK(sm::help_pages(art) == 6);
  // Without the main strip the pack offers no classic look.
  auto lacking = files;
  lacking.erase("gfx/mainmenu.lmp");
  CHECK(!sm::load_classic_art(fixture_find(lacking), &art, &error));
  CHECK(error.find("mainmenu") != std::string::npos);
}

TEST(both_looks_lay_every_page_out_clean) {
  const auto files = sm::fixture_files();
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  sm::ClassicArt art;
  std::string error;
  CHECK(sm::load_classic_art(fixture_find(files), &art, &error));
  for (const char* look : {"modern", "classic"}) {
    if (std::string(look) == "classic") g.set_art(art);
    for (const char* page :
         {"main", "single", "multi", "host", "setup", "options", "video", "help", "quit"}) {
      for (const auto& [width, height] : {std::pair{1280, 720}, std::pair{390, 844}}) {
        g.show(host, page);
        CHECK(g.classic() == (std::string(look) == "classic"));
        DrawList dl;
        dl.begin(width, height);
        g.draw(dl, width, height, 0.25);
        for (const std::string& p : g.problems())
          CHECK_MSG(false, std::string(look) + " " + page + ": " + p);
        CHECK(dl.size() > 0);
      }
    }
  }
  // The classic root's rows are the strip's, 20 classic px apart, the cursor's row lit.
  g.show(host, "main");
  const sm::ClassicLayout l = sm::layout_classic(g.main(), art, 1280, 720);
  CHECK(l.strip && l.scale == 3);
  CHECK(l.items[1].y - l.items[0].y == 60);
}
