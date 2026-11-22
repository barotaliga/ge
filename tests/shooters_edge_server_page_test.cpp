// games/shooters_edge/menu/server_page: OPTIONS > SERVER SETTINGS filled from a sheet: the first
// page's rows and a row a subpage, the votes marked with an asterisk and a legend, the settings the
// player cannot change dimmed; a row changed at once sends its words; a row a vote decides is
// asked about when the focus leaves it or the page is left (YES sends the vote, NO sends nothing);
// the page asks the window for the sheet as it opens and lets it go when the player leaves.
//
// ge:summary Test: the server settings pages, the vote question, the lines they send
// ge:keywords test server-settings server-page vote-question serversheet asterisk
// ge:skill ui net testing
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "shooters_edge/menu/server_page.hpp"

namespace sm = shooters_edge::menu;
namespace gs = ge::server;
using namespace ge::ui;

namespace {

struct World {
  ge::console::Console con;
  std::vector<std::string> ran;
  World() {
    for (const char* c : {"cmd", "serversheet"})
      con.add_command(c, "", [this, c](ge::console::Console&, const ge::console::Args& a) {
        ran.push_back(std::string(c) + (a.size() > 1 ? " " + a.rest(0) : ""));
      });
  }
  bool sent(const std::string& line) const {
    return std::find(ran.begin(), ran.end(), line) != ran.end();
  }
};

gs::SettingsSheet sheet() {
  gs::SettingsSheet s;
  s.whole = true;
  s.revision = 1;
  gs::SheetRow map{.name = "map", .words = "Map", .value = "dm3", .values = "TEXT: a map"};
  map.how = gs::SheetHow::vote;
  gs::SheetRow frags{.name = "fraglimit",
                     .words = "Frag limit",
                     .value = "20",
                     .values = "0: no limit; 1..N: that many frags"};
  frags.how = gs::SheetHow::vote;
  gs::SheetRow dm{.name = "deathmatch",
                  .words = "Deathmatch mode",
                  .value = "3",
                  .values = "1: one; 2: two; 3: three; 4: four; 5: five",
                  .line = "dmm%v"};
  dm.how = gs::SheetHow::direct;
  gs::SheetRow fps{.name = "maxfps", .words = "Packets a second", .group = "Server", .value = "77"};
  gs::SheetRow jump{.name = "speedjump:3",
                    .words = "Easy jump for Ann",
                    .group = "Player help",
                    .value = "off",
                    .values = "off: not granted; on: granted",
                    .line = "vote speedjump 3 %v"};
  jump.how = gs::SheetHow::vote;
  for (gs::SheetRow r : {map, frags, dm, fps, jump}) gs::sheet_put(s, std::move(r));
  return s;
}

const Item* item(const Menu& m, std::string_view page, std::string_view id) {
  const Page* p = m.page(page);
  return p ? p->find(id) : nullptr;
}

}  // namespace

TEST(the_options_page_opens_server_settings_in_both_games) {
  for (const sm::MenuTable* t : {&sm::classic_menu(), &sm::q2_menu()}) {
    const Menu m = sm::build_menu(*t);
    const Item* row = item(m, "options", "server");
    CHECK(row != nullptr && row->opens == "server" && m.page("server") != nullptr);
  }
}

TEST(the_pages_show_the_sheet_votes_marked_and_fixed_rows_dimmed) {
  const gs::SettingsSheet s = sheet();
  Menu m = sm::build_menu();
  sm::ServerPage page;
  page.fill(m, &s, {"dm2", "dm3", "e1m2"}, false, false);
  const Page& first = *m.page("server");
  CHECK(first.items[0].id == "map" && first.items[0].text == "MAP*");
  CHECK_MSG(first.items[0].choices.size() == 3 &&
                first.items[0].choices[static_cast<size_t>(first.items[0].choice)] == "dm3",
            std::to_string(first.items[0].choices.size()));
  const Item* frags = item(m, "server", "fraglimit");
  CHECK(frags != nullptr && frags->text == "FRAG LIMIT*" && frags->choices[0] == "NO LIMIT");
  CHECK(frags->choices[static_cast<size_t>(frags->choice)] == "20");
  CHECK(item(m, "server", "deathmatch")->text == "DEATHMATCH MODE");  // changed at once: no mark
  CHECK(item(m, "server", "server/Server") != nullptr);
  CHECK(item(m, "server", "server/Player help") != nullptr);
  CHECK(item(m, "server", "legend") != nullptr);
  CHECK(item(m, "server/Server", "maxfps")->disabled);
  CHECK(item(m, "server/Player help", "speedjump:3") != nullptr);
  // Quake II's words in small letters.
  Menu q = sm::build_menu(sm::q2_menu());
  page.fill(q, &s, {}, true, false);
  CHECK(item(q, "server", "fraglimit")->text == "frag limit*");
  // No sheet: the page says so.
  Menu none = sm::build_menu();
  page.fill(none, nullptr, {}, false, true);
  CHECK(item(none, "server", "none") != nullptr && item(none, "server", "back")->disabled);
}

TEST(a_row_changed_at_once_sends_its_words) {
  World w;
  const gs::SettingsSheet s = sheet();
  Menu m = sm::build_menu();
  m.open("server");
  sm::ServerPage page;
  page.event({EventKind::opened, "server", ""}, m, w.con);
  CHECK(w.sent("serversheet open") && page.open());
  page.fill(m, &s, {}, false, false);
  CHECK(m.focus("deathmatch"));
  for (const MenuEvent& e : m.input(MenuInput{.right = true})) page.event(e, m, w.con);
  CHECK_MSG(w.sent("cmd dmm4"), w.ran.empty() ? "" : w.ran.back());
  CHECK(w.sent("serversheet changed"));
}

TEST(leaving_a_row_turned_for_a_vote_asks_first) {
  World w;
  const gs::SettingsSheet s = sheet();
  Menu m = sm::build_menu();
  m.open("options");
  m.open("server");
  sm::ServerPage page;
  page.event({EventKind::opened, "server", ""}, m, w.con);
  page.fill(m, &s, {}, false, false);
  CHECK(m.focus("fraglimit"));
  for (const MenuEvent& e : m.input(MenuInput{.right = true})) page.event(e, m, w.con);
  CHECK(page.pending_name() == "fraglimit" && page.pending_value() == "25");
  CHECK(!w.sent("cmd vote fraglimit 25"));  // nothing until the player leaves the row
  // The focus moves on: the question.
  const std::string before = m.top().id;
  for (const MenuEvent& e : m.input(MenuInput{.down = true})) page.event(e, m, w.con);
  page.after(m, before, "fraglimit", w.con);
  CHECK_MSG(m.top().id == sm::kVotePage, m.top().id);
  CHECK_MSG(item(m, sm::kVotePage, "what")->text == "FRAG LIMIT: 20 TO 25",
            item(m, sm::kVotePage, "what")->text);
  CHECK(m.focused() != nullptr && m.focused()->id == "yes");
  for (const MenuEvent& e : m.input(MenuInput{.accept = true})) page.event(e, m, w.con);
  CHECK(w.sent("cmd vote fraglimit 25"));
  CHECK(page.pending_name().empty() && m.top().id == "server");
  // Again, and back out of the page: asked, and NO sends nothing.
  w.ran.clear();
  page.fill(m, &s, {}, false, false);
  CHECK(m.focus("fraglimit"));
  for (const MenuEvent& e : m.input(MenuInput{.left = true})) page.event(e, m, w.con);
  CHECK(page.pending_value() == "15");
  for (const MenuEvent& e : m.input(MenuInput{.back = true})) page.event(e, m, w.con);
  CHECK(m.top().id == sm::kVotePage);
  CHECK(m.focus("no"));
  for (const MenuEvent& e : m.input(MenuInput{.accept = true})) page.event(e, m, w.con);
  CHECK(!w.sent("cmd vote fraglimit 15") && page.pending_name().empty());
  // Out of every page of it: the sheet let go.
  page.after(m, m.top().id, "", w.con);
  CHECK_MSG(w.sent("serversheet close"), m.top().id);
  CHECK(!page.open());
}

TEST(the_window_opens_a_page_of_it_directly) {
  World w;
  const gs::SettingsSheet s = sheet();
  sm::GameMenu menu;
  menu.add_cvars(w.con);  // as the window does: the menu knows its console
  sm::Host host{.console = &w.con, .sheet = &s};
  menu.show(host, "server/Player help");
  CHECK_MSG(menu.main().top().id == "server/Player help", menu.main().top().id);
  CHECK(w.sent("serversheet open"));
  CHECK(menu.main().top().find("speedjump:3") != nullptr);
  menu.hide();
  CHECK(w.sent("serversheet close"));
}
