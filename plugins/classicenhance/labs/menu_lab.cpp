// LAB `classic-menu`: the classic-enhance menu (menu.hpp) LOOKED AT, on the CPU's 2D layer. The
// root page (the pack, ENHANCE, the switches), the switches' group list and one group's toggles
// with some turned off, and the enhancing page over a real run of the pipeline on the lab's
// fixture pack (fixtures.hpp): caught halfway through the textures, finished (READY, PLAY lit),
// and cancelled mid-stage (STOPPED, the finished stages kept). The run's reports go to a
// `ProgressBoard` on a clock of the lab's own (each report 0.4 s on), so the times on the panel
// and the digest are the same everywhere. `--source=<path>` runs it over the player's own pack;
// those pictures are derived from the pack and stay on this machine.
//
// Every layout is held to `layout_problems`, and the panel to its area: a panel narrower than a
// stage's row fails the lab. The switches page is driven through `apply_menu_event`, so the
// toggles shown are the cvars' values.
//
// ge:summary Lab: the classic-enhance menu, its switches and the enhancing watched, three viewports
// ge:keywords lab classicenhance menu enhance progress panel switches toggles viewport cancel ready
// ge:skill classicenhance labs
// ge:test tests/lab_runner_test.cpp
#include <atomic>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "assets/sha256.hpp"
#include "classicenhance/labs/fixtures.hpp"
#include "classicenhance/menu.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/progress.hpp"
#include "console/cvar.hpp"
#include "lab/lab.hpp"
#include "ui/background.hpp"
#include "ui/menu_view.hpp"
#include "ui/raster.hpp"

namespace {

namespace ce = ge::classicenhance;
namespace fs = std::filesystem;
using namespace ge::ui;

struct Viewport {
  int w, h;
};

std::vector<Viewport> parse_viewports(const std::string& s) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < s.size()) {
    size_t end = s.find(',', at);
    if (end == std::string::npos) end = s.size();
    const std::string one = s.substr(at, end - at);
    const size_t x = one.find('x');
    if (x != std::string::npos) {
      const int w = std::atoi(one.substr(0, x).c_str()), h = std::atoi(one.substr(x + 1).c_str());
      if (w > 0 && h > 0) out.push_back({w, h});
    }
    at = end + 1;
  }
  return out;
}

// The boards a run went through: one after every report.
struct Run {
  std::vector<ce::BoardView> boards;
  bool ok = false;
  std::string error;
};

Run run_fixture(const ce::Source& src, const fs::path& root, bool force, int cancel_at_texture) {
  Run out;
  ce::ProgressBoard board;
  std::atomic<bool> cancel{false};
  double now = 0;
  ce::RunOptions opts;
  opts.textures.scale = 2;
  opts.sounds.extend = 0;
  opts.force = force;
  opts.cancel = &cancel;
  opts.progress = [&](const ce::Progress& p) {
    now += 0.4;
    board.on(p, now);
    out.boards.push_back(board.view());
    if (cancel_at_texture > 0 && p.stage == ce::Stage::textures &&
        p.kind == ce::Progress::Kind::item && int(p.done) >= cancel_at_texture) {
      cancel.store(true);
    }
  };
  ce::RunReport report;
  out.ok = ce::run_pipeline(src, root, opts, &report, &out.error);
  return out;
}

ge::lab::Picture to_picture(Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height;
  p.rgba = std::move(img.rgba);
  return p;
}

void run(ge::lab::Lab& lab) {
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=classic-menu ARGS=--viewports=1280x720,390x844");
    return;
  }

  // The fixture pack, verified as a known pack so the pipeline takes it; or the player's own.
  const ce::Palette palette = ce::labs::fixture_palette();
  ce::Source src;
  std::string error;
  // The source keeps a pointer to its known entry, so the entry lives as long as the source.
  std::string sha;
  ce::KnownFile known[1] = {};
  if (lab.arg("source").empty()) {
    src.path = "fixture";
    src.pack = ce::labs::fixture_pack(palette);
    sha = ge::assets::sha256_hex(src.pack);
    known[0] = {sha.c_str(), uint32_t(src.pack.size()), "the lab's pack"};
    if (!ce::verify_pack(&src, &error, known)) {
      lab.fail("the fixture pack did not verify: " + error,
               "check fixture_pack in plugins/classicenhance/labs/fixtures.cpp. Skill: "
               "classicenhance.");
      return;
    }
  } else if (!ce::open_source(lab.arg("source"), &src, &error)) {
    lab.fail(error, "next: give --source a release zip, an installed folder or its pack.");
    return;
  }
  const fs::path root = fs::temp_directory_path() / "ge_lab_classic_menu";
  fs::remove_all(root);
  const Run full = run_fixture(src, root, false, 0);
  if (!full.ok) {
    lab.fail("the pipeline failed on the fixture pack: " + full.error,
             "run `ge lab run enhance` and the classicenhance tests to find the stage. Skill: "
             "classicenhance.");
    fs::remove_all(root);
    return;
  }
  // Halfway through the textures; the end.
  ce::BoardView mid = full.boards.front();
  for (const ce::BoardView& b : full.boards) {
    const ce::StageView& t = b.stages[size_t(ce::Stage::textures)];
    if (t.state == ce::StageState::running && t.total > 0 && 2 * t.done >= t.total) {
      mid = b;
      break;
    }
  }
  const ce::BoardView done = full.boards.back();
  const uint32_t textures = done.stages[size_t(ce::Stage::textures)].total;
  // Cancelled one texture in, the stages before it kept (forced, so every stage runs again).
  const Run stopped = run_fixture(src, root, true, 1);
  fs::remove_all(root);
  const ce::BoardView cancelled = stopped.boards.back();
  if (stopped.ok || !cancelled.failed) {
    lab.fail("cancelling the run did not stop it",
             "check Reports::item in plugins/classicenhance/pipeline.cpp reads RunOptions::cancel. "
             "Skill: classicenhance.");
  }

  // The menu, the switches bound to cvars.
  ge::console::Cvars cvars;
  ce::Switches values;
  ce::bind_switches(cvars, &values);
  const std::vector<std::string> packs = {"installed game", "release zip"};
  MenuStyle st;
  st.logo = "CLASSIC";
  st.help = "ENTER SELECT   ESC BACK";
  st.footer = lab.arg("source").empty() ? "FIXTURE PACK" : "YOUR OWN PACK";
  const Stop stops[] = {{0, hex(0x1b2a3a)}, {0.55f, hex(0x101a26)}, {1, hex(0x07090d)}};
  Background bg = Background::gradient(180, stops, 3);
  bg.glow(0.75f, 0.35f, 0.6f, hex(0xf0a63f, 0.10f)).vignette(0.45f);

  int problems = 0, cramped = 0;
  auto shoot = [&](const std::string& id, const std::string& title, const std::string& caption,
                   const std::string& look, Menu& m, const Viewport& v,
                   const ce::BoardView* board) {
    MenuLayout L = layout_menu(m, v.w, v.h, st);
    for (const std::string& p : layout_problems(m, L, st)) {
      problems++;
      std::string what = id;
      what.append(": ").append(p);
      lab.fail(what,
               "fix the words in plugins/classicenhance/switches.cpp or menu.cpp; rerun `make lab "
               "LAB=classic-menu`. Skill: classicenhance.");
    }
    MenuView view;
    view.settle(m, L);
    DrawList dl;
    dl.begin(v.w, v.h);
    draw_background(dl, bg, {0, 0, float(v.w), float(v.h)}, 0);
    view.draw(dl, m, L, st);
    std::vector<Image> store;
    if (board != nullptr) {
      ce::PanelStyle ps;
      ps.scale = L.scale;
      const Rect area = ce::panel_area(L);
      if (area.w < 260 * L.scale || area.h < 200 * L.scale) {
        cramped++;
        lab.fail(id + ": the progress panel has " + std::to_string(int(area.w)) + " x " +
                     std::to_string(int(area.h)) + " px",
                 "give it room in panel_area (plugins/classicenhance/menu.cpp). Skill: "
                 "classicenhance.");
      }
      ce::draw_progress(dl, *board, area, ps, store, packs[0]);
    }
    Image img = make_image(v.w, v.h, hex(0x000000));
    render(dl, img);
    const std::string size = std::to_string(v.w) + "x" + std::to_string(v.h);
    lab.shot({id + "-" + size,
              title + " AT " + size,
              caption,
              look,
              {{"viewport", size}, {"page", m.top().id}, {"textures", std::to_string(textures)}}},
             to_picture(std::move(img)));
  };

  for (const Viewport& v : viewports) {
    {
      Menu m = ce::classic_menu(packs, values);
      m.input({});
      m.focus("enhance");
      shoot(
          "root", "THE CLASSIC MENU",
          "The root page: the pack to use (the packs found on this machine), ENHANCE (prominent), "
          "the switches and back.",
          "ENHANCE lit and bigger than the rest; the pack's name fits its slab; the hint under "
          "the column",
          m, v, nullptr);
    }
    {
      Menu m = ce::classic_menu(packs, values);
      m.open("switches");
      m.input({});
      shoot("switches", "THE SWITCHES' GROUPS",
            "Every group of enhancements a page: textures, light, the world, liquids and sky, "
            "effects, air, the picture, models, sound.",
            "every group's name fits; nothing runs off the screen", m, v, nullptr);
    }
    {
      // EFFECTS with MUZZLE FIRE and BUBBLES turned off through the menu's own events.
      ce::Switches local;
      ge::console::Cvars cv;
      ce::bind_switches(cv, &local);
      Menu m = ce::classic_menu(packs, local);
      m.open("switches.effects");
      m.input({});
      int pack = 0;
      for (const char* id : {"muzzles", "bubbles"}) {
        m.focus(id);
        MenuInput accept;
        accept.accept = true;
        for (const MenuEvent& e : m.input(accept)) ce::apply_menu_event(e, m, cv, &local, &pack);
      }
      if (local.muzzles || local.bubbles || !local.flames) {
        lab.fail("toggling MUZZLE FIRE and BUBBLES did not reach their cvars",
                 "check apply_menu_event in plugins/classicenhance/menu.cpp. Skill: "
                 "classicenhance.");
      }
      m.focus("flames");
      shoot("effects", "THE EFFECTS' SWITCHES",
            "One group's page: a toggle a switch, MUZZLE FIRE and BUBBLES turned off through the "
            "menu (their ce_ cvars read 0), then every switch of the group on or off and the "
            "defaults.",
            "MUZZLE FIRE and BUBBLES read OFF, the rest ON; FIRE's hint under the column says what "
            "off shows",
            m, v, nullptr);
    }
    const std::pair<const ce::BoardView*, const char*> states[] = {
        {&mid, "mid"}, {&done, "ready"}, {&cancelled, "stopped"}};
    for (const auto& [board, name] : states) {
      Menu m = ce::classic_menu(packs, values);
      m.open("enhancing");
      ce::sync_enhancing(m, *board);
      m.input({});
      const std::string n = name;
      if (n == "ready") m.focus("play");
      shoot("enhancing-" + n,
            n == "mid"     ? "ENHANCING, HALFWAY THROUGH THE TEXTURES"
            : n == "ready" ? "ENHANCING DONE"
                           : "ENHANCING CANCELLED",
            n == "mid" ? "The run caught halfway through the textures: the pack copied, the "
                         "textures stage working on its item, the stages after it waiting; the "
                         "last textures beside their enhanced albedo."
            : n == "ready"
                ? "Every stage done: READY, the whole bar full, PLAY lit and enabled."
                : "CANCEL pressed one texture in: STOPPED, the pack's stage kept, the textures' "
                  "stage failed as cancelled, the rest never started.",
            n == "mid"     ? "the overall bar part full; TEXTURES WORKING with its count and item; "
                             "pairs of thumbnails, the enhanced one sharper"
            : n == "ready" ? "every row DONE in green; READY at the top; PLAY enabled and lit"
                           : "STOPPED at the top; the textures' row FAILED in red; what to do next "
                             "at the bottom",
            m, v, board);
    }
  }

  lab.metric("reports", double(full.boards.size()), "count");
  lab.metric("textures", double(textures), "count");
  lab.metric("fraction.mid", mid.fraction(), "ratio");
  lab.metric("seconds.total", done.elapsed, "s");
  lab.metric("switches", double(ce::kSwitchCount), "count");
  lab.metric("layout.problems", problems, "count");
  lab.metric("panel.cramped", cramped, "count");
}

const ge::lab::LabSpec kSpec{
    "classic-menu",
    ge::lab::LabKind::picture,
    "plugin:classicenhance",
    "plugins/classicenhance/menu.hpp, plugins/classicenhance/progress.hpp, "
    "plugins/classicenhance/switches.hpp",
    "classicenhance",
    "Does the classic-enhance menu read at a desktop's and a phone's size, can the player watch "
    "every stage of the enhancing until it is done, and is every enhancement a switch?",
    {
        {"viewports", "1280x720,390x844,844x390",
         "screen sizes, WxH, comma-separated (a desktop, a phone upright, a phone on its side)"},
        {"source", "",
         "the classic pack (zip, folder or pack); empty: the lab's fixture pack. Its pictures stay "
         "on this machine: never attach them to a PR or publish them"},
    },
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
