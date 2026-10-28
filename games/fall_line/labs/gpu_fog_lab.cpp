// LAB `fog`: the fog in areas (engine/render/fog.hpp) in Fall Line's frame. Five scenes over a
// seed's map, each drawn without its fog and with it, from a camera the scene places:
// - WOODS: under the late morning sun, the wood the sun lights best seen across the sun (the `wood`
//   view), a ground mist laid over it, the trees' shadows falling through it;
// - SHAFTS: the same mist from the wood's shaded side, looking toward the sun through the trees,
//   so they throw shafts through it toward the eye;
// - VALLEY: from over the summit, a bank of fog lying in the valley round the base, thinning
//   upward, its top lit by the sun;
// - GLOW: at night, a glowing haze on the snow ahead (what a fire or a pool of lava lights), red
//   and orange, among clear air;
// - DUST: at noon on the piste, a thin warm dust in the air round the eye, carved into drifting
//   wisps by the noise on the wind.
// --frames frames are drawn up to each picture at 60 a second, so the fog's history has settled.
//
// Beside each: the volumes in reach and the froxel range (deterministic), the share of the
// picture the fog changed and its mean change in luma (measured), and the two passes' GPU ms
// (measured). The lab fails when a scene's fog finds no volume in reach or leaves the picture as
// it was, or on a GPU error. Without an adapter it notes that and draws placeholders, so the
// digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the fog in areas: woods mist with shafts, a valley bank, a glow, drifting dust
// ge:keywords lab fog mist ground-fog valley-fog dust glow light-shafts god-rays froxel volume
// ge:keywords fall-line picture
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
#include "render/fog.hpp"
#include "render/view.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;
namespace r = ge::render;

constexpr double kDeg = 3.14159265358979323846 / 180;

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
  double range = 160;  // the froxels' reach, m
  r::FrameView view;
  std::vector<r::FogVolume> fog;
  std::string what;  // the caption's account of the fog
};

// The direction toward the sun along the ground (unit, x and z).
void sun_flat(const r::FrameView& v, double& sx, double& sz) {
  double sun[3];
  r::sun_direction(v, sun);
  const double l = std::max(std::hypot(sun[0], sun[2]), 1e-6);
  sx = sun[0] / l;
  sz = sun[2] / l;
}

// The wood the `wood` view looks at (70 m along its gaze), under `hour`'s sun.
void wood_at(const mg::Level& level, double hour, r::FrameView& v, double& tx, double& tz) {
  fall_line::app::day_view(level, hour, v);
  fall_line::app::make_view(level, "wood", v);
  tx = v.eye[0] + 70 * std::sin(v.heading);
  tz = v.eye[2] + 70 * std::cos(v.heading);
}

// A ground mist lying on the wood at (tx, tz), turned to the sun along the ground (sx, sz).
r::FogVolume wood_mist(double tx, double tz, double sx, double sz) {
  r::FogVolume f;
  f.centre[0] = tx;
  f.centre[2] = tz;
  f.hug_ground = true;
  f.half[0] = f.half[2] = 110;
  f.half[1] = 20;  // 40 m deep over the snow
  f.yaw = std::atan2(sx, sz);
  f.density = 0.035;
  f.falloff = 0.05;
  f.edge = 12;
  f.phase = 0.7;
  f.noise = 0.45;
  f.noise_size = 40;
  f.wind[0] = 0.6;
  return f;
}

const char* kMistWhat =
    "a ground mist 220 m across and 40 m deep lying on the snow of the wood, "
    "0.035/m on the snow thinning upward 5 % a metre, forward-scattering "
    "(g 0.7)";

Scene woods(const mg::Level& level, double hour) {
  Scene s;
  s.id = "woods";
  s.look =
      "the wood standing in a pale mist, the trees' shadows falling through it as darker "
      "bands across the sunlit mist, the mist thinning upward and toward the edges";
  double tx = 0, tz = 0, sx = 0, sz = 0;
  wood_at(level, hour, s.view, tx, tz);
  sun_flat(s.view, sx, sz);
  s.fog = {wood_mist(tx, tz, sx, sz)};
  s.what = kMistWhat;
  return s;
}

Scene shafts(const mg::Level& level, double hour) {
  Scene s;
  s.id = "shafts";
  s.look =
      "from inside the wood toward the low sun: the trunks dark against a glowing mist, "
      "bright shafts of sun between them fanning toward the eye";
  double tx = 0, tz = 0, sx = 0, sz = 0;
  wood_at(level, hour, s.view, tx, tz);
  sun_flat(s.view, sx, sz);
  // From the wood's shaded side, looking toward the sun through the trees at a point 8 m over
  // the snow 40 m beyond the wood's middle.
  const double ex = tx - 40 * sx, ez = tz - 40 * sz;
  const double ax = tx + 40 * sx, az = tz + 40 * sz;
  s.view.eye[0] = ex;
  s.view.eye[2] = ez;
  s.view.eye[1] = level.ground_at(ex, ez) + 2.0;
  s.view.heading = std::atan2(sx, sz) + 14 * kDeg;  // the sun just off the left of the picture
  s.view.pitch = std::atan2(level.ground_at(ax, az) + 8 - s.view.eye[1], 80.0);
  s.view.fov_y = 62 * kDeg;
  s.fog = {wood_mist(tx, tz, sx, sz)};
  s.what = kMistWhat;
  return s;
}

Scene valley(const mg::Level& level, double hour) {
  Scene s;
  s.id = "valley";
  s.look =
      "a white sea of fog filling the valleys, its top lit and soft, the slopes and the far "
      "ridges standing clear out of it";
  s.range = 4000;
  fall_line::app::day_view(level, hour, s.view);
  fall_line::app::make_view(level, "vista", s.view);
  const auto& m = level.mountain;
  r::FogVolume f;
  // Over the whole map and round it, so every low ground is under it: the sea of fog an
  // inversion holds in the valleys.
  f.centre[0] = f.centre[2] = 0.5 * level.size;
  f.half[0] = f.half[2] = 3000;
  f.half[1] = 170;
  f.centre[1] = m.base.y - 40 + f.half[1];  // its floor 40 m under the base, its top 300 m over
  f.density = 0.08;
  f.falloff = 0.005;
  f.edge = 70;
  f.phase = 0.2;
  f.noise = 0.35;
  f.noise_size = 300;
  s.fog = {f};
  s.what =
      "a sea of fog over every low ground, its top 300 m over the base, 0.08/m at its floor "
      "thinning 0.5 % a metre, its top softened over 70 m and carved by the noise";
  return s;
}

Scene glow(const mg::Level& level, double hour) {
  Scene s;
  s.id = "glow";
  s.look =
      "a soft orange-red glow hanging over the snow ahead, brightest at its heart, the snow "
      "and the trees round it dark in the night, the stars and the far slope behind it";
  fall_line::app::day_view(level, hour, s.view);
  fall_line::app::make_view(level, "piste", s.view);
  const double x = s.view.eye[0] + 28 * std::sin(s.view.heading);
  const double z = s.view.eye[2] + 28 * std::cos(s.view.heading);
  r::FogVolume f;
  f.shape = r::FogVolume::Shape::ellipsoid;
  f.centre[0] = x;
  f.centre[2] = z;
  f.centre[1] = level.ground_at(x, z) + 1.5;
  f.half[0] = f.half[2] = 9;
  f.half[1] = 5;
  f.density = 0.12;
  f.falloff = 0.15;
  f.edge = 4;
  f.albedo[0] = 0.6;
  f.albedo[1] = 0.45;
  f.albedo[2] = 0.35;
  f.glow[0] = 0.9;
  f.glow[1] = 0.32;
  f.glow[2] = 0.06;
  f.noise = 0.5;
  f.noise_size = 10;
  f.wind[1] = 0.8;  // the hot air rising
  s.fog = {f};
  s.what =
      "an ellipsoid of glowing haze 18 m across 28 m ahead, 0.12/m, glowing 0.9 0.32 0.06 "
      "cd/m2 a metre of extinction, rising on its own heat";
  return s;
}

Scene dust(const mg::Level& level, double hour) {
  Scene s;
  s.id = "dust";
  s.look =
      "a thin warm haze over the sunlit piste, the near snow clear and the far slope "
      "veiled in a warm glow, uneven wisps drifting across the view";
  fall_line::app::day_view(level, hour, s.view);
  fall_line::app::make_view(level, "corduroy@0:-6", s.view);
  r::FogVolume f;
  for (int k = 0; k < 3; ++k) f.centre[k] = s.view.eye[k];
  f.half[0] = f.half[2] = 200;
  f.half[1] = 40;
  f.centre[1] = s.view.eye[1] + 20;
  f.density = 0.012;
  f.edge = 30;
  f.albedo[0] = 0.85;
  f.albedo[1] = 0.72;
  f.albedo[2] = 0.55;
  f.phase = 0.3;
  f.noise = 0.85;
  f.noise_size = 26;
  f.wind[0] = 3;
  f.wind[2] = 1;
  s.fog = {f};
  s.what =
      "a dust 400 m across round the eye, 0.012/m, warm (albedo 0.85 0.72 0.55), its noise "
      "carving 85 % of it into wisps drifting at 3 m/s";
  return s;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (frames < 1 || frames > 600 || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
    lab.fail("--frames must be 1..600 and --width and --height 1..4096",
             "next: pass --frames=16 --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level level = mg::generate_level(seed, opts);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  std::vector<Scene> scenes;
  const std::string& want = lab.arg("scenes");
  auto wanted = [&](const char* id) {
    return want == "all" ||
           ("," + want + ",").find(std::string(",") + id + ",") != std::string::npos;
  };
  if (wanted("woods")) scenes.push_back(woods(level, lab.arg_num("morning")));
  if (wanted("shafts")) scenes.push_back(shafts(level, lab.arg_num("morning")));
  if (wanted("valley")) scenes.push_back(valley(level, lab.arg_num("morning") + 1));
  if (wanted("glow")) scenes.push_back(glow(level, lab.arg_num("night")));
  if (wanted("dust")) scenes.push_back(dust(level, 12));
  if (scenes.empty()) {
    lab.fail("--scenes `" + want + "` names no scene",
             "Pass --scenes=all or ids from woods,shafts,valley,glow,dust, comma-separated. Skill: "
             "render.");
    return;
  }
  for (const Scene& s : scenes) {
    r::FogSettings fs;
    fs.range = s.range;
    const size_t in = r::fog_in_reach(fs, s.view, s.fog).size();
    lab.metric(s.id + ".volumes", static_cast<double>(in), "count");
    lab.metric(s.id + ".range", s.range, "m");
    const r::FogVolume& f0 = s.fog.front();
    lab.metric(s.id + ".distance",
               std::hypot(f0.centre[0] - s.view.eye[0], f0.centre[1] - s.view.eye[1],
                          f0.centre[2] - s.view.eye[2]),
               "m");
    lab.metric(s.id + ".below", s.view.eye[1] - f0.centre[1], "m");
    if (in == 0) {
      lab.fail("the " + s.id + " scene's fog has no volume in the froxels' reach",
               "Check the scene's camera and volume in games/fall_line/labs/gpu_fog_lab.cpp, and "
               "fog_in_reach in engine/render/fog.cpp. Skill: render.");
    }
  }

  auto shot_of = [&](const Scene& s, bool on) {
    const std::string id = s.id + (on ? "" : "-off");
    std::string title = s.id + (on ? " IN FOG" : " CLEAR") + ", SEED " + seed_s;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const std::string caption = on ? "Seed " + seed_s + ", " + s.what + "; the froxels reach " +
                                         fixed(s.range, 0) + " m; " + std::to_string(frames) +
                                         " frames drawn."
                                   : "Seed " + seed_s + ", the " + s.id +
                                         " scene's camera with no fog: what the fog is "
                                         "judged against.";
    return ge::lab::Shot{id,
                         title,
                         caption,
                         on ? s.look : "the same view in clear air",
                         {{"seed", seed_s},
                          {"scene", s.id},
                          {"fog", on ? "on" : "off"},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "fog", .validation = 1});
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
  uint32_t errors = 0, late = 0;
  bool first = true;
  for (const Scene& s : scenes) {
    // A frame is made once a device (its load ends), and each scene's froxels have their reach.
    if (!first) {
      errors += device->errors.load();
      late += device->pipelines_after_load;
      device = ge::gpu::create_device({.label = "fog", .validation = 1});
      if (!device) {
        lab.fail("the GPU device could not be made again",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
    }
    first = false;
    fall_line::app::FrameOptions options;
    options.froxels.range = s.range;
    options.weather = lab.arg_num("weather") != 0;
    auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height),
                                              wgpu::TextureFormat::RGBA8Unorm, options);
    if (!frame) {
      lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    const ge::gpu::Target target = ge::gpu::create_target(
        *device, "fog.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    std::vector<uint8_t> clear;
    for (bool on : {false, true}) {
      fall_line::app::cut(*frame);
      fall_line::app::FrameScene scene;
      if (on) scene.fog = s.fog;
      r::FrameView view = s.view;
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
      // What the fog did to the picture.
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
        lab.fail("the " + s.id + " scene's fog leaves the picture as it was",
                 "Check prepare_fog and the froxels in engine/render/fog.cpp and local_fog in "
                 "engine/render/wgsl/frame.wgsl. Skill: render.");
      }
      frame->recorder.wait(*device);
      for (int p = 0; p < frame->recorder.stats.count; ++p) {
        const auto& ps = frame->recorder.stats.passes[static_cast<size_t>(p)];
        if (std::string_view(ps.name).starts_with("fog") && !std::isnan(ps.gpu_ms)) {
          lab.metric(s.id + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms", Det::measured);
        }
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
    "fog",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/fog.hpp (the fog in areas) in Fall Line's frame",
    "render",
    "Does the fog in areas read as a mist in the woods with the sun's shafts through it, a fog "
    "bank in the valley, a glowing haze at night and a drifting dust, each veiling what is behind "
    "it and lit by the light round it?",
    {
        {"seed", "38", "the map's seed"},
        {"scenes", "all",
         "the scenes drawn: all, or ids comma-separated (woods,shafts,valley,glow,dust)"},
        {"morning", "11", "the solar hour of the woods (the valley's an hour later)"},
        {"night", "22", "the solar hour of the glow"},
        {"weather", "1", "1: the map's own sky (its cloud and deck); 0: a clear one"},
        {"frames", "16", "frames drawn, at 60 a second, up to each picture"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=2 --width=160 --height=90 --scenes=woods,glow",
    run,
};
GE_LAB(kSpec);

}  // namespace
