// LAB `se-timedemo`: THE DEMO BENCHMARK WITHOUT A WINDOW. A recorded game played flat out the
// way the window's `benchmark` command plays it (client/timedemo.hpp: one demo frame a drawn
// frame, every frame timed but the first after a level start), drawn into an offscreen target by
// the window's own world frame and HUD. By default the demo is written in code (demo_write.hpp):
// a walk round the fixture halls (plugins/brush) in the format `--format` names, in a pack on
// the search path. `--basedir` (and `--game`) opens a game folder instead and `--demo` names its
// demo (`demo1`), so the same lab measures a real recording on any machine.
//
// What it shows: three frames of the run (its start, the middle, the end), the frame time of
// every timed frame as a line with the median and the 99th percentile as rules, the GPU's time
// for each pass of the world's frame as boxes (where the timer has timestamp queries), and the
// report as metrics. The frame count and the levels are deterministic; every time is measured.
//
// ge:summary Lab: the demo benchmark headless: a demo played flat out, its frames and fps chart
// ge:keywords lab se-timedemo timedemo benchmark demo fps frame-time one-percent-low chart gpu
// ge:keywords offscreen
// ge:skill shooter perf labs
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;
namespace ls = ge::lab::liquid_scene;

uint64_t now_ns() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

std::string fixed(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// The halls and a demo of a walk round the hall, in a pack.
bool fixture_pack(const std::string& format, std::vector<uint8_t>& out, std::string& error) {
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error))
    return false;
  const cl::DemoScript s = cl::loop_script("halls", {256, 192, 24}, 120, 600);
  std::vector<uint8_t> demo =
      format == "client" ? cl::write_client_demo(s) : cl::write_single_demo(s);
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}, {"demo1.dem", demo}}, &out, &error);
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  const std::string basedir = lab.arg("basedir");
  const std::string name = lab.arg("demo");
  std::string error;
  shooters_edge::look::Picture picture;  // the world reads it every frame
  if (!lab.arg("look").empty()) picture.look = lab.arg("look");
  cl::SearchPath files;
  if (basedir.empty()) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(lab.arg("format"), pak, error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
  } else if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
    lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
    return;
  }
  cl::Timedemo bench;
  if (!cl::timedemo_start(files, name, bench, error)) {
    lab.fail(error, "Name a demo the packs hold (--demo=demo1). Skill: shooter.");
    return;
  }
  lab.note("demo: " + bench.found);

  auto device = ge::gpu::create_device({.label = "se-timedemo", .validation = 0});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  if (device) lab.note("adapter: " + ge::gpu::adapter_line(*device));
  ge::gpu::Target out;
  if (device) out = ge::gpu::create_target(*device, "se-timedemo.final", w, h);
  cl::WorldDrawPtr world;
  std::unique_ptr<cl::LoadedMap> map;
  se::Hud hud = se::make_hud();
  se::HudArt art = se::free_art();
  art.finish();

  // First count the frames (no drawing), so the pictures can be taken at the start, the middle
  // and the end of the same run.
  int64_t total = 0;
  int levels = 0;
  {
    cl::Timedemo count;
    cl::timedemo_start(files, name, count, error);
    for (cl::TimedemoStep st; (st = cl::timedemo_step(count, error)) == cl::TimedemoStep::frame ||
                              st == cl::TimedemoStep::new_level;) {
      total++;
      if (st == cl::TimedemoStep::new_level) levels++;
    }
  }
  const int64_t picks[3] = {std::min<int64_t>(total - 1, cl::kSkipAfterLevel + 1), total / 2,
                            total - 1};
  const char* ids[3] = {"start", "middle", "end"};
  int64_t frame = 0;
  int shot = 0;
  std::string stop;
  const uint64_t first_ns = now_ns();
  for (;;) {
    const cl::TimedemoStep st = cl::timedemo_step(bench, error);
    if (st == cl::TimedemoStep::end) break;
    if (st == cl::TimedemoStep::error) {
      stop = error;
      break;
    }
    if (st == cl::TimedemoStep::new_level) {
      map = cl::load_named_map(files, bench.demo.state.map, error);
      if (!map) {
        stop = "the demo's map " + bench.demo.state.map + ": " + error;
        break;
      }
      if (device) {
        world.reset();
        world = cl::create_world_draw(*device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                      picture, {}, error);
        if (!world) {
          lab.fail(error, "Read the gpu: lines above. Skill: render.");
          return;
        }
      }
    }
    const double t = static_cast<double>(now_ns() - first_ns) * 1e-9;
    if (device && world) {
      ge::render::FrameView view =
          cl::demo_view(bench.demo.state, ge::camera::FirstPersonRow{}.fov_y, t);
      view.frame = static_cast<uint64_t>(frame);
      ge::ui::DrawList overlay;
      overlay.begin(static_cast<int>(w), static_cast<int>(h));
      se::fill_hud(hud, cl::demo_status(bench.demo.state, t), art);
      hud.layout.draw(overlay,
                      hud.layout.frame(static_cast<int>(w), static_cast<int>(h), se::hud_style()),
                      se::hud_style());
      cl::draw_world(*device, *world, view, cl::FxFrame{}, nullptr, 0, overlay, out.view);
      if (bench.skip == 0) cl::record_passes(*world, bench.passes);
      if (shot < 3 && frame == picks[shot]) {
        const ge::gpu::Image img = ge::gpu::read_now(*device, out);
        lab.shot({ids[shot],
                  "DEMO FRAME " + std::to_string(frame) + " OF " + std::to_string(total),
                  "The benchmark's frame " + std::to_string(frame) + " of " + bench.name + " on " +
                      bench.demo.state.map +
                      ", drawn by the window's world frame and "
                      "HUD from the demo's eye.",
                  "the map from the recorded eye, the HUD's stats from the demo",
                  {{"demo", bench.name}, {"size", lab.arg("size")}}},
                 gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
                 gl::Det::measured);
        shot++;
      } else {
        // Each frame finished before the next is timed, as a present waits for a free image.
        ge::gpu::wait(*device, device->queue.OnSubmittedWorkDone(
                                   wgpu::CallbackMode::WaitAnyOnly,
                                   [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {}));
      }
    } else if (!device && shot < 3 && frame == picks[shot]) {
      ls::placeholders(lab, {ids[shot++]});
    }
    cl::timedemo_mark(bench, now_ns());
    frame++;
  }
  if (!stop.empty()) lab.fail(stop, "Read the reason above. Skill: shooter.");

  const cl::BenchStats s = cl::bench_stats(bench.frame_s);
  const std::string setup =
      std::to_string(w) + "x" + std::to_string(h) + ", offscreen" +
      (device ? ", " + device->report.name + " on " + device->report.backend : ", no GPU");
  for (const std::string& line : cl::bench_report(bench.name, setup, s)) lab.note(line);
  lab.metric("demo.frames", static_cast<double>(frame), "count");
  lab.metric("demo.levels", levels, "count");
  lab.metric("fps.average", s.avg_fps, "fps", gl::Det::measured);
  lab.metric("fps.max", s.max_fps, "fps", gl::Det::measured);
  lab.metric("fps.min", s.min_fps, "fps", gl::Det::measured);
  lab.metric("fps.low-1pct", s.low1_fps, "fps", gl::Det::measured);
  lab.metric("fps.low-0.1pct", s.low01_fps, "fps", gl::Det::measured);
  lab.metric("frame.p50", s.p50_ms, "ms", gl::Det::measured);
  lab.metric("frame.p99", s.p99_ms, "ms", gl::Det::measured);

  gl::LineChart lc;
  lc.frame = {"frame time of every timed frame, " + bench.name, setup, "frame", "frame time (ms)"};
  gl::Trace tr{"frame time", {}, {}};
  for (size_t i = 0; i < bench.frame_s.size(); i++) {
    tr.x.push_back(static_cast<double>(i));
    tr.y.push_back(bench.frame_s[i] * 1e3);
  }
  lc.traces.push_back(std::move(tr));
  lc.rules = {s.p50_ms, s.p99_ms};
  const gl::Chart ch = gl::line_chart(lc);
  lab.note(gl::chart_text(ch, 4));
  lab.shot({"frame-times",
            "FRAME TIMES: " + bench.name,
            "Every timed frame's length in milliseconds, in the order drawn; the dashed rules are "
            "the median (" +
                fixed(s.p50_ms, 2) + " ms) and the 99th percentile (" + fixed(s.p99_ms, 2) +
                " ms). Average " + fixed(s.avg_fps, 1) + " fps, 1% low " + fixed(s.low1_fps, 1) +
                " fps.",
            "a flat line near the median; spikes are the frames the 1% low is made of",
            {{"demo", bench.name}, {"size", lab.arg("size")}}},
           gl::to_picture(ch.plot), gl::Det::measured);
  // Where the GPU's time went: each pass's readings as a box, dearest first.
  lab.note(cl::pass_report(bench.passes, 12));
  if (!bench.passes.names.empty()) {
    std::vector<gl::Sample> boxes;
    for (const cl::PassCost& c : cl::pass_costs(bench.passes)) {
      for (size_t i = 0; i < bench.passes.names.size(); i++)
        if (bench.passes.names[i] == c.name) boxes.push_back({c.name, bench.passes.ms[i]});
      lab.metric("gpu." + c.name, c.median_ms, "ms", gl::Det::measured);
    }
    boxes.insert(boxes.begin(), gl::Sample{"(frame span)", bench.passes.span_ms});
    const gl::Chart pc =
        gl::box_chart({"GPU time per pass, " + bench.name + ", r_look " + picture.look, setup,
                       "GPU time (ms)", "pass"},
                      boxes);
    lab.note(gl::chart_text(pc, 4));
    lab.shot(
        {"gpu-passes",
         "GPU BY PASS: " + bench.name,
         "Every timed frame's GPU time for each pass of the world's frame (r_look " + picture.look +
             "), dearest first; the top row is the frame's span on the GPU. Software "
             "adapters' times say nothing about a real GPU.",
         "one or two passes holding most of the span are where the frame's cost is",
         {{"demo", bench.name}, {"size", lab.arg("size")}, {"look", picture.look}}},
        gl::to_picture(pc.plot), gl::Det::measured);
  } else {
    ls::placeholders(lab, {"gpu-passes"});
  }
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-timedemo",
    gl::LabKind::bench,
    "game:shooters_edge",
    "games/shooters_edge/client (the demo readers, the benchmark, the world's frame)",
    "shooter",
    "How fast does the window's frame play a demo back, and how even are its frames?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"format", "single", "the written demo's format: single or client"},
     {"basedir", "", "a folder holding a game folder: play its demo instead of the fixture"},
     {"game", "id1", "the game folder under --basedir"},
     {"demo", "demo1", "the demo to play, by name or path"},
     {"look", "", "r_look for the run: classic or modern (empty: the game's default)"}},
    "--size=320,180",
    run,
};
GE_LAB(kSpec);

}  // namespace
