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
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "lab/png.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "shooters_edge/menu/carved_letters.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_console.hpp"
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
  CHECK(root.items.size() == 6);  // no HELP: its pages show only at the free episode's end
  const char* rows[] = {"games", "single", "multi", "demos", "options", "quit"};
  for (size_t i = 0; i < 6; i++) CHECK(root.items[i].id == rows[i]);
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

// GAME TYPE COOPERATIVE with a campaign to play starts a coop game on the map, so bots may join;
// DEATHMATCH starts the map as a match.
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
        {.id = "classic", .install = "/a/game", .edition = "Shareware", .playing = true},
        {.id = "sequel", .install = "/b/game", .edition = "Full game", .playing = false},
    };
  });
  w.con.execute("menu_games");
  g.update({}, false, host, 0.016);
  CHECK(g.open() && g.in_games() && listed == 1);
  // The names are pictures, never words: each game's decodes, an unknown id's is empty.
  CHECK(!sm::game_name_picture("classic").rgba.empty());
  CHECK(!sm::game_name_picture("sequel").rgba.empty());
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
  CHECK(ask.asked && !ask.browse && ask.id == "sequel" && ask.install == "/b/game");
  CHECK(!g.take_game().asked);  // once
  // FIND ANOTHER INSTALL, the last row: the folder chooser.
  g.update(down, false, host, 0.016);
  g.update(accept, false, host, 0.016);
  ask = g.take_game();
  CHECK(ask.asked && ask.browse && ask.id.empty());
  // A game never found: its folder is asked for.
  g.games().set_lister(
      [] { return std::vector<sm::GameRow>{{.id = "classic", .install = "", .edition = ""}}; });
  g.show(host, "games");
  g.update(accept, false, host, 0.016);
  ask = g.take_game();
  CHECK(ask.asked && ask.browse && ask.id == "classic");
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
  CHECK(item(m, "load", "s0")->text == sm::kEmptySlot);
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
          "ce_weather", "help",     "quit",     "load",   "save",     "games"}) {
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
    for (const auto& c : dl.cmds())
      for (const auto& [ch, img] : a.letters) n += c.box.image == &img ? 1 : 0;
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
  CHECK(pack.letters.size() == 15);
  // Every letter its own columns, on the 24 px line (the tails below the 20 px row).
  for (const auto& [ch, img] : pack.letters) CHECK(img.height == 24 && img.width > 0);
  CHECK(root(art, nullptr).second == 0);
  CHECK(root(pack, nullptr).second == 5 + 10);  // DEMOS, SELECT GAME (the space no picture)
  CHECK(root(pack, &pack.glyphs).first < root(art, &art.glyphs).first);
  CHECK(sm::carved_text_width(pack, "Demos") > 0 && sm::carved_text_width(pack, "Demos!") == 0);

  // SELECT GAME's title and the games' names: a picture a letter, five for the first game and
  // seven for the sequel's (its numeral); the stand-in's draw none and fall back to the pictures.
  const auto names = [&](const sm::ClassicArt& a) {
    sm::GamesPage page;
    page.set_games({{.id = "classic", .install = "", .edition = ""},
                    {.id = "sequel", .install = "", .edition = ""}});
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

TEST(the_sequel_s_menu_is_drawn_in_its_plates_and_banners) {
  // The sequel's pictures at their sizes, painted flat: the plates, their lit copies, the plaque,
  // the logo, the cursor's frames and the banners.
  sm::ClassicArt art;
  art.glyphs = make_image(128, 128, hex(0xffffff));
  const std::pair<const char*, int> plates[] = {{"m_main_game", 99},
                                                {"m_main_multiplayer", 215},
                                                {"m_main_options", 150},
                                                {"m_main_quit", 100},
                                                {"m_main_video", 118}};
  for (const auto& [name, w] : plates) {
    art.pictures.emplace(name, make_image(w, 32, hex(0x404040)));
    art.pictures.emplace(std::string(name) + "_sel", make_image(w, 32, hex(0x606060)));
  }
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
  CHECK(l.items[1].w == 99 * 3);  // SINGLE PLAYER is the sequel's GAME plate
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
  pack.strip_print = 0xfd50f20825e19f40ULL;
  CHECK(sm::carved_width(art, "games") == 0 && sm::carved_width(pack, "games") == 206);
  CHECK(glyphs(art) == 10 + 5);  // SELECT GAME (no space drawn), DEMOS
  CHECK(glyphs(pack) == 0);

  // A page under the root takes its banner; SELECT GAME's title and the names are plates: 17
  // pieces for the title, 8 for the first game, 13 for the sequel's.
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
  page.set_games({{.id = "classic", .install = "", .edition = ""},
                  {.id = "sequel", .install = "", .edition = ""}});
  DrawList dl;
  dl.begin(1280, 720);
  page.draw_classic(dl, pack, 1280, 720, 0);
  int pieces = 0;
  for (const auto& c : dl.cmds())
    for (const auto& [name, img] : pack.pictures)
      if (c.box.image == &img && name.rfind("m_main_", 0) == 0) pieces++;
  CHECK(pieces == 17 + 8 + 13);
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
  out.mode = "coop";
  out.players = 1;
  out.max_players = 8;
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
