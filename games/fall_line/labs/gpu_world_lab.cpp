// LAB `world`: the game's own frame, photographed. It generates a seed's map, makes Fall Line's
// frame (games/fall_line/app/frame.hpp: the pass table over engine/render) on a headless device
// (the software adapter in the GPU-less container and on CI) and renders each named view
// (views.hpp) offscreen, reads it back and hands it to `Lab::shot` with its caption. Beside each
// picture: the tiles the terrain's GPU cull drew, every pass's GPU ms (measured: a software
// adapter's times say nothing about a real GPU) and the frame's CPU half.
//
// The frame renders at --scale of the picture's size and TAA-U brings it up (--taa=0: no TAA, at
// full size), each view a camera cut drawn --settle frames before the capture. Then, per view:
// the SHIMMER (the mean change of a channel, in 8-bit levels, from one more frame of the still
// camera: what the jitter leaves after the resolve) and the OVERDRAW probe (the fragments the
// terrain shades per covered pixel with the depth prepass, which must be 1 but for coincident
// surfaces, and without it, the cost the prepass saves).
//
// What must hold on every row is deterministic (the views shot, the pass table, the render size
// and jitter cycle, no pipeline after load, no GPU error); pixels and timings are measured. Without
// an adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's frame from named views of a seed, captured headless
// ge:keywords lab world picture screenshot views frame terrain sky post gpu-capture headless
// ge:keywords tiles-drawn gpu-ms cpu-ms pass-table fall-line taa render-scale shimmer overdraw
// ge:skill render labs
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

// The terrain's overdraw over the frame's last depth: the mean count over covered pixels after
// the prepass (`equal`), then without it (`greater`, which rewrites the depth; the next view's
// frames clear it). Blocks.
void overdraw(ge::gpu::Device& device, fall_line::app::GameFrame& f, const wgpu::Texture& count,
              const wgpu::TextureView& count_view, double* with, double* without) {
  const auto& t = f.targets;
  const std::vector<float> depth = ge::render::read_depth(device, t);
  for (int pass = 0; pass < 2; ++pass) {
    wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
    ge::render::encode_terrain_overdraw(f.terrain, f.pipelines, encoder, t, f.frame_group,
                                        count_view, pass == 0);
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
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || settle < 1 ||
      options.scale < 0.25 || options.scale > 1) {
    lab.fail("--width and --height must be 1..4096, --settle at least 1 and --scale 0.25..1",
             "Pass a size such as --width=640 --height=360 --scale=1.");
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
  const ge::mountain::mapgen::Level level = ge::mountain::mapgen::generate_level(seed, opts);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::string hour_s = hour < 0 ? fixed(level.sun.hour, 2) + " (the map's)" : fixed(hour, 2);
  auto shot_of = [&](const std::string& name) {
    std::string title = "SEED " + seed_s + ", " + name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return ge::lab::Shot{
        name,
        title,
        std::string(fall_line::app::view_caption(name)) +
            " The ground is snow (white), the packed piste (a cooler grey-blue) and rock where it "
            "is too steep for snow, lit by the sun and the sky, hazed with distance; the sky is "
            "the atmosphere's at the map's hour.",
        "ground meets the sky at a clean horizon; no cracks, holes or seams between the "
        "terrain's rings (the ring edges sit at 64, 128, 256 m ... from the eye); far ridges "
        "bluer and paler than near ones; the sun's side brighter than the shadow side; no NaN "
        "(black or magenta) pixels",
        {{"seed", seed_s},
         {"view", name},
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
    for (const std::string& name : views) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(name), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("overdraw-faults", 0, "count");
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
  int overdraw_faults = 0;

  // Tiles depend on the adapter's float rounding at the frustum's edge, so the per-view costs
  // are a note, not a table (a table's cells are hashed).
  std::string costs = "per view (tiles drawn / draws / dispatches):";
  for (const std::string& name : views) {
    ge::render::FrameView view;
    fall_line::app::day_view(level, hour, view);
    if (!fall_line::app::make_view(level, name, view)) {
      lab.fail("no view named '" + name + "'",
               "Pick from games/fall_line/app/views.hpp (or --views=all).");
      continue;
    }
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, view, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    frame->recorder.wait(*device);
    // One more frame of the still camera: what changes is the jitter's shimmer.
    {
      view.frame = static_cast<uint64_t>(settle);
      fall_line::app::draw(*device, *frame, view, target.view);
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
    lab.metric(name + ".cpu-ms", stats.cpu_ms, "ms", Det::measured);
    for (int p = 0; p < stats.count; ++p) {
      const auto& ps = stats.passes[static_cast<size_t>(p)];
      if (!std::isnan(ps.gpu_ms)) {
        lab.metric(name + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms", Det::measured);
      }
    }
    costs += " " + name + " " + std::to_string(tiles) + "/" + std::to_string(stats.draws()) + "/" +
             std::to_string(stats.dispatches()) + ";";
    ge::lab::Picture picture{width, height, img.rgba};
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      lab.fail("the readback of view '" + name + "' came back the wrong size",
               "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
      continue;
    }
    lab.shot(shot_of(name), picture, Det::measured);
  }
  lab.note(costs);
  lab.metric("overdraw-faults", overdraw_faults, "count");
  if (overdraw_faults != 0) {
    lab.fail(
        "the colour pass missed covered pixels, or shaded them more than once, after the "
        "depth prepass",
        "The prepass and the colour pass must compute the same depth bits: check the "
        "terrain's vertex stage (@invariant position, one module). Skill: render.");
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
    },
    "--views=start,vista --width=160 --height=90 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
