// LAB `world`: the game's own frame, photographed. It generates a seed's map, makes Fall Line's
// frame (games/fall_line/app/frame.hpp: the pass table over engine/render) on a headless device
// (the software adapter in the GPU-less container and on CI) and renders each named view
// (views.hpp) offscreen, reads it back and hands it to `Lab::shot` with its caption. Beside each
// picture: the tiles the terrain's GPU cull drew, every pass's GPU ms (measured: a software
// adapter's times say nothing about a real GPU) and the frame's CPU half.
//
// What must hold on every row is deterministic (the views shot, the pass table, no pipeline
// after load, no GPU error); pixels and timings are measured. Without an adapter it notes that
// and draws placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead.
// Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's frame from named views of a seed, captured headless
// ge:keywords lab world picture screenshot views frame terrain sky post gpu-capture headless
// ge:keywords tiles-drawn gpu-ms cpu-ms pass-table fall-line
// ge:skill render labs
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
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || settle < 1) {
    lab.fail("--width and --height must be 1..4096 and --settle at least 1",
             "Pass a size such as --width=640 --height=360.");
    return;
  }
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
         {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "world", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const std::string& name : views) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(name), none, Det::measured);
    }
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.driver + " / " + r.backend +
           (r.software ? " (software)" : ""));

  auto frame =
      fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height), wgpu::TextureFormat::RGBA8Unorm);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "world.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

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
    for (int i = 0; i < settle; ++i) {
      view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, view, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    frame->recorder.wait(*device);
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
        {"width", "640", "the render width, px"},
        {"height", "360", "the render height, px"},
        {"settle", "2", "frames drawn before each capture"},
    },
    "--views=start,vista --width=160 --height=90 --settle=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
