// LAB `se-timedemo`: THE DEMO BENCHMARK WITHOUT A WINDOW. A recorded game played flat out the
// way the window's `timedemo` command plays it (client/timedemo.hpp: one demo frame a drawn
// frame, every frame timed but the first after a level start), drawn into an offscreen target by
// the window's own world frame and HUD. By default the demo is written in code (demo_write.hpp):
// a walk round the fixture halls (plugins/brush) in the format `--format` names, in a pack on
// the search path; `--format=match` is the highlight finder's four-player match instead (with
// `--stage-pak=fixture`, the other players drawn as the window draws them). `--basedir` (and
// `--game`) opens a game folder instead and `--demo` names its demo (`demo1`), so the same lab
// measures a real recording on any machine. Frames are paced as the window paces them: at most one
// in flight, the frame before waited for once this one is submitted, so the CPU's part overlaps
// the GPU's; the three pictures' frames (each waits for its read back) are not timed.
//
// THE CPU (`cpu by zone`): every timed frame's timing zones (prof/zones.hpp: the demo's step, the
// HUD, the world's prepares, its encode and submit, the wait for the frame before), each zone's
// median a frame, as a note and as `cpu.<zone>` metrics.
//
// What it shows: three frames of the run (its start, the middle, the end), the frame time and the
// GPU span of every timed frame as lines with the median and the 99th percentile as rules, and
// the report as metrics. The frame count and the levels are deterministic; every time is measured.
//
// THE PRICE (`--price=1`): the demo played again with each drawn ce_ switch off in turn and once
// without the HUD, each run between two all-on runs; each run's frames as a box and what each
// switch saved off the median frame against the all-on runs either side (timedemo.hpp's `prices`).
// A pass's own GPU timestamps cannot rank the passes (a tiled GPU overlaps them); the whole frame
// with one thing gone can.
//
// ge:summary Lab: the demo benchmark headless: a demo played flat out, its frames and fps chart
// ge:keywords lab se-timedemo timedemo benchmark demo fps frame-time one-percent-low chart gpu
// ge:keywords offscreen price switch-off-pricing cpu-zones frame-in-flight
// ge:skill shooter perf labs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "classicenhance/switches.hpp"
#include "core/libm/libm.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "prof/zones.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/labs/staged.hpp"
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
  std::vector<uint8_t> demo = format == "client"      ? cl::write_client_demo(s)
                              : format == "multiview" ? cl::write_multiview_demo(s)
                              : format == "match" ? cl::write_multiview_match(cl::highlight_match())
                                                  : cl::write_single_demo(s);
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}, {"demo1.dem", demo}}, &out, &error);
}

// One play of the demo with `picture` (and the HUD unless `no_hud`): every timed frame and its
// GPU span; the three pictures when `picks` is given.
// A point in front of the demo's eye: within about 40 degrees of where it looks.
constexpr int64_t kBlastLag = 1;  // frames from a blast read to its picture
constexpr double kDegToRad = 3.14159265358979323846 / 180;
bool in_front(const cl::DemoState& s, const ge::shooter::Vec3f& at) {
  const double yaw = s.angles.y * kDegToRad, pitch = s.angles.x * kDegToRad;
  const double f[3] = {ge::core::libm::cos(pitch) * ge::core::libm::cos(yaw),
                       ge::core::libm::cos(pitch) * ge::core::libm::sin(yaw),
                       -ge::core::libm::sin(pitch)};
  const double d[3] = {at.x - s.eye.x, at.y - s.eye.y, at.z - s.eye.z};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  return len > 0 && (f[0] * d[0] + f[1] * d[1] + f[2] * d[2]) / len > 0.75;
}

struct Played {
  cl::Timedemo bench;
  int64_t frames = 0;
  std::string stop;
  int64_t explosions = 0, effects = 0;  // the demo's temporary entities read (demo_fx.hpp)
  int64_t blast_frame = -1;             // the frame the `blast` picture caught (-1: none)
  // With `zones` asked for: each timing zone's ms in every timed frame (prof/zones.hpp), the
  // zones in the order they first opened.
  bool zones = false;
  std::vector<std::pair<uint32_t, std::vector<double>>> zone_ms;
};

struct Rig {
  gl::Lab& lab;
  const cl::SearchPath& files;
  std::string name;
  ge::gpu::Device* device = nullptr;
  ge::gpu::Target* out = nullptr;
  uint32_t w = 0, h = 0;
  int64_t total = 0;
};

bool play(Rig& rig, const shooters_edge::look::Picture& picture, bool no_hud, bool shots,
          Played& p) {
  gl::Lab& lab = rig.lab;
  std::string error;
  if (!cl::timedemo_start(rig.files, rig.name, p.bench, error)) {
    lab.fail(error, "Name a demo the packs hold (--demo=demo1). Skill: shooter.");
    return false;
  }
  cl::Timedemo& bench = p.bench;
  cl::WorldDrawPtr world;
  size_t world_models = 0;  // the model list's length the world was made with
  cl::FxFrame fx;
  cl::FxState fx_state;  // the demo's effects as they live, as the window
  cl::DemoFx demo_fx;
  fx_state.on = cl::fx_switches(picture);
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(rig.files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }
  std::unique_ptr<cl::LoadedMap> map;
  se::Hud hud = se::make_hud();
  se::HudArt art = se::free_art();
  art.finish();
  const int64_t total = rig.total;
  const int64_t picks[3] = {std::min<int64_t>(total - 1, cl::kSkipAfterLevel + 1), total / 2,
                            total - 1};
  const char* ids[3] = {"start", "middle", "end"};
  int shot = shots ? 0 : 3;
  int64_t blast_at = -1;  // the frame the `blast` picture is taken (-1: no blast seen yet)
  const uint32_t w = rig.w, h = rig.h;
  ge::gpu::Device* device = rig.device;
  const uint64_t first_ns = now_ns();
  // At most one frame in flight, as the window keeps it (client/main.cpp): the frame before this
  // one is waited for after this one is submitted, so the CPU's part overlaps the GPU's.
  wgpu::Future in_flight{};
  const auto settle = [&] {
    if (device && in_flight.id != 0) ge::gpu::wait(*device, in_flight);
    in_flight = {};
  };
  std::vector<ge::prof::ZoneTotal> rows;
  if (p.zones) {
    ge::prof::start(4096);
    rows.reserve(64);
  }
  for (;;) {
    cl::TimedemoStep st;
    {
      GE_ZONE("demo.step");
      st = cl::timedemo_step(bench, error);
    }
    if (st == cl::TimedemoStep::end) break;
    if (st == cl::TimedemoStep::error) {
      p.stop = error;
      break;
    }
    if (st == cl::TimedemoStep::new_level) {
      settle();
      demo_fx = {};
      fx_state.explosions_seen = fx_state.effects_seen = 0;
      map = cl::load_named_map(rig.files, bench.demo.state.map, error);
      if (!map) {
        p.stop = "the demo's map " + bench.demo.state.map + ": " + error;
        break;
      }
      if (device) {
        world.reset();
        world =
            cl::create_world_draw(*device, rig.files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                  picture, {}, error, {bench.demo.state.models, {}});
        world_models = bench.demo.state.models.size();
        if (!world) {
          lab.fail(error, "Read the gpu: lines above. Skill: render.");
          return false;
        }
      }
    }
    // A client recording's model list may grow after its level started: the window's rule.
    if (device && world && map && bench.demo.state.models.size() != world_models) {
      settle();
      world.reset();
      world = cl::create_world_draw(*device, rig.files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                    picture, {}, error, {bench.demo.state.models, {}});
      if (!world) {
        lab.fail(error, "Read the gpu: lines above. Skill: render.");
        return false;
      }
      world_models = bench.demo.state.models.size();
    }
    for (const cl::DemoEffect& e : bench.demo.state.effects)
      (e.kind == cl::DemoEffectKind::explosion ? p.explosions : p.effects)++;
    const double t = static_cast<double>(now_ns() - first_ns) * 1e-9;
    if (device && world) {
      ge::render::FrameView view =
          cl::demo_view(bench.demo.state, ge::camera::FirstPersonRow{}.fov_y, t);
      view.frame = static_cast<uint64_t>(p.frames);
      ge::ui::DrawList overlay;
      overlay.begin(static_cast<int>(w), static_cast<int>(h));
      if (!no_hud) {
        GE_ZONE("hud");
        se::fill_hud(hud, cl::demo_status(bench.demo.state, t), art);
        hud.layout.draw(overlay,
                        hud.layout.frame(static_cast<int>(w), static_cast<int>(h), se::hud_style()),
                        se::hud_style());
      }
      {
        GE_ZONE("placed");  // the players, entities and effects, as the window
        fx_state.models = &cl::world_models(*world);
        const size_t had = demo_fx.explosions.size();
        cl::demo_fx_step(bench.demo.state, demo_fx, map.get(), view, fx_state, t, 1.0 / 72, fx);
        // The first explosion in front of the eye: its picture a few frames into its burst.
        for (size_t k = had; blast_at < 0 && k < demo_fx.explosions.size(); k++)
          if (in_front(bench.demo.state, demo_fx.explosions[k])) blast_at = p.frames + kBlastLag;
      }
      cl::draw_world(*device, *world, view, fx, nullptr, 0, overlay, rig.out->view);
      // A picture's frame waits for its read back, which a window never does: not timed.
      if (shot < 3 && p.frames == picks[shot]) bench.skip = std::max(bench.skip, 1);
      if (bench.skip == 0) cl::record_span(*world, bench.passes);
      if (shots && p.frames == blast_at) {
        settle();
        const ge::gpu::Image img = ge::gpu::read_now(*device, *rig.out);
        lab.shot({"blast",
                  "A BLAST IN A PLAYED DEMO",
                  "The first explosion the demo sends in front of the recorded eye, " +
                      std::to_string(kBlastLag) +
                      " frames into its burst: the temporary entity the recording carries, drawn "
                      "by the effects a game played here draws.",
                  "the fireball and its light on the walls and floor where the rocket ended; "
                  "the rocket's trail before it",
                  {{"demo", bench.name}, {"frame", std::to_string(p.frames)}}},
                 gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
                 gl::Det::measured);
        p.blast_frame = p.frames;
      }
      if (shot < 3 && p.frames == picks[shot]) {
        settle();
        const ge::gpu::Image img = ge::gpu::read_now(*device, *rig.out);
        lab.shot(
            {ids[shot],
             "DEMO FRAME " + std::to_string(p.frames) + " OF " + std::to_string(total),
             "The benchmark's frame " + std::to_string(p.frames) + " of " + bench.name + " on " +
                 bench.demo.state.map +
                 ", drawn by the window's world frame and "
                 "HUD from the demo's eye.",
             "the map from the recorded eye, the other players where the demo has them, the HUD's "
             "stats from the demo",
             {{"demo", bench.name}, {"size", lab.arg("size")}}},
            gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
            gl::Det::measured);
        shot++;
      } else {
        const wgpu::Future done = device->queue.OnSubmittedWorkDone(
            wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
        GE_ZONE("gpu.wait");
        settle();
        in_flight = done;
      }
    } else if (!device && shot < 3 && p.frames == picks[shot]) {
      ls::placeholders(lab, {ids[shot++]});
    }
    // The frame's zones, kept when it is timed (the frames a level's load holds back are not).
    if (p.zones) {
      if (bench.skip == 0) {
        ge::prof::totals(ge::prof::spans(), rows);
        for (const ge::prof::ZoneTotal& z : rows) {
          auto at = std::find_if(p.zone_ms.begin(), p.zone_ms.end(),
                                 [&](const auto& e) { return e.first == z.name; });
          if (at == p.zone_ms.end()) at = p.zone_ms.insert(p.zone_ms.end(), {z.name, {}});
          at->second.push_back(static_cast<double>(z.incl_ns) * 1e-6);
        }
      }
      ge::prof::clear();
    }
    cl::timedemo_mark(bench, now_ns());
    p.frames++;
  }
  settle();
  if (p.zones) ge::prof::stop();
  return true;
}

// A run's timed frames in ms.
std::vector<double> ms_of(const std::vector<double>& frame_s) {
  std::vector<double> ms;
  ms.reserve(frame_s.size());
  for (const double f : frame_s) ms.push_back(f * 1e3);
  return ms;
}

cl::PriceRun price_run(const std::string& name, const Played& p) {
  const std::vector<double> ms = ms_of(p.bench.frame_s);
  return {name, cl::median_of(ms),
          p.bench.passes.span_ms.empty() ? std::nan("") : cl::median_of(p.bench.passes.span_ms)};
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  const std::string basedir = lab.arg("basedir");
  const std::string name = lab.arg("demo");
  const bool price = lab.arg("price") == "1";
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
  if (!shooters_edge::labs::stage_asked(lab, files)) return;
  {
    cl::Timedemo probe;
    if (!cl::timedemo_start(files, name, probe, error)) {
      lab.fail(error, "Name a demo the packs hold (--demo=demo1). Skill: shooter.");
      return;
    }
    lab.note("demo: " + probe.found);
  }

  auto device = ge::gpu::create_device({.label = "se-timedemo", .validation = 0});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  if (device) lab.note("adapter: " + ge::gpu::adapter_line(*device));
  ge::gpu::Target out;
  if (device) out = ge::gpu::create_target(*device, "se-timedemo.final", w, h);

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
  Rig rig{lab, files, name, device ? device.get() : nullptr, &out, w, h, total};
  Played base;
  base.zones = true;
  if (!play(rig, picture, false, true, base)) return;
  if (!base.stop.empty()) lab.fail(base.stop, "Read the reason above. Skill: shooter.");
  const cl::Timedemo& bench = base.bench;

  const cl::BenchStats s = cl::bench_stats(bench.frame_s);
  const std::string setup =
      std::to_string(w) + "x" + std::to_string(h) + ", offscreen" +
      (device ? ", " + device->report.name + " on " + device->report.backend : ", no GPU");
  for (const std::string& line : cl::bench_report(bench.name, setup, s)) lab.note(line);
  lab.note(cl::span_report(bench.passes));
  for (const std::string& line : cl::pass_report(bench.passes, bench.passes.span_ms.size()))
    lab.note(line);
  for (const auto& [pass, ms] : bench.passes.pass_ms) {
    if (bench.passes.span_ms.empty()) break;
    double sum = 0;
    for (const double m : ms) sum += m;
    lab.metric("gpu.pass." + pass, sum / static_cast<double>(bench.passes.span_ms.size()), "ms",
               gl::Det::measured);
  }
  // Where the CPU's part of a frame goes: each zone's median a timed frame (nested zones inside
  // their parent's), the frame's own median beside them.
  {
    std::string line = "cpu by zone, medians (ms): frame " + fixed(s.p50_ms, 3);
    for (const auto& [zone, ms] : base.zone_ms) {
      const double m = cl::median_of(ms);
      line += "; " + std::string(ge::prof::zone_name(zone)) + " " + fixed(m, 3);
      lab.metric("cpu." + std::string(ge::prof::zone_name(zone)), m, "ms", gl::Det::measured);
    }
    lab.note(line);
  }
  lab.metric("demo.frames", static_cast<double>(base.frames), "count");
  lab.metric("demo.levels", levels, "count");
  lab.metric("demo.explosions", static_cast<double>(base.explosions), "count");
  lab.metric("demo.effects", static_cast<double>(base.effects), "count");
  lab.metric("fps.average", s.avg_fps, "fps", gl::Det::measured);
  lab.metric("fps.max", s.max_fps, "fps", gl::Det::measured);
  lab.metric("fps.min", s.min_fps, "fps", gl::Det::measured);
  lab.metric("fps.low-1pct", s.low1_fps, "fps", gl::Det::measured);
  lab.metric("fps.low-0.1pct", s.low01_fps, "fps", gl::Det::measured);
  lab.metric("frame.p50", s.p50_ms, "ms", gl::Det::measured);
  lab.metric("frame.p99", s.p99_ms, "ms", gl::Det::measured);
  if (!bench.passes.span_ms.empty())
    lab.metric("gpu.span.p50", cl::median_of(bench.passes.span_ms), "ms", gl::Det::measured);

  gl::LineChart lc;
  lc.frame = {"frame time of every timed frame, " + bench.name, setup, "frame", "frame time (ms)"};
  gl::Trace tr{"frame time", {}, {}};
  for (size_t i = 0; i < bench.frame_s.size(); i++) {
    tr.x.push_back(static_cast<double>(i));
    tr.y.push_back(bench.frame_s[i] * 1e3);
  }
  lc.traces.push_back(std::move(tr));
  if (!bench.passes.span_ms.empty()) {
    gl::Trace sp{"GPU span", {}, {}};
    for (size_t i = 0; i < bench.passes.span_ms.size(); i++) {
      sp.x.push_back(static_cast<double>(i));
      sp.y.push_back(bench.passes.span_ms[i]);
    }
    lc.traces.push_back(std::move(sp));
  }
  lc.rules = {s.p50_ms, s.p99_ms};
  const gl::Chart ch = gl::line_chart(lc);
  lab.note(gl::chart_text(ch, 4));
  lab.shot({"frame-times",
            "FRAME TIMES: " + bench.name,
            "Every timed frame's length in milliseconds, in the order drawn, and its GPU span "
            "where the GPU has timestamp queries; the dashed rules are the median (" +
                fixed(s.p50_ms, 2) + " ms) and the 99th percentile (" + fixed(s.p99_ms, 2) +
                " ms). Average " + fixed(s.avg_fps, 1) + " fps, 1% low " + fixed(s.low1_fps, 1) +
                " fps.",
            "a flat line near the median; spikes are the frames the 1% low is made of",
            {{"demo", bench.name}, {"size", lab.arg("size")}}},
           gl::to_picture(ch.plot), gl::Det::measured);

  // THE PRICE: the demo again with each drawn switch off in turn, each between two all-on runs.
  if (price && device) {
    std::vector<cl::PriceRun> bases{price_run("", base)}, off;
    std::vector<std::vector<double>> frames_of;               // each switched-off run's frames, ms
    std::vector<double> base_ms = ms_of(base.bench.frame_s);  // every all-on run's frames, ms
    const auto row = [&](const std::string& label, const shooters_edge::look::Picture& pic,
                         bool no_hud) {
      Played p;
      if (!play(rig, pic, no_hud, false, p)) return false;
      off.push_back(price_run(label, p));
      frames_of.push_back(ms_of(p.bench.frame_s));
      Played again;
      if (!play(rig, picture, false, false, again)) return false;
      bases.push_back(price_run("", again));
      const std::vector<double> again_ms = ms_of(again.bench.frame_s);
      base_ms.insert(base_ms.end(), again_ms.begin(), again_ms.end());
      lab.note("price: " + label + " off, median frame " + fixed(off.back().frame_p50_ms, 2) +
               " ms, everything on after it " + fixed(bases.back().frame_p50_ms, 2) + " ms");
      return true;
    };
    for (const ge::classicenhance::SwitchSpec& sw : ge::classicenhance::switches()) {
      if (sw.bake || sw.group == "SOUND" || !(picture.ce.*sw.field)) continue;
      const std::string only = "," + lab.arg("price-only") + ",";
      if (only != ",," && only.find("," + std::string(sw.id) + ",") == std::string::npos) continue;
      shooters_edge::look::Picture without = picture;
      without.ce.*sw.field = false;
      if (!row("ce_" + std::string(sw.id), without, false)) return;
    }
    if (lab.arg("price-only").empty() && !row("the HUD", picture, true)) return;
    const std::vector<cl::Price> saved = cl::prices(bases, off);
    for (const std::string& line : cl::price_report(bases, saved)) lab.note(line);
    for (const cl::Price& pr : saved)
      lab.metric("price." + pr.name, pr.frame_ms, "ms", gl::Det::measured);
    lab.metric("price.noise", cl::price_noise(bases), "ms", gl::Det::measured);
    // Every all-on run's frames as one box, then each switched-off run's, the most saved first.
    std::vector<gl::Sample> boxes{
        {"everything on (" + std::to_string(bases.size()) + " runs)", base_ms}};
    for (const cl::Price& pr : saved)
      for (size_t i = 0; i < off.size(); i++)
        if (off[i].name == pr.name)
          boxes.push_back({pr.name + " off, saves " + fixed(pr.frame_ms, 2), frames_of[i]});
    const gl::Chart pc =
        gl::box_chart({"frame time with each thing off, " + bench.name + ", r_look " + picture.look,
                       setup, "frame time (ms)", "run"},
                      boxes);
    lab.note(gl::chart_text(pc, 4));
    lab.shot({"price",
              "THE PRICE: " + bench.name,
              "The whole demo played with each drawn switch off in turn (and once without the "
              "HUD), each run between two runs with everything on; each run's frames as a box, "
              "the most saved first, and its saving against the mean of the all-on runs either "
              "side of it. A saving under " +
                  fixed(cl::price_noise(bases), 2) +
                  " ms (the largest gap between neighbouring all-on runs) is noise.",
              "the rows whose saving clears the noise are what the look pays for",
              {{"demo", bench.name}, {"size", lab.arg("size")}, {"look", picture.look}}},
             gl::to_picture(pc.plot), gl::Det::measured);
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
     {"format", "single",
      "the written demo's format: single, client, multiview, or match (four players)"},
     {"basedir", "", "a folder holding a game folder: play its demo instead of the fixture"},
     {"game", ge::classicenhance::kProfiles[0].base_dir.data(), "the game folder under --basedir"},
     {"demo", "demo1", "the demo to play, by name or path"},
     {"look", "", "r_look for the run: classic or modern (empty: the game's default)"},
     {"price", "0",
      "1: play the demo again with each drawn ce_ switch off in turn (and without the HUD), each "
      "between two all-on runs, and chart what each saved off the whole frame"},
     {"price-only", "",
      "with --price=1: price only these switches, ids without the ce_ (relief,parallax); empty: "
      "every drawn one"}},
    "--size=320,180",
    run,
};
GE_LAB(kSpec);

}  // namespace
