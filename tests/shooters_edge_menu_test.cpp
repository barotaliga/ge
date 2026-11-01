// Shooter's Edge's main menu (games/shooters_edge/menu): the classic pages and their rows; a row
// dimmed while the console knows nothing it needs and lit once it does; a setting read from its
// cvar and written by a console line; NEW GAME's START running the rules on the server and the
// map; ESCAPE opening the menu and going back out of it; the classic pictures read from a pack's
// files and refused without them; the classic layout's rows on the 320 x 200 screen; the modern
// and the classic looks drawing every page clean; LOAD and SAVE's slots, their lines and the
// console lines they run.
//
// ge:summary Tests the main menu: pages, dimming by the console, cvars, START, ESCAPE, both looks
// ge:keywords test menu main-menu dimmed cvar console new-game start escape classic modern layout
// ge:skill ui testing
#include <algorithm>
#include <cmath>
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
  for (const char* row : {"enhance", "art", "light", "liquids", "fire"})
    CHECK_MSG(!item(g.main(), "enhanced", row)->disabled, row);
  CHECK(item(g.main(), "enhanced", "weather")->disabled);  // no sv_weather on this console
  g.show(host, "ce_liquids");
  CHECK(item(g.main(), "ce_liquids", "ce_water")->on);
  CHECK(g.main().focus("ce_water"));
  MenuInput accept;
  accept.accept = true;
  g.update(accept, false, host, 0.016);
  CHECK(w.con.cvars().find("ce_water")->value == "0");
  g.show(host, "enhanced");
  CHECK(g.main().focus("enhance"));
  g.update(accept, false, host, 0.016);
  CHECK(g.in_enhance());
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
    for (const char* page : {"main", "single", "multi", "host", "setup", "options", "video",
                             "weapon", "controls", "enhanced", "ce_art", "ce_light", "ce_liquids",
                             "ce_fire", "ce_weather", "help", "quit", "load", "save"}) {
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
