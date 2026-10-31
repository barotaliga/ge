// LAB `se-menu`: Shooter's Edge's main menu (games/shooters_edge/menu) LOOKED AT, on the CPU's 2D
// layer over a painted arena standing in for the game under it, page by page at each viewport:
//
//   - MODERN: the engine's menu view, the game's name over the column, every row the game cannot
//     do yet dimmed (the console knows no cvar or command it needs), and classic-enhance's own
//     menu mounted under OPTIONS;
//   - CLASSIC: the same pages in the pack's own pictures on the 320 x 200 screen: the plaque, the
//     titles, the strips with the spinning cursor, the glyph rows with their values, the help
//     pages and the quit box. The lab's pictures are the fixture set painted in code
//     (classic_art.hpp); `--pack=<path>` (the release zip, an installed folder or its pack) draws
//     them again in the player's own pack. Those pictures are the pack's: they stay on this
//     machine and on a private page, and are left out of the digest.
//
// THE CAMPAIGN'S SCREENS (level_screens.hpp) are drawn too, in the engine's type, the fixture and
// the pack: the intermission, an episode's finale typed part way, the solo panel.
//
// The console is set up as the window sets it up (the mouse's cvars, the HUD's style, the menu's
// own cvars and the ce_ switches, `map`, `sv`, `quit` and `toggleconsole`), so the rows dimmed in
// the pictures are the rows dimmed in the game. Every layout is held to the view's problems check
// (`layout_problems`, `classic_problems`).
//
// ge:summary Lab: the main menu's pages in the modern and the classic look at three viewports
// ge:keywords lab se-menu menu main-menu classic modern dimmed pages viewports pack pictures
// ge:keywords intermission finale solo-panel level-screens campaign
// ge:skill ui labs
// ge:test tests/shooters_edge_menu_test.cpp tests/shooters_edge_level_screens_test.cpp
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "classicenhance/source.hpp"
#include "console/console.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace ui = ge::ui;
namespace sm = shooters_edge::menu;

// The parts in one string, appended into one buffer.
std::string cat(std::initializer_list<std::string_view> parts) {
  std::string out;
  for (std::string_view p : parts) out += p;
  return out;
}

struct Viewport {
  int w, h;
  std::string name() const { return std::to_string(w) + "x" + std::to_string(h); }
};

std::vector<Viewport> parse_viewports(const std::string& text) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find(',', at);
    if (end == std::string::npos) end = text.size();
    const std::string one = text.substr(at, end - at);
    const size_t x = one.find('x');
    if (x != std::string::npos) {
      const int w = std::atoi(one.substr(0, x).c_str()), h = std::atoi(one.substr(x + 1).c_str());
      if (w > 0 && h > 0) out.push_back({w, h});
    }
    at = end + 1;
  }
  return out;
}

// A dim stone arena with a lit doorway: the game the menu darkens.
ui::Image arena(int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  ui::Box b;
  const ui::Stop wall[2] = {{0, ui::hex(0x2a2420)}, {1, ui::hex(0x4a3e34)}};
  b.rect = {0, 0, fw, fh * 0.6f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, wall, 2);
  dl.box(b);
  const ui::Stop floor[2] = {{0, ui::hex(0x3a3028)}, {1, ui::hex(0x16120e)}};
  b.rect = {0, fh * 0.6f, fw, fh * 0.4f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, floor, 2);
  dl.box(b);
  const ui::Stop door[2] = {{0, ui::hex(0xffd9a0)}, {1, ui::hex(0xa86a30)}};
  b.rect = {fw * 0.58f, fh * 0.22f, fw * 0.12f, fh * 0.38f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, door, 2);
  b.radius = fw * 0.02f;
  dl.box(b);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

// The window's console: what the menu's rows read and run.
struct Game {
  ge::console::Console con;
  shooters_edge::client::ControlSettings controls;
  shooters_edge::hud::Hud hud = shooters_edge::hud::make_hud();
  ge::console::Cvars server{true};
  sm::GameMenu menu;
  sm::Host host;
  Game() {
    using CArgs = ge::console::Args;
    for (const char* c : {"map", "sv", "quit", "newgame"})
      con.add_command(c, "", [](ge::console::Console&, const CArgs&) {});
    con.add_command("toggleconsole", "", [](ge::console::Console&, const CArgs&) {});
    shooters_edge::client::add_cvars(con, controls);
    shooters_edge::hud::add_cvars(con, hud);
    menu.add_cvars(con);
    for (const char* c : {"deathmatch", "fraglimit", "timelimit", "teamplay", "skill"})
      server.add({.name = c, .fallback = "0"});
    server.set("deathmatch", "1", ge::console::Source::local);
    server.set("fraglimit", "20", ge::console::Source::local);
    server.set("timelimit", "15", ge::console::Source::local);
    host = {.console = &con, .server = &server, .maps = {"start", "e1m1", "dm6", "arena"}};
    menu.set_packs({"id1/pak0.pak"});
  }
};

struct PageShot {
  const char* page;
  const char* focus;  // the row with the focus ("" the first)
  const char* caption;
};

constexpr PageShot kPages[] = {
    {"main", "multi",
     "The root: SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP and QUIT, the focus on MULTIPLAYER."},
    {"single", "",
     "SINGLE PLAYER: NEW GAME runs `newgame` (the packs' single-player program from the map "
     "start); LOAD and SAVE are dimmed until saves land."},
    {"multi", "host",
     "MULTIPLAYER: JOIN A GAME dimmed until the client plays over the network; NEW GAME and "
     "SETUP."},
    {"host", "fraglimit",
     "NEW GAME: the server's rules read from its cvars (deathmatch 1, 20 frags, 15 minutes); "
     "MAX PLAYERS, TEAMPLAY and SKILL dimmed (no remote players yet, the baseline mod reads "
     "neither)."},
    {"options", "sensitivity",
     "OPTIONS: GO TO CONSOLE, RESET TO DEFAULTS, MOUSE SPEED and INVERT MOUSE work; the rows "
     "whose cvar the game has not got are dimmed."},
    {"video", "hud_style",
     "VIDEO: the HUD's look and the menu's look (WORLD LOOK lights up once `r_look` is a cvar)."},
    {"help", "",
     "HELP: the keys (in the classic look, the pack's help pages, turned with the "
     "arrows)."},
    {"quit", "", "QUIT: the question, YES quits and NO goes back."},
};

// The pack at `path` as the menu's files.
struct PackFiles {
  ge::classicenhance::Source src;
  ge::vfs::Pak dir;
  sm::FindFile find() const {
    return [this](std::string_view p, std::vector<uint8_t>&) -> std::span<const uint8_t> {
      const ge::vfs::PakEntry* e = ge::vfs::pak_find(dir, p);
      return e ? ge::vfs::pak_file(src.pack, *e) : std::span<const uint8_t>{};
    };
  }
};

void run(ge::lab::Lab& lab) {
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=se-menu ARGS=--viewports=1280x720,390x844");
    return;
  }
  // The sets of classic pictures: the fixture always, the player's pack when named.
  struct Set {
    std::string name;
    sm::ClassicArt art;
    ge::lab::Det det;
  };
  std::vector<Set> sets;
  {
    const auto files = sm::fixture_files();
    sm::ClassicArt art;
    std::string error;
    const sm::FindFile find = [&files](std::string_view p,
                                       std::vector<uint8_t>&) -> std::span<const uint8_t> {
      const auto it = files.find(p);
      return it == files.end() ? std::span<const uint8_t>{} : std::span<const uint8_t>(it->second);
    };
    if (!sm::load_classic_art(find, &art, &error)) {
      lab.fail("the fixture's pictures did not load: " + error,
               "check fixture_files in games/shooters_edge/menu/classic_art.cpp. Skill: ui.");
      return;
    }
    sets.push_back({"fixture", std::move(art), ge::lab::Det::deterministic});
  }
  PackFiles pack;
  if (!lab.arg("pack").empty()) {
    std::string error;
    sm::ClassicArt art;
    if (!ge::classicenhance::open_source(lab.arg("pack"), &pack.src, &error) ||
        !ge::vfs::read_pak(pack.src.pack, &pack.dir, &error) ||
        !sm::load_classic_art(pack.find(), &art, &error))
      lab.fail("the pack's menu pictures did not load: " + error,
               "give --pack the release zip, an installed folder or its pak0.pak. Skill: ui.");
    else
      sets.push_back({"pack", std::move(art), ge::lab::Det::measured});
  }

  int problems = 0;
  const auto snap = [&](Game& g, const Viewport& v, const std::string& id, const std::string& title,
                        const std::string& caption, const std::string& look, const std::string& art,
                        ge::lab::Det det) {
    ui::DrawList dl;
    dl.begin(v.w, v.h);
    g.menu.settle();
    g.menu.draw(dl, v.w, v.h, 0.25);
    for (const std::string& p : g.menu.problems()) {
      problems++;
      lab.fail(cat({look, " look at ", v.name(), ", ", id, ": ", p}),
               "shorten the row in games/shooters_edge/menu/main_menu.cpp (kEntries) or fix the "
               "view; rerun `make lab LAB=se-menu`. Skill: ui.");
    }
    ui::Image img = arena(v.w, v.h);
    ui::render(dl, img);
    lab.shot({id,
              title,
              caption,
              "the dimmed rows faded and skipped by the focus; nothing off the screen; the values "
              "the cvars hold",
              {{"look", look},
               {"art", art},
               {"viewport", v.name()},
               {"problems", std::to_string(g.menu.problems().size())}}},
             picture_of(std::move(img)), det);
  };

  // The modern look, then the classic look in each set of pictures.
  for (int pass = -1; pass < static_cast<int>(sets.size()); pass++) {
    const bool classic = pass >= 0;
    const std::string look = classic ? "classic" : "modern";
    const std::string art = classic ? sets[static_cast<size_t>(pass)].name : "none";
    const ge::lab::Det det =
        classic ? sets[static_cast<size_t>(pass)].det : ge::lab::Det::deterministic;
    for (const Viewport& v : viewports) {
      if (classic && v.h > v.w) continue;  // the classic screen is landscape
      Game g;
      if (classic) g.menu.set_art(sets[static_cast<size_t>(pass)].art);
      int dimmed = 0, rows = 0;
      for (const PageShot& ps : kPages) {
        g.menu.show(g.host, ps.page);
        if (*ps.focus) g.menu.main().focus(ps.focus);
        for (const ui::Item& it : g.menu.main().top().items) {
          if (it.kind == ui::ItemKind::label || it.kind == ui::ItemKind::back) continue;
          rows++;
          dimmed += it.disabled ? 1 : 0;
        }
        const std::string id =
            cat({look, classic ? "-" : "", classic ? art : "", "-", ps.page, "-", v.name()});
        snap(g, v, id, cat({"THE ", look, " MENU, ", ps.page, ", AT ", v.name()}), ps.caption, look,
             art, det);
      }
      if (classic) {
        // Two more help pages, as the arrows turn them.
        g.menu.show(g.host, "help");
        ui::MenuInput right;
        right.right = true;
        g.menu.update(right, false, g.host, 1.0 / 60);
        snap(g, v, cat({look, "-", art, "-help-2-", v.name()}), "THE CLASSIC HELP, PAGE 2",
             "The help pages are the pack's own pictures, one at a time.", look, art, det);
      } else {
        // Classic enhance's own menu, mounted under OPTIONS.
        g.menu.show(g.host, "options");
        g.menu.main().focus("classic");
        ui::MenuInput accept;
        accept.accept = true;
        g.menu.update(accept, false, g.host, 1.0 / 60);
        snap(g, v, "modern-classic-enhance-" + v.name(), "OPTIONS: CLASSIC ENHANCE",
             "Classic enhance's own menu in the main menu's place: the pack, ENHANCE, the "
             "switches. Back at its root returns to OPTIONS.",
             look, art, det);
      }
      if (pass <= 0) lab.metric(look + "." + v.name() + ".dimmed-rows", dimmed, "count");
      if (pass <= 0) lab.metric(look + "." + v.name() + ".rows", rows, "count");
    }
  }
  lab.metric("problems", problems, "count");

  // The campaign's screens (level_screens.hpp) over the game: in the engine's type, then in each
  // set of pictures.
  const sm::LevelStats stats{
      .minutes = 3, .seconds = 7, .secrets = 2, .total_secrets = 6, .kills = 41, .total_kills = 52};
  // A stand-in episode's end, written for the lab.
  constexpr std::string_view kFinale =
      "The last gate folds shut behind you and\nthe hall falls silent. The stone in your\n"
      "hand is warm, and somewhere below the\nfloor a bell rings once. One of four.\n"
      "Three more wait beyond the dark.";
  for (int pass = -1; pass < static_cast<int>(sets.size()); pass++) {
    const sm::ClassicArt none;
    const sm::ClassicArt& art = pass < 0 ? none : sets[static_cast<size_t>(pass)].art;
    const std::string name = pass < 0 ? "none" : sets[static_cast<size_t>(pass)].name;
    const ge::lab::Det det =
        pass < 0 ? ge::lab::Det::deterministic : sets[static_cast<size_t>(pass)].det;
    for (const Viewport& v : viewports) {
      if (v.h > v.w) continue;
      const auto screen = [&](const char* what, const std::string& title,
                              const std::string& caption, const auto& draw) {
        ui::DrawList dl;
        dl.begin(v.w, v.h);
        draw(dl);
        ui::Image img = arena(v.w, v.h);
        ui::render(dl, img);
        lab.shot(
            {cat({"level-", what, "-", name, "-", v.name()}),
             title,
             caption,
             "the figures beside their labels, the text centred and typed, the panel above the "
             "status bar",
             {{"art", name}, {"viewport", v.name()}}},
            picture_of(std::move(img)), det);
      };
      screen("intermission", "THE INTERMISSION, " + name + ", AT " + v.name(),
             "The level done: the title, the labels and the completed time, the secrets found "
             "and the kills, 3:07, 2/6, 41/52, in the big figures.",
             [&](ui::DrawList& dl) { sm::draw_intermission(dl, art, v.w, v.h, stats); });
      screen("finale", "THE FINALE, TYPED, " + name + ", AT " + v.name(),
             "An episode's end: its text typed out at eight characters a second, here 12.5 s in "
             "(100 characters), each line centred by its whole width.",
             [&](ui::DrawList& dl) { sm::draw_level_text(dl, art, v.w, v.h, kFinale, 100); });
      screen("solo", "THE SOLO PANEL, " + name + ", AT " + v.name(),
             "The scores key held in a campaign: the map's title, then the kills, the secrets and "
             "the time on the map.",
             [&](ui::DrawList& dl) {
               sm::draw_solo_panel(dl, art, v.w, v.h, "The Grey Halls", stats);
             });
    }
  }
}

const ge::lab::LabSpec kSpec{
    "se-menu",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/menu (the main menu's pages, both looks) over engine/ui's menus",
    "ui",
    "Does Shooter's Edge's main menu read in the modern look and in the pack's own pictures, with "
    "every row the game cannot do yet dimmed?",
    {{"viewports", "1280x720,1920x1080,390x844", "the screen sizes, comma-separated WxH"},
     {"pack", "",
      "the player's classic pack (the release zip, an installed folder or its pak0.pak): the "
      "classic look drawn again in its own pictures (never committed or published beyond a "
      "private page)"}},
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
