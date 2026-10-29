// LAB `gpu-snow`: THE SNOW LAYER MIRRORED ON THE GPU (render/snow_mirror.hpp), on the software
// adapter. A square of powder --size m a side, 40 cm deep, is skied top to bottom by a skier the
// game names as the focus, carving S-turns, while a snowmobile climbs the left edge and a walker
// plods up the right, far from him. The layer keeps --budget fine tiles and folds what no focus is
// near every second (snow/lod.hpp); the mirror's fine window of --window tiles follows the skier
// and is updated every fourth step, as a game would once a frame. Pictured:
//
// - THE DROP read twice at the end, side by side: on the CPU (`snow::sample`) and on the GPU
//   (`snow_column` through the mirror), shaded by how far the snow stands under the cover;
// - THE SOURCES the GPU reads each sample from: the fine atlas, the coarse view, the cover, with
//   the window outlined;
// - THE UPLOADS: the bytes each update sent over the run.
//
// The lab fails when a raw sample the GPU reads is not the one the CPU expects
// (`snow_mirror_expect`), when the GPU's column strays from the physics' by a tenth of a
// millimetre, or on a GPU error. With no adapter the pictures are placeholders.
//
// ge:summary Lab: the snow layer mirrored on the GPU: the drop twice, the sources, the uploads
// ge:keywords lab gpu snow snow-mirror atlas page-table coarse-view uploads readback drop window
// ge:keywords software-adapter
// ge:skill snow render labs
// ge:test tests/render_snow_mirror_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/frame.hpp"
#include "render/snow_marks.hpp"
#include "render/snow_mirror.hpp"
#include "snow/layer.hpp"
#include "snow/lod.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"

namespace {

namespace sn = ge::snow;
namespace gl = ge::lab;
namespace gr = ge::render;
namespace lm = ge::core::libm;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

constexpr double kHz = 120;     // steps a second
constexpr double kSpeed = 12;   // m/s down the square
constexpr double kCover = 0.4;  // m of powder
constexpr int kEvery = 4;       // steps between mirror updates

// The skier's line: S-turns down the middle.
double line_x(double z, double half) { return 0.2 * half * lm::sin(z * 0.07); }

// A skier's pair of skis at (x, z) heading `heading`, each from where it stood a step ago.
struct Skier {
  double last[2][2] = {{0, 0}, {0, 0}};
  bool started = false;
  void step(sn::Layer& layer, double x, double z, double heading, double t, int& presses) {
    const double rx = lm::cos(heading), rz = -lm::sin(heading);
    for (int k = 0; k < 2; k++) {
      const double side = k ? 0.11 : -0.11;
      const double sx = x + rx * side, sz = z + rz * side;
      sn::Imprint imp;
      imp.shape = sn::capsule(started ? last[k][0] : sx, started ? last[k][1] : sz, sx, sz, 0.035);
      imp.sink = 0.08;
      imp.side_push = 0.05;
      imp.motion = sn::Motion::slide;
      sn::press(layer, imp, t);
      presses++;
      last[k][0] = sx;
      last[k][1] = sz;
    }
    started = true;
  }
};

struct Run {
  int presses = 0, steps = 0;
  double end_x = 0, end_z = 0;
};

// The traffic, step by step; `each` is called every kEvery steps with the skier's place and the
// time, for the mirror's update.
template <typename F>
Run ski(sn::Layer& layer, double half, F&& each) {
  Run run;
  Skier skier;
  const sn::Imprint tread = sn::reference_contact(4).imprint;
  const sn::Imprint boot = sn::reference_contact(2).imprint;
  const double v = kSpeed / kHz, top = 2 * half - 4;
  run.steps = static_cast<int>((top - 4) / v);
  for (int n = 0; n < run.steps; n++) {
    const double t = n / kHz, z = top - n * v, x = line_x(z, half);
    const double focus[2] = {x, z};
    sn::set_focus(layer, focus, t);
    const double heading = lm::atan2(line_x(z - v, half) - x, -v);
    skier.step(layer, x, z, heading, t, run.presses);
    if (n % 4 == 0) {  // the snowmobile climbing the left edge, 1.5 times the skier's pace
      const double zs = 4 + n * v * 1.5;
      if (zs < top) {
        sn::press(layer, sn::place(tread, -half + 10, zs, 0), t);
        run.presses++;
      }
    }
    if (n % 48 == 0) {  // the walker plodding up the right, a boot every 0.4 s
      const double zw = 4 + n * v * 0.12;
      sn::press(layer, sn::place(boot, half - 12 + 0.3 * ((n / 48) % 2), zw, 0), t);
      run.presses++;
    }
    if (n % static_cast<int>(kHz) == 0) sn::fold_stale(layer, t);
    if (n % kEvery == 0) each(x, z, t);
    run.end_x = x;
    run.end_z = z;
  }
  return run;
}

void outline(gl::Picture& p, double x0, double y0, double s, gl::Rgba c) {
  gl::draw_line(p, x0, y0, x0 + s, y0, c);
  gl::draw_line(p, x0, y0 + s, x0 + s, y0 + s, c);
  gl::draw_line(p, x0, y0, x0, y0 + s, c);
  gl::draw_line(p, x0 + s, y0, x0 + s, y0 + s, c);
}

// The shade of a column whose surface stands `drop` m under the cover; `scale` m reads darkest.
gl::Rgba worn(double drop, double scale) {
  const double w = ge::core::clamp(drop / scale, -0.5, 1);
  if (w < 0) {  // a berm: warm
    const double b = -2 * w;
    return {static_cast<uint8_t>(250 - 20 * b), static_cast<uint8_t>(250 - 60 * b),
            static_cast<uint8_t>(255 - 130 * b), 255};
  }
  auto mix = [&](double a, double b) { return static_cast<uint8_t>(a + (b - a) * w); };
  return {mix(250, 40), mix(251, 70), mix(255, 120), 255};
}

void run(gl::Lab& lab) {
  const double size = lab.arg_num("size"), half = size / 2;
  const auto budget = static_cast<uint32_t>(lab.arg_num("budget"));
  const int window = static_cast<int>(lab.arg_num("window"));
  const int ppm = static_cast<int>(lab.arg_num("px-per-m"));
  const std::vector<std::string> ids = {"drop", "near", "marks", "sources", "uploads"};

  sn::LayerSpec spec;
  spec.region = {-half, 0, half, size};
  spec.cover.depth = kCover;
  spec.max_tiles = budget;
  spec.near = 32;
  spec.stale = 5;
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, spec);

  auto device = ge::gpu::create_device({.label = "gpu-snow", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    std::vector<gr::MarkStamp> stamps;
    const Run r = ski(layer, half, [&](double, double, double) {
      gr::marks_from_records(layer.records, stamps);
      sn::clear_records(layer);
    });
    gr::marks_from_records(layer.records, stamps);  // the steps after the last update's
    lab.metric("presses", r.presses, "count");
    lab.metric("folds", layer.folds, "count");
    lab.metric("marks", static_cast<double>(stamps.size()), "count");
    lab.metric("mismatches", 0, "count");
    placeholders(lab, ids, 640, 360);
    return;
  }
  ge::gpu::Device& d = *device;
  gr::SnowMirror mirror;
  gr::create_snow_mirror(d, mirror, layer, {.window = window});
  // THE FINE MARKS, from the layer's imprint records: a 48 m window at 2.3 cm a texel.
  ge::gpu::PipelineSet pipelines;
  gr::SnowMarks marks;
  gr::MarkOptions mark_options;
  mark_options.fine_size = 2048;
  mark_options.fine_span = 48;
  mark_options.coarse_size = 512;
  gr::create_snow_marks(d, pipelines, marks, -half, 0, size, mark_options);
  if (!pipelines.finish(d))
    lab.fail("the marks' pipeline failed", "Read the line above. Skill: gpu.");
  gr::FrameRecorder rec;
  rec.init(d, "gpu-snow", 2);
  std::vector<gr::MarkStamp> stamps;
  size_t stamped = 0;
  const auto mark_frame = [&](double x, double z) {
    gr::marks_from_records(layer.records, stamps);
    sn::clear_records(layer);
    gr::add_mark_stamps(marks, std::span(stamps).subspan(stamped));
    stamped = stamps.size();
    rec.begin_frame(d);
    gr::prepare_snow_marks(d, pipelines, marks, x, z);
    wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
    gr::encode_snow_marks(marks, pipelines, rec, encoder);
    rec.end_frame(encoder);
    wgpu::CommandBuffer commands = encoder.Finish();
    d.queue.Submit(1, &commands);
    rec.after_submit(d);
  };

  using Clock = std::chrono::steady_clock;
  gl::Trace sent{"KB sent", {}, {}}, tiles_sent{"fine tiles", {}, {}};
  double update_ms = 0, worst_ms = 0;
  int updates = 0;
  const Run r = ski(layer, half, [&](double x, double z, double t) {
    const auto t0 = Clock::now();
    gr::update_snow_mirror(d, mirror, layer, x, z);
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    update_ms += ms;
    if (ms > worst_ms) worst_ms = ms;
    updates++;
    sent.x.push_back(t);
    sent.y.push_back(static_cast<double>(mirror.last.bytes) / 1024);
    tiles_sent.x.push_back(t);
    tiles_sent.y.push_back(mirror.last.tiles);
    mark_frame(x, z);
  });
  gr::update_snow_mirror(d, mirror, layer, r.end_x, r.end_z);
  mark_frame(r.end_x, r.end_z);
  ge::gpu::end_load(d);

  // THE RAW CELLS, every sample, a strip of rows at a time.
  int wrong = 0;
  long long sources[3] = {0, 0, 0};
  std::vector<uint8_t> source_of(static_cast<size_t>(layer.cols) * layer.rows, 0);
  const int strip = 256;
  for (int r0 = 0; r0 < layer.rows; r0 += strip) {
    const int h = std::min(strip, layer.rows - r0);
    const std::vector<gr::SnowRaw> gpu = gr::read_snow_raw(d, mirror, 0, r0, layer.cols, h);
    for (int j = 0; j < h; j++) {
      for (int c = 0; c < layer.cols; c++) {
        const gr::SnowRaw want = gr::snow_mirror_expect(mirror, layer, c, r0 + j);
        const gr::SnowRaw& got = gpu[static_cast<size_t>(j) * layer.cols + c];
        const bool same = want.depth == got.depth && want.mass == got.mass &&
                          want.bits == got.bits && want.source == got.source &&
                          (want.source != 2 || want.samples == got.samples);
        if (!same) wrong++;
        sources[want.source]++;
        source_of[static_cast<size_t>(r0 + j) * layer.cols + c] = static_cast<uint8_t>(got.source);
      }
    }
  }
  if (wrong > 0) {
    lab.fail(std::to_string(wrong) + " raw samples the GPU reads differ from the layer's",
             "Run tests/render_snow_mirror_test.cpp; read `update_snow_mirror` and `snow_raw` "
             "(render/wgsl/snow_mirror.wgsl). Skill: snow.");
  }

  // A live fine tile outside the window is read through its summary on the GPU, as folding it
  // would leave it, so the physics' fine column is not what the picture is meant to show there: a
  // point with any of its four samples in one is left out of the comparison and counted.
  const auto summarised = [&](double x, double z) {
    const double fx = (x - spec.region.x0) / spec.cell, fz = (z - spec.region.z0) / spec.cell;
    const int c0 = std::clamp(static_cast<int>(std::floor(fx)), 0, layer.cols - 1);
    const int r0 = std::clamp(static_cast<int>(std::floor(fz)), 0, layer.rows - 1);
    for (int dr = 0; dr < 2; dr++) {
      for (int dc = 0; dc < 2; dc++) {
        const int c = std::min(c0 + dc, layer.cols - 1), rr = std::min(r0 + dr, layer.rows - 1);
        const int tile = (rr / sn::kTileCells) * layer.tile_cols + c / sn::kTileCells;
        if (layer.slots[static_cast<size_t>(tile)] >= 0 &&
            source_of[static_cast<size_t>(rr) * layer.cols + c] != 1) {
          return true;
        }
      }
    }
    return false;
  };
  // THE COLUMN at every pixel's centre of a square `side` m from (x0, z0), on both sides: the
  // physics' on the left, the GPU's on the right, each compared.
  double worst = 0;
  int strays = 0, compared = 0, skipped = 0;
  const auto twin = [&](double x0, double z0, double side, int px, double scale) {
    const int n = static_cast<int>(side * px);
    std::vector<float> xz;
    xz.reserve(static_cast<size_t>(n) * n * 2);
    for (int j = 0; j < n; j++) {
      for (int i = 0; i < n; i++) {
        xz.push_back(static_cast<float>(x0 + (i + 0.5) / px));
        xz.push_back(static_cast<float>(z0 + side - (j + 0.5) / px));
      }
    }
    const std::vector<gr::SnowColumnRead> got = gr::read_snow_columns(d, mirror, xz);
    gl::Picture pic = gl::create_picture(2 * n + 8, n, {255, 255, 255, 255});
    for (int j = 0; j < n; j++) {
      for (int i = 0; i < n; i++) {
        const size_t k = static_cast<size_t>(j) * n + i;
        const sn::Column want = sn::sample(layer, xz[2 * k], xz[2 * k + 1]);
        gl::set_pixel(pic, i, j, worn(want.drop, scale));
        gl::set_pixel(pic, n + 8 + i, j, worn(got[k].drop, scale));
        if (summarised(xz[2 * k], xz[2 * k + 1])) {
          skipped++;
          continue;
        }
        compared++;
        const double gap =
            std::fmax(std::fabs(want.depth - got[k].depth), std::fabs(want.drop - got[k].drop));
        if (want.touched != (got[k].touched > 0) || gap >= 1e-4) strays++;
        worst = std::fmax(worst, gap);
      }
    }
    gl::draw_text(pic, 4, 4, "CPU", {200, 40, 40});
    gl::draw_text(pic, n + 12, 4, "GPU", {200, 40, 40});
    return pic;
  };
  const int w = static_cast<int>(size * ppm);
  const gl::Picture drop = twin(-half, 0, size, ppm, 0.02);
  const double near_side = 24;
  const gl::Picture near =
      twin(r.end_x - near_side / 2, std::max(0.0, r.end_z - 4), near_side, ppm * 8, 0.08);
  if (strays > 0) {
    lab.fail(std::to_string(strays) +
                 " points where the GPU's column strays from the physics' "
                 "(worst " +
                 fixed(worst * 1000, 3) + " mm)",
             "Read `snow_column` in render/wgsl/snow_mirror.wgsl against `snow::sample`. Skill: "
             "snow.");
  }
  // THE MARKS beside the mirror over a 4 m square on the skier's last metres, at a centimetre a
  // pixel: the GPU's column (the simulation's 10 cm) on the left, the fine marks' depth and their
  // berms, warm, on the right.
  const ge::gpu::Image fine = ge::gpu::read_now(d, marks.fine);
  const gr::MarkRect fr = gr::fine_rect(marks);
  constexpr double kMarkSide = 4;
  constexpr int kMarkPx = 100;
  const int mn = static_cast<int>(kMarkSide * kMarkPx);
  const double mz0 = r.end_z + 1, mx0 = line_x(mz0 + kMarkSide / 2, half) - kMarkSide / 2;
  std::vector<float> mxz;
  mxz.reserve(static_cast<size_t>(mn) * mn * 2);
  for (int j = 0; j < mn; j++) {
    for (int i = 0; i < mn; i++) {
      mxz.push_back(static_cast<float>(mx0 + (i + 0.5) / kMarkPx));
      mxz.push_back(static_cast<float>(mz0 + kMarkSide - (j + 0.5) / kMarkPx));
    }
  }
  const std::vector<gr::SnowColumnRead> mcol = gr::read_snow_columns(d, mirror, mxz);
  gl::Picture marks_pic = gl::create_picture(2 * mn + 8, mn, {255, 255, 255, 255});
  for (int j = 0; j < mn; j++) {
    for (int i = 0; i < mn; i++) {
      const size_t k = static_cast<size_t>(j) * mn + i;
      gl::set_pixel(marks_pic, i, j, worn(mcol[k].drop, 0.08));
      const int c = static_cast<int>(std::floor((mxz[2 * k] - fr.x0) / fr.texel));
      const int rr = static_cast<int>(std::floor((mxz[2 * k + 1] - fr.z0) / fr.texel));
      double under = 0;
      if (c >= 0 && rr >= 0 && c < static_cast<int>(fine.width) &&
          rr < static_cast<int>(fine.height)) {
        const size_t t = (static_cast<size_t>(rr) * fine.width + static_cast<size_t>(c)) * 4;
        const double dv = fine.rgba[t] / 255.0, bv = fine.rgba[t + 1] / 255.0;
        under = dv * dv * gr::kMarkDepth - bv * bv * gr::kMarkBerm;
      }
      gl::set_pixel(marks_pic, mn + 8 + i, j, worn(under, 0.08));
    }
  }
  gl::draw_text(marks_pic, 4, 4, "MIRROR 10 CM", {200, 40, 40});
  gl::draw_text(marks_pic, mn + 12, 4, "MARKS 2.3 CM", {200, 40, 40});

  // THE SOURCES.
  gl::Picture src = gl::create_picture(w, w, {255, 255, 255, 255});
  const double cell = spec.cell;
  for (int j = 0; j < w; j++) {
    for (int i = 0; i < w; i++) {
      const int c = std::min(layer.cols - 1, static_cast<int>((i + 0.5) / ppm / cell));
      const int rr = std::min(layer.rows - 1, static_cast<int>((size - (j + 0.5) / ppm) / cell));
      const uint8_t s = source_of[static_cast<size_t>(rr) * layer.cols + c];
      const gl::Rgba colour = s == 1   ? gl::Rgba{235, 120, 40, 255}
                              : s == 2 ? gl::Rgba{70, 120, 210, 255}
                                       : gl::Rgba{246, 248, 252, 255};
      gl::set_pixel(src, i, j, colour);
    }
  }
  const double tile_px = sn::kTileCells * cell * ppm;
  outline(src, mirror.tx0 * tile_px, w - (mirror.tz0 + window) * tile_px, window * tile_px,
          {200, 30, 30, 255});

  // THE NUMBERS. The traffic's are the CPU's and cut the digest; the mirror's are measured.
  lab.metric("presses", r.presses, "count");
  lab.metric("folds", layer.folds, "count");
  lab.metric("marks", static_cast<double>(stamps.size()), "count");
  lab.metric("mismatches", wrong, "count");
  lab.metric("updates", updates, "count", Det::measured);
  lab.metric("tiles-sent", mirror.total.tiles, "count", Det::measured);
  lab.metric("blocks-sent", mirror.total.blocks, "count", Det::measured);
  lab.metric("sent", static_cast<double>(mirror.total.bytes) / 1024, "KB", Det::measured);
  lab.metric("column-worst", worst * 1000, "mm", Det::measured);
  lab.metric("columns-compared", compared, "count", Det::measured);
  lab.metric("columns-summarised", skipped, "count", Det::measured);
  lab.metric("update-mean", updates ? update_ms / updates : 0, "ms", Det::measured);
  lab.metric("update-worst", worst_ms, "ms", Det::measured);
  // What re-sending the whole window on every update would have cost.
  const double naive = static_cast<double>(window) * window * sizeof(sn::Tile) * updates;
  gl::LabTable table{"the mirror", {"what", "number"}, {}};
  table.rows.push_back({"updates (every 4 steps)", std::to_string(updates)});
  table.rows.push_back(
      {"fine tiles / coarse blocks sent",
       std::to_string(mirror.total.tiles) + " / " + std::to_string(mirror.total.blocks)});
  table.rows.push_back({"bytes sent over the run",
                        fixed(static_cast<double>(mirror.total.bytes) / (1024 * 1024), 2) + " MB"});
  table.rows.push_back({"the whole window every update", fixed(naive / (1024 * 1024), 1) + " MB"});
  table.rows.push_back({"samples read fine / coarse / cover",
                        std::to_string(sources[1]) + " / " + std::to_string(sources[2]) + " / " +
                            std::to_string(sources[0])});
  table.rows.push_back({"raw samples that differ", std::to_string(wrong)});
  table.rows.push_back({"worst column gap (points compared / read summarised)",
                        fixed(worst * 1000, 4) + " mm (" + std::to_string(compared) + " / " +
                            std::to_string(skipped) + ")"});
  table.rows.push_back({"update, mean / worst", fixed(updates ? update_ms / updates : 0, 3) +
                                                    " / " + fixed(worst_ms, 3) + " ms"});
  lab.table(table, Det::measured);

  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"size", lab.arg("size")}, {"budget", lab.arg("budget")}, {"window", lab.arg("window")}};
  lab.shot({"drop", "THE SAME SNOW ON BOTH SIDES",
            fixed(size, 0) +
                " m of powder from above (downhill is down), shaded by how far the surface stands "
                "under the cover: on the left the physics' `snow::sample`, on the right the GPU's "
                "`snow_column` read through the mirror. The skier's S-turns run the middle, a "
                "snowmobile climbs the left and a walker the right.",
            "the halves alike everywhere but the skier's track just above the window: a sharp "
            "line on the left (live fine tiles) and a soft band on the right (their summary, as "
            "folding them will leave it); the folded track above and the far tracks the same "
            "soft band on both sides",
            inputs},
           drop, Det::measured);
  lab.shot({"near", "WHERE THE SKIER IS: THE FINE WINDOW",
            "24 m round the skier at the end, eight times closer, inside the fine window: the "
            "physics' column on the left, the GPU's on the right, read from the fine atlas.",
            "the two halves identical: the two sharp grooves of his skis", inputs},
           near, Det::measured);
  lab.shot({"marks", "THE FINE MARKS OVER THE MIRROR",
            "4 m of the skier's last metres at a centimetre a pixel: on the left the mirror's "
            "column at the simulation's 10 cm, on the right the fine marks drawn from the layer's "
            "imprint records (render/snow_marks.hpp) "
            "at 2.3 cm a texel, with the berms warm. The marks are the detail a material draws on "
            "top of the mirror's surface.",
            "on the right the two ski grooves crisp and narrow; on the left the same grooves wider "
            "and blurred over the 10 cm samples",
            inputs},
           marks_pic, Det::measured);
  lab.shot({"sources", "WHERE THE GPU READS EACH SAMPLE FROM",
            "Orange: the fine atlas, inside the window (red, " + std::to_string(window) +
                " tiles a side) round the skier at the end; blue: the coarse view (folded blocks, "
                "and the summaries of live tiles outside the window); white: the cover.",
            "orange only inside the red square round the skier's last metres; blue along the "
            "rest of his track and the far tracks; white elsewhere",
            inputs},
           src, Det::measured);
  gl::LineChart chart;
  chart.frame = {
      "What each update sent",
      "the mirror updated every fourth step; a tile is re-sent only when its stamp moved", "t (s)",
      "KB"};
  chart.traces = {sent};
  chart.spikes = false;
  lab.shot({"uploads", "UPLOADS ONLY WHERE THE SNOW CHANGED",
            "The bytes each update wrote: the tiles under the skier's skis re-sent as he presses "
            "them, a burst as the window moves on a tile, and the coarse blocks as tiles fold.",
            "a low steady floor (the one or two tiles under him) with bursts at each window move "
            "and each second's fold; never the whole layer",
            inputs},
           gl::to_picture(gl::line_chart(chart).plot), Det::measured);
  // Last, as the placeholders leave them, so a row with no adapter cuts the same digest.
  lab.metric("gpu-errors", d.errors.load(), "count");
  lab.metric("pipelines-after-load", d.pipelines_after_load, "count");
}

const gl::LabSpec kSpec{
    "gpu-snow",
    gl::LabKind::picture,
    "engine",
    "engine/render/snow_mirror.hpp, engine/render/snow_marks.hpp",
    "snow",
    "Does the GPU read the snow the physics wrote, to the bit, while sending only what changed, "
    "and do the fine marks drawn from its imprint records sit where the snow was pressed?",
    {
        {"size", "192", "the square's side, m"},
        {"budget", "64", "fine tiles the layer keeps at most"},
        {"window", "8", "the mirror's fine window, tiles a side"},
        {"px-per-m", "2", "the pictures' pixels a metre"},
    },
    "--size=64 --budget=24 --window=4 --px-per-m=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
