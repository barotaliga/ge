// LAB `valley-fog`: the sea of fog in the valleys (engine/render/valley_fog.hpp) in Fall Line's
// frame. Four scenes over a seed's map, each drawn without the fog and with it:
// - SEA: from over the summit (the `vista` view) under the late morning sun, the valleys filled
//   to a top a third of the way up the mountain, the slopes and the ridges standing out of it;
// - AERIAL: from high over a corner of the map (the `aerial` view), the whole mountain an island
//   in it and the sea out to the horizon;
// - DAWN: the sea view at a low sun, the top lit gold and grazing;
// - INSIDE: from the finish (the `lookback` view), under the top: a grey gloom, the summit lost.
// --frames frames are drawn up to each picture at 60 a second, so the frame's history has settled.
//
// Beside each: the eye's height over the top (deterministic), the share of the picture the fog
// changed and its mean change in luma (measured). The lab fails when a scene's fog leaves the
// picture as it was, or on a GPU error. Without an adapter it notes that and draws placeholders,
// so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the sea of fog in the valleys from the summit, from the air, at dawn
// ge:summary and from under it
// ge:keywords lab valley-fog sea-of-fog inversion fog-top islands gloom fall-line picture
// ge:skill render labs
#include <algorithm>
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
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/valley_fog.hpp"
#include "render/view.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;
namespace r = ge::render;

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

struct Scene {
  std::string id;
  std::string look;
  r::FrameView view;
  r::ValleyFog fog;
  std::string what;  // the caption's account of the fog
};

// The sea the scenes share: its top `share` of the way from the base to the summit.
r::ValleyFog sea(const mg::Level& level, double share, double density) {
  const auto& m = level.mountain;
  r::ValleyFog f;
  f.top_m = m.base.y + share * (m.summit.y - m.base.y);
  f.density = density;
  f.edge_m = 30;
  f.wave_m = 18;
  f.wave_size_m = 700;
  f.drift[0] = 1.5;
  f.drift[1] = 0.5;
  return f;
}

std::string sea_what(const mg::Level& level, const r::ValleyFog& f) {
  return "a sea of fog in the valleys, its top " + fixed(f.top_m - level.mountain.base.y, 0) +
         " m over the base waving " + fixed(f.wave_m, 0) + " m on a " + fixed(f.wave_size_m, 0) +
         " m noise, " + fixed(f.density, 3) + "/m under it (a visual range of " +
         fixed(3 / f.density, 0) + " m), its top softened over " + fixed(f.edge_m, 0) + " m";
}

Scene make(const mg::Level& level, const char* id, const char* view, double hour, double share,
           const char* look) {
  Scene s;
  s.id = id;
  s.look = look;
  fall_line::app::day_view(level, hour, s.view);
  fall_line::app::make_view(level, view, s.view);
  s.fog = sea(level, share, 0.02);
  s.what = sea_what(level, s.fog);
  return s;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const double share = lab.arg_num("top");
  if (frames < 1 || frames > 600 || width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
      share <= 0 || share >= 1) {
    lab.fail("--frames must be 1..600, --width and --height 1..4096 and --top between 0 and 1",
             "next: pass --frames=8 --width=640 --height=360 --top=0.35.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level level = mg::generate_level(seed, opts);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const double hour = lab.arg_num("morning");

  std::vector<Scene> scenes;
  const std::string& want = lab.arg("scenes");
  auto wanted = [&](const char* id) {
    return want == "all" ||
           ("," + want + ",").find(std::string(",") + id + ",") != std::string::npos;
  };
  if (wanted("sea")) {
    scenes.push_back(make(level, "sea", "vista", hour, share,
                          "a white sea filling the valleys, its top lit and waving, the slopes and "
                          "the ridges standing out of it as islands, its edge on them sharp"));
  }
  if (wanted("aerial")) {
    scenes.push_back(make(level, "aerial", "aerial", hour, share,
                          "the mountain an island in a sea of fog reaching to the horizon, the "
                          "far sea veiled blue by the air in front of it"));
  }
  if (wanted("dawn")) {
    scenes.push_back(make(level, "dawn", "vista", lab.arg_num("dawn"), share,
                          "the sea's top lit warm by a low sun, dimmer than at noon, the slopes "
                          "over it in the same light"));
  }
  if (wanted("inside")) {
    // The top over the finish, wherever the base lies: the eye under it.
    Scene s = make(level, "inside", "lookback", hour, share,
                   "under the fog's top: a grey gloom, the near snow dim, everything past a "
                   "hundred metres lost");
    s.fog.top_m = std::max(s.fog.top_m, s.view.eye[1] + 80);
    s.what = sea_what(level, s.fog);
    scenes.push_back(s);
  }
  if (scenes.empty()) {
    lab.fail("--scenes `" + want + "` names no scene",
             "Pass --scenes=all or ids from sea,aerial,dawn,inside, comma-separated. Skill: "
             "render.");
    return;
  }
  for (const Scene& s : scenes) {
    lab.metric(s.id + ".over-top", s.view.eye[1] - s.fog.top_m, "m");
    lab.metric(s.id + ".top-over-base", s.fog.top_m - level.mountain.base.y, "m");
  }

  auto shot_of = [&](const Scene& s, bool on) {
    const std::string id = s.id + (on ? "" : "-off");
    std::string title = s.id + (on ? " IN VALLEY FOG" : " CLEAR") + ", SEED " + seed_s;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const std::string caption =
        on ? "Seed " + seed_s + ", " + s.what + "; " + std::to_string(frames) + " frames drawn."
           : "Seed " + seed_s + ", the " + s.id +
                 " scene's camera with no fog: what the fog is judged against.";
    return ge::lab::Shot{id,
                         title,
                         caption,
                         on ? s.look : "the same view in clear air",
                         {{"seed", seed_s},
                          {"scene", s.id},
                          {"fog", on ? "on" : "off"},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "valley-fog", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Scene& s : scenes) {
      for (bool on : {false, true}) {
        ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
        ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot(shot_of(s, on), none, Det::measured);
      }
    }
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  fall_line::app::FrameOptions options;
  options.weather = lab.arg_num("weather") != 0;
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "valley-fog.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  for (const Scene& s : scenes) {
    std::vector<uint8_t> clear;
    for (bool on : {false, true}) {
      fall_line::app::cut(*frame);
      const fall_line::app::FrameScene scene;
      r::FrameView view = s.view;
      if (on) view.valley = s.fog;
      for (int n = 0; n < frames; ++n) {
        view.frame = static_cast<uint64_t>(n);
        view.time_s = 30 - static_cast<double>(frames - 1 - n) / 60;
        fall_line::app::draw(*device, *frame, view, scene, target.view);
      }
      const ge::gpu::Image img = ge::gpu::read_now(*device, target);
      lab.shot(shot_of(s, on), ge::lab::Picture{width, height, img.rgba}, Det::measured);
      if (!on) {
        clear = img.rgba;
        continue;
      }
      size_t changed = 0;
      double luma = 0;
      for (size_t i = 0; i + 3 < img.rgba.size() && i + 3 < clear.size(); i += 4) {
        const double a = 0.2126 * clear[i] + 0.7152 * clear[i + 1] + 0.0722 * clear[i + 2];
        const double b = 0.2126 * img.rgba[i] + 0.7152 * img.rgba[i + 1] + 0.0722 * img.rgba[i + 2];
        if (std::fabs(a - b) >= 2) ++changed;
        luma += b - a;
      }
      const double pixels = static_cast<double>(width) * height;
      lab.metric(s.id + ".changed", static_cast<double>(changed) / pixels, "share", Det::measured);
      lab.metric(s.id + ".luma", luma / pixels, "levels", Det::measured);
      if (static_cast<double>(changed) < pixels * 0.01) {
        lab.fail("the " + s.id + " scene's valley fog leaves the picture as it was",
                 "Check valley_rows in engine/render/valley_fog.cpp and valley_fog in "
                 "engine/render/wgsl/valley_fog.wgsl. Skill: render.");
      }
    }
  }
  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
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
    "valley-fog",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/valley_fog.hpp (the sea of fog in the valleys) in Fall Line's frame",
    "render",
    "Does the valley fog read as a white sea filling the valleys from the summit and the air, its "
    "top lit by the hour's sun and waving, its edge on the slopes sharp at any range, and as a "
    "grey gloom from under it?",
    {
        {"seed", "38", "the map's seed"},
        {"scenes", "all", "the scenes drawn: all, or ids comma-separated (sea,aerial,dawn,inside)"},
        {"top", "0.35", "the top's share of the way from the base to the summit"},
        {"morning", "11", "the solar hour of the sea, the aerial and the inside scenes"},
        {"dawn", "7.5", "the solar hour of the dawn scene"},
        {"weather", "0", "1: the map's own sky (its cloud and deck); 0: a clear one"},
        {"frames", "8", "frames drawn, at 60 a second, up to each picture"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=2 --width=160 --height=90 --scenes=sea,inside",
    run,
};
GE_LAB(kSpec);

}  // namespace
