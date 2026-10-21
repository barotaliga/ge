// LAB `taa`: what TAA-U leaves behind on a moving camera (engine/render/taa.hpp), over Fall Line's
// frame. It flies the camera of a named view forward at --speed, turning at --turn, for --frames
// frames at 60 a second, and photographs the last; then it cuts and draws that same camera still
// for one jitter cycle (the converged reference) twice, and once more with no history (a single
// jittered frame: the aliasing TAA-U removes).
//
// GHOSTING is the moving frame against the converged still: the mean change of a channel, in
// 8-bit levels, and the share of pixels off by more than 8 levels (history that clipping failed
// to reject, or detail still converging). ALIASING is the single frame against the still. Both
// are measured. STILLS REPEAT: two converged stills drawn after a cut must be the same bits (the
// history reset leaves nothing behind); a difference fails the lab.
//
// Without an adapter it notes that and draws placeholders, so the digest is the same on every
// row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: TAA-U on a moving camera against the converged still: ghosting, aliasing,
// stills repeat
// ge:keywords lab taa taa-u ghosting aliasing moving-camera history-reset stills-repeat jitter
// ge:keywords fall-line render-scale
// ge:skill render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/taa.hpp"

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
  return v && std::strcmp(v, "1") == 0;
#endif
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// The mean change of a channel (8-bit levels) and the share of pixels with a channel off by more
// than 8 levels.
void difference(const ge::gpu::Image& a, const ge::gpu::Image& b, double* mean, double* share) {
  const size_t n = std::min(a.rgba.size(), b.rgba.size()) / 4;
  double sum = 0;
  size_t off = 0;
  for (size_t i = 0; i < n; ++i) {
    int worst = 0;
    for (int c = 0; c < 3; ++c) {
      const int d = std::abs(int{a.rgba[i * 4 + c]} - int{b.rgba[i * 4 + c]});
      sum += d;
      worst = std::max(worst, d);
    }
    if (worst > 8) ++off;
  }
  *mean = n ? sum / (3.0 * static_cast<double>(n)) : 0;
  *share = n ? 100.0 * static_cast<double>(off) / static_cast<double>(n) : 0;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& name = lab.arg("view");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const double speed = lab.arg_num("speed"), turn = lab.arg_num("turn");
  fall_line::app::FrameOptions options;
  options.scale = lab.arg_num("scale");
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || frames < 1 ||
      options.scale < 0.25 || options.scale > 1) {
    lab.fail("--width and --height must be 1..4096, --frames at least 1 and --scale 0.25..1",
             "Pass --width=320 --height=180 --frames=24 --scale=1.");
    return;
  }
  const int phases = ge::render::taa_phases(options.scale);
  lab.metric("frames", frames, "count");
  lab.metric("jitter-phases", phases, "count");

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const ge::mountain::mapgen::Level level = ge::mountain::mapgen::generate_level(seed, opts);
  ge::render::FrameView start;
  fall_line::app::day_view(level, -1, start);
  if (!fall_line::app::make_view(level, name, start)) {
    lab.fail("no view named '" + name + "'", "Pick from games/fall_line/app/views.hpp.");
    return;
  }
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::string motion = fixed(speed, 0) + " M/S, TURNING " + fixed(turn, 0) + " DEG/S";
  const std::vector<std::pair<std::string, std::string>> facts = {
      {"seed", seed_s},
      {"view", name},
      {"frames", std::to_string(frames)},
      {"scale", fixed(options.scale, 2)},
      {"motion", fixed(speed, 1) + " m/s, " + fixed(turn, 1) + " deg/s"}};
  const ge::lab::Shot shots[3] = {
      {"moving", "SEED " + seed_s + ", MOVING AT " + motion,
       "The last of " + std::to_string(frames) +
           " frames of the camera flying forward and turning, resolved by TAA-U from its "
           "history.",
       "no smear or trail behind ridges and edges as they move; no ghost of what was there a "
       "frame ago; as sharp as the still beside it; no shimmer on the far ridges",
       facts},
      {"still", "SEED " + seed_s + ", THE SAME CAMERA, STILL",
       "The same camera after a cut, drawn still for one jitter cycle: the converged reference.",
       "clean edges with no stair-steps; the far ridges' detail intact", facts},
      {"ghosting", "SEED " + seed_s + ", MOVING AGAINST STILL, X8",
       "The difference between the moving frame and the still, eight times brighter: where the "
       "history lags or ghosts.",
       "dark almost everywhere; at most thin lines along moving edges; no bright band or copy of "
       "a ridge offset from where it is",
       facts}};

  auto device = ge::gpu::create_device({.label = "taa", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const ge::lab::Shot& shot : shots) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot, none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("still-repeat-faults", 0, "count");
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }

  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "taa.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  // The flight: forward along the heading, turning, 60 frames a second.
  ge::render::FrameView view = start;
  fall_line::app::cut(*frame);
  for (int i = 0; i < frames; ++i) {
    if (i > 0) {
      view.heading += turn * 3.14159265358979323846 / 180.0 / 60.0;
      view.eye[0] += std::sin(view.heading) * speed / 60.0;
      view.eye[2] += std::cos(view.heading) * speed / 60.0;
    }
    view.frame = static_cast<uint64_t>(i);
    view.time_s = i / 60.0;
    fall_line::app::draw(*device, *frame, view, target.view);
  }
  const ge::gpu::Image moving = ge::gpu::read_now(*device, target);

  // The same camera after a cut: still for one cycle, twice; then a single frame.
  auto still = [&](int count) {
    fall_line::app::cut(*frame);
    for (int i = 0; i < count; ++i) {
      view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, view, target.view);
    }
    return ge::gpu::read_now(*device, target);
  };
  const ge::gpu::Image settled = still(phases);
  const ge::gpu::Image again = still(phases);
  const ge::gpu::Image single = still(1);

  double ghost = 0, ghost_share = 0, alias = 0, alias_share = 0, repeat = 0, repeat_share = 0;
  difference(moving, settled, &ghost, &ghost_share);
  difference(single, settled, &alias, &alias_share);
  difference(again, settled, &repeat, &repeat_share);
  lab.metric("ghosting", ghost, "levels", Det::measured);
  lab.metric("ghosting-pixels", ghost_share, "%", Det::measured);
  lab.metric("aliasing", alias, "levels", Det::measured);
  lab.metric("aliasing-pixels", alias_share, "%", Det::measured);

  const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  if (moving.rgba.size() != bytes || settled.rgba.size() != bytes) {
    lab.fail("a readback came back the wrong size",
             "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
    return;
  }
  ge::lab::Picture diff = ge::lab::create_picture(width, height, {0, 0, 0});
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(width) + x) * 4;
      int d = 0;
      for (int c = 0; c < 3; ++c) {
        d = std::max(d, std::abs(int{moving.rgba[i + c]} - int{settled.rgba[i + c]}));
      }
      const auto v = static_cast<uint8_t>(std::min(d * 8, 255));
      ge::lab::set_pixel(diff, x, y, {v, v, v});
    }
  }
  lab.shot(shots[0], ge::lab::Picture{width, height, moving.rgba}, Det::measured);
  lab.shot(shots[1], ge::lab::Picture{width, height, settled.rgba}, Det::measured);
  lab.shot(shots[2], diff, Det::measured);

  const bool repeats = again.rgba == settled.rgba;
  lab.metric("still-repeat-faults", repeats ? 0 : 1, "count");
  if (!repeats) {
    lab.fail("two stills drawn after a cut differ (by " + fixed(repeat, 3) + " levels)",
             "A cut must leave nothing of the last frame: check fall_line::app::cut, "
             "reset_taa and FrameHistory. Skill: render.");
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "taa",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/taa (TAA-U) over Fall Line's frame",
    "render",
    "What does TAA-U leave behind on a moving camera, against the converged still?",
    {
        {"seed", "38", "the map's seed"},
        {"view", "vista", "the view the flight starts from (games/fall_line/app/views.hpp)"},
        {"width", "320", "the picture's width, px"},
        {"height", "180", "the picture's height, px"},
        {"scale", "1", "the render size over the picture's (0.25..1)"},
        {"frames", "24", "frames flown before the capture, at 60 a second"},
        {"speed", "40", "the camera's speed, m/s"},
        {"turn", "20", "the camera's turn rate, degrees a second"},
    },
    "--width=160 --height=90 --frames=6",
    run,
};
GE_LAB(kSpec);

}  // namespace
