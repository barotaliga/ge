// Shooter's Edge's main menu (games/shooters_edge/menu): the classic pages and their rows; a row
// dimmed while the console knows nothing it needs and lit once it does; a setting read from its
// cvar and written by a console line; NEW GAME's START running the rules on the server and the
// map; ESCAPE opening the menu and going back out of it; the classic pictures read from a pack's
// files and refused without them; the classic layout's rows on the 320 x 200 screen; the modern
// and the classic looks drawing every page clean; LOAD and SAVE's slots, their lines and the
// console lines they run; CLASSIC ENHANCED's LOAD GAME asking the window for a folder and showing
// why one was refused, ENHANCE dimmed until a game is loaded; OPTIONS ▸ SCREENSHOTS looking
// through the screenshots, deleting one after asking, and `menu_screenshots` opening it; DEMOS
// on the root (its own band between the classic strip's), listing the recordings of a folder,
// refusing a file the check refuses and handing one it passes to the window.
//
// ge:summary Tests the main menu: pages, dimming by the console, cvars, START, ESCAPE, both looks
// ge:keywords test menu main-menu dimmed cvar console new-game start escape classic modern layout
// ge:keywords load-game take_load load_refused screenshots gallery menu_screenshots
// ge:keywords server-browser join-a-game menu_servers servers connect demos menu_demos take_demo
// ge:skill ui testing
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "forward/routes.hpp"
#include "lab/png.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "shooters_edge/menu/carved_letters.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/figures.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "shooters_edge/menu/plate_letters.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

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
  CHECK(root.items.size() == 6);  // no HELP: its pages show only at the free episode's end
  const char* rows[] = {"games", "single", "multi", "demos", "options", "quit"};
  for (size_t i = 0; i < 6; i++) CHECK(root.items[i].id == rows[i]);
  for (const char* p : {"single", "multi", "host", "setup", "options", "video", "help", "quit"})
    CHECK_MSG(m.page(p) != nullptr, std::string("no page ") + p);
  CHECK(m.page("single")->items.size() == 4);  // three rows, as the classic strip, and BACK
  CHECK(m.page("multi")->items.size() == 5);   // the strip's three, RANKED GAME and BACK
  for (const sm::Entry& e : sm::entries())
    CHECK_MSG(item(m, e.page, e.id) != nullptr, std::string(e.page) + "." + std::string(e.id));
}

TEST(rows_dim_until_the_console_knows_what_they_need) {
  World w;
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  sm::refresh(m, host);
  CHECK(item(m, "single", "new")->disabled);   // no single-player program: no `newgame`
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
  // So does NEW GAME once the packs carry a campaign (the window registers `newgame` then).
  w.con.add_command("newgame", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::refresh(m, host);
  CHECK(!item(m, "single", "new")->disabled);
  // The server's rules light when the server has them; MAX PLAYERS waits for network play.
  ge::console::Cvars server(true);
  server.add({.name = "skill", .fallback = "1"});
  server.add({.name = "fraglimit", .fallback = "20"});
  host.server = &server;
  sm::refresh(m, host);
  CHECK(!item(m, "host", "skill")->disabled);
  CHECK(item(m, "host", "maxclients")->disabled);
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
  CHECK(std::find(w.ran.begin(), w.ran.end(), "sv teamplay 0") != w.ran.end());
  CHECK(!w.ran.empty() && w.ran.back() == "map dm1");
}

TEST(ranked_game_looks_for_a_duel_once_the_console_can) {
  World w;
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con};
  sm::refresh(m, host);
  CHECK(item(m, "multi", "ranked")->disabled);  // no `ranked` command: no account client
  w.con.add_command("ranked", "", [&w](ge::console::Console&, const ge::console::Args& a) {
    w.ran.push_back("ranked " + a.rest(0));
  });
  sm::refresh(m, host);
  CHECK(!item(m, "multi", "ranked")->disabled);
  m.open("multi");
  for (const MenuEvent& e : press(m, "ranked")) sm::apply_event(e, m, host);
  CHECK(m.top().id == "ranked");
  sm::Outcome out = sm::Outcome::none;
  for (const MenuEvent& e : press(m, "duel")) out = sm::apply_event(e, m, host);
  CHECK(out == sm::Outcome::close);
  CHECK(!w.ran.empty() && w.ran.back() == "ranked 1on1");
  m.open("ranked");
  for (const MenuEvent& e : press(m, "leave")) sm::apply_event(e, m, host);
  CHECK(w.ran.back() == "ranked leave");
}

// GAME TYPE COOPERATIVE with a campaign to play starts a coop game on the map, so bots may join;
// DEATHMATCH starts the map as a match.
TEST(new_game_asks_first_while_a_game_runs) {
  World w;
  w.con.add_command("newgame", "", [&w](ge::console::Console&, const ge::console::Args&) {
    w.ran.push_back("newgame");
  });
  Menu m = sm::build_menu();
  sm::Host idle{.console = &w.con};
  sm::refresh(m, idle);
  m.open("single");
  for (const MenuEvent& e : press(m, "new")) sm::apply_event(e, m, idle);
  CHECK(w.ran.size() == 1);  // no game running: started at once
  // A game running: the question, then its YES starts the new game.
  w.ran.clear();
  ge::console::Cvars server(true);
  sm::Host host{.console = &w.con, .server = &server};
  m.reset();
  sm::refresh(m, host);
  m.open("single");
  for (const MenuEvent& e : press(m, "new")) sm::apply_event(e, m, host);
  CHECK(w.ran.empty());
  CHECK(m.top().id == "newgame");
  for (const MenuEvent& e : press(m, "yes")) sm::apply_event(e, m, host);
  CHECK(w.ran.size() == 1 && w.ran.back() == "newgame");
}

TEST(new_game_start_cooperative_starts_a_coop_game) {
  World w;
  w.con.add_command("coopgame", "", [&w](ge::console::Console&, const ge::console::Args& a) {
    w.ran.push_back("coopgame " + a.rest(0));
  });
  Menu m = sm::build_menu();
  ge::console::Cvars server(true);
  for (const char* c : {"deathmatch", "fraglimit", "timelimit", "teamplay"})
    server.add({.name = c, .fallback = "0"});
  sm::Host host{.console = &w.con, .server = &server, .maps = {"start", "e1m1"}};
  sm::refresh(m, host);
  m.open("multi");
  for (const MenuEvent& e : press(m, "host")) sm::apply_event(e, m, host);
  CHECK(m.focus("map"));
  MenuInput right;
  right.right = true;
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  for (const MenuEvent& e : press(m, "start")) sm::apply_event(e, m, host);
  CHECK(std::find(w.ran.begin(), w.ran.end(), "sv deathmatch 0") != w.ran.end());
  CHECK_MSG(!w.ran.empty() && w.ran.back() == "coopgame 0 e1m1", w.ran.back());
  // DEATHMATCH: the map as a match.
  w.ran.clear();
  m.open("multi");
  for (const MenuEvent& e : press(m, "host")) sm::apply_event(e, m, host);
  CHECK(m.focus("deathmatch"));
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  for (const MenuEvent& e : press(m, "start")) sm::apply_event(e, m, host);
  CHECK(std::find(w.ran.begin(), w.ran.end(), "sv deathmatch 1") != w.ran.end());
  CHECK_MSG(!w.ran.empty() && w.ran.back().rfind("map ", 0) == 0, w.ran.back());
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
  g.update(accept, false, host, 0.016);  // SELECT GAME, the root's first row
  CHECK(g.in_games());
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);
  CHECK(!g.in_games() && g.open() && g.main().top().id == "main");
  MenuInput down;
  down.down = true;
  g.update(down, false, host, 0.016);
  g.update(accept, false, host, 0.016);  // SINGLE PLAYER
  CHECK(g.main().top().id == "single");
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

TEST(quakes_menu_words_open_each_page) {
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start"}};
  const struct {
    const char* word;
    const char* page;
  } words[] = {{"menu_singleplayer", "single"}, {"menu_load", "load"},   {"menu_save", "save"},
               {"menu_multiplayer", "multi"},   {"menu_setup", "setup"}, {"menu_keys", "controls"},
               {"menu_video", "video"}};
  for (const auto& wp : words) {
    w.con.execute(wp.word);
    g.update({}, false, host, 0.016);
    CHECK_MSG(g.open() && g.main().top().id == wp.page, wp.word);
  }
}

TEST(every_press_leaves_the_sound_it_makes) {
  using ge::ui::MenuCue;
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start"}};
  g.update({}, false, host, 0.016);
  CHECK(g.take_cue() == MenuCue::none);  // closed, nothing pressed: silent
  g.update({}, true, host, 0.016);       // Escape opens it
  CHECK(g.take_cue() == MenuCue::enter);
  CHECK(g.take_cue() == MenuCue::none);  // taken once
  MenuInput down;
  down.down = true;
  g.update(down, false, host, 0.016);  // the cursor moves
  CHECK(g.take_cue() == MenuCue::move);
  MenuInput up;
  up.up = true;
  g.update(up, false, host, 0.016);
  CHECK(g.take_cue() == MenuCue::move);
  g.update(down, false, host, 0.016);  // past SELECT GAME, the first row
  CHECK(g.take_cue() == MenuCue::move);
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);  // into SINGLE PLAYER
  CHECK(g.main().top().id == "single");
  CHECK(g.take_cue() == MenuCue::enter);
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);  // out to the main menu
  CHECK(g.main().top().id == "main");
  CHECK(g.take_cue() == MenuCue::back);
  g.update(back, false, host, 0.016);  // the menu closed
  CHECK(!g.open());
  CHECK(g.take_cue() == MenuCue::back);
  w.con.execute("togglemenu");  // the console's Escape opens it, and closes it again
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.take_cue() == MenuCue::enter);
  w.con.execute("togglemenu");
  g.update({}, false, host, 0.016);
  CHECK(!g.open() && g.take_cue() == MenuCue::back);
}

TEST(customize_controls_shows_the_binds_and_binds_the_next_key) {
  World w;
  w.con.add_button("forward", "");
  w.con.add_button("jump", "");
  w.con.execute("bind w +forward; bind up +forward; bind space +jump");
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con};
  CHECK(sm::bound_keys(w.con.binds(), "+forward") == (std::vector<std::string>{"up", "w"}));
  CHECK(sm::keys_text(sm::bound_keys(w.con.binds(), "+forward")) == "UP, W");
  CHECK(sm::keys_text({}) == "---");
  g.show(host, "controls");
  CHECK(item(g.main(), "controls", "c1")->choices[0] == "UP, W");  // MOVE FORWARD
  CHECK(item(g.main(), "controls", "c0")->disabled);               // no +attack button here
  CHECK(g.main().focus("c5"));                                     // JUMP
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(g.waiting_key());
  CHECK(item(g.main(), "controls", "c5")->choices[0] == "PRESS A KEY");
  g.press_key("enter", host);  // the frame's own ENTER binds nothing
  CHECK(g.waiting_key());
  g.update({}, false, host, 0.016);
  g.press_key("mouse2", host);
  CHECK(!g.waiting_key());
  CHECK(w.con.binds().get(ge::console::BindMode::game, "mouse2") == "+jump");
  CHECK(item(g.main(), "controls", "c5")->choices[0] == "MOUSE2");  // "MOUSE2, SPACE" is too wide
  // A third key replaces both; BACKSPACE clears; ESCAPE leaves the move as it was.
  CHECK(sm::bind_control(w.con, "+jump", "e"));
  CHECK(sm::bound_keys(w.con.binds(), "+jump") == std::vector<std::string>{"e"});
  CHECK(!sm::bind_control(w.con, "+jump", "nosuchkey"));
  g.update(accept, false, host, 0.016);
  g.update({}, false, host, 0.016);
  g.press_key("escape", host);
  CHECK(sm::bound_keys(w.con.binds(), "+jump") == std::vector<std::string>{"e"});
  g.update(accept, false, host, 0.016);
  g.update({}, false, host, 0.016);
  g.press_key("backspace", host);
  CHECK(sm::bound_keys(w.con.binds(), "+jump").empty());
  CHECK(item(g.main(), "controls", "c5")->choices[0] == "---");
}

TEST(brightness_is_gamma_drawn_brighter_to_the_right) {
  World w;
  double gamma = 1;
  w.con.cvars().bind({.name = "gamma", .fallback = "1", .min = 0.3, .max = 1.5}, &gamma);
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con};
  sm::refresh(m, host);
  CHECK(item(m, "options", "gamma")->value == 0);  // gamma 1: the slider's left end, 0 %
  m.open("options");
  CHECK(m.focus("gamma"));
  MenuInput right;
  right.right = true;
  for (const MenuEvent& e : m.input(right)) sm::apply_event(e, m, host);
  CHECK(std::fabs(gamma - 0.95) < 1e-9);  // a step right (10 %) is brighter: a lower gamma
  w.con.execute("gamma 0.5");
  sm::refresh(m, host);
  CHECK(item(m, "options", "gamma")->value == 100);
}

TEST(classic_enhanced_pages_set_their_cvars_and_open_the_enhance_menu) {
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);  // the ce_ switches
  sm::Host host{.console = &w.con};
  g.show(host, "enhanced");
  for (const char* row : {"art", "light", "liquids", "fire"})
    CHECK_MSG(!item(g.main(), "enhanced", row)->disabled, row);
  CHECK(item(g.main(), "enhanced", "weather")->disabled);  // no sv_weather on this console
  CHECK(item(g.main(), "enhanced", "load")->disabled);     // no `basedir`: the window's
  CHECK(item(g.main(), "enhanced", "enhance")->disabled);  // no game loaded
  g.show(host, "ce_liquids");
  CHECK(item(g.main(), "ce_liquids", "ce_water")->on);
  CHECK(g.main().focus("ce_water"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(w.con.cvars().find("ce_water")->value == "0");
  // A game loaded: ENHANCE shows its enhancing, here why it did not start (no install there).
  g.set_game((std::filesystem::temp_directory_path() / "ge_se_menu_no_install").string());
  g.show(host, "enhanced");
  CHECK(g.main().focus("enhance"));
  g.update(accept, false, host, 0.016);
  CHECK(g.in_enhance());
  CHECK(g.enhance().top().id == "enhancing");
  CHECK(g.job() == nullptr && !g.take_ended());
  // BACK returns to CLASSIC ENHANCED, never to classic-enhance's own pack page.
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);
  CHECK(!g.in_enhance());
  CHECK(g.main().top().id == "enhanced");
}

TEST(select_game_opens_a_kept_install_and_asks_for_a_folder_otherwise) {
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con};
  int listed = 0;
  g.games().set_lister([&listed] {
    listed++;
    return std::vector<sm::GameRow>{
        {.id = "q1", .install = "/a/game", .playing = true},
        {.id = "q2", .install = "/b/game", .playing = false},
    };
  });
  w.con.execute("menu_games");
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.in_games() && listed == 1);
  // The names are pictures, never words: each game's decodes, an unknown id's is empty.
  CHECK(!sm::game_name_picture("q1").rgba.empty());
  CHECK(!sm::game_name_picture("q2").rgba.empty());
  CHECK(sm::game_name_picture("other").rgba.empty());
  // The cursor starts on the game playing; ENTER on it goes back to the root.
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(!g.in_games() && g.main().top().id == "main");
  CHECK(!g.take_game().asked);
  // The other game with a kept install: the window is asked to open it.
  g.show(host, "games");
  MenuInput down;
  down.down = true;
  g.update(down, false, host, 0.016);
  g.update(accept, false, host, 0.016);
  sm::GameMenu::GameAsk ask = g.take_game();
  CHECK(ask.asked && !ask.browse && ask.id == "q2" && ask.install == "/b/game");
  CHECK(!g.take_game().asked);  // once
  // ADD GAME, the last row: the folder chooser.
  g.update(down, false, host, 0.016);
  g.update(accept, false, host, 0.016);
  ask = g.take_game();
  CHECK(ask.asked && ask.browse && ask.id.empty());
}

TEST(load_game_asks_the_window_for_a_folder_and_says_why_one_was_refused) {
  World w;
  w.con.add_command("basedir", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::GameMenu g;
  g.add_cvars(w.con);
  CHECK(w.con.cvars().find("ce_auto")->value == "1");  // a game loaded is enhanced at once
  sm::Host host{.console = &w.con};
  g.show(host, "enhanced");
  CHECK(!item(g.main(), "enhanced", "load")->disabled);
  CHECK(!g.take_load());
  CHECK(g.main().focus("load"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(g.take_load());
  CHECK(!g.take_load());  // once
  g.load_refused("no id1 folder");
  CHECK(item(g.main(), "enhanced", "load")->hint == "NOT LOADED: no id1 folder");
  g.set_game("/a/game");
  g.show(host, "enhanced");
  CHECK(item(g.main(), "enhanced", "load")->hint.find("NOT LOADED") == std::string::npos);
  CHECK(!item(g.main(), "enhanced", "enhance")->disabled);
  CHECK(g.game() == "/a/game");
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

TEST(a_classic_game_s_console_is_in_its_letters_over_its_background) {
  const auto files = sm::fixture_files();
  sm::ClassicArt art;
  std::string error;
  CHECK_MSG(sm::load_classic_art(fixture_find(files), &art, &error), error);
  sm::ClassicConsole look;
  sm::classic_console(art, look);
  CHECK(look.on() && look.style.font == &look.font);
  CHECK(look.back != nullptr && look.back == art.find("conback"));
  ge::console::Console con;
  ge::console::ConsoleView view(con);
  view.add(ge::console::LineKind::output, "a line");
  view.set_open(true);
  for (int k = 0; k < 60; k++) view.frame(1 / 30.0);
  DrawList dl;
  dl.begin(1280, 720);
  sm::draw_game_console(dl, view, look, 0);
  int backs = 0;
  for (const auto& c : dl.cmds()) backs += c.box.image == look.back ? 1 : 0;
  CHECK(backs == 1);
  // No pictures read: the engine's own look, no background.
  sm::ClassicConsole none;
  sm::classic_console(sm::ClassicArt{}, none);
  CHECK(!none.on() && none.back == nullptr);
}

TEST(load_and_save_show_the_slots_and_run_their_lines) {
  World w;
  for (const char* c : {"load", "save"})
    w.con.add_command(c, "", [&w, c](ge::console::Console&, const ge::console::Args& a) {
      w.ran.push_back(std::string(c) + " " + a.rest(0));
    });
  Menu m = sm::build_menu();
  sm::Host host{.console = &w.con};
  host.saves.assign(sm::kSaveSlots, {});
  host.saves[3] = "the hall, kills 3/12";
  host.saves[5] = "the long corridor of doors, kills 4/31";
  host.saves[6] = "the gate house of the far keep kills:0/12";
  sm::refresh(m, host);
  CHECK(item(m, "load", "s5")->text == "the long corrido, kills 4/31");  // the modern look
  sm::refresh(m, host, true);  // the classic look's column holds less
  CHECK(item(m, "load", "s5")->text == "the long c, kills 4/31");
  CHECK(item(m, "load", "s6")->text == "the gate ho kills:0/12");
  sm::refresh(m, host);
  CHECK(item(m, "single", "load")->opens == "load");
  CHECK(item(m, "single", "save")->opens == "save");
  CHECK(m.page("load")->items.size() == sm::kSaveSlots + 1);  // the slots and BACK
  CHECK(item(m, "load", "s0")->disabled);                     // an empty slot loads nothing
  CHECK(item(m, "load", "s0")->text == sm::kUnusedSlot);
  CHECK(!item(m, "load", "s3")->disabled);
  CHECK(item(m, "load", "s3")->text == "the hall, kills 3/12");
  CHECK(!item(m, "save", "s0")->disabled);  // any slot takes a save
  // A slot pressed runs its line and closes the menu.
  m.open("load");
  for (const MenuEvent& e : press(m, "s3"))
    CHECK(sm::apply_event(e, m, host) ==
          (e.kind == EventKind::activated ? sm::Outcome::close : sm::Outcome::none));
  m.reset();
  m.open("save");
  for (const MenuEvent& e : press(m, "s0")) sm::apply_event(e, m, host);
  CHECK(w.ran.size() == 2 && w.ran[0] == "load s3" && w.ran[1] == "save s0");
  // Without the commands both pages are dimmed.
  World bare;
  sm::Host none{.console = &bare.con};
  none.saves = host.saves;
  sm::refresh(m, none);
  CHECK(item(m, "load", "s3")->disabled && item(m, "save", "s0")->disabled);
}

TEST(both_looks_lay_every_page_out_clean) {
  const auto files = sm::fixture_files();
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  host.saves = {"", "the hall, kills 3/12", "a line longer than the label column holds"};
  sm::ClassicArt art;
  std::string error;
  CHECK(sm::load_classic_art(fixture_find(files), &art, &error));
  for (const char* look : {"modern", "classic"}) {
    if (std::string(look) == "classic") g.set_art(art);
    for (const char* page :
         {"main",       "single",   "multi",    "host",   "setup",    "options",    "video",
          "weapon",     "controls", "enhanced", "ce_art", "ce_light", "ce_liquids", "ce_fire",
          "ce_weather", "help",     "quit",     "load",   "save",     "games",      "ranked"}) {
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
  // SELECT GAME and DEMOS have no words in the strip: each its own band, the strip's rows in
  // between and after.
  CHECK(l.bands[0] == -1 && l.bands[1] == 0 && l.bands[2] == 1 && l.bands[3] == -1 &&
        l.bands[4] == 2);
  CHECK(l.items[4].y - l.items[3].y == 60);

  // DEMOS and SELECT GAME set in the strips' own letters, each cut into its own picture, only on
  // the strips the cuts were measured on: the stand-in's strips have another print, so no letter
  // is cut and the rows fall back to the glyphs; with the pack's print, a picture a letter.
  CHECK(art.strip_print != 0 && art.letters.empty());
  const auto letters = [](const sm::ClassicArt& a, const DrawList& dl) {
    int n = 0;
    // The carved letters at every tone (SELECT GAME greys the games not playing).
    for (const auto& c : dl.cmds())
      for (const sm::Tone tone : {sm::Tone::full, sm::Tone::half, sm::Tone::grey})
        for (const auto& [ch, img] : sm::toned(a.letters, tone)) n += c.box.image == &img ? 1 : 0;
    return n;
  };
  const auto root = [&](const sm::ClassicArt& a, const ge::ui::Image* img) {
    DrawList dl;
    dl.begin(1280, 720);
    sm::draw_classic(dl, g.main(), a, l, 0, 0);
    int n = 0;
    for (const auto& c : dl.cmds()) n += c.box.image == img ? 1 : 0;
    return std::pair{n, letters(a, dl)};
  };
  // The pack's strips stood in by solid ones of their size, so every letter's box is cut whole.
  sm::ClassicArt pack = art;
  pack.pictures["mainmenu"] = make_image(240, 112, hex(0x804020));
  pack.pictures["sp_menu"] = make_image(232, 64, hex(0x804020));
  pack.strip_print = 0xb378b8eec94a7f36ULL;
  sm::cut_letters(&pack);
  CHECK(pack.letters.size() == 52);  // A to Z and a to z: 15 cut, the rest carved
  // Every letter its own columns, on the 24 px line (the tails below the 20 px row).
  for (const auto& [ch, img] : pack.letters) CHECK(img.height == 24 && img.width > 0);
  CHECK(root(art, nullptr).second == 0);
  CHECK(root(pack, nullptr).second == 5 + 10);  // DEMOS, SELECT GAME (the space no picture)
  CHECK(root(pack, &pack.glyphs).first < root(art, &art.glyphs).first);
  CHECK(sm::carved_text_width(pack, "Demos") > 0 && sm::carved_text_width(pack, "Demos!") == 0);

  // SELECT GAME's title and the games' names: a picture a letter, five for Quake and
  // seven for Quake II's (its numeral); the stand-in's draw none and fall back to the pictures.
  const auto names = [&](const sm::ClassicArt& a) {
    sm::GamesPage page;
    page.set_games({{.id = "q1", .install = "/a"}, {.id = "q2", .install = "/b"}});
    DrawList dl;
    dl.begin(1280, 720);
    page.draw_classic(dl, a, 1280, 720, 0);
    return letters(a, dl);
  };
  CHECK(names(art) == 0);
  CHECK(names(pack) == 10 + 5 + 7);

  // A speck a letter's box catches of a neighbour is left out: the u here a 5 px square and a
  // 2 px speck beside it in Quit's u box.
  sm::ClassicArt specks = pack;
  Image strip = make_image(240, 100, hex(0x000000, 0));
  const auto dot = [&](int x, int y) {
    const size_t i = (static_cast<size_t>(y) * 240 + static_cast<size_t>(x)) * 4;
    strip.rgba[i] = 200;
    strip.rgba[i + 3] = 255;
  };
  for (int y = 85; y < 90; y++)
    for (int x = 25; x < 30; x++) dot(x, y);
  dot(38, 92);
  dot(39, 92);
  specks.pictures["mainmenu"] = strip;
  sm::cut_letters(&specks);
  CHECK(specks.letters.count('u') == 1 && specks.letters.at('u').width == 5);
  CHECK(specks.letters.count('i') == 0);  // Quit's i box holds nothing here
}

TEST(q2_s_menu_is_drawn_in_its_plates_and_banners) {
  // Quake II's pictures at their sizes, painted flat: the plates, their lit copies, the plaque,
  // the logo, the cursor's frames and the banners.
  sm::ClassicArt art;
  art.glyphs = make_image(128, 128, hex(0xffffff));
  const std::pair<const char*, int> plates[] = {{"m_main_game", 99},
                                                {"m_main_multiplayer", 215},
                                                {"m_main_options", 150},
                                                {"m_main_quit", 100},
                                                {"m_main_video", 118}};
  for (const auto& [name, w] : plates) {
    art.pictures.emplace(name, make_image(w, 32, hex(0x2c2c2c)));
    art.pictures.emplace(std::string(name) + "_sel", make_image(w, 32, hex(0x303030)));
  }
  // One letter's ink on MULTIPLAYER's tag: a bright bar in its I's cell (x 86 to 102), plain and
  // lit, reaching the pad of its T's cell (to x 89).
  for (const auto& [name, v] :
       {std::pair{"m_main_multiplayer", 0xc0}, {"m_main_multiplayer_sel", 0xe0}})
    for (int y = 8; y <= 21; y++)
      for (int x = 89; x <= 95; x++)
        for (int c = 0; c < 3; c++)
          art.pictures.at(name).rgba[(static_cast<size_t>(y) * 215 + static_cast<size_t>(x)) * 4 +
                                     static_cast<size_t>(c)] = static_cast<uint8_t>(v);
  art.pictures.emplace("m_main_plaque", make_image(38, 166, hex(0x303030)));
  art.pictures.emplace("m_main_logo", make_image(36, 42, hex(0x303030)));
  for (int f = 0; f < 15; f++)
    art.pictures.emplace("m_cursor" + std::to_string(f), make_image(22, 29, hex(0xff0000)));
  for (const char* b : {"m_banner_game", "m_banner_multiplayer", "m_banner_options",
                        "m_banner_video", "m_banner_join_server"})
    art.pictures.emplace(b, make_image(275, 32, hex(0x505050)));
  CHECK(art.plates() && !art.loaded() && art.menu_ready());
  CHECK(art.screen_height() == 240);
  CHECK(sm::banner_of("options") == "m_banner_options" && sm::banner_of("help").empty());

  // Every page reads on the 320 x 240 screen; the root a plate a row, 36 classic px apart.
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  g.set_art(art);
  CHECK(g.classic());
  for (const char* page :
       {"main", "single", "multi", "options", "video", "help", "quit", "games"}) {
    g.show(host, page);
    DrawList dl;
    dl.begin(1280, 720);
    g.draw(dl, 1280, 720, 0.25);
    for (const std::string& p : g.problems()) CHECK_MSG(false, std::string(page) + ": " + p);
  }
  g.show(host, "main");
  const sm::ClassicLayout l = sm::layout_classic(g.main(), art, 1280, 720);
  CHECK(l.plates && !l.strip && l.scale == 3);
  CHECK(l.items[1].y - l.items[0].y == 108);
  CHECK(l.items[1].w == 99 * 3);  // SINGLE PLAYER is Quake II's GAME plate
  for (const Rect& r : l.items) CHECK(r.w <= 0 || (r.y >= 0 && r.bottom() <= 720));

  // SELECT GAME and DEMOS built from the plates only where they were measured: elsewhere in
  // big glyphs.
  const auto glyphs = [&](const sm::ClassicArt& a) {
    DrawList dl;
    dl.begin(1280, 720);
    sm::draw_classic(dl, g.main(), a, sm::layout_classic(g.main(), a, 1280, 720), 0, 0);
    int n = 0;
    for (const auto& c : dl.cmds()) n += c.box.image == &a.glyphs ? 1 : 0;
    return n;
  };
  sm::ClassicArt pack = art;
  pack.strip_print = sm::kPlatePrint;
  sm::cut_plate_letters(&art);  // not the measured plates: nothing cut
  CHECK(art.letters.empty() && art.letters_lit.empty());
  sm::cut_plate_letters(&pack);
  // A to Z and a to z, both ends and the metal, plain and lit; every letter the tag's height with
  // a 4 px pad each side of its cell, a small letter three px narrower than its capital, a lit
  // letter as wide as its plain one.
  CHECK(pack.letters.size() == 55 && pack.letters_lit.size() == 55);
  for (const auto& [c, img] : pack.letters) {
    CHECK(img.height == 32 && img.width > 0);
    CHECK(pack.letters_lit.at(c).width == img.width);
    if (c >= 'a' && c <= 'z')
      CHECK(img.width + 3 == pack.letters.at(static_cast<char>(c - 32)).width);
  }
  CHECK(pack.letters.at('I').width == 17 + 8 && pack.letters.at('_').width == 120);
  // The I is its bar and a rim of the tag around it, see-through beyond; the T beside it holds
  // none of the I's ink, though the bar reaches its pad.
  const auto px = [](const Image& img, int x, int y) {
    const size_t at = (static_cast<size_t>(y) * static_cast<size_t>(img.width) + x) * 4;
    return std::pair{img.rgba[at], img.rgba[at + 3]};
  };
  const Image& ink = pack.letters.at('I');
  CHECK((px(ink, 4 + 3 + 3, 14) == std::pair<uint8_t, uint8_t>{0xc0, 255}));  // the bar
  CHECK(px(pack.letters_lit.at('I'), 4 + 3 + 3, 14).first == 0xe0);
  CHECK((px(ink, 4 + 3 - 1, 14) == std::pair<uint8_t, uint8_t>{0x2c, 255}));  // the rim, whole
  CHECK(px(ink, 4 + 3 - 3, 14).second == 63);                                 // fading
  CHECK(px(ink, 4 + 3 - 4, 14).second == 0 && px(ink, 10, 2).second == 0);    // see-through
  const Image& t = pack.letters.at('T');
  bool clear = true;
  for (size_t i = 3; i < t.rgba.size(); i += 4) clear = clear && t.rgba[i] == 0;
  CHECK(clear);
  // The metal is the banner's, shaded to GAME's tag (0x50 to 0x2c); the banner's frame with it.
  CHECK(pack.letters.at('_').rgba[0] == 0x2c);
  CHECK(std::abs(sm::banner_shade(pack) - 44.0f / 80.0f) < 1e-3f);
  // The tag's ends, the space and the letters' cells edge to edge: 19 + 7 x 17 + 3 x 16 + 6 + 13.
  std::vector<float> xs;
  CHECK(sm::plate_layout(pack, "SELECT GAME", &xs) == 19 + 7 * 17 + 3 * 16 + 6 + 13);
  CHECK(xs.size() == 13 && xs[0] == 0 && xs[1] == 19 && xs[2] == 36);
  CHECK(sm::plate_layout(pack, "Ab", nullptr) == 19 + 17 + 14 + 13);
  CHECK(sm::plate_layout(pack, "A!", nullptr) == 0);  // a letter no tag gives
  CHECK(sm::carved_width(art, "games") == 0 && sm::carved_width(pack, "games") == 205);
  CHECK(glyphs(art) == 10 + 5);  // SELECT GAME (no space drawn), DEMOS
  CHECK(glyphs(pack) == 0);

  // A title the pack has no banner for, on a banner of its own: GAME's frame, the plain at least
  // as wide as the pack's (196 px), the letters centred on it; wider for a longer title.
  std::vector<float> bx;
  CHECK(sm::banner_layout(pack, "DEMOS", &bx) == 275);
  CHECK(bx.size() == 5 && bx[0] == 40 + std::floor((196 - (16 + 16 + 17 + 17 + 17)) / 2.0f));
  CHECK(sm::banner_layout(pack, "TEXTURES AND MODELS", nullptr) ==
        40 + 17 * 12 + 16 * 5 + 12 + 8 + 39);
  CHECK(sm::banner_layout(art, "DEMOS", nullptr) == 0);  // no letters cut

  // A page under the root takes its banner; SELECT GAME's title is on a banner and the names on
  // tags: the title's 10 letters over 2 strips of metal, Quake's 5 letters, its ends and
  // a strip, Quake II's 7 letters, its ends and 2 strips (a space draws nothing).
  g.show(host, "options");
  {
    DrawList dl;
    dl.begin(1280, 720);
    g.draw(dl, 1280, 720, 0);
    int n = 0;
    for (const auto& c : dl.cmds()) n += c.box.image == g.art().find("m_banner_options") ? 1 : 0;
    CHECK(n == 1);
  }
  sm::GamesPage page;
  page.set_games({{.id = "q1", .install = "/a"}, {.id = "q2", .install = "/b"}});
  DrawList dl;
  dl.begin(1280, 720);
  page.draw_classic(dl, pack, 1280, 720, 0);
  int pieces = 0;
  for (const auto& c : dl.cmds())
    for (const auto* set : {&pack.letters, &pack.letters_lit})
      for (const sm::Tone tone : {sm::Tone::full, sm::Tone::half, sm::Tone::grey})
        for (const auto& [ch, img] : sm::toned(*set, tone)) pieces += c.box.image == &img ? 1 : 0;
  CHECK(pieces == 12 + 8 + 11);
}

TEST(enhanced_pictures_draw_at_the_classic_sizes) {
  // An upgrade with four times the pixels for the glyph sheet and the main strip; every other
  // picture is the pack's, scaled up nearest to the same density.
  const auto files = sm::fixture_files();
  std::vector<std::string> asked;
  const sm::PictureUpgrade upgrade = [&asked](std::string_view name, int w, int h, Image* out) {
    asked.emplace_back(name);
    if (name != "conchars" && name != "gfx/mainmenu") return uint32_t{0};
    *out = make_image(w * 4, h * 4, {1, 1, 1, 1});
    return uint32_t{4};
  };
  sm::ClassicArt plain, art;
  std::string error;
  CHECK_MSG(sm::load_classic_art(fixture_find(files), &plain, &error), error);
  CHECK_MSG(sm::load_classic_art(fixture_find(files), &art, &error, upgrade), error);
  CHECK(art.density == 4 && plain.density == 1);
  CHECK(art.glyphs.width == 512 && art.glyphs.height == 512);
  // The pictures by the cache's names: the gfx/ pictures under gfx/, the wad's by their own.
  CHECK(std::find(asked.begin(), asked.end(), "gfx/qplaque") != asked.end());
  CHECK(std::find(asked.begin(), asked.end(), "num_0") != asked.end());
  for (std::string_view n : sm::classic_picture_names()) {
    const Image* a = art.find(n);
    const Image* p = plain.find(n);
    CHECK_MSG(a != nullptr && p != nullptr && a->width == 4 * p->width &&
                  art.width_of(*a) == static_cast<float>(p->width),
              std::string(n));
  }
  // The same layout as the pack's own.
  World w;
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .maps = {"start", "arena"}};
  g.set_art(std::move(art));
  g.show(host, "main");
  const sm::ClassicLayout a = sm::layout_classic(g.main(), g.art(), 1280, 720);
  const sm::ClassicLayout b = sm::layout_classic(g.main(), plain, 1280, 720);
  CHECK(a.items.size() == b.items.size() && a.items[0].w == b.items[0].w);
  DrawList dl;
  dl.begin(1280, 720);
  g.draw(dl, 1280, 720, 0.25);
  for (const std::string& p : g.problems()) CHECK_MSG(false, p);
}

TEST(screenshots_page_looks_through_and_deletes_the_screenshots) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "ge_se_menu_gallery";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  for (const char* name :
       {"shooters_edge-2026-11-05-101500.png", "shooters_edge-2026-11-05-101501.png"}) {
    const std::vector<uint8_t> rgba(size_t{32} * 18 * 4, 200);
    CHECK(ge::lab::write_file((dir / name).string(), ge::lab::encode_png_rgb(32, 18, rgba)));
  }
  World w;
  w.con.add_command("screenshot", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::GameMenu g;
  g.add_cvars(w.con);
  g.set_screenshots(dir.string());
  sm::Host host{.console = &w.con, .maps = {"start"}};
  g.show(host, "options");
  CHECK(!item(g.main(), "options", "screenshots")->disabled);  // lit: the console has `screenshot`
  CHECK(g.main().focus("screenshots"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(g.in_gallery() && g.gallery().count() == 2);
  CHECK(g.gallery().current()->name == "shooters_edge-2026-11-05-101501.png");  // newest first
  for (const auto& [width, height] :
       {std::pair{1280, 720}, std::pair{390, 844}, std::pair{844, 390}}) {
    DrawList dl;
    dl.begin(width, height);
    g.draw(dl, width, height, 0);
    for (const std::string& p : g.problems()) CHECK_MSG(false, p);
  }
  MenuInput del;
  del.del = true;
  g.update(del, false, host, 0.016);
  CHECK(g.gallery().mode() == ge::capture::GalleryMode::confirm);
  g.update(accept, false, host, 0.016);
  CHECK(g.gallery().count() == 1 && !fs::exists(dir / "shooters_edge-2026-11-05-101501.png"));
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);
  CHECK(!g.in_gallery() && g.open() && g.main().top().id == "options");
  w.con.execute("menu_screenshots");
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.in_gallery());
}

namespace {

size_t toy_status(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = '?';
  return 1;
}

bool toy_reply(const uint8_t* data, size_t size, ge::net::ServerInfo& out) {
  if (size < 2 || data[0] != '!') return false;
  out = {};
  out.name = std::string(reinterpret_cast<const char*>(data) + 1, size - 1);
  out.map = "keep";
  out.mode = out.name == "empty" || out.name == "far" ? "team" : "coop";
  out.players = out.name == "full" ? 8 : out.name == "empty" ? 0 : 1;
  out.max_players = 8;
  out.keys = {{"hostname", out.name}, {"*gamedir", out.name == "far" ? "fortress" : "qw"}};
  return true;
}

}  // namespace

TEST(join_a_game_lists_a_server_and_connects_to_it) {
  namespace gn = ge::net;
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  gn::Transport server, browser;
  gn::open_sim_transport(server, net, gn::sim_address(5, 27500));
  gn::open_sim_transport(browser, net, gn::sim_address(1, 27001));
  ge::console::Console con;
  std::string joined;
  con.add_command("connect", "", [&](ge::console::Console&, const ge::console::Args& a) {
    joined = a.size() > 1 ? a[1] : "";
  });
  sm::GameMenu menu;
  menu.add_cvars(con);
  sm::Host host{.console = &con};
  menu.servers().set_query({toy_status, toy_reply, nullptr, nullptr});
  menu.servers().use_transport(&browser);
  menu.servers().set_lan_ports(27500, 27500);
  menu.show(host, "multi");
  ge::ui::MenuInput accept;
  accept.accept = true;
  menu.update(accept, false, host, 0.016);  // JOIN A GAME is the page's first row
  CHECK(menu.in_servers());
  int64_t now = 0;
  uint8_t buf[64];
  gn::Address from;
  for (int i = 0; i < 60; i++) {
    now += 16000;
    gn::sim_advance(net, now);
    while (gn::transport_receive(server, buf, sizeof buf, from, now) >= 0) {
      const uint8_t reply[] = {'!', 'h', 'o', 'm', 'e'};
      gn::transport_send(server, from, reply, sizeof reply, now);
    }
    menu.update({}, false, host, 0.016);
  }
  CHECK(menu.servers().rows().size() == 1);
  CHECK(menu.servers().list().entries[0].info.name == "home");
  menu.update(accept, false, host, 0.016);
  CHECK(joined == "sim:5:27500");
  CHECK(!menu.open());
}

TEST(join_a_game_s_colours_grey_a_locked_server_and_green_a_near_one) {
  ge::net::ServerInfo in;
  ge::ui::Color c;
  CHECK(!sm::ServersPage::row_colour(in, 80, c));  // far and open: the usual colour
  CHECK(sm::ServersPage::row_colour(in, 12, c) && c.g == 1.0f && c.r == 0.0f);
  CHECK(!sm::ServersPage::row_colour(in, -1, c));  // never answered
  in.keys = {{"NoFake", "enabled"}};
  CHECK(sm::ServersPage::row_colour(in, 12, c) && c.r == c.g && c.r < 0.6f);  // grey wins
  in.keys = {{"anticheat", "1"}};
  CHECK(sm::ServersPage::row_colour(in, 80, c) == false);
  in.keys = {{"AntiCheat", "2"}};
  CHECK(sm::ServersPage::row_colour(in, 80, c) && c.r == c.b);
  in.keys = {{"needpass", "1"}};
  CHECK(sm::ServersPage::row_colour(in, 80, c));
  in.keys.clear();
  in.locked = true;
  CHECK(sm::ServersPage::row_colour(in, 80, c));
  ge::console::Console con;
  sm::GameMenu menu;
  menu.add_cvars(con);
  CHECK(con.cvars().find("sb_colorservers")->value == "0");
}

TEST(join_a_game_s_filters_hide_full_and_empty_servers_a_mode_and_a_ping_kept_as_cvars) {
  namespace gn = ge::net;
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  // Three servers: "full" (8 of 8, coop), "empty" (0 of 8, team) and "far" (1 of 8, team, 120 ms
  // away).
  gn::Transport server[3], browser;
  for (uint32_t k = 0; k < 3; k++)
    gn::open_sim_transport(server[k], net, gn::sim_address(5 + k, 27500));
  gn::open_sim_transport(browser, net, gn::sim_address(1, 27001));
  gn::LinkConditions far;
  far.latency_ms = 60;
  gn::sim_set_path(net, 1, 7, far);
  ge::console::Console con;
  con.add_command("connect", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::GameMenu menu;
  menu.add_cvars(con);
  sm::Host host{.console = &con};
  menu.servers().set_query({toy_status, toy_reply, nullptr, nullptr});
  menu.servers().use_transport(&browser);
  menu.servers().set_lan_ports(27500, 27500);
  con.execute("menu_servers");
  int64_t now = 0;
  uint8_t buf[64];
  gn::Address from;
  const auto run = [&](const ge::ui::MenuInput& in) {
    now += 16000;
    gn::sim_advance(net, now);
    for (int k = 0; k < 3; k++)
      while (gn::transport_receive(server[k], buf, sizeof buf, from, now) >= 0) {
        const char* names[] = {"!full", "!empty", "!far"};
        gn::transport_send(server[k], from, reinterpret_cast<const uint8_t*>(names[k]),
                           std::strlen(names[k]), now);
      }
    menu.update(in, false, host, 0.016);
  };
  for (int i = 0; i < 90; i++) run({});
  const sm::ServersPage& page = menu.servers();
  CHECK(page.rows().size() == 3);
  CHECK(page.filter_words().empty() && page.hidden() == 0);
  const auto key = [&](char c) {
    ge::ui::MenuInput in;
    in.letter = c;
    run(in);
  };
  const auto shown = [&] {
    std::vector<std::string> out;
    for (int r : page.rows()) out.push_back(page.list().entries[static_cast<size_t>(r)].info.name);
    std::sort(out.begin(), out.end());
    return out;
  };
  key('f');
  CHECK(shown() == (std::vector<std::string>{"empty", "far"}));
  CHECK(con.cvars().find("sb_hidefull")->value == "1");
  CHECK(page.hidden() == 1 && page.filter_words() == "NOT FULL");
  key('e');
  CHECK(shown() == (std::vector<std::string>{"far"}));
  CHECK(page.filter_words() == "NOT FULL OR EMPTY");
  key('f');
  key('e');
  key('m');  // the modes A to Z: coop first
  CHECK(con.cvars().find("sb_mode")->value == "coop");
  CHECK(shown() == (std::vector<std::string>{"full"}));
  key('m');
  CHECK(shown() == (std::vector<std::string>{"empty", "far"}));
  key('m');  // past the last: every mode again
  CHECK(con.cvars().find("sb_mode")->value.empty() && page.rows().size() == 3);
  key('p');  // under 50 ms: the far one goes
  CHECK(con.cvars().find("sb_maxping")->value == "50");
  CHECK(shown() == (std::vector<std::string>{"empty", "full"}));
  for (int i = 0; i < 4; i++) key('p');  // 100, 150, 250, then any
  CHECK(con.cvars().find("sb_maxping")->value == "0" && page.rows().size() == 3);
  // The other filters, from the console: only the empty, the ping limit, the info filter.
  con.execute("sb_hidenotempty 1");
  run({});
  CHECK(shown() == (std::vector<std::string>{"empty"}) && page.filter_words() == "EMPTY ONLY");
  con.execute("sb_hidenotempty 0; sb_hidehighping 1; sb_pinglimit 80");
  run({});
  CHECK(shown() == (std::vector<std::string>{"empty", "full"}));
  CHECK(page.filter_words() == "PING UNDER 80");
  con.execute("sb_hidehighping 0; sb_info_filter \"-*gamedir=FORT\"");
  run({});
  CHECK(shown() == (std::vector<std::string>{"empty", "full"}));
  con.execute("sb_info_filter \"\"");
  // The sort: by codes; RIGHT puts the next column first, D turns it round.
  con.execute("sb_sortservers 6");
  run({});
  CHECK(page.sort_words() == "players");
  CHECK(page.list().entries[static_cast<size_t>(page.rows().front())].info.name == "empty");
  key('d');
  CHECK(con.cvars().find("sb_sortservers")->value == "-6" &&
        page.sort_words() == "players (high to low)");
  CHECK(page.list().entries[static_cast<size_t>(page.rows().front())].info.name == "full");
  {
    ge::ui::MenuInput right;
    right.right = true;
    run(right);
  }
  CHECK_MSG(con.cvars().find("sb_sortservers")->value == "16",
            con.cvars().find("sb_sortservers")->value);
  con.execute(
      "sb_sortservers 32; sb_showaddress 1; sb_showgamedir 1; sb_showfraglimit 1; "
      "sb_showtimelimit 1; sb_status 0");
  run({});
  CHECK(page.sort_words() == "ping, then address" && page.hide_on_join());
  for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
    ge::ui::DrawList dl;
    dl.begin(w, h);
    menu.draw(dl, w, h, 0);
    for (const std::string& p : menu.problems()) CHECK_MSG(false, p);
  }
  con.execute("sb_autohide 0");
  CHECK(!page.hide_on_join());
  // Set from the console, as a config does; every server hidden says so and draws in bounds.
  con.execute("sb_mode duel");
  run({});
  CHECK(page.rows().empty() && page.hidden() == 3);
  for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
    ge::ui::DrawList dl;
    dl.begin(w, h);
    menu.draw(dl, w, h, 0);
    for (const std::string& p : menu.problems()) CHECK_MSG(false, p);
  }
}

namespace {

// The routes test's world: "far" (60 ms each way) and "hop", a forwarder that reaches far in 15.
bool route_reply(const uint8_t* data, size_t size, ge::net::ServerInfo& out) {
  if (!toy_reply(data, size, out)) return false;
  if (out.name == "hop") out.mode = "forward";
  return true;
}
bool route_is_forwarder(const ge::net::ServerInfo& info) { return info.mode == "forward"; }
size_t route_ask(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = 'A';
  return 1;
}
bool route_read(const uint8_t* data, size_t size, ge::forward::Reach& out) {
  if (size != 1 || data[0] != 'R') return false;
  out.emplace_back(ge::net::sim_address(7, 27500), 15);
  return true;
}

}  // namespace

TEST(join_a_game_finds_a_route_through_a_forwarder_and_joins_by_it) {
  namespace gn = ge::net;
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  gn::Transport far, hop, browser, routes;
  gn::open_sim_transport(far, net, gn::sim_address(7, 27500));
  gn::open_sim_transport(hop, net, gn::sim_address(6, 27500));
  gn::open_sim_transport(browser, net, gn::sim_address(1, 27001));
  gn::open_sim_transport(routes, net, gn::sim_address(1, 27002));
  gn::LinkConditions slow;
  slow.latency_ms = 60;
  gn::sim_set_path(net, 1, 7, slow);
  ge::console::Console con;
  std::string joined, chain;
  con.add_command("connect", "", [&](ge::console::Console&, const ge::console::Args& a) {
    joined = a.size() > 1 ? a[1] : "";
  });
  con.cvars().bind({.name = "cl_proxyaddr", .fallback = "", .kind = ge::console::CvarKind::text},
                   &chain);
  sm::GameMenu menu;
  menu.add_cvars(con);
  con.execute("sb_findroutes 1");
  sm::Host host{.console = &con};
  menu.servers().set_query({toy_status, route_reply, nullptr, nullptr});
  menu.servers().set_routes({route_is_forwarder, route_ask, route_read});
  menu.servers().use_transport(&browser);
  menu.servers().use_route_transport(&routes);
  menu.servers().set_lan_ports(27500, 27500);
  con.execute("menu_servers");
  int64_t now = 0;
  uint8_t buf[64];
  gn::Address from;
  const auto run = [&](const ge::ui::MenuInput& in) {
    now += 16000;
    gn::sim_advance(net, now);
    while (gn::transport_receive(far, buf, sizeof buf, from, now) >= 0)
      gn::transport_send(far, from, reinterpret_cast<const uint8_t*>("!far"), 4, now);
    while (gn::transport_receive(hop, buf, sizeof buf, from, now) >= 0) {
      if (buf[0] == '?')
        gn::transport_send(hop, from, reinterpret_cast<const uint8_t*>("!hop"), 4, now);
      else if (buf[0] == 'A')
        gn::transport_send(hop, from, reinterpret_cast<const uint8_t*>("R"), 1, now);
    }
    menu.update(in, false, host, 0.016);
  };
  for (int i = 0; i < 400 && !menu.servers().routes_ready(); i++) run({});
  const sm::ServersPage& page = menu.servers();
  CHECK(page.routes_ready());
  int far_index = -1, far_row = -1;
  for (size_t i = 0; i < page.list().entries.size(); i++)
    if (page.list().entries[i].info.name == "far") far_index = static_cast<int>(i);
  for (size_t r = 0; r < page.rows().size(); r++)
    if (page.rows()[r] == far_index) far_row = static_cast<int>(r);
  CHECK(far_index >= 0 && far_row >= 0 && page.rows().size() == 2);
  const ge::forward::Route r = page.route_to(far_index);
  CHECK(r.found && r.forwarders.size() == 1 && r.forwarders[0] == gn::sim_address(6, 27500));
  CHECK(r.ping_ms < r.direct_ms);
  // A join from the page goes through the forwarder: cl_proxyaddr names it, connect the server.
  ge::ui::MenuInput down;
  down.down = true;
  while (page.selected() != far_row) run(down);
  ge::ui::MenuInput accept;
  accept.accept = true;
  run(accept);
  CHECK(joined == "sim:7:27500");
  CHECK(chain == "sim:6:27500");
  // sb_showproxies 0 hides the forwarder; 2 lists it alone.
  con.execute("sb_showproxies 0");
  con.execute("menu_servers");
  for (int i = 0; i < 60; i++) run({});
  CHECK(menu.servers().rows().size() == 1);
}

TEST(join_a_game_without_a_protocol_says_so) {
  ge::console::Console con;
  con.add_command("connect", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::GameMenu menu;
  menu.add_cvars(con);
  sm::Host host{.console = &con};
  con.execute("menu_servers");
  menu.update({}, false, host, 0.016);
  CHECK(menu.in_servers());
  CHECK(!menu.servers().note().empty());
  ge::ui::DrawList dl;
  dl.begin(1280, 720);
  menu.draw(dl, 1280, 720, 0);
  CHECK(menu.problems().empty());
  ge::ui::MenuInput back;
  back.back = true;
  menu.update(back, false, host, 0.016);
  CHECK(!menu.in_servers());
  CHECK(menu.open());
}

TEST(demos_lists_a_folder_s_recordings_and_plays_one_that_checks) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "ge_se_menu_demos";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "older");
  for (const char* name : {"good.mvd.gz", "bad.dem", "notes.zip", "readme.txt"})
    std::ofstream(dir / name) << name;
  const std::vector<std::string> kExt = {".mvd", ".dem"};
  World w;
  w.con.add_command("playdemo", "", [](ge::console::Console&, const ge::console::Args&) {});
  sm::GameMenu g;
  g.add_cvars(w.con);
  g.demos().set_extensions(kExt);
  g.demos().set_game_places({{"GAME", dir.string()}});
  std::vector<std::string> checked;
  g.demos().set_check([&](const std::string& path, std::string& facts, std::string& why) {
    checked.push_back(path);
    if (path.ends_with("bad.dem")) {
      why = "not a demo: no format starts like this";
      return false;
    }
    facts = "MULTIVIEW";
    return true;
  });
  sm::Host host{.console = &w.con};
  g.show(host);
  CHECK(!item(g.main(), "main", "demos")->disabled);  // lit: the console has `playdemo`
  CHECK(g.main().focus("demos"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(g.in_demos());
  // The game's folder first: `..`, the folder, the two recordings; never the zip of nothing or
  // the readme.
  const auto& rows = g.demos().browser().entries();
  CHECK(rows.size() == 4);
  CHECK(rows[1].name == "older" && rows[1].folder);
  for (const auto& [width, height] :
       {std::pair{1280, 720}, std::pair{390, 844}, std::pair{844, 390}}) {
    DrawList dl;
    dl.begin(width, height);
    g.draw(dl, width, height, 0);
    for (const std::string& p : g.problems()) CHECK_MSG(false, p);
  }
  // The arrows to the row named `name`.
  const auto select = [&](std::string_view name) {
    int at = -1;
    for (size_t i = 0; i < rows.size(); i++)
      if (rows[i].name == name) at = static_cast<int>(i);
    CHECK(at >= 0);
    for (int guard = 0; guard < 8 && g.demos().browser().selected() != at; guard++) {
      MenuInput step;
      (g.demos().browser().selected() < at ? step.down : step.up) = true;
      g.update(step, false, host, 0.016);
    }
    CHECK(g.demos().browser().selection()->name == name);
  };
  // A file the check refuses: the reason shown, nothing played, the page still up.
  select("bad.dem");
  g.update(accept, false, host, 0.016);
  CHECK(g.in_demos() && g.take_demo().empty());
  CHECK(g.demos().browser().note().find("not a demo") == 0);
  // One it passes: handed to the window, the menu closed, the folder kept for next time.
  select("good.mvd.gz");
  CHECK(g.demos().browser().note().empty());
  g.update(accept, false, host, 0.016);
  CHECK(!g.open());
  CHECK(g.take_demo() == (dir / "good.mvd.gz").string());
  CHECK(w.con.cvars().find("menu_demo_dir")->value == dir.string());
  // `menu_demos` opens it again, on that folder; BACKSPACE goes up a folder; ESC back to the root.
  w.con.execute("menu_demos");
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.in_demos() && g.demos().browser().dir() == dir.string());
  MenuInput del;
  del.del = true;
  g.update(del, false, host, 0.016);
  CHECK(g.demos().browser().dir() == dir.parent_path().string());
  CHECK(g.demos().browser().selection()->name == dir.filename().string());
  MenuInput back;
  back.back = true;
  g.update(back, false, host, 0.016);
  CHECK(!g.in_demos() && g.open() && g.main().top().id == "main");
  fs::remove_all(dir, ec);
}

TEST(q2_menu_has_its_own_pages_flags_keys_and_in_game_page) {
  World w;
  w.con.add_command("newgame", "", [&w](ge::console::Console&, const ge::console::Args&) {
    w.ran.push_back("newgame");
  });
  w.con.add_command("weapnext", "", [](ge::console::Console&, const ge::console::Args&) {});
  w.con.add_command("use", "", [](ge::console::Console&, const ge::console::Args&) {});
  w.con.cvars().add({.name = "dmflags", .fallback = "0"});
  w.con.cvars().add({.name = "cl_disable_explosions", .fallback = "0"});
  w.con.cvars().add({.name = "cl_noglow", .fallback = "0"});
  sm::GameMenu g;
  g.add_cvars(w.con);
  sm::Host host{.console = &w.con, .table = &sm::q2_menu()};
  g.set_table(sm::q2_menu());
  for (const char* page : {"gameflags", "input", "keys", "legacykeys", "weapons", "sound",
                           "effects", "railtrail", "screen", "crosshair", "downloads", "game"})
    CHECK(g.main().page(page) != nullptr);
  // The game page: a skill, then a new game.
  g.show(host, "single");
  CHECK(item(g.main(), "single", "nightmare") != nullptr);
  CHECK(sm::apply_event({.kind = EventKind::activated, .page = "single", .item = "hard"}, g.main(),
                        host) == sm::Outcome::close);
  CHECK(w.ran.size() >= 2 && w.ran[w.ran.size() - 2] == "sv skill 2" && w.ran.back() == "newgame");
  // A flag row toggles one bit; "allow health" is bit 0 cleared.
  g.show(host, "gameflags");
  CHECK(item(g.main(), "gameflags", "health")->on);
  g.main().page("gameflags")->find("weaponsstay")->on = true;
  sm::apply_event({.kind = EventKind::changed, .page = "gameflags", .item = "weaponsstay"},
                  g.main(), host);
  g.main().page("gameflags")->find("health")->on = false;
  sm::apply_event({.kind = EventKind::changed, .page = "gameflags", .item = "health"}, g.main(),
                  host);
  CHECK(w.ran.back() == "sv dmflags 1");  // the flags go to the server, read from here
  // An inverted client toggle and a bit of a client cvar.
  g.show(host, "effects");
  CHECK(item(g.main(), "effects", "cl_noglow")->on);  // glowing: yes while cl_noglow is 0
  g.main().page("effects")->find("rockets")->on = false;
  sm::apply_event({.kind = EventKind::changed, .page = "effects", .item = "rockets"}, g.main(),
                  host);
  CHECK(w.con.cvars().find("cl_disable_explosions")->value == "2");
  // A key row shows its keys and binds the next one.
  w.con.execute("bind q weapnext; bind mwheelup weapnext");
  g.show(host, "keys");
  CHECK(item(g.main(), "keys", "weapnext")->choices[0] == "mwheelup or q");
  CHECK(item(g.main(), "keys", "inven")->choices[0] == "???");
  CHECK(item(g.main(), "keys", "inven")->disabled);  // no `inven` in this console
  g.show(host, "weapons");
  CHECK(g.main().focus("railgun"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(g.waiting_key());
  g.update({}, false, host, 0.016);
  g.press_key("r", host);
  CHECK(w.con.binds().get(ge::console::BindMode::game, "r") == "use Railgun");
  // ESCAPE in a level opens the in-game page.
  g.hide();
  host.in_level = true;
  g.update({}, true, host, 0.016);
  CHECK(g.open() && g.main().top().id == "game");
  // With the help computer up, ESCAPE puts it away and opens nothing.
  g.hide();
  w.con.add_command("putaway", "", [&w](ge::console::Console&, const ge::console::Args&) {
    w.ran.push_back("putaway");
  });
  host.screen_up = true;
  g.update({}, true, host, 0.016);
  CHECK(!g.open() && !w.ran.empty() && w.ran.back() == "putaway");
}

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
