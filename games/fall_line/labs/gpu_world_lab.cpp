// LAB `world`: the game's own frame, photographed. It generates a seed's map, makes Fall Line's
// frame (games/fall_line/app/frame.hpp: the pass table over engine/render) on a headless device
// (the software adapter in the GPU-less container and on CI) and renders each named view
// (views.hpp) offscreen, reads it back and hands it to `Lab::shot` with its caption. Then the
// bot skis the map (with --rivals beside him) and each --moments second of the run is shot from
// the chase camera, the racers drawn as figures. Beside each picture: the tiles the terrain's GPU
// cull drew, what the instances' cull did (drawn early, drawn late, hidden), every pass's GPU ms
// (measured: a software adapter's times say nothing about a real GPU) and the frame's CPU half.
//
// The frame renders at --scale of the picture's size and TAA-U brings it up (--taa=0: no TAA, at
// full size), each view a camera cut drawn --settle frames before the capture. Then, per view:
// the SHIMMER (the mean change of a channel, in 8-bit levels, from one more frame of the still
// camera: what the jitter leaves after the resolve) and the OVERDRAW probe (the fragments the
// terrain shades per covered pixel with the depth prepass, which must be 1 but for coincident
// surfaces, and without it, the cost the prepass saves), both over the ground and the instances;
// and the CULL check: the same still drawn again with the depth pyramid's culling off must be the
// same picture, or the cull dropped something that shows. And the sun's CASCADES over the whole
// run, without a device: how many frames of the chase camera redraw the near and the far one.
//
// What must hold on every row is deterministic (the views shot, the pass table, the render size
// and jitter cycle, no pipeline after load, no GPU error); pixels and timings are measured. Without
// an adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's frame from named views of a seed, captured headless
// ge:keywords lab world picture screenshot views frame terrain sky post gpu-capture headless
// ge:keywords tiles-drawn gpu-ms cpu-ms pass-table fall-line taa render-scale shimmer overdraw
// ge:keywords instances forest figures run-moments chase-camera occlusion-culling cull-check
// ge:keywords sun-shadow cascade-redraws shadow-cache
// ge:skill render labs
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/shadows.hpp"
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

// The views a run shoots: the --views list, or every named view.
std::vector<std::string> views_of(const ge::lab::Lab& lab) {
  std::vector<std::string> out;
  const std::string& list = lab.arg("views");
  if (list.empty() || list == "all") {
    for (std::string_view n : fall_line::app::view_names()) out.emplace_back(n);
    return out;
  }
  size_t i = 0;
  while (i <= list.size()) {
    const size_t j = list.find(',', i);
    const size_t end = j == std::string::npos ? list.size() : j;
    if (end > i) out.push_back(list.substr(i, end - i));
    i = end + 1;
  }
  return out;
}

// The --moments list: seconds of the run, each a shot from the chase camera.
std::vector<double> moments_of(const ge::lab::Lab& lab) {
  std::vector<double> out;
  const std::string& list = lab.arg("moments");
  size_t i = 0;
  while (i < list.size()) {
    const size_t j = list.find(',', i);
    const size_t end = j == std::string::npos ? list.size() : j;
    if (end > i) out.push_back(std::strtod(list.substr(i, end - i).c_str(), nullptr));
    i = end + 1;
  }
  return out;
}

// The terrain's indirect instance count, read back now (tiles drawn the last frame).
uint32_t tiles_drawn(ge::gpu::Device& device, const wgpu::Buffer& indirect) {
  wgpu::BufferDescriptor desc{};
  desc.label = "world.tiles.readback";
  desc.size = 20;
  desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(indirect, 0, rb, 0, 20);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  uint32_t count = 0;
  ge::gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, 20, wgpu::CallbackMode::WaitAnyOnly,
                                    [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, 20)) {
    std::memcpy(&count, static_cast<const uint8_t*>(p) + 4, 4);
  }
  rb.Unmap();
  return count;
}

// The overdraw of the ground and the instances over the frame's last depth: the mean count over
// covered pixels after the prepass (`equal`), then without it (`greater`, which rewrites the
// depth; the next view's frames clear it). Blocks.
void overdraw(ge::gpu::Device& device, fall_line::app::GameFrame& f, const wgpu::Texture& count,
              const wgpu::TextureView& count_view, double* with, double* without) {
  const auto& t = f.targets;
  const std::vector<float> depth = ge::render::read_depth(device, t);
  for (int pass = 0; pass < 2; ++pass) {
    wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
    ge::render::encode_terrain_overdraw(f.terrain, f.pipelines, encoder, t, f.frame_group,
                                        count_view, pass == 0);
    ge::render::encode_instances_overdraw(f.families, f.instancing, f.pipelines, encoder, t,
                                          f.frame_group, f.instance_light, count_view, pass == 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.queue.Submit(1, &commands);
    const std::vector<float> n =
        ge::render::read_red(device, count, 0, t.width, t.height, wgpu::TextureFormat::R16Float);
    double sum = 0, covered = 0;
    for (size_t i = 0; i < n.size(); ++i) {
      if (depth[i] > 0) {
        sum += n[i];
        ++covered;
      }
    }
    (pass == 0 ? *with : *without) = covered > 0 ? sum / covered : 0;
  }
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double hour = lab.arg_num("hour");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  fall_line::app::FrameOptions options;
  options.taa = lab.arg_num("taa") != 0;
  options.scale = lab.arg_num("scale");
  options.overdraw_probe = true;
  options.snow = lab.arg_num("snow") != 0;
  options.trails = lab.arg_num("trails") != 0;
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || settle < 1 ||
      options.scale < 0.25 || options.scale > 1) {
    lab.fail("--width and --height must be 1..4096, --settle at least 1 and --scale 0.25..1",
             "next: pass a size such as --width=640 --height=360 --scale=1.");
    return;
  }
  const double scale = options.taa ? options.scale : 1.0;
  const uint32_t render_w = ge::render::taa_render_size(static_cast<uint32_t>(width), scale);
  const uint32_t render_h = ge::render::taa_render_size(static_cast<uint32_t>(height), scale);
  lab.metric("render-width", render_w, "px");
  lab.metric("render-height", render_h, "px");
  lab.metric("jitter-phases", options.taa ? ge::render::taa_phases(scale) : 0, "count");
  const std::vector<std::string> views = views_of(lab);
  const std::string faults = fall_line::app::pass_table_faults();
  lab.metric("pass-table-faults", faults.empty() ? 0 : 1, "count");
  if (!faults.empty()) {
    lab.fail("Fall Line's pass table is out of order: " + faults,
             "Fix kPassTable in games/fall_line/app/frame.cpp. Skill: render.");
  }
  lab.metric("views", static_cast<double>(views.size()), "count");

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::string hour_s = hour < 0 ? fixed(level.sun.hour, 2) + " (the map's)" : fixed(hour, 2);

  // What is shot: the named views over the empty map, then the run's moments.
  struct Scene {
    std::string name;
    std::string caption;
    ge::render::FrameView view;
    std::vector<ge::skier::SkierState> skiers;
    // The racers every fourth step since the last moment: the tracks laid before it is shot.
    std::vector<std::vector<ge::skier::SkierState>> laid;
  };
  std::vector<Scene> scenes;
  for (const std::string& name : views) {
    Scene sc{name, std::string(fall_line::app::view_caption(name)), {}, {}, {}};
    fall_line::app::day_view(level, hour, sc.view);
    if (!fall_line::app::make_view(level, name, sc.view)) {
      lab.fail("no view named '" + name + "'",
               "next: pick from games/fall_line/app/views.hpp (or --views=all).");
      continue;
    }
    scenes.push_back(std::move(sc));
  }
  const std::vector<double> moments = moments_of(lab);
  const int rivals = static_cast<int>(lab.arg_num("rivals"));
  if (!moments.empty()) {
    fall_line::sim::Driver run(shared, std::max(rivals, 0));
    int steps = 0;
    for (double t : moments) {
      Scene sc;
      while (run.state().t < t && !run.over()) {
        run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
        if (++steps % 4 == 0) {
          sc.laid.emplace_back();
          for (const auto& racer : run.state().racers) sc.laid.back().push_back(racer.skier);
        }
      }
      sc.name = "run-" + fixed(t, 0);
      sc.caption = "The bot's run " + fixed(run.state().t, 1) +
                   " s after the start, from the chase camera behind and over him; the racers "
                   "are placeholder figures (boxes, cylinders and a ball, each in his own "
                   "jacket) and the trees placeholder cones and crowns.";
      for (const auto& racer : run.state().racers) sc.skiers.push_back(racer.skier);
      fall_line::app::day_view(level, hour, sc.view);
      fall_line::app::chase_view(level, sc.skiers[0], sc.view);
      scenes.push_back(std::move(sc));
    }
  }
  lab.metric("moments", static_cast<double>(moments.size()), "count");

  // The sun's cascades over the whole run: the chase camera at 60 frames a second, each frame
  // deciding (render/shadows.hpp `update_cascade`, the decision the frame makes) whether a cascade
  // is drawn again. Deterministic and with no device: the counts are what the frame would draw.
  {
    fall_line::sim::Driver run(shared, 0);
    const ge::render::ShadowSettings set;
    std::array<ge::render::ShadowCascade, ge::render::kCascades> cascades{};
    std::array<int, ge::render::kCascades> redraws{};
    int frames = 0;
    ge::render::FrameView v;
    fall_line::app::day_view(level, hour, v);
    double sun[3];
    ge::render::sun_direction(v, sun);
    while (!run.over()) {
      fall_line::app::chase_view(level, run.state().racers[0].skier, v);
      for (int c = 0; c < ge::render::kCascades; ++c) {
        float m[16];
        if (ge::render::update_cascade(set, c, sun, v.eye, cascades[static_cast<size_t>(c)], m)) {
          ++redraws[static_cast<size_t>(c)];
        }
      }
      ++frames;
      for (int k = 0; k < 2 && !run.over(); ++k) {
        run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
      }
    }
    lab.metric("cascade-frames", frames, "count");
    lab.metric("cascade-redraws.near", redraws[0], "count");
    lab.metric("cascade-redraws.far", redraws[1], "count");
    lab.metric("cascade-redraws-per-frame",
               frames > 0 ? static_cast<double>(redraws[0] + redraws[1]) / frames : 0, "count");
  }

  auto shot_of = [&](const Scene& sc) {
    std::string title = "SEED " + seed_s + ", " + sc.name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return ge::lab::Shot{
        sc.name,
        title,
        sc.caption +
            " The ground is snow (white), the packed piste (a cooler grey-blue) and rock where it "
            "is too steep for snow, lit by the sun and the sky, hazed with distance; the sky is "
            "the atmosphere's at the map's hour.",
        "ground meets the sky at a clean horizon; no cracks, holes or seams between the "
        "terrain's rings (the ring edges sit at 64, 128, 256 m ... from the eye); trees stand on "
        "the ground, not over it, and none pops or is missing at the screen's edge; a figure "
        "stands on its skis on the snow; far ridges bluer and paler than near ones; the sun's "
        "side brighter than the shadow side; no NaN (black or magenta) pixels",
        {{"seed", seed_s},
         {"view", sc.name},
         {"hour", hour_s},
         {"size", std::to_string(width) + "x" + std::to_string(height)},
         {"render", std::to_string(render_w) + "x" + std::to_string(render_h) +
                        (options.taa ? ", TAA-U" : ", no TAA")}}};
  };

  auto device = ge::gpu::create_device({.label = "world", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Scene& sc : scenes) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(sc), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("overdraw-faults", 0, "count");
    lab.metric("cull-faults", 0, "count");
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.driver + " / " + r.backend +
           (r.software ? " (software)" : ""));

  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "world.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  const wgpu::Texture count = ge::render::make_texture(
      *device, "world.overdraw", render_w, render_h, 1, wgpu::TextureFormat::R16Float,
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);
  const wgpu::TextureView count_view = count.CreateView();
  int overdraw_faults = 0, cull_faults = 0;

  // A still of the scene: a cut, then `settle` frames.
  auto still = [&](Scene& sc) {
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      sc.view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, sc.view, {sc.skiers}, target.view);
    }
    return ge::gpu::read_now(*device, target);
  };

  // Tiles and instances depend on the adapter's float rounding at the frustum's edge, so the
  // per-view costs are a note, not a table (a table's cells are hashed).
  std::string costs =
      "per view (tiles drawn / trees drawn early+late, hidden / draws / dispatches):";
  for (Scene& sc : scenes) {
    const std::string& name = sc.name;
    for (const auto& racers : sc.laid) fall_line::app::track(*frame, {racers});
    const ge::gpu::Image img = still(sc);
    frame->recorder.wait(*device);
    const ge::render::InstanceStats trees = ge::render::read_instance_stats(*device, frame->forest);
    const ge::render::InstanceStats figures =
        ge::render::read_instance_stats(*device, frame->figures);
    // One more frame of the still camera: what changes is the jitter's shimmer.
    {
      sc.view.frame = static_cast<uint64_t>(settle);
      fall_line::app::draw(*device, *frame, sc.view, {sc.skiers}, target.view);
      const ge::gpu::Image next = ge::gpu::read_now(*device, target);
      double diff = 0;
      for (size_t i = 0; i < std::min(img.rgba.size(), next.rgba.size()); ++i) {
        if (i % 4 != 3) diff += std::abs(int{img.rgba[i]} - int{next.rgba[i]});
      }
      lab.metric(name + ".shimmer", diff / (static_cast<double>(width) * height * 3), "levels",
                 Det::measured);
    }
    double with = 0, without = 0;
    overdraw(*device, *frame, count, count_view, &with, &without);
    lab.metric(name + ".shaded-per-pixel", with, "count", Det::measured);
    lab.metric(name + ".overdraw-without-prepass", without, "count", Det::measured);
    // Over 1 only where two surfaces lie at the very same depth (two clipmap levels meeting at a
    // ring's edge), which both pass `equal`: a few in ten thousand. Under 1 is a pixel the colour
    // pass missed.
    if (with != 0 && (with < 1.0 || with > 1.001)) ++overdraw_faults;
    const auto& stats = frame->recorder.stats;
    const uint32_t tiles = tiles_drawn(*device, frame->terrain.indirect);
    lab.metric(name + ".tiles", tiles, "count", Det::measured);
    lab.metric(name + ".trees-early", trees.early, "count", Det::measured);
    lab.metric(name + ".trees-late", trees.late, "count", Det::measured);
    lab.metric(name + ".trees-hidden", trees.hidden, "count", Det::measured);
    lab.metric(name + ".trees-outside", trees.frustum + trees.small, "count", Det::measured);
    lab.metric(name + ".figure-pieces", figures.early + figures.late, "count", Det::measured);
    lab.metric(name + ".cpu-ms", stats.cpu_ms, "ms", Det::measured);
    for (int p = 0; p < stats.count; ++p) {
      const auto& ps = stats.passes[static_cast<size_t>(p)];
      if (!std::isnan(ps.gpu_ms)) {
        lab.metric(name + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms", Det::measured);
      }
    }
    costs += " " + name + " " + std::to_string(tiles) + " / " + std::to_string(trees.early) + "+" +
             std::to_string(trees.late) + ", " + std::to_string(trees.hidden) + " / " +
             std::to_string(stats.draws()) + " / " + std::to_string(stats.dispatches()) + ";";
    // THE CULL CHECK: the same still with the pyramid's culling off draws every instance in the
    // frustum; on the software adapter the two pictures are the same bits, so a pixel that differs
    // is one the cull dropped although it shows.
    {
      frame->options.occlusion = false;
      const ge::gpu::Image all = still(sc);
      frame->options.occlusion = true;
      double differ = 0;
      for (size_t i = 0; i + 3 < std::min(img.rgba.size(), all.rgba.size()); i += 4) {
        if (std::memcmp(&img.rgba[i], &all.rgba[i], 3) != 0) ++differ;
      }
      lab.metric(name + ".cull-diff-pixels", differ, "count", Det::measured);
      if (differ > 0) ++cull_faults;
    }
    ge::lab::Picture picture{width, height, img.rgba};
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      lab.fail("the readback of view '" + name + "' came back the wrong size",
               "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
      continue;
    }
    lab.shot(shot_of(sc), picture, Det::measured);
  }
  lab.note(costs);
  lab.metric("overdraw-faults", overdraw_faults, "count");
  if (overdraw_faults != 0) {
    lab.fail(
        "the colour pass missed covered pixels, or shaded them more than once, after the "
        "depth prepass",
        "The prepass and the colour pass must compute the same depth bits: check the "
        "terrain's and the instances' vertex stages (@invariant position, one module each). "
        "Skill: render.");
  }
  lab.metric("cull-faults", cull_faults, "count");
  if (cull_faults != 0) {
    lab.fail("the depth pyramid's cull dropped instances that show (the .cull-diff-pixels rows)",
             "Check engine/render/instances_cull.wgsl's `hidden` (the box's nearest corner, the "
             "mip it reads, the margin) and the late phase. Skill: render.");
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (device->pipelines_after_load != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_frame's manifest. Skill: gpu, § Pipelines.");
  }
}

const ge::lab::LabSpec kSpec{
    "world",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app (the frame, its pass table, its views) over engine/render",
    "render",
    "What does Fall Line's frame draw from each named view of a seed's map?",
    {
        {"seed", "38", "the map's seed"},
        {"views", "all", "the views to shoot, comma-separated (games/fall_line/app/views.hpp)"},
        {"hour", "-1", "the solar hour to light the map at; negative: the map's own"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"settle", "8", "frames drawn after each view's cut before the capture"},
        {"taa", "1", "TAA-U on (1) or off (0)"},
        {"scale", "1", "the render size over the picture's, with TAA (0.25..1)"},
        {"snow", "1", "the snow's material (1) or the albedo lit (0)"},
        {"trails", "1", "the tracks (1) or none (0); drawn only with the snow's material"},
        {"moments", "3,15,30",
         "seconds of the bot's run to shoot from the chase camera, comma-"
         "separated (empty: none)"},
        {"rivals", "3", "the bots racing beside him"},
    },
    "--views=start,vista --moments=4 --width=160 --height=90 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
