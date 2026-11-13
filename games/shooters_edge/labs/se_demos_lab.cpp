// LAB `se-demos`: the main menu's DEMOS (menu/demos_page.hpp over the engine's folder browser,
// vfs/browse.hpp) LOOKED AT, on the CPU's 2D layer, at each viewport, with the window's own check
// of a picked file (client/demo_check.hpp). The lab writes a player's folders of its own: the
// game's folder with a recording of each format, and a Downloads folder with recordings packed
// every way a player meets them (`.mvd.gz`, a client recording in a `.zip` with a readme beside it,
// `.mvd.tar.gz`,
// `.mvd.7z`), a web page saved as `broken.mvd`, and files that must not be listed (`maps.zip`,
// `readme.txt`, a hidden one). The same presses a player gives walk it:
//
//   root      the main menu's root with DEMOS between MULTIPLAYER and OPTIONS (modern look)
//   classic   the same root in the classic pictures: DEMOS in its own band between the strip's
//   classic-list  DEMOS itself in the classic look: the game's letters, never the modern slabs
//   list      DEMOS opened on Downloads: folders, then the recordings newest first
//   refused   ENTER on `broken.mvd`: the check's reason in the footer, nothing played
//   game      LEFT to the game's folder: a recording of each format
//
// and each recording is checked (every one passes; `broken.mvd` is refused), its facts line in
// the table. `--dir=<folder>` lists a folder of the player's own instead of the lab's Downloads
// (`--pick=<name>` the file the shots select), `--pack=<path>` draws the classic root in the
// player's pack's pictures, `--backdrop=<png>` draws the pages over a frame of the game; those
// runs are the player's files: measured, left out of the digest, kept on this machine.
//
// ge:summary Lab: the DEMOS page listing packed recordings, refusing a bad one, both looks' root
// ge:keywords lab se-demos demos demo-browser browse downloads packed gz zip 7z tar check refused
// ge:keywords menu_demos classic-root classic-list viewports
// ge:skill ui shooter labs
// ge:test tests/shooters_edge_menu_test.cpp tests/shooters_edge_demo_check_test.cpp
// ge:test tests/vfs_browse_test.cpp
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "classicenhance/source.hpp"
#include "console/console.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "shooters_edge/client/demo_check.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"
#include "vfs/lzma.hpp"
#include "vfs/pak.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"

namespace {

namespace fs = std::filesystem;
namespace ui = ge::ui;
namespace sm = shooters_edge::menu;
namespace cl = shooters_edge::client;

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

void put(const fs::path& p, const std::vector<uint8_t>& bytes, int age_s) {
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::seconds(age_s));
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// The lab's player: the game's folder and Downloads (above), under `root`.
void write_folders(const fs::path& root) {
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "Games" / "base", ec);
  fs::create_directories(root / "Downloads" / "tournament 2026", ec);
  fs::create_directories(root / "Downloads" / "wallpapers", ec);
  fs::create_directories(root / "Desktop", ec);
  const cl::DemoScript s = cl::loop_script("dm4", {128, 192, 24}, 64, 90);
  const std::vector<uint8_t> mvd = cl::write_multiview_demo(s);
  const std::vector<uint8_t> client = cl::write_client_demo(s);
  const std::string client_ext = cl::plain_extension(cl::DemoFormat::client);
  const fs::path game = root / "Games" / "base", dl = root / "Downloads";
  put(game / "practice.mvd", mvd, 3600 * 5);
  put(game / ("duel" + client_ext), client, 86400 * 2);
  put(game / "intro.dem", cl::write_single_demo(s), 86400 * 40);
  put(dl / "final-dm4.mvd.gz", ge::vfs::write_gzip(mvd, "final-dm4.mvd"), 120);
  put(dl / ("semi-dm4" + client_ext + ".zip"),
      ge::vfs::write_stored_zip(
          {{"readme.txt", text("semi final, map 2")}, {"semi" + client_ext, client}}),
      3600 * 3);
  put(dl / "team-dm4.mvd.tar.gz", ge::vfs::write_gzip(ge::vfs::write_tar({{"team.mvd", mvd}})),
      86400);
  put(dl / "league-dm4.mvd.7z", ge::vfs::write_7z("league.mvd", mvd, 5), 86400 * 3);
  put(dl / "broken.mvd", text("<!DOCTYPE html><title>404 Not Found</title>\n"), 600);
  put(dl / "maps.zip", ge::vfs::write_stored_zip({{"dm4.bsp", text("not listed")}}), 60);
  put(dl / "readme.txt", text("not listed"), 30);
  put(dl / ".hidden.mvd", mvd, 10);
}

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

// The game under the menu: the frame given (`--backdrop`, scaled to cover), or a dark arena.
ui::Image under(int w, int h, const ui::Image* backdrop) {
  ui::DrawList dl;
  dl.begin(w, h);
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  ui::Box b;
  if (backdrop != nullptr) {
    const float s = std::max(fw / static_cast<float>(backdrop->width),
                             fh / static_cast<float>(backdrop->height));
    const float bw = static_cast<float>(backdrop->width) * s,
                bh = static_cast<float>(backdrop->height) * s;
    b.rect = {(fw - bw) / 2, (fh - bh) / 2, bw, bh};
    b.image = backdrop;
    b.uv = {0, 0, static_cast<float>(backdrop->width), static_cast<float>(backdrop->height)};
    b.feather = 0;
    dl.box(b);
  } else {
    const ui::Stop wall[2] = {{0, ui::hex(0x3a2f26)}, {1, ui::hex(0x1c1712)}};
    b.rect = {0, 0, fw, fh * 0.6f};
    b.fill = ui::linear(0, 0, 0, b.rect.h, wall, 2);
    dl.box(b);
    const ui::Stop floor[2] = {{0, ui::hex(0x2a2520)}, {1, ui::hex(0x0b0a09)}};
    b.rect = {0, fh * 0.6f, fw, fh * 0.4f};
    b.fill = ui::linear(0, 0, 0, b.rect.h, floor, 2);
    dl.box(b);
  }
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

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
             "run make lab LAB=se-demos ARGS=--viewports=1280x720,390x844");
    return;
  }
  // Relative folders (the run's own, removed at the end): their paths are drawn on the page, so
  // they read the same on every machine and the pictures stay in the digest (with '/' between
  // folders on every system: the generic form).
  const fs::path root = "previews/se-demos-player";
  write_folders(root);
  const bool own = !lab.arg("dir").empty();
  const fs::path downloads = own ? fs::path(lab.arg("dir")) : root / "Downloads";
  const std::string pick = own ? lab.arg("pick") : "broken.mvd";
  std::optional<ui::Image> backdrop;
  if (!lab.arg("backdrop").empty()) {
    const std::optional<ge::lab::Picture> p = ge::lab::read_png(lab.arg("backdrop"));
    if (!p) {
      lab.fail("--backdrop=" + lab.arg("backdrop") + " is not a PNG that reads",
               "next: give it a frame of the game as a PNG (a screenshot, or a se-demo-video lab "
               "still)");
      return;
    }
    backdrop = ui::Image{.width = p->width, .height = p->height, .rgba = p->rgba};
  }
  const ge::lab::Det det = own || backdrop ? ge::lab::Det::measured : ge::lab::Det::deterministic;
  const cl::GameAdapter& game = cl::classic_adapter();
  double check_ms = 0;
  int checks = 0;
  std::vector<std::string> facts_seen;
  const auto check = [&](const std::string& path, std::string& facts, std::string& why) {
    const auto t0 = std::chrono::steady_clock::now();
    cl::DemoFacts f;
    const bool ok = cl::check_demo_file(path, game, f, why);
    check_ms +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    checks++;
    if (ok) facts = cl::facts_line(f);
    return ok;
  };

  struct Game {
    ge::console::Console con;
    sm::GameMenu menu;
    sm::Host host;
    Game() {
      for (const char* c : {"playdemo", "map", "sv", "quit", "connect", "screenshot"})
        con.add_command(c, "", [](ge::console::Console&, const ge::console::Args&) {});
      menu.add_cvars(con);
      host = {.console = &con, .maps = {"start", "dm4"}};
    }
  };
  int problems = 0;
  // The lab's player's places only, never the machine's own home: a list of the runner's real
  // folders would move the pictures from one machine to the next.
  const auto places = [&](Game& g) {
    g.menu.demos().set_extensions(cl::listed_extensions(game.game->demo_extensions));
    g.menu.demos().set_game_places({{"GAME", (root / "Games" / "base").generic_string()}});
    g.menu.demos().set_player_places({{"HOME", root.generic_string()},
                                      {"DOWNLOADS", downloads.generic_string()},
                                      {"DESKTOP", (root / "Desktop").generic_string()}});
    g.menu.demos().set_check(check);
    g.con.execute("menu_demo_dir \"" + downloads.generic_string() + "\"");
  };
  for (const Viewport& v : viewports) {
    Game g;
    places(g);
    const auto press = [&](ui::MenuInput in) { g.menu.update(in, false, g.host, 0.016); };
    const auto snap = [&](const std::string& id, const std::string& title,
                          const std::string& caption, const std::string& look_for) {
      g.menu.settle();
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      g.menu.draw(dl, v.w, v.h, 0.25);
      for (const std::string& p : g.menu.problems()) {
        problems++;
        std::string what = v.name();
        what += ", ";
        what += id;
        what += ": ";
        what += p;
        lab.fail(what,
                 "shorten the words in games/shooters_edge/menu/demos_page.cpp or fix its "
                 "layout; rerun `make lab LAB=se-demos`. Skill: ui.");
      }
      ui::Image img = under(v.w, v.h, backdrop ? &*backdrop : nullptr);
      ui::render(dl, img);
      const ge::vfs::FolderBrowser& b = g.menu.demos().browser();
      lab.shot({id + "-" + v.name(),
                title + " AT " + v.name(),
                caption,
                look_for,
                {{"viewport", v.name()},
                 {"rows", std::to_string(b.entries().size())},
                 {"files", std::to_string(b.files())}}},
               picture_of(std::move(img)), det);
    };
    ui::MenuInput accept, left, back;
    accept.accept = true;
    left.left = true;
    back.back = true;

    g.menu.show(g.host);
    g.menu.main().focus("demos");
    snap("root", "DEMOS ON THE MAIN MENU",
         "The root of the main menu with DEMOS between MULTIPLAYER and OPTIONS, focused, its hint "
         "in the footer.",
         "DEMOS lit, not dimmed (the console has playdemo)");
    press(accept);
    if (!g.menu.in_demos()) {
      lab.fail("ENTER on DEMOS did not open the demos page",
               "check apply_event's `demos` and GameMenu::update in games/shooters_edge/menu. "
               "Skill: ui.");
      return;
    }
    snap("list", "THE DOWNLOADS FOLDER",
         "DEMOS opens where the last demo was played from (menu_demo_dir): the folders first, then "
         "the recordings newest first, packed as a player downloads them (.mvd.gz, a client "
         "recording in a .zip, "
         ".mvd.tar.gz, .mvd.7z); maps.zip, readme.txt and the hidden file are not listed.",
         "DOWNLOADS lit in the places; no maps.zip or readme.txt; ages and sizes on the right");
    lab.metric(v.name() + ".listed", g.menu.demos().browser().files(), "count", det);
    // To the file the shots pick, by the arrows.
    const auto& rows = g.menu.demos().browser().entries();
    int at = -1;
    for (size_t i = 0; i < rows.size(); i++)
      if (rows[i].name == pick) at = static_cast<int>(i);
    for (int guard = 0; at >= 0 && guard < 400 && g.menu.demos().browser().selected() != at;
         guard++) {
      ui::MenuInput step;
      (g.menu.demos().browser().selected() < at ? step.down : step.up) = true;
      press(step);
    }
    if (at >= 0) {
      press(accept);
      const bool played = !g.menu.open();
      snap(played ? "checked" : "refused", played ? "CHECKED AND PLAYED" : "NOT A DEMO: REFUSED",
           played ? "ENTER on a recording: the check read it to its level, the menu closed and "
                    "the window plays it."
                  : "ENTER on broken.mvd, a web page saved under a recording's name: the check "
                    "reads its bytes, not its name, and refuses it with the reason; nothing "
                    "starts.",
           played ? "the game under the menu" : "the reason in the footer, in the accent");
      if (played) press({});
      if (played) g.con.execute("menu_demos");
      if (played) press({});
    }
    press(left);
    snap("game", "THE GAME'S FOLDER",
         "LEFT: the places in turn, here the game's folder, where the window records: a "
         "recording of each format.",
         "GAME lit; a recording of each format");
    press(back);
  }

  // The classic root: DEMOS in its own band between the strip's rows.
  {
    Game g;
    places(g);
    sm::ClassicArt art;
    std::string error;
    const auto files = sm::fixture_files();
    const sm::FindFile find_fixture = [&files](std::string_view p,
                                               std::vector<uint8_t>&) -> std::span<const uint8_t> {
      const auto it = files.find(p);
      return it == files.end() ? std::span<const uint8_t>{} : std::span<const uint8_t>(it->second);
    };
    PackFiles pack;
    const bool real = !lab.arg("pack").empty();
    const bool loaded = real
                            ? ge::classicenhance::open_source(lab.arg("pack"), &pack.src, &error) &&
                                  ge::vfs::read_pak(pack.src.pack, &pack.dir, &error) &&
                                  sm::load_classic_art(pack.find(), &art, &error)
                            : sm::load_classic_art(find_fixture, &art, &error);
    if (!loaded) {
      lab.fail("the menu's classic pictures did not load: " + error,
               real ? "next: give --pack the release zip, an installed folder or its pak0.pak"
                    : "next: check fixture_files in games/shooters_edge/menu/classic_art.cpp");
    } else {
      g.menu.set_art(std::move(art));
      g.menu.show(g.host);
      g.menu.main().focus("demos");
      const Viewport v{1280, 800};
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      g.menu.draw(dl, v.w, v.h, 0.25);
      for (const std::string& p : g.menu.problems()) {
        problems++;
        lab.fail("classic root: " + p, "fix layout_classic in classic_view.cpp. Skill: ui.");
      }
      ui::Image img = under(v.w, v.h, backdrop ? &*backdrop : nullptr);
      ui::render(dl, img);
      lab.shot({"classic-root",
                real ? "DEMOS IN THE PACK'S OWN MENU" : "DEMOS IN THE CLASSIC LOOK",
                "The classic root: each row of the pack's strip its own band, DEMOS (no words for "
                "it in the picture) between MULTIPLAYER and OPTIONS, cut letter by letter from the "
                "strips' own carved words on the pack (big glyphs on other strips), the spinning "
                "cursor beside it.",
                "SELECT GAME first, then the strip's rows in order with DEMOS fourth; the cursor "
                "on DEMOS",
                {{"look", real ? "pack" : "fixture"}}},
               picture_of(std::move(img)), real || backdrop ? ge::lab::Det::measured : det);
      // The page itself in the classic look: the game's letters, never the modern slabs.
      g.menu.show(g.host, "demos");
      g.menu.settle();
      ui::DrawList page;
      page.begin(v.w, v.h);
      g.menu.draw(page, v.w, v.h, 0.25);
      for (const std::string& p : g.menu.problems()) {
        problems++;
        lab.fail("classic demos: " + p,
                 "fix DemosPage::draw_classic in classic_pages.cpp. Skill: ui.");
      }
      ui::Image list = under(v.w, v.h, backdrop ? &*backdrop : nullptr);
      ui::render(page, list);
      lab.shot({"classic-list",
                real ? "DEMOS IN THE PACK'S OWN LETTERS" : "DEMOS IN THE CLASSIC LOOK'S LETTERS",
                "DEMOS once a classic game is loaded: the place, the folder, the folders and the "
                "recordings with their sizes in the game's own glyph sheet under the carved "
                "title, the cursor, the keys at the foot. No modern slabs in the classic game.",
                "glyph rows right of the plaque; the carved DEMOS title",
                {{"look", real ? "pack" : "fixture"}}},
               picture_of(std::move(list)), real || backdrop ? ge::lab::Det::measured : det);
    }
  }

  // Every recording checked, its facts in the table.
  ge::lab::LabTable t;
  t.title = "THE CHECK OF EACH FILE";
  t.columns = {"file", "passes", "what it found"};
  int passed = 0, refused = 0;
  for (const fs::path& dir : {root / "Games" / "base", downloads}) {
    std::vector<ge::vfs::FolderEntry> list;
    std::string error;
    if (!ge::vfs::list_folder(dir.string(), cl::listed_extensions(game.game->demo_extensions), list,
                              error))
      continue;
    for (const ge::vfs::FolderEntry& e : list) {
      if (e.folder) continue;
      std::string facts, why;
      const bool ok = check(e.path, facts, why);
      (ok ? passed : refused)++;
      t.rows.push_back({e.name, ok ? "yes" : "no", ok ? facts : why});
    }
  }
  lab.table(t, det);
  lab.metric("passed", passed, "count", det);
  lab.metric("refused", refused, "count", det);
  lab.metric("problems", problems, "count");
  lab.metric("check-ms", checks > 0 ? check_ms / checks : 0, "ms", ge::lab::Det::measured);
  if (!own && (passed != 7 || refused != 1))
    lab.fail("the check passed " + std::to_string(passed) + " and refused " +
                 std::to_string(refused) + " of the lab's files; 7 and 1 expected",
             "read the table: a packing that no longer opens, or broken.mvd taken for a demo; "
             "check client/demo_check.cpp and vfs/unpack.cpp. Skill: shooter.");
  std::error_code ec;
  fs::remove_all(root, ec);
}

const ge::lab::LabSpec kSpec{
    "se-demos",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/menu/demos_page (the main menu's DEMOS) over engine/vfs/browse",
    "ui",
    "Does DEMOS list a folder's recordings (packed too, and nothing else), refuse a file that is "
    "no demo, and sit on the main menu in both looks, at every screen shape?",
    {{"viewports", "1280x720,390x844,844x390", "the screen sizes, comma-separated WxH"},
     {"dir", "", "a folder of the player's own to list in place of the lab's Downloads"},
     {"pick", "", "with --dir: the file the shots select and play"},
     {"pack", "", "the player's classic pack (zip, folder or pak0.pak) for the classic root"},
     {"backdrop", "", "a PNG of the game drawn under the pages"}},
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
