// LAB `hud-layout`: Fall Line's HUD as data (engine/hud/layout.hpp) with every widget up, laid
// out three ways at the three reference viewports (a desktop, a phone held upright, a phone on
// its side):
//
//   - the GAME'S layout: `hud_layout`'s places, untouched;
//   - a PLAYER'S layout: a cfg of `seta` lines (the speedometer and the edge bar moved to the
//     bottom centre and drawn bigger, the stars smaller, the seed fainter, the news hidden), run
//     through the console exactly as a player's config would be;
//   - RESET: `hud_reset` after the player's cfg, which must put every box back where the game's
//     layout had it.
//
// The game's layout must lay out clean (no two boxes overlapping, none off the screen) and the
// reset must match it box for box: either failing fails the lab. The player's layout is the
// player's own, so its problems are counted, not failed. The backdrop is painted (daylight on
// snow, the HUD's worst light), so the lab needs no GPU and its pictures are the same bits on
// every row.
//
// ge:summary Lab: Fall Line's HUD as data, the game's layout, a player's cfg and the reset
// ge:keywords lab hud hud-layout layout cvars seta hud_reset player-layout viewports elements
// ge:keywords layout-problems screenshot
// ge:skill ui labs console
// ge:test tests/hud_layout_test.cpp
#include <string>
#include <vector>

#include "console/console.hpp"
#include "core/num_text.hpp"
#include "fall_line/hud/view.hpp"
#include "fall_line/hud/words.hpp"
#include "hud/layout.hpp"
#include "lab/lab.hpp"
#include "ui/raster.hpp"

namespace {

namespace fh = fall_line::hud;
namespace gh = ge::hud;
namespace ui = ge::ui;

struct Viewport {
  int w, h;
  std::string name() const { return std::to_string(w) + "x" + std::to_string(h); }
};

// The player's own layout, as a config would hold it.
constexpr const char* kPlayerCfg =
    "seta hud_speedo_anchor bottom; seta hud_speedo_scale 1.25\n"
    "seta hud_edge_anchor bottom; seta hud_edge_scale 1.25\n"
    "seta hud_stars_scale 0.7\n"
    "seta hud_seed_alpha 0.55\n"
    "seta hud_news_show 0\n";

ui::Image snowfield(int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  ui::Box b;
  const ui::Stop sky[2] = {{0, ui::hex(0x2d5f9a)}, {1, ui::hex(0xbcd3ea)}};
  b.rect = {0, 0, float(w), float(h) * 0.42f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, sky, 2);
  dl.box(b);
  const ui::Stop snow[2] = {{0, ui::hex(0xdfe8f2)}, {1, ui::hex(0xf9fbfd)}};
  b.rect = {0, float(h) * 0.42f, float(w), float(h) * 0.58f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, snow, 2);
  dl.box(b);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

// Every widget up: a stuck prompt, four stars searching, a best air, a full news column.
fh::HudSnapshot staged(const gh::Style& st, fh::HudRun& run) {
  fh::HudSnapshot s;
  s.t = 30;
  s.seed = 38, s.x = -1412, s.z = -2290;
  s.speed_kmh = 104, s.edge = -0.6;
  s.air_s = 2.31, s.air_best = true, s.best_air_s = 1.92;
  s.skied_m = 12480, s.run = "B12 BLACK";
  s.prompt = fh::Prompt::stuck;
  fh::WantedTile w;
  w.stars = 4, w.flashing = true, w.search = 0.4, w.survived = 754;
  s.wanted = w;
  run.news.push(fh::words::wipeout(ge::skier::Wipeout::across), st.bad, s.t - 3);
  run.news.push(fh::words::landed(2.31, 3.4), st.good, s.t - 2);
  run.news.push(fh::words::kTree, st.bad, s.t - 1);
  run.news.push(fh::words::lift_top(252), st.ink, s.t - 0.5);
  return s;
}

bool same_box(const ui::Rect& a, const ui::Rect& b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void run(ge::lab::Lab& lab) {
  const gh::Style style = fh::hud_style();
  fh::HudRun news;
  const fh::HudSnapshot snap = staged(style, news);
  fh::HudLayout hud = fh::hud_layout(style);
  ge::console::Console console;
  fh::add_hud_cvars(console, hud);
  const std::vector<Viewport> viewports{{1280, 720}, {390, 844}, {844, 390}};

  int game_problems = 0, player_problems = 0, reset_moved = 0, player_moved = 0;
  for (const Viewport& v : viewports) {
    std::vector<gh::Placed> game;
    for (const char* layout : {"game", "player", "reset"}) {
      const std::string which = layout;
      if (which == "player") console.execute(kPlayerCfg);
      if (which == "reset") console.execute("hud_reset");
      fh::fill_hud(hud, style, snap, news);
      const gh::Frame f = hud.layout.frame(v.w, v.h, style);
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      const std::vector<gh::Placed> placed = hud.layout.draw(dl, f, style);
      const std::vector<std::string> problems = gh::layout_problems(f, placed);
      if (which == "game") {
        game = placed;
        game_problems += static_cast<int>(problems.size());
        for (const std::string& p : problems)
          lab.fail("the game's layout at " + v.name() + ": " + p,
                   "move or shrink the element in games/fall_line/hud/view.cpp (hud_layout); "
                   "rerun `make lab LAB=hud-layout`. Skill: ui.");
      } else {
        int moved = 0;
        for (const gh::Placed& p : placed) {
          bool kept = false;
          for (const gh::Placed& g : game) kept |= g.name == p.name && same_box(g.rect, p.rect);
          moved += !kept;
        }
        moved += static_cast<int>(game.size()) - static_cast<int>(placed.size());
        if (which == "player") {
          player_moved += moved;
          player_problems += static_cast<int>(problems.size());
        } else {
          reset_moved += moved;
          if (moved != 0)
            lab.fail("hud_reset left " + std::to_string(moved) +
                         " box(es) off the game's place at " + v.name(),
                     "every hud_ cvar add_hud_cvars binds must be in the reset's list "
                     "(engine/hud/layout.cpp). Skill: ui.");
        }
      }
      if (which == "reset") continue;  // the same pixels as the game's, when it passes
      ui::Image img = snowfield(v.w, v.h);
      ui::render(dl, img);
      ge::lab::Picture pic;
      pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
      const bool game_one = which == "game";
      lab.shot({which + "-" + v.name(),
                (game_one ? "THE GAME'S LAYOUT AT " : "A PLAYER'S LAYOUT AT ") + v.name() +
                    (v.h > v.w ? " (UPRIGHT)" : ""),
                game_one ? "Fall Line's HUD in hud_layout's places with every widget up: the "
                           "seed's column top left, the hunt top right, the air clock top "
                           "centre, the speedometer and the edge bar bottom left, the news bottom "
                           "right, the prompt above the centre."
                         : "The same HUD after a player's five seta lines: the speedometer and the "
                           "edge bar moved to the bottom centre at 1.25 times their size, the "
                           "stars at 0.7, the seed's line faded, the news hidden.",
                game_one ? "nothing overlaps or leaves the screen; the air clock under the "
                           "corners on the upright phone"
                         : "only the named elements moved or changed; the stack under the stars "
                           "follows their new size",
                {{"viewport", v.name()},
                 {"scale", ge::core::num::to_fixed(f.scale, 2)},
                 {"boxes", std::to_string(placed.size())},
                 {"problems", std::to_string(problems.size())}}},
               pic);
    }
  }
  lab.metric("game.problems", game_problems, "count");
  lab.metric("player.problems", player_problems, "count");
  lab.metric("player.moved", player_moved, "count");
  lab.metric("reset.moved", reset_moved, "count");
  lab.metric("elements", static_cast<double>(hud.layout.size()), "count");
}

const ge::lab::LabSpec kSpec{
    "hud-layout",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/hud/layout.hpp (elements, places, hud_ cvars) under games/fall_line/hud/view.hpp",
    "ui",
    "Does Fall Line's HUD, declared as data, lay out clean in the game's places, follow a "
    "player's cfg to the element, and come back whole after hud_reset?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
