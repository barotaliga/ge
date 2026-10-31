// LAB `se-highlights`: A DEMO'S HIGHLIGHTS AND ITS PLAY BAR. A demo is scanned for its kills
// (client/demo_scan.hpp) and Shooter's Edge's highlight rules run over them
// (client/demo_highlights.hpp, on engine/replay/highlights.hpp). By default the demo is the
// scripted match (demo_write.hpp's `highlight_match`: a double kill, a long rocket, an air gib in a
// streak, an annihilated 200 stack, a telefrag, and a long rocket by another player) written as a
// multiview recording on the fixture halls; `--file=<path>` scans a real one (plain or gzip), and
// `--basedir` (with `--game`) gives its map for the pictures.
//
// What it shows: every highlight found (time, whose, kinds, score, what), every kill the scan
// read, the play bar of each player with highlights (engine/ui/play_bar.hpp, drawn by the CPU
// raster: the markers, their windows, a tooltip), and, with a GPU, the watched player's view at
// each of his highlights with the bar over it as the window shows it while paused there (the frames
// drawn by client/demo_video.hpp, the bar drawn over them). The scan's counts, the highlights and
// the bars are deterministic; the GPU's pictures and the times are measured.
//
// ge:summary Lab: a demo's highlights found and shown on the play bar, over the game's frames
// ge:keywords lab se-highlights highlights demo scan play-bar markers multi-kill long-shot
// ge:keywords air-kill annihilation telefrag streak tooltip
// ge:skill shooter replay labs
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "ui/play_bar.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace gu = ge::ui;
namespace gr = ge::replay;
namespace cl = shooters_edge::client;
namespace ls = ge::lab::liquid_scene;
namespace se = shooters_edge::hud;

std::string fixed(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

const char* weapon_name(shooters_edge::hud::Weapon w) {
  static const char* const names[] = {"melee",           "shotgun",       "double shotgun",
                                      "nailgun",         "heavy nailgun", "grenade launcher",
                                      "rocket launcher", "beam gun"};
  return names[static_cast<int>(w)];
}

gu::Color color_of(uint32_t rgba) {
  return {static_cast<float>(rgba >> 24 & 0xff) / 255.0f,
          static_cast<float>(rgba >> 16 & 0xff) / 255.0f,
          static_cast<float>(rgba >> 8 & 0xff) / 255.0f, static_cast<float>(rgba & 0xff) / 255.0f};
}

std::vector<gu::PlayBarMarker> markers_of(const gr::Highlights& h) {
  std::vector<gu::PlayBarMarker> out;
  for (const gr::Highlight& x : h.list) {
    const gr::HighlightKind* k = gr::kind_of(cl::demo_kinds(), x.kind);
    out.push_back({x.at_s, x.from_s, x.to_s, color_of(k ? k->rgba : ~0u), x.text});
  }
  return out;
}

// The bar over `img` (paused at `at_s`, the pointer on marker `hover` when it is 0 or more).
void draw_bar(gu::Image& img, const cl::DemoScan& scan, const std::vector<gu::PlayBarMarker>& m,
              double at_s, int hover, const std::string& title, float bottom_px = 0) {
  const gu::PlayBarLayout l = gu::play_bar_layout(img.width, img.height, bottom_px);
  gu::PlayBarView v;
  v.length_s = scan.length_s;
  v.position_s = at_s;
  v.playing = false;
  v.markers = m;
  v.title = title;
  if (hover >= 0 && static_cast<size_t>(hover) < m.size()) {
    const double f = m[static_cast<size_t>(hover)].at_s / std::max(scan.length_s, 1e-9);
    v.pointer_x = l.track.x + static_cast<float>(f) * l.track.w;
    v.pointer_y = l.track.y + l.track.h / 2;
  }
  gu::DrawList list;
  list.begin(img.width, img.height);
  gu::draw_play_bar(list, l, v);
  gu::render(list, img);
}

gl::Picture to_lab(const gu::Image& img) { return {img.width, img.height, img.rgba}; }

void run(gl::Lab& lab) {
  const std::string file = lab.arg("file");
  std::vector<uint8_t> bytes;
  if (file.empty()) {
    bytes = cl::write_multiview_match(cl::highlight_match());
  } else {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      lab.fail("se-highlights: " + file + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), {});
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"file", file.empty() ? "(the scripted match)" : file}};
  cl::Demo d;
  std::string error;
  if (!cl::open_demo(bytes, d, error)) {
    lab.fail("se-highlights: " + error, "next: check the file; skill: shooter");
    return;
  }
  d.track = static_cast<int>(lab.arg_num("track"));
  const auto t0 = std::chrono::steady_clock::now();
  cl::DemoScan scan;
  if (!cl::scan_demo(d, scan, error)) {
    lab.fail("se-highlights: the scan stopped: " + error, "next: read the reader; skill: shooter");
    return;
  }
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const int watched = d.track >= 0 ? d.track : scan.tracked;
  const gr::Highlights all = cl::demo_highlights(scan, -1);
  const gr::Highlights mine = cl::demo_highlights(scan, watched);

  lab.metric("scan.length", scan.length_s, "s");
  lab.metric("scan.frames", static_cast<double>(scan.frames), "count");
  lab.metric("scan.deaths", static_cast<double>(scan.deaths), "count");
  lab.metric("scan.kills", static_cast<double>(scan.deaths - scan.unmatched), "count");
  lab.metric("scan.unmatched", static_cast<double>(scan.unmatched), "count");
  lab.metric("scan.rockets", static_cast<double>(scan.rockets), "count");
  lab.metric("scan.wall", wall, "s", gl::Det::measured);
  lab.metric("highlights.all", static_cast<double>(all.list.size()), "count");
  lab.metric("highlights.watched", static_cast<double>(mine.list.size()), "count");
  for (const gr::HighlightKind& k : cl::demo_kinds()) {
    int64_t n = 0;
    for (const gr::Highlight& h : all.list) n += std::count(h.kinds.begin(), h.kinds.end(), k.kind);
    lab.metric("highlights." + k.kind, static_cast<double>(n), "count");
  }

  gl::LabTable hl{"every highlight", {"time", "whose", "kinds", "score", "window", "what"}, {}};
  for (const gr::Highlight& h : all.list) {
    std::string kinds;
    for (const std::string& k : h.kinds) kinds += (kinds.empty() ? "" : " + ") + k;
    hl.rows.push_back(
        {gu::play_clock(h.at_s) + " (" + fixed(h.at_s, 2) + ")",
         cl::player_name(scan.names, h.subject) + (h.subject == watched ? " (watched)" : ""), kinds,
         fixed(h.score, 2), fixed(h.from_s, 1) + ".." + fixed(h.to_s, 1), h.text});
  }
  lab.table(hl);
  gl::LabTable kills{"every kill the scan read (the first 60)",
                     {"time", "killer", "victim", "weapon", "distance", "rocket", "air", "gib",
                      "telefrag", "stack", "lost in s"},
                     {}};
  for (size_t i = 0; i < scan.kills.size() && i < 60; i++) {
    const cl::DemoKill& k = scan.kills[i];
    kills.rows.push_back(
        {fixed(k.t, 2), k.killer < 0 ? "-" : cl::player_name(scan.names, k.killer),
         cl::player_name(scan.names, k.victim), k.killer < 0 ? "-" : weapon_name(k.weapon),
         fixed(k.distance, 0), k.rocket_flight > 0 ? fixed(k.rocket_flight, 0) : "-",
         k.airborne ? "yes" : "", k.gib ? "yes" : "", k.telefrag ? "yes" : "",
         k.stack > 0 ? std::to_string(k.stack) : "-", k.stack > 0 ? fixed(k.stack_s, 2) : "-"});
  }
  lab.table(kills);
  lab.note("the scan read " + std::to_string(scan.frames) + " frames, " + fixed(scan.length_s, 1) +
           " s of timeline, in " + fixed(wall * 1000, 1) + " ms");

  // Every player's bar with his highlights, stacked: the bottom of a 1280x720 frame each.
  constexpr int kW = 1280, kH = 720, kStrip = 120;
  std::vector<int> owners;
  for (int k = 0; k < 32; k++)
    if ((scan.players >> k & 1u) != 0) owners.push_back(k);
  gu::Image sheet = gu::make_image(kW, kStrip * static_cast<int>(owners.size()), gu::hex(0x1b1f27));
  for (size_t i = 0; i < owners.size(); i++) {
    const gr::Highlights h = cl::demo_highlights(scan, owners[i]);
    const std::vector<gu::PlayBarMarker> m = markers_of(h);
    gu::Image frame = gu::make_image(kW, kH, gu::hex(0x1b1f27));
    const size_t n = h.list.size();
    draw_bar(frame, scan, m, n ? h.list[0].at_s : 0, n ? 0 : -1,
             cl::player_name(scan.names, owners[i]) + "  ·  " + std::to_string(n) +
                 (n == 1 ? " highlight" : " highlights"));
    for (int y = 0; y < kStrip; y++)
      std::copy_n(frame.rgba.begin() + (static_cast<ptrdiff_t>(kH - kStrip + y) * kW * 4), kW * 4,
                  sheet.rgba.begin() + ((static_cast<ptrdiff_t>(i) * kStrip + y) * kW * 4));
  }
  if (!owners.empty())
    lab.shot({"bars", "EVERY PLAYER'S PLAY BAR",
              "The play bar the window draws over a demo, once for each player in it, with a "
              "marker for each of his highlights in its kind's colour (multi kill gold, long "
              "rocket blue, air kill violet, annihilation red, telefrag orange, streak green), "
              "its window shaded on the track, paused on his first with the pointer over it.",
              "a marker where each highlight in the table is; the tooltip names the first", in},
             to_lab(sheet));

  // The watched player's view at each of his highlights, the bar over it.
  auto device = ge::gpu::create_device({.label = "se-highlights", .validation = 0});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  const size_t shots = std::min<size_t>(mine.list.size(),
                                        static_cast<size_t>(std::max(0.0, lab.arg_num("moments"))));
  std::vector<std::string> ids;
  for (size_t i = 0; i < shots; i++) ids.push_back("moment-" + std::to_string(i + 1));
  if (!device) {
    for (const std::string& id : ids) ls::placeholders(lab, {id.c_str()});
    return;
  }
  lab.note("adapter: " + ge::gpu::adapter_line(*device));
  cl::SearchPath files;
  if (!lab.arg("basedir").empty()) {
    if (!cl::open_game_dir(lab.arg("basedir"), lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
  }
  std::vector<uint8_t> pak;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries = {{"match.mvd", bytes}};
  if (lab.arg("basedir").empty()) {
    std::vector<uint8_t> bsp;
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error)) {
      lab.fail(error, "Read the reason above. Skill: brush.");
      return;
    }
    entries.push_back({"maps/halls.bsp", std::move(bsp)});
  }
  if (!ge::vfs::write_pak(entries, &pak, &error) ||
      !cl::add_archive(files, "pak9.pak", std::move(pak), error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  cl::DemoVideoOptions o;
  o.demo = "match.mvd";
  o.width = kW;
  o.height = kH;
  o.fps = 20;
  o.codec = "";
  o.container = "mkv";
  o.sound = false;
  o.track = watched;
  o.seconds = shots ? mine.list[shots - 1].at_s + 0.2 : 0;
  std::vector<uint8_t> video;
  o.memory = &video;
  const std::vector<gu::PlayBarMarker> m = markers_of(mine);
  // The bar clears the HUD the frames carry, as the window's does.
  float inset = 0;
  {
    se::Hud hud = se::make_hud();
    se::HudArt art = se::free_art();
    art.finish();
    se::fill_hud(hud, cl::demo_status(cl::DemoState{}, 0), art);
    gu::DrawList scrap;
    scrap.begin(kW, kH);
    const auto placed =
        hud.layout.draw(scrap, hud.layout.frame(kW, kH, se::hud_style()), se::hud_style());
    inset = cl::hud_inset(placed, kH);
  }
  std::vector<int64_t> want;  // the picture just before each moment
  for (size_t i = 0; i < shots; i++)
    want.push_back(std::max<int64_t>(0, static_cast<int64_t>((mine.list[i].at_s - 0.15) * o.fps)));
  std::vector<gl::Picture> got(shots);
  const auto keep = [&](int64_t k, double, const gl::Picture& p) {
    for (size_t i = 0; i < shots; i++)
      if (want[i] == k) got[i] = p;
  };
  cl::DemoVideoReport r;
  if (!cl::demo_video(*device, files, o, r, error, keep)) {
    lab.fail("se-highlights: the frames: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (size_t i = 0; i < shots; i++) {
    if (got[i].rgba.empty()) continue;
    gu::Image img{got[i].width, got[i].height, got[i].rgba};
    const gr::Highlight& h = mine.list[i];
    draw_bar(img, scan, m, h.at_s, static_cast<int>(i),
             cl::player_name(scan.names, watched) + "  ·  " + std::to_string(mine.list.size()) +
                 " highlights",
             inset);
    lab.shot({ids[i], "AT " + gu::play_clock(h.at_s) + ": " + h.text,
              "The watched player's view just before his highlight at " + fixed(h.at_s, 2) +
                  " s (" + h.text +
                  "), drawn from the demo by the world's frame, with the play bar "
                  "over it as the window shows it paused there, the pointer on the marker.",
              "the playhead on the marker the tooltip names", in},
             to_lab(img), gl::Det::measured);
  }
}

const gl::LabSpec kSpec{
    "se-highlights",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client: the demo scan, the highlight rules, the play bar (demo_scan, "
    "demo_highlights, demo_playback; engine/replay/highlights, engine/ui/play_bar)",
    "shooter",
    "Which moments of a demo are worth watching again, does the finder find each one, and does the "
    "play bar show them where they happened?",
    {
        {"file", "", "a demo's path (plain or gzip); empty: the scripted match"},
        {"track", "-1", "the player whose highlights are pictured (-1: the lowest playing)"},
        {"basedir", "", "a folder holding a game folder: the demo's map from there"},
        {"game", "id1", "the game folder under --basedir"},
        {"moments", "6", "the watched player's highlights pictured with the GPU, the first n"},
    },
    "--moments=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
