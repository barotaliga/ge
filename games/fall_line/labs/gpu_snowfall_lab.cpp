// LAB `snowfall`: the falling snow (engine/render/snowfall.hpp) in Fall Line's frame, carried by
// the map's wind (plugins/mountain/air/air.hpp). A seed's map is drawn under three snowing skies
// in turn, its own weather replaced by each (a FLURRY, a steady SNOW and a STORM), from a named
// view, --frames frames at 60 a second up to --at seconds into the run, so the flakes in the
// picture have flown on the wind those frames.
//
// Beside each: the fall, the visual range and the wind at the shot and the flakes drawn
// (deterministic: the air is a pure function of the map and the clock), and the two passes' GPU
// ms (measured). The lab fails when a snowing sky draws no flakes. Without an adapter it notes
// that and draws placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails
// instead. Built only with GE_GPU.
//
// ge:summary Lab: the falling snow under a flurry, a steady snow and a storm, on the wind
// ge:keywords lab snowfall flakes flurries snow storm wind visibility fall-line picture
// ge:skill render labs
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/air/air.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/snowfall.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;

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
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

struct Sky {
  const char* id;
  mg::WeatherKind kind;
  double snowfall;
  double wind;  // m/s at 10 m
  const char* look;
};

constexpr Sky kSkies[] = {
    {"flurries", mg::WeatherKind::flurries, 0.3, 3,
     "a scatter of flakes drifting down, a few near the lens streaked, the far snow clear"},
    {"snow", mg::WeatherKind::snow, 0.6, 5,
     "a steady fall filling the near air, the flakes slanting a little with the wind"},
    {"storm", mg::WeatherKind::storm, 0.95, 12,
     "a blizzard: dense flakes driven near level by the wind, streaked, the near air white"},
};

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& view_name = lab.arg("view");
  const double at = lab.arg_num("at");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (at < 0 || frames < 1 || frames > 600 || width <= 0 || height <= 0 || width > 4096 ||
      height > 4096) {
    lab.fail("--at must be 0 or more, --frames 1..600 and --width and --height 1..4096",
             "next: pass --at=30 --frames=60 --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level base = mg::generate_level(seed, opts);
  ge::render::FrameView probe;
  if (!fall_line::app::make_view(base, view_name, probe)) {
    lab.fail("no view `" + view_name + "`",
             "Pass --view= one of games/fall_line/app/views.cpp's names. Skill: render.");
    return;
  }
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  // THE SKIES: the map under each, and the air at the shot.
  std::vector<mg::Level> levels;
  levels.reserve(std::size(kSkies));
  for (const Sky& sky : kSkies) {
    mg::Level l = base;
    l.weather.kind = sky.kind;
    l.weather.snowfall = sky.snowfall;
    l.weather.wind = sky.wind;
    levels.push_back(std::move(l));
    const mg::Level& level = levels.back();
    const ge::mountain::air::Wind w = ge::mountain::air::wind_at(level, at);
    const ge::mountain::air::Fall f = ge::mountain::air::fall_at(level, at);
    const std::string k = sky.id;
    lab.metric(k + ".fall", f.fall, "share");
    lab.metric(k + ".visibility", f.visibility, "m");
    lab.metric(k + ".wind", w.speed, "m/s");
    lab.metric(k + ".gust", w.gust, "share");
    lab.metric(k + ".flakes",
               ge::render::flakes_drawn(fall_line::app::FrameOptions{}.flakes, f.fall), "count");
    if (ge::render::flakes_drawn(fall_line::app::FrameOptions{}.flakes, f.fall) == 0) {
      lab.fail("the " + k + " sky draws no flakes at " + fixed(at, 1) + " s",
               "Check fall_at in plugins/mountain/air/air.cpp (the squalls' breath). Skill: "
               "mountain.");
    }
  }

  auto shot_of = [&](size_t i) {
    const Sky& sky = kSkies[i];
    const mg::Level& level = levels[i];
    const ge::mountain::air::Fall f = ge::mountain::air::fall_at(level, at);
    const ge::mountain::air::Wind w = ge::mountain::air::wind_at(level, at);
    std::string title = std::string(sky.id) + ", SEED " + seed_s + " " + view_name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const std::string caption = "Seed " + seed_s + "'s `" + view_name + "` view under " + sky.id +
                                " (fall " + fixed(f.fall, 2) + ", " + fixed(f.visibility, 0) +
                                " m visibility, wind " + fixed(w.speed, 1) + " m/s), " +
                                std::to_string(frames) + " frames up to " + fixed(at, 1) + " s.";
    return ge::lab::Shot{sky.id,
                         title,
                         caption,
                         sky.look,
                         {{"seed", seed_s},
                          {"sky", sky.id},
                          {"view", view_name},
                          {"t", fixed(at, 2)},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "snowfall", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (size_t i = 0; i < levels.size(); ++i) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(i), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  uint32_t errors = 0, late = 0;
  for (size_t i = 0; i < levels.size(); ++i) {
    // A frame is made once a device (its load ends), so each sky's map has a device of its own.
    if (i > 0) {
      errors += device->errors.load();
      late += device->pipelines_after_load;
      device = ge::gpu::create_device({.label = "snowfall", .validation = 1});
      if (!device) {
        lab.fail("the GPU device could not be made again",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
    }
    const mg::Level& level = levels[i];
    fall_line::app::FrameOptions options;
    auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height),
                                              wgpu::TextureFormat::RGBA8Unorm, options);
    if (!frame) {
      lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    const ge::gpu::Target target = ge::gpu::create_target(
        *device, "snowfall.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    ge::render::FrameView view;
    fall_line::app::make_view(level, view_name, view);
    for (int n = 0; n < frames; ++n) {
      view.frame = static_cast<uint64_t>(n);
      view.time_s = at - static_cast<double>(frames - 1 - n) / 60;
      fall_line::app::draw(*device, *frame, view, {}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    lab.shot(shot_of(i), ge::lab::Picture{width, height, img.rgba}, Det::measured);
    frame->recorder.wait(*device);
    for (int p = 0; p < frame->recorder.stats.count; ++p) {
      const auto& ps = frame->recorder.stats.passes[static_cast<size_t>(p)];
      if (std::string_view(ps.name).starts_with("snowfall") && !std::isnan(ps.gpu_ms)) {
        lab.metric(std::string(kSkies[i].id) + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms",
                   Det::measured);
      }
    }
  }
  errors += device->errors.load();
  late += device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (late != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_frame's manifest. Skill: gpu, § Pipelines.");
  }
}

const ge::lab::LabSpec kSpec{
    "snowfall",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/snowfall.hpp (the falling snow) on plugins/mountain/air/air.hpp's wind in "
    "Fall Line's frame",
    "render",
    "Does the falling snow read as a flurry, a steady snow and a storm, carried on the wind?",
    {
        {"seed", "38", "the map's seed"},
        {"view", "start", "the named view to shoot from"},
        {"at", "30", "seconds into the run the shot is taken at"},
        {"frames", "60", "frames drawn, at 60 a second, up to the shot"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=4 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
