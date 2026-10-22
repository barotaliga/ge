// LAB `snow`: the snow's material (plugins/mountain/app/snow.hpp) up close. It generates a seed's
// map, makes Fall Line's frame on a headless device and shoots the close views of the snow in
// the sun (games/fall_line/app/views.hpp: `corduroy` over the groomed piste, `powder` across the
// sun on the loose snow beside it, `glitter` toward the sun) at each --hours hour, every view
// twice: PLAIN (every gain of the material at nought: the game's albedo lit by the sun and the
// sky, the ground as it was before the material) and SNOW (the material's tuned look). Beside
// each picture, over the nearest snow (the lowest third of the frame): its mean luma, its DETAIL
// (the mean step of luma to the next pixel right and down, in 8-bit levels: what the relief, the
// comb and the lanes add) and its GLINTS (pixels 8 levels or more over the mean of the eight round
// them, all of which are bright: the crystals' flares on the snow, not an edge).
//
// What must hold on every row is deterministic (the shots taken, no pipeline after load, no GPU
// error); pixels are measured. Without an adapter it notes that and draws placeholders, so the
// digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the snow's material up close, plain and with the material, at a sweep of hours
// ge:keywords lab snow snow-material corduroy powder glitter glints detail luma before-after
// ge:keywords fall-line picture close-views
// ge:skill mountain render labs
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/snow.hpp"
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

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

std::vector<std::string> list_of(const std::string& list) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i <= list.size()) {
    const size_t j = list.find(',', i);
    const size_t end = j == std::string::npos ? list.size() : j;
    if (end > i) out.push_back(list.substr(i, end - i));
    i = end + 1;
  }
  return out;
}

struct GroundStats {
  double luma = 0, detail = 0, glints = 0;
};

// Over the lowest third of an RGBA picture: the nearest snow.
GroundStats ground_stats(const std::vector<uint8_t>& rgba, int w, int h) {
  std::vector<double> y(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (size_t i = 0; i < y.size(); ++i) {
    y[i] = 0.2126 * rgba[i * 4] + 0.7152 * rgba[i * 4 + 1] + 0.0722 * rgba[i * 4 + 2];
  }
  auto at = [&](int x, int yy) {
    return y[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  };
  GroundStats s;
  double n = 0;
  for (int yy = 2 * h / 3; yy < h - 1; ++yy) {
    for (int x = 1; x < w - 1; ++x) {
      const double c = at(x, yy);
      s.luma += c;
      s.detail += 0.5 * (std::fabs(at(x + 1, yy) - c) + std::fabs(at(x, yy + 1) - c));
      double round = 0, darkest = 255;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          round += at(x + dx, yy + dy);
          darkest = std::min(darkest, at(x + dx, yy + dy));
        }
      }
      // On snow (the neighbours all bright), not the edge of a trunk or a shadow.
      if (darkest >= 120 && c >= round / 8 + 8) ++s.glints;
      ++n;
    }
  }
  if (n > 0) {
    s.luma /= n;
    s.detail /= n;
  }
  return s;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::vector<double> hours = lab.arg_nums("hours");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  const std::vector<std::string> views = list_of(lab.arg("views"));
  if (hours.empty() || views.empty() || width <= 0 || height <= 0 || width > 4096 ||
      height > 4096 || settle < 1) {
    lab.fail(
        "--hours and --views need at least one each, --width and --height 1..4096 and "
        "--settle at least 1",
        "next: pass --hours=10,12 --views=corduroy,powder,glitter --width=640 --height=360.");
    return;
  }
  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const ge::mountain::mapgen::Level level = ge::mountain::mapgen::generate_level(seed, opts);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  struct Scene {
    std::string id, view, hour;
    bool snow;
    ge::render::FrameView frame;
  };
  std::vector<Scene> scenes;
  for (double hr : hours) {
    const std::string hour_s = hr < 0 ? fixed(level.sun.hour, 2) : fixed(hr, 2);
    for (const std::string& name : views) {
      for (const bool snow : {false, true}) {
        std::string id = name;
        id += "-";
        id += hour_s;
        id += snow ? "-snow" : "-plain";
        Scene sc{std::move(id), name, hour_s, snow, {}};
        fall_line::app::day_view(level, hr, sc.frame);
        if (!fall_line::app::make_view(level, name, sc.frame)) {
          lab.fail("no view named '" + name + "'",
                   "next: pick from games/fall_line/app/views.hpp (corduroy, powder, glitter).");
          return;
        }
        scenes.push_back(std::move(sc));
      }
    }
  }
  lab.metric("shots", static_cast<double>(scenes.size()), "count");

  auto shot_of = [&](const Scene& sc) {
    std::string title = "SEED " + seed_s + ", " + sc.view + " AT " + sc.hour + " H, " +
                        (sc.snow ? "SNOW" : "PLAIN");
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return ge::lab::Shot{
        sc.id,
        title,
        std::string(fall_line::app::view_caption(sc.view)) +
            (sc.snow ? " In the snow's material: the relief, the comb and the lanes bent into "
                       "the light, the blue wrap round the shadow side, the glitter and the "
                       "glare."
                     : " PLAIN: every gain of the snow's material at nought, the albedo lit by "
                       "the sun and the sky alone."),
        sc.snow ? "the comb's ribs run down the piste and fade before they shimmer; the lanes a "
                  "swath wide, each a shade of its own; the loose snow swells and grains, no "
                  "pattern repeating; glints are points, not confetti, and only on loose snow in "
                  "the sun; the shadow side bluer, never black; no seams where the piste meets "
                  "the loose snow"
                : "the same view and light as the SNOW shot beside it",
        {{"seed", seed_s},
         {"view", sc.view},
         {"hour", sc.hour},
         {"material", sc.snow ? "snow" : "plain"},
         {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "snow", .validation = 1});
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
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  fall_line::app::FrameOptions options;
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "snow.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  const ge::mountain::app::SnowLook tuned;
  const ge::mountain::app::SnowLook none{0, 0, 0, 0, 0, 0};
  for (Scene& sc : scenes) {
    ge::mountain::app::set_snow_look(*device, frame->snow, sc.snow ? tuned : none);
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      sc.frame.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, sc.frame, {}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      lab.fail("the readback of '" + sc.id + "' came back the wrong size",
               "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
      continue;
    }
    const GroundStats s = ground_stats(img.rgba, width, height);
    lab.metric(sc.id + ".luma", s.luma, "levels", Det::measured);
    lab.metric(sc.id + ".detail", s.detail, "levels", Det::measured);
    lab.metric(sc.id + ".glints", s.glints, "count", Det::measured);
    lab.shot(shot_of(sc), ge::lab::Picture{width, height, img.rgba}, Det::measured);
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
    "snow",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "plugins/mountain/app/snow.wgsl (the snow's material) in Fall Line's frame",
    "mountain",
    "How does the snow's material change the snow up close, against the plain lit albedo?",
    {
        {"seed", "38", "the map's seed"},
        {"hours", "10,12,14", "the solar hours, comma-separated; negative: the map's own"},
        {"views", "corduroy,powder,glitter", "the close views (games/fall_line/app/views.hpp)"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"settle", "8", "frames drawn after each shot's cut before the capture"},
    },
    "--hours=12 --views=corduroy --width=160 --height=90 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
