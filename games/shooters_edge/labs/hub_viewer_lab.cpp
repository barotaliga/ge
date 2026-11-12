// LAB `hub-viewer`: A DEMO PLAYED IN THE BROWSER (docs/hub.md § Watching in a browser). A team
// match of bots on the fixture halls (net/classic_team.hpp) is recorded, packed into a replay file
// version 2 and taken by the hub's archive; the halls go in a content pack beside the web viewer
// (`--viewer=<dir>`, the web build's `shooters_edge_viewer/`). The hub serves its Demo page, the
// viewer's files, the pack and the demo over loopback HTTP, and a headless Chromium with WebGPU
// (on its software adapter) opens the page: the viewer fetches the pack and the demo and draws the
// match on the canvas. It answers whether the page starts the viewer with the right words, what a
// visitor downloads (the viewer, the pack, the demo), and whether the picture is there.
//
// The demo, the pack and the page are deterministic. The viewer's size is the web build's, and the
// pictures are a browser's (GE_CHROMIUM or the container's), so both are measured. Without a built
// viewer or a browser it says so and draws nothing.
//
// ge:summary Lab: a bots' demo played by the web viewer in headless Chromium off the hub's page
// ge:keywords lab hub-viewer web-viewer wasm webgpu browser chromium demo-page canvas pack bytes
// ge:skill net labs
// ge:test tests/hub_watch_test.cpp tests/web_viewer_args_test.cpp
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "brush/write.hpp"
#include "hub/archive.hpp"
#include "hub/http.hpp"
#include "hub/watch.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "lab/png.hpp"
#include "net/address.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace gh = ge::hub;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace fs = std::filesystem;

gh::HubSpec spec() {
  gh::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ffa", 3}};
  s.score = "frags";
  return s;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

int64_t file_bytes(const fs::path& p) {
  std::error_code ec;
  const auto n = fs::file_size(p, ec);
  return ec ? -1 : static_cast<int64_t>(n);
}

// The browser's flags for WebGPU on its software adapter (SwiftShader over Vulkan).
std::string chromium_line(const std::string& browser, const std::string& url, const fs::path& png,
                          int w, int h, int wait_ms) {
  return "\"" + browser +
         "\" --headless=new --no-sandbox --hide-scrollbars --enable-unsafe-webgpu "
         "--enable-unsafe-swiftshader --use-webgpu-adapter=swiftshader --enable-features=Vulkan "
         "--use-vulkan=swiftshader --use-angle=swiftshader --force-device-scale-factor=1 "
         "--window-size=" +
         std::to_string(w) + "," + std::to_string(h) + " --timeout=" + std::to_string(wait_ms) +
         " --enable-logging=stderr --screenshot=\"" + png.string() + "\" \"" + url + "\" > \"" +
         png.string() + ".log\" 2>&1";
}

void run(gl::Lab& lab) {
  std::string error;
  std::vector<uint8_t> halls;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &halls, &error)) {
    lab.fail("hub-viewer: the fixture halls: " + error, "run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("hub-viewer: the baseline mod: " + error,
             "run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  cn::TeamMatchOptions o;
  o.map = halls;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = static_cast<int>(
      std::lround(lab.arg_num("minutes") * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport match = cn::run_team_match(o);
  if (!match.ok) {
    lab.fail("hub-viewer: the match: " + match.error, "run tests/shooters_edge_team_test.cpp");
    return;
  }
  std::vector<uint8_t> ger, pak;
  if (!cl::pack_demo(match.mvd, ger, error)) {
    lab.fail("hub-viewer: packing the demo: " + error, "run tests/shooters_edge_demo_v2_test.cpp");
    return;
  }
  if (!ge::vfs::write_pak({{"maps/halls.bsp", halls}}, &pak, &error)) {
    lab.fail("hub-viewer: the halls' pack: " + error, "run tests/vfs_pak_test.cpp");
    return;
  }

  // The hub's folders: the archive, and the viewer's with the pack beside it.
  std::error_code ec;
  const fs::path root = fs::temp_directory_path(ec) / "ge-hub-viewer-lab";
  fs::remove_all(root, ec);
  const fs::path viewer = root / "viewer";
  fs::create_directories(viewer, ec);
  {
    std::ofstream(viewer / "pak0.pak", std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  }
  const fs::path built = lab.arg("viewer");
  const bool have_viewer = fs::is_regular_file(built / "shooters_edge_viewer.js", ec) &&
                           fs::is_regular_file(built / "shooters_edge_viewer.wasm", ec);
  for (const char* f : {"shooters_edge_viewer.js", "shooters_edge_viewer.wasm"}) {
    if (have_viewer) {
      fs::copy_file(built / f, viewer / f, fs::copy_options::overwrite_existing, ec);
    } else {
      // Stand-ins, so the Demo page (digested) is the one a hub with the viewer serves, whether
      // or not this machine has the web build.
      std::ofstream(viewer / f) << "stand-in\n";
    }
  }
  gh::Archive archive;
  gh::DemoRow row;
  if (!gh::archive_open(archive, (root / "archive").string(), error) ||
      gh::archive_add(archive, ger, 1794480000, {}, error, &row) != gh::AddResult::added) {
    lab.fail("hub-viewer: the archive: " + error, "run tests/hub_archive_test.cpp");
    return;
  }

  // The Demo page and what it hands the viewer.
  gh::Site site;
  site.archive = &archive;
  site.viewer_dir = viewer.string();
  gh::Answer page;
  gh::watch_answer(gh::Poller{}, spec(), "/demo/" + row.sha, 0, site, page);
  lab.metric("demo_kb", static_cast<double>(ger.size()) / 1024.0, "KiB");
  lab.metric("pack_kb", static_cast<double>(pak.size()) / 1024.0, "KiB");
  if (have_viewer) {
    if (page.code != 200 || !has(page.body, "<canvas id=\"view\"") ||
        !has(page.body, "\"--pack=/viewer/pak0.pak\"") ||
        !has(page.body, "\"--demo=/" + gh::archive_path(row.sha) + "\"")) {
      lab.fail("the Demo page does not start the viewer with the demo and the pack",
               "run tests/hub_watch_test.cpp, then read watch.cpp's viewer_slot");
    }
    const double js = static_cast<double>(file_bytes(viewer / "shooters_edge_viewer.js"));
    const double wasm = static_cast<double>(file_bytes(viewer / "shooters_edge_viewer.wasm"));
    lab.metric("viewer_js_kb", js / 1024.0, "KiB", gl::Det::measured);
    lab.metric("viewer_wasm_kb", wasm / 1024.0, "KiB", gl::Det::measured);
    lab.metric(
        "first_visit_kb",
        (js + wasm + static_cast<double>(pak.size() + ger.size() + page.body.size())) / 1024.0,
        "KiB", gl::Det::measured);
  } else {
    lab.note("no built viewer in " + built.string() +
             ": the pictures need the web build (cmake --preset web, then "
             "cmake --build build/web --target shooters_edge_viewer)");
  }
  lab.attach("demo.html", {page.body.begin(), page.body.end()});

  const std::string browser = lab.arg("browser") == "0" ? "" : gl::hub_browser::find_browser();
  if (!have_viewer || browser.empty()) {
    if (have_viewer) lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium)");
    fs::remove_all(root, ec);
    return;
  }

  // The hub on loopback, answering while the browser loads the page.
  gh::HttpServer http;
  http.archive = &archive;
  http.viewer_dir = viewer.string();
  if (!gh::http_open(http, ge::net::loopback_v4(0), error)) {
    lab.note("no loopback TCP here (" + error + "): pages not drawn");
    fs::remove_all(root, ec);
    return;
  }
  const std::string base = "http://127.0.0.1:" + std::to_string(http.listener.local.port);
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  int drawn = 0;
  for (const int wait_s :
       {static_cast<int>(lab.arg_num("wait")), 2 * static_cast<int>(lab.arg_num("wait"))}) {
    const fs::path png = root / ("at-" + std::to_string(wait_s) + "s.png");
    std::atomic<bool> done{false};
    std::thread t([&] {
      // A browser that fails leaves no picture, which the check after the loop reports.
      [[maybe_unused]] const int rc = std::system(
          chromium_line(browser, base + "/demo/" + row.sha, png, w, h, wait_s * 1000).c_str());
      done = true;
    });
    const gh::Poller none;
    while (!done) {
      gh::http_update(http, none, spec(), 0);
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    t.join();
    const std::optional<gl::Picture> pic = gl::read_png(png.string());
    if (!pic) {
      lab.note("Chromium drew no picture after " + std::to_string(wait_s) + " s");
      continue;
    }
    drawn++;
    lab.shot({"after-" + std::to_string(wait_s) + "s",
              "THE DEMO PAGE " + std::to_string(wait_s) + " S AFTER IT OPENED",
              "GET /demo/{sha256} in headless Chromium with WebGPU on its software adapter, which "
              "offers only WebGPU's default limits: the viewer fetches the halls' pack and the "
              "bots' demo from the hub, then draws the match on the canvas, or says on the status "
              "line why it cannot.",
              "the halls from a player's eye on the canvas; else the status line's reason (a "
              "renderer over the browser's limits names the world).",
              {{"wait_s", std::to_string(wait_s)}}},
             *pic, gl::Det::measured);
  }
  lab.metric("pages_drawn", drawn, "pictures", gl::Det::measured);
  gh::http_close(http);
  if (lab.arg("keep") == "1") {
    lab.note("the hub's folders are kept in " + root.string() + " (viewer/, archive/)");
  } else {
    fs::remove_all(root, ec);
  }
}

const gl::LabSpec kSpec{
    "hub-viewer",
    gl::LabKind::picture,
    "game:shooters_edge",
    "engine/hub/watch (the Demo page), engine/web (the viewer's arguments and the page's calls) "
    "and web/viewer.cpp (the game's viewer)",
    "net",
    "Does the hub's Demo page start the game's web viewer with the demo and its pack, how much "
    "does a first visit download, and does the match play on the canvas?",
    {
        {"minutes", "0.5", "the bots' match's length"},
        {"viewer", "build/web/shooters_edge_viewer", "the web build's viewer folder"},
        {"browser", "1", "open the page in a headless Chromium when one is found (0: never)"},
        {"wait", "8",
         "seconds the browser waits before its first picture (the second waits twice)"},
        {"width", "1280", "the browser's window width, px"},
        {"height", "900", "the browser's window height, px"},
        {"keep", "0", "1: keep the hub's folders (the viewer, the pack, the archive) after a run"},
    },
    "--minutes=0.1 --browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
