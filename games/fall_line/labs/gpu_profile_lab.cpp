// LAB `profile`: what a frame of Fall Line costs, pass by pass (docs/rendering.md § Measurement).
// It generates a seed's map, makes the game's frame on a headless device and draws --frames
// frames of the bot's run (with --rivals beside him) from the chase camera at 60 frames a second,
// starting --from seconds in, as the game draws them: one continuous camera, TAA's history and
// the shadows' cascades carried frame to frame. After each frame it waits for the frame's GPU
// times, so every pass's time is this frame's.
//
// It reports, per pass of the pass table, the median, 95th percentile and worst GPU ms, the CPU
// ms encoding it, and the draws and dispatches; the frame's GPU total; the renderer's CPU half
// (every prepare and encode, `FrameStats::cpu_ms`): its median, 95th percentile, worst, and the
// frames over --cpu-budget ms, over every frame but the first (the cut's, which decides everything
// afresh and is reported alone); and where a frame's CPU goes, part by part: the prepare's timing
// zones (GE_ZONE: frame.sky, frame.terrain, frame.figures, frame.instances, frame.shadows), each
// pass's encoding, and the simulation's two steps between frames by their zones (the physics,
// `skier`; the game's logic, `course` and `meet`; the bots, `bot`; the tape and the hash). Every
// frame's numbers are attached as `frames.tsv` (one row a frame, for `ge graph`). Times are
// measured (a software adapter's say nothing of a real GPU's); the pass table and the frame count
// are deterministic. Without an adapter it notes that and reports the deterministic half alone, so
// the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's frame cost per pass (GPU and CPU ms), its prepare and its sim steps
// ge:keywords lab profile bench gpu-ms cpu-ms cpu-half pass-cost median p95 budget frame-cost
// ge:keywords fall-line timestamps chase-camera run zones physics game-logic series
// ge:skill render perf labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/ladder.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "mountain/mapgen/generate.hpp"
#include "prof/zones.hpp"

namespace {

using ge::lab::Det;

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
#endif
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// The value at share `q` (0..1) of the sorted samples (the nearest rank).
double quantile(std::vector<double> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const auto k = static_cast<size_t>(std::ceil(q * static_cast<double>(v.size())));
  return v[std::min(v.size() - 1, k == 0 ? 0 : k - 1)];
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const double from = lab.arg_num("from");
  const double budget = lab.arg_num("cpu-budget");
  const int rivals = static_cast<int>(lab.arg_num("rivals"));
  fall_line::app::FrameOptions options;
  options.taa = lab.arg_num("taa") != 0;
  options.scale = lab.arg_num("scale");
  options.snow = lab.arg_num("snow") != 0;
  options.trails = lab.arg_num("trails") != 0;
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || frames < 1 || from < 0 ||
      options.scale < 0.25 || options.scale > 1) {
    lab.fail(
        "--width and --height must be 1..4096, --frames at least 1, --from at least 0 and "
        "--scale 0.25..1",
        "next: pass a size such as --width=1280 --height=720 --frames=120.");
    return;
  }
  // --quality: the game's r_quality over these options (app/ladder.hpp's quality_pick).
  std::string quality_error;
  if (const auto pick = fall_line::app::quality_pick(
          lab.arg("quality"), lab.arg_num("budget"), static_cast<uint32_t>(width),
          static_cast<uint32_t>(height), quality_error)) {
    options = fall_line::app::apply_picture(*pick, options);
  } else if (!quality_error.empty()) {
    lab.fail(quality_error, "next: pass --quality=custom, low, medium, high, top, auto or 0..1.");
    return;
  }
  const std::string faults = fall_line::app::pass_table_faults();
  lab.metric("pass-table-faults", faults.empty() ? 0 : 1, "count");
  if (!faults.empty()) {
    lab.fail("Fall Line's pass table is out of order: " + faults,
             "Fix kPassTable in games/fall_line/app/frame.cpp. Skill: render.");
  }
  lab.metric("frames", frames, "count");

  auto device = ge::gpu::create_device({.label = "profile", .validation = 0});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: nothing was timed.");
    // The series is attached empty, so the digest (which holds its name) is every row's.
    lab.attach("frames.tsv", {}, Det::measured);
    return;
  }
  const auto& rep = device->report;
  lab.note("adapter: " + rep.name + " / " + rep.driver + " / " + rep.backend +
           (rep.software ? " (software: its times say nothing of a real GPU's)" : ""));

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "profile.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  fall_line::sim::Driver racing(shared, std::max(rivals, 0));
  auto step = [&] { racing.step(fall_line::sim::bot_controls(racing.state(), 0, racing.plan())); };
  while (racing.state().t < from && !racing.over()) step();

  // Per pass, in the table's order: every frame's GPU ms, and the last frame's draws and
  // dispatches.
  std::vector<std::string> names;
  std::map<std::string, std::vector<double>> gpu, pass_cpu;
  // Per CPU part (a zone of the frame or of the steps between frames), its exclusive ms each
  // frame; and the frames' own rows for the series.
  std::vector<std::string> parts;
  std::map<std::string, std::vector<double>> part_ms;
  std::vector<double> prepare, sim;
  std::string series;
  std::vector<ge::prof::ZoneTotal> rows;
  auto fold = [&](size_t a, size_t e, int k, const char* prefix) {
    ge::prof::totals(ge::prof::spans().subspan(a, e - a), rows);
    for (const ge::prof::ZoneTotal& t : rows) {
      const std::string name = prefix + std::string(ge::prof::zone_name(t.name));
      std::vector<double>& v = part_ms[name];
      if (v.empty() && std::find(parts.begin(), parts.end(), name) == parts.end()) {
        parts.push_back(name);
      }
      v.resize(static_cast<size_t>(k) + 1, 0);
      v[static_cast<size_t>(k)] += static_cast<double>(t.excl_ns) / 1e6;
    }
  };
  ge::prof::start(static_cast<size_t>(frames) * 256);
  std::map<std::string, std::pair<uint32_t, uint32_t>> work;
  std::vector<double> cpu, totals, gpu_spans;
  double cpu_first = 0;
  std::vector<ge::skier::SkierState> skiers;
  ge::render::FrameView view;
  fall_line::app::day_view(level, -1, view);
  fall_line::app::cut(*frame);
  for (int k = 0; k < frames; ++k) {
    skiers.clear();
    for (const auto& racer : racing.state().racers) skiers.push_back(racer.skier);
    fall_line::app::chase_view(level, skiers[0], view);
    view.frame = static_cast<uint64_t>(k);
    view.time_s = racing.state().t;
    const size_t drawn_from = ge::prof::spans().size();
    // Two steps between frames: the figures' lags run as in the window at 60 Hz.
    fall_line::app::draw(*device, *frame, view, {skiers, 2.0 / 120.0, k == 0}, target.view);
    const size_t drawn_to = ge::prof::spans().size();
    frame->recorder.wait(*device);
    const ge::render::FrameStats& st = frame->recorder.stats;
    // The first frame after the cut uploads and decides everything afresh (both cascades, the
    // horizon's bakes): reported alone, so the steady frames' figures are the budget's.
    if (k == 0) {
      cpu_first = st.cpu_ms;
    } else {
      cpu.push_back(st.cpu_ms);
    }
    // The sum of the passes' times, and the frame's span on the GPU (FrameStats::gpu_span_ms):
    // on a tiled GPU the passes overlap, so the sum counts the overlap twice; the span is the
    // frame's time.
    double total = 0;
    for (int i = 0; i < st.count; ++i) {
      const ge::render::PassStat& p = st.passes[static_cast<size_t>(i)];
      const std::string name(p.name);
      if (k == 0) names.push_back(name);
      if (std::isfinite(p.gpu_ms)) {
        gpu[name].push_back(p.gpu_ms);
        total += p.gpu_ms;
      }
      work[name] = {p.draws, p.dispatches};
    }
    totals.push_back(total);
    if (k > 0 && std::isfinite(st.gpu_span_ms())) gpu_spans.push_back(st.gpu_span_ms());
    const int64_t t0 = ge::prof::now_ns();
    for (int s = 0; s < 2 && !racing.over(); ++s) step();
    const double sim_ms = static_cast<double>(ge::prof::now_ns() - t0) / 1e6;
    fold(drawn_from, drawn_to, k, "");
    fold(drawn_to, ge::prof::spans().size(), k, "sim.");
    if (k > 0) {
      prepare.push_back(st.prepare_ms);
      sim.push_back(sim_ms);
      for (int i = 0; i < st.count; ++i) {
        const ge::render::PassStat& p = st.passes[static_cast<size_t>(i)];
        pass_cpu[std::string(p.name)].push_back(p.cpu_ms);
      }
    }
    if (k == 0) {
      series = "# title: Fall Line's frame, seed " + fixed(seed, 0);
      series += ": CPU and GPU ms a frame\n# x: frame\n# unit.cpu_ms: ms\n# unit.gpu_ms: ms\n";
      series += "frame\tcpu_ms\tprepare_ms\tsim_ms\tgpu_ms";
      for (int i = 0; i < st.count; ++i) {
        const std::string name(st.passes[static_cast<size_t>(i)].name);
        series += "\tgpu.";
        series += name;
        series += "\tcpu.";
        series += name;
      }
      series += "\n";
    }
    auto cell = [&](double v) {
      series += "\t";
      series += fixed(v, 4);
    };
    series += std::to_string(k);
    cell(st.cpu_ms);
    cell(st.prepare_ms);
    cell(sim_ms);
    cell(total);
    for (int i = 0; i < st.count; ++i) {
      const ge::render::PassStat& p = st.passes[static_cast<size_t>(i)];
      cell(std::isfinite(p.gpu_ms) ? p.gpu_ms : 0);
      cell(p.cpu_ms);
    }
    series += "\n";
  }
  ge::prof::stop();
  lab.metric("passes", static_cast<double>(names.size()), "count", Det::measured);

  ge::lab::LabTable table{"GPU ms per pass over " + std::to_string(frames) + " frames",
                          {"pass", "median", "p95", "worst", "cpu median", "draws", "dispatches"},
                          {}};
  for (const std::string& name : names) {
    const std::vector<double>& v = gpu[name];
    const double med = quantile(v, 0.5), p95 = quantile(v, 0.95), worst = quantile(v, 1.0);
    const double cpu_med = quantile(pass_cpu[name], 0.5);
    table.rows.push_back({name, fixed(med, 3), fixed(p95, 3), fixed(worst, 3), fixed(cpu_med, 3),
                          std::to_string(work[name].first), std::to_string(work[name].second)});
    lab.metric("cpu-ms." + name + ".median", cpu_med, "ms", Det::measured);
    lab.metric("gpu-ms." + name + ".median", med, "ms", Det::measured);
    lab.metric("gpu-ms." + name + ".p95", p95, "ms", Det::measured);
  }
  table.rows.push_back({"(frame)", fixed(quantile(totals, 0.5), 3),
                        fixed(quantile(totals, 0.95), 3), fixed(quantile(totals, 1.0), 3),
                        fixed(quantile(cpu, 0.5), 3), "", ""});
  table.rows.push_back({"(span)", fixed(quantile(gpu_spans, 0.5), 3),
                        fixed(quantile(gpu_spans, 0.95), 3), fixed(quantile(gpu_spans, 1.0), 3), "",
                        "", ""});
  lab.metric("gpu-ms.span.median", quantile(gpu_spans, 0.5), "ms", Det::measured);
  lab.metric("gpu-ms.span.p95", quantile(gpu_spans, 0.95), "ms", Det::measured);
  lab.table(std::move(table), Det::measured);

  // Where a frame's CPU goes: the prepare's zones, then the simulation's steps between frames.
  ge::lab::LabTable split{"CPU ms a frame, by part (exclusive: a zone's own time)",
                          {"part", "median", "p95", "worst"},
                          {}};
  split.rows.push_back({"prepare (before the first pass)", fixed(quantile(prepare, 0.5), 3),
                        fixed(quantile(prepare, 0.95), 3), fixed(quantile(prepare, 1.0), 3)});
  for (const std::string& name : parts) {
    std::vector<double> v = part_ms[name];
    v.resize(static_cast<size_t>(frames), 0);
    v.erase(v.begin());  // the cut's frame is reported alone
    split.rows.push_back({name, fixed(quantile(v, 0.5), 3), fixed(quantile(v, 0.95), 3),
                          fixed(quantile(v, 1.0), 3)});
    lab.metric("cpu-ms.zone." + name + ".median", quantile(v, 0.5), "ms", Det::measured);
  }
  split.rows.push_back({"sim (two steps a frame)", fixed(quantile(sim, 0.5), 3),
                        fixed(quantile(sim, 0.95), 3), fixed(quantile(sim, 1.0), 3)});
  lab.table(std::move(split), Det::measured);
  lab.metric("cpu-ms.prepare.median", quantile(prepare, 0.5), "ms", Det::measured);
  lab.metric("sim-ms.median", quantile(sim, 0.5), "ms", Det::measured);
  lab.attach("frames.tsv", std::vector<uint8_t>(series.begin(), series.end()), Det::measured);
  lab.metric("gpu-ms.frame.median", quantile(totals, 0.5), "ms", Det::measured);
  lab.metric("gpu-ms.frame.p95", quantile(totals, 0.95), "ms", Det::measured);
  lab.metric("cpu-ms.median", quantile(cpu, 0.5), "ms", Det::measured);
  lab.metric("cpu-ms.p95", quantile(cpu, 0.95), "ms", Det::measured);
  lab.metric("cpu-ms.worst", quantile(cpu, 1.0), "ms", Det::measured);
  const auto over = std::count_if(cpu.begin(), cpu.end(), [&](double ms) { return ms > budget; });
  lab.metric("cpu-over-budget", static_cast<double>(over), "count", Det::measured);
  lab.metric("cpu-ms.first", cpu_first, "ms", Det::measured);
  lab.note("the CPU half (every prepare and encode): median " + fixed(quantile(cpu, 0.5), 3) +
           " ms, p95 " + fixed(quantile(cpu, 0.95), 3) + " ms, worst " +
           fixed(quantile(cpu, 1.0), 3) + " ms; " + std::to_string(over) + " of " +
           std::to_string(cpu.size()) + " frames after the first over the " + fixed(budget, 2) +
           " ms budget; the first frame after the cut " + fixed(cpu_first, 3) +
           " ms (a debug or sanitizer build is several times slower: judge a release one)");
  lab.metric("gpu-errors", device->errors.load(), "count", Det::measured);
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "profile",
    ge::lab::LabKind::bench,
    "game:fall_line",
    "games/fall_line/app (the frame and its pass table) over engine/render",
    "perf",
    "What does each pass of Fall Line's frame cost on the GPU, and the renderer on the CPU?",
    {
        {"seed", "38", "the map's seed"},
        {"width", "1280", "the output's width, px"},
        {"height", "720", "the output's height, px"},
        {"taa", "1", "TAA-U on (1) or off (0)"},
        {"scale", "1", "the render size over the output's, with TAA (0.25..1)"},
        {"frames", "120", "frames drawn and timed (at 60 a second of the run)"},
        {"from", "10", "seconds into the bot's run the first frame is drawn at"},
        {"rivals", "3", "the bots racing beside him"},
        {"snow", "1", "the snow's material (1) or the albedo lit (0)"},
        {"trails", "1", "the tracks (1) or none (0); drawn only with the snow's material"},
        {"cpu-budget", "1", "the CPU half's budget a frame, ms"},
        {"quality", "custom",
         "the game's r_quality: custom (the flags above), low, medium, high, top, auto or 0..1"},
        {"budget", "8.3", "the GPU ms a frame fits with --quality=auto (the game's r_budget)"},
    },
    "--width=160 --height=90 --frames=4 --from=3 --rivals=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
