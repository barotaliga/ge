// LAB `hub-viewer`: A DEMO PLAYED IN THE BROWSER (docs/hub.md § Watching in a browser). A team
// match of bots on the fixture halls (net/classic_team.hpp) is recorded, packed into a replay file
// version 2 and taken by the hub's archive; the halls go in a content pack beside the web viewer
// (`--viewer=<dir>`, the web build's `shooters_edge_viewer/`). The hub serves its Demo page, the
// viewer's files, the pack and the demo over loopback HTTP, and a headless Chromium with WebGPU
// (on its software adapter) opens the page, driven over its DevTools pipe so the picture is taken
// after a wall-clock wait (hub_browser.hpp's `live_draw`): the viewer fetches the pack and the
// demo and draws the match on the canvas. It answers whether the page starts the viewer with the
// right words, what a visitor downloads (the viewer, the pack, the demo), and whether the picture
// is there.
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
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "hub/archive.hpp"
#include "hub/http.hpp"
#include "hub/watch.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "lab/png.hpp"
#include "net/address.hpp"
#include "net/status.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "tv/node.hpp"
#include "tv/websocket.hpp"
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
const std::vector<std::string> kWebGpuFlags = {
    "--enable-unsafe-webgpu",   "--enable-unsafe-swiftshader", "--use-webgpu-adapter=swiftshader",
    "--enable-features=Vulkan", "--use-vulkan=swiftshader",    "--use-angle=swiftshader",
    "--enable-logging=stderr"};

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
  // A page drawn after `wait_s` while the hub (and `pump`, when given) answers on this thread.
  const auto draw =
      [&](const std::string& path, const std::string& name, int wait_s, const gh::Poller& nodes,
          const std::function<void()>& pump,
          const std::vector<gl::hub_browser::LiveAct>& acts = {}) -> std::optional<gl::Picture> {
    const fs::path png = root / (name + ".png");
    std::atomic<bool> done{false};
    std::optional<gl::Picture> pic;
    std::thread t([&] {
      // A browser that fails leaves no picture, which the caller reports.
      pic = gl::hub_browser::live_draw(browser, kWebGpuFlags, base + path, w, h, wait_s * 1000, png,
                                       png.string() + ".log", acts);
      done = true;
    });
    while (!done) {
      gh::http_update(http, nodes, spec(), 0);
      if (pump) pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    t.join();
    if (!pic)
      lab.note("Chromium drew no picture of " + path + " after " + std::to_string(wait_s) + " s");
    return pic;
  };
  for (const int wait_s :
       {static_cast<int>(lab.arg_num("wait")), 2 * static_cast<int>(lab.arg_num("wait"))}) {
    const std::optional<gl::Picture> got =
        draw("/demo/" + row.sha, "at-" + std::to_string(wait_s) + "s", wait_s, gh::Poller{}, {});
    if (!got) continue;
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
             *got, gl::Det::measured);
  }

  // The controls: the canvas clicked (focused), Space pauses, the pointer over the picture shows
  // the play bar with its highlights.
  if (lab.arg("controls") != "0") {
    namespace hb = gl::hub_browser;
    const int wait_s = static_cast<int>(lab.arg_num("wait"));
    std::vector<hb::LiveAct> acts = hb::click(w / 3, h / 2, 500);
    for (hb::LiveAct& a : hb::key("Space", " ", 500)) acts.push_back(a);
    acts.push_back(hb::move(w / 3 + 40, h / 2, 6000));
    const std::optional<gl::Picture> got =
        draw("/demo/" + row.sha, "controls", wait_s, gh::Poller{}, {}, acts);
    if (got) {
      drawn++;
      lab.shot({"controls",
                "THE DEMO PAUSED ON THE CANVAS'S KEYS",
                "The Demo page " + std::to_string(wait_s) +
                    " s in: the canvas clicked, Space pressed, the pointer moved over the picture. "
                    "The viewer plays a demo with the window's own controls (demo_playback.hpp): "
                    "pause, skips, speed and highlight jumps on the keys, the play bar over the "
                    "picture.",
                "the play bar along the picture's foot with the playhead stopped and the play "
                "button shown; the picture still.",
                {{"wait_s", std::to_string(wait_s)}}},
               *got, gl::Det::measured);
    }
  }

  // A player's own demo and packs (`--real-demo`, `--real-packs`): the recording packed into a
  // replay file, taken by the archive, and its Demo page drawn from the packs beside the viewer.
  // Nothing of them is kept or digested.
  if (const std::string real = lab.arg("real-demo"); !real.empty()) {
    std::ifstream f(real, std::ios::binary);
    const std::vector<uint8_t> rec{std::istreambuf_iterator<char>(f), {}};
    std::vector<uint8_t> real_ger;
    gh::DemoRow real_row;
    const fs::path packs = lab.arg("real-packs");
    for (const char* name : {"pak0.pak", "pak1.pak"}) {
      if (fs::is_regular_file(packs / name, ec))
        fs::copy_file(packs / name, viewer / name, fs::copy_options::overwrite_existing, ec);
    }
    if (rec.empty() || !cl::pack_demo(rec, real_ger, error)) {
      lab.note("the demo " + real + " did not pack: " + error);
    } else if (gh::archive_add(archive, real_ger, 1794480100, {}, error, &real_row) !=
               gh::AddResult::added) {
      lab.note("the archive did not take " + real + ": " + error);
    } else {
      const int wait_s = 2 * static_cast<int>(lab.arg_num("wait"));
      const std::optional<gl::Picture> got =
          draw("/demo/" + real_row.sha, "real", wait_s, gh::Poller{}, {});
      if (got) {
        drawn++;
        lab.shot({"real",
                  "A PLAYER'S OWN DEMO ON THE DEMO PAGE " + std::to_string(wait_s) + " S IN",
                  "The demo " + fs::path(real).filename().string() +
                      " packed into a replay file and filed by the hub's archive, played by the "
                      "web viewer from the player's own packs.",
                  "the demo's map and players drawn on the canvas.",
                  {{"wait_s", std::to_string(wait_s)}}},
                 *got, gl::Det::measured);
      }
    }
  }

  // The same match live: a TV node fed the replay file, paced to the match's own clock by its
  // delay, with WebSocket viewers; the hub's Watch page points the viewer at it.
  if (lab.arg("live") != "0") {
    ge::tv::Node node;
    node.config.name = "hub-viewer";
    node.config.delay_ms = 1000;
    ge::tv::TvWeb web;
    if (!ge::tv::tv_web_listen(web, ge::net::loopback_v4(0), error)) {
      lab.note("no WebSocket listener here (" + error + "): the live page not drawn");
    } else {
      using clock = std::chrono::steady_clock;
      const auto t0 = clock::now();
      const auto now_ms = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
      };
      ge::tv::tv_feed(node, ger, now_ms());
      ge::tv::tv_end(node);
      gh::Poller nodes;
      gh::HubNode tv;
      tv.address = ge::net::ipv4(127, 0, 0, 1, 28000);
      tv.kind = tv.status.kind = ge::net::NodeKind::tv;
      tv.listed = tv.have = true;
      tv.status.keys = {{"viewers", "1"},
                        {"delay_s", "1"},
                        {std::string(gh::kWebPortKey), std::to_string(web.listener.local.port)}};
      gh::HubNode server;
      server.address = ge::net::ipv4(127, 0, 0, 1, 27500);
      server.listed = server.have = true;
      server.status.name = "The halls, live";
      server.status.map = "halls";
      server.status.mode = "4on4";
      server.status.phase = ge::net::StatusPhase::playing;
      ge::net::StatusStream st;
      st.node = tv.address;
      st.stream = 1;
      server.status.streams.push_back(st);
      nodes.nodes = {tv, server};
      const auto pump = [&] {
        ge::tv::tv_web_receive(web, node);
        ge::tv::tv_update(node, now_ms(), [&](int link, std::span<const uint8_t> msg) {
          return ge::tv::tv_web_send(web, link, msg);
        });
        ge::tv::tv_web_flush(web);
      };
      const int wait_s = static_cast<int>(lab.arg_num("wait"));
      const std::optional<gl::Picture> got =
          draw(gh::watch_path(tv.address, 1), "live", wait_s, nodes, pump);
      lab.metric("live_bytes_out", static_cast<double>(node.bytes_out) / 1024.0, "KiB",
                 gl::Det::measured);
      if (got) {
        drawn++;
        lab.shot({"live",
                  "A LIVE BROADCAST ON THE WATCH PAGE " + std::to_string(wait_s) + " S IN",
                  "GET /watch/{tv-node}/{stream}: the viewer opens the TV node's WebSocket, says "
                  "hello, and plays the match as the node lets each one-second block go (a "
                  "second behind the match), the replay file unpacked block by block as it "
                  "comes.",
                  "the halls on the canvas and a status line that is empty, or says it is "
                  "waiting for the match.",
                  {{"wait_s", std::to_string(wait_s)}}},
                 *got, gl::Det::measured);
      }
      ge::tv::tv_web_close(web);
    }
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
        {"wait", "25",
         "wall-clock seconds from opening the page to its first picture (the second waits twice); "
         "a software GPU takes about 20 s to the first frame"},
        {"width", "1280", "the browser's window width, px"},
        {"height", "900", "the browser's window height, px"},
        {"controls", "1", "also shoot the Demo page paused by its keys with the play bar (0: not)"},
        {"real-demo", "", "a player's own recording to play too (measured, never kept)"},
        {"real-packs", "", "the folder holding that recording's packs (pak0.pak, pak1.pak)"},
        {"live", "1", "also watch the match live through a TV node on the Watch page (0: not)"},
        {"keep", "0", "1: keep the hub's folders (the viewer, the pack, the archive) after a run"},
    },
    "--minutes=0.1 --browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
