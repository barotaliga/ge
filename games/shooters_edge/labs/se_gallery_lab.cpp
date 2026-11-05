// LAB `se-gallery`: Shooter's Edge's screenshots page (OPTIONS ▸ SCREENSHOTS,
// menu/gallery_page.hpp) over the engine's gallery (capture/gallery.hpp) LOOKED AT, on the CPU's 2D
// layer, at each viewport. Ten screenshots of painted scenes (at the viewport's own size, as that
// screen takes them) are written as JPEGs with the window's names into a folder of the lab's own
// (one more file damaged on purpose), the menu is opened on the page by `menu_screenshots`, and the
// same presses a player gives walk it:
//
//   grid      the newest first, the selection moved one cell right, its facts in the footer
//   last      the selection moved to the oldest: the last page (a phone's grid holds fewer)
//   viewer    ENTER: the picture at full size, its place in the list
//   confirm   DELETE: the question over the viewer
//   deleted   ENTER: the file gone, the viewer on the next one, the count one less
//   empty     every file deleted: the empty folder says F12 and where they are kept
//
// Every layout is held to the page's problems check. The loads are timed (measured).
//
// ge:summary Lab: the screenshots page's grid, viewer, delete and empty folder at three viewports
// ge:keywords lab se-gallery gallery screenshots thumbnail viewer confirm delete empty viewports
// ge:keywords menu_screenshots
// ge:skill ui replay labs
// ge:test tests/capture_gallery_test.cpp tests/shooters_edge_menu_test.cpp
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "capture/still.hpp"
#include "console/console.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace fs = std::filesystem;
namespace ui = ge::ui;
namespace sm = shooters_edge::menu;
namespace gc = ge::capture;

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

// A painted stand-in for a frame of the game: a wall and floor in scene `i`'s colours, a lit
// doorway moving along the wall, the scene's number on a plaque.
ui::Image scene(int i, int w, int h) {
  static constexpr uint32_t kWalls[] = {0x2a2420, 0x1f2a33, 0x332222, 0x23301f, 0x2e2638,
                                        0x3a3326, 0x202020, 0x2b1d14, 0x18303a, 0x3a2a1a};
  static constexpr uint32_t kLights[] = {0xffd9a0, 0x9fd8ff, 0xff8a5c, 0xc8ff9a, 0xd9b0ff,
                                         0xfff2a8, 0xffffff, 0xff6a2a, 0x7af0ff, 0xffc070};
  ui::DrawList dl;
  dl.begin(w, h);
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  ui::Box b;
  const ui::Stop wall[2] = {{0, ui::hex(kWalls[i % 10])}, {1, ui::hex(kWalls[(i + 3) % 10])}};
  b.rect = {0, 0, fw, fh * 0.62f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, wall, 2);
  dl.box(b);
  const ui::Stop floor[2] = {{0, ui::hex(0x3a3028)}, {1, ui::hex(0x0e0c0a)}};
  b.rect = {0, fh * 0.62f, fw, fh * 0.38f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, floor, 2);
  dl.box(b);
  const ui::Stop door[2] = {{0, ui::hex(kLights[i % 10])}, {1, ui::hex(0x5a3a20)}};
  b.rect = {fw * (0.12f + 0.07f * static_cast<float>(i % 10)), fh * 0.24f, fw * 0.12f, fh * 0.38f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, door, 2);
  b.radius = fw * 0.02f;
  dl.box(b);
  dl.rect({fw * 0.06f, fh * 0.08f, fw * 0.16f, fh * 0.12f}, ui::hex(0x000000, 0.5f), 8);
  dl.text(ui::ui_font(), fh * 0.08f, fw * 0.14f, fh * 0.175f, "E" + std::to_string(i + 1),
          ui::hex(0xffffff), ui::Align::center);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

// The darkened game under the menu.
ui::Image under(int w, int h) {
  ui::Image img = scene(0, w, h);
  return img;
}

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

constexpr int kShots = 10;

// The folder as a player's would be after ten F12s: the window's names a minute apart (the
// first two in one second, told apart by a count), and one file damaged.
void write_folder(const fs::path& dir, const Viewport& v, ge::lab::Lab& lab) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  for (int i = 0; i < kShots; ++i) {
    const ui::Image img = scene(i, v.w, v.h);  // taken on this screen
    gc::StillOptions o;                        // the window's default: jpg at 90
    std::vector<uint8_t> bytes;
    gc::StillFormat used{};
    std::string note;
    gc::encode_still(o, img.width, img.height, img.rgba, bytes, used, note);
    std::string minute = std::to_string(10 + i);
    std::string name = "shooters_edge-2026-11-05-21" + minute + "00";
    if (i == 1) name = "shooters_edge-2026-11-05-211000-2";
    if (!ge::lab::write_file((dir / (name + ".jpg")).string(), bytes))
      lab.fail("the lab's screenshot " + name + " was not written to " + dir.string(),
               "check the temporary folder is writable; rerun `make lab LAB=se-gallery`");
  }
  ge::lab::write_file((dir / "shooters_edge-2026-11-04-180000.jpg").string(),
                      std::vector<uint8_t>{0xFF, 0xD8, 0xFF, 0xD9});
}

struct Game {
  ge::console::Console con;
  sm::GameMenu menu;
  sm::Host host;
  Game() {
    con.add_command("screenshot", "", [](ge::console::Console&, const ge::console::Args&) {});
    menu.add_cvars(con);
    host = {.console = &con};
  }
};

void run(ge::lab::Lab& lab) {
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports=" + lab.arg("viewports") + " named no WxH size",
             "run make lab LAB=se-gallery ARGS=--viewports=1280x720,390x844");
    return;
  }
  // A relative folder (the run's own, removed at the end): its name is drawn on the empty page,
  // so it reads the same on every machine and the pictures stay in the digest.
  const fs::path dir = "previews/se-gallery-screenshots";
  int problems = 0;
  double load_ms = 0;
  for (const Viewport& v : viewports) {
    write_folder(dir, v, lab);
    Game g;
    g.menu.set_screenshots(dir.string());
    g.con.execute("menu_screenshots");
    g.menu.update({}, false, g.host, 0.016);
    if (!g.menu.in_gallery()) {
      lab.fail("menu_screenshots did not open the screenshots page",
               "check GameMenu::show in games/shooters_edge/menu/game_menu.cpp. Skill: ui.");
      return;
    }
    gc::Gallery& gal = g.menu.gallery();
    const auto press = [&](ui::MenuInput in) { g.menu.update(in, false, g.host, 0.016); };
    // A draw first, so the page sets the grid the screen holds; then the loads settled.
    const auto snap = [&](const std::string& id, const std::string& title,
                          const std::string& caption, const std::string& look_for) {
      ui::DrawList probe;
      probe.begin(v.w, v.h);
      g.menu.draw(probe, v.w, v.h, 0);
      const auto t0 = std::chrono::steady_clock::now();
      gal.settle();
      load_ms +=
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      g.menu.draw(dl, v.w, v.h, 0);
      for (const std::string& p : g.menu.problems()) {
        problems++;
        std::string what = v.name();
        what += ", " + id + ": ";
        what += p;
        lab.fail(what,
                 "shorten the words in games/shooters_edge/menu/gallery_page.cpp or fix its "
                 "layout; rerun `make lab LAB=se-gallery`. Skill: ui.");
      }
      ui::Image img = under(v.w, v.h);
      ui::render(dl, img);
      lab.shot({id + "-" + v.name(),
                title + " AT " + v.name(),
                caption,
                look_for,
                {{"viewport", v.name()},
                 {"screenshots", std::to_string(gal.count())},
                 {"grid", std::to_string(gal.columns()) + "x" + std::to_string(gal.rows())},
                 {"selected", std::to_string(gal.selected())}}},
               picture_of(std::move(img)));
    };

    ui::MenuInput right, accept, del, back;
    right.right = true;
    accept.accept = true;
    del.del = true;
    back.back = true;
    ui::DrawList first;
    first.begin(v.w, v.h);
    g.menu.draw(first, v.w, v.h, 0);
    press(right);
    snap("grid", "THE SCREENSHOTS, NEWEST FIRST",
         "OPTIONS > SCREENSHOTS: eleven files, newest first, in cells of the screen's shape; the "
         "selection one cell right, its name, time, size, format and bytes in the footer.",
         "the selected cell ringed; every thumbnail its own scene; the count and page at the top "
         "right; the keys under the footer");
    lab.metric(v.name() + ".columns", gal.columns(), "count");
    lab.metric(v.name() + ".rows", gal.rows(), "count");
    lab.metric(v.name() + ".pages", gal.pages(), "count");
    gal.select(gal.count() - 1);
    snap("last", "THE LAST PAGE",
         "The selection on the oldest file, a damaged one: the last page, the cell saying it "
         "does not read.",
         "DOES NOT READ in the selected cell; the page number moved on");
    gal.select(1);
    press(accept);
    snap("viewer", "ONE SCREENSHOT AT FULL SIZE",
         "ENTER on the second newest: the picture at full size on a dark ground, its place in "
         "the list at the top right, its facts in the footer.",
         "the picture whole and centred, not stretched; 2 / 11 at the top right");
    press(del);
    snap("confirm", "DELETE, ASKED FIRST",
         "DELETE (or a Mac's delete key, BACKSPACE): the question over the viewer, the file's "
         "name, ENTER to delete and ESC to keep it.",
         "the box centred and its words inside it; the picture still behind");
    press(accept);
    snap("deleted", "DELETED: THE NEXT ONE SHOWN",
         "ENTER: the file deleted from the folder, the viewer on the one after it, the count "
         "one less.",
         "2 / 10 at the top right; a different scene than before");
    lab.metric(v.name() + ".after-delete", gal.count(), "count");
    press(back);
    while (gal.count() > 0) {
      gal.select(0);
      press(del);
      press(accept);
    }
    snap("empty", "NO SCREENSHOTS YET",
         "Every file deleted: the empty folder says F12 takes one and where they are kept (the "
         "path cut from the left when it is long).",
         "the words centred; the folder's path inside the screen");
  }
  lab.metric("problems", problems, "count");
  lab.metric("load-ms", load_ms, "ms", ge::lab::Det::measured);
  std::error_code ec;
  fs::remove_all(dir, ec);
}

const ge::lab::LabSpec kSpec{
    "se-gallery",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/menu/gallery_page (the screenshots page) over engine/capture/gallery",
    "ui",
    "Does the screenshots page show the screenshots taken, view one, delete one after asking and "
    "say so when there are none, at every screen shape?",
    {{"viewports", "1280x720,390x844,844x390", "the screen sizes, comma-separated WxH"}},
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
