// LAB `spray`: what the skis throw (plugins/skier/app/spray.hpp) in Fall Line's frame, flown and
// drawn by the engine's particles (engine/render/particles.hpp). Each scene is one of the ride
// scenarios (plugins/skier/scenarios.hpp: a hand-built map, a moment, a scripted input) stepped at
// 120 Hz and drawn at 60 frames a second from its start to its shot, so the spray in the picture
// is the spray those frames threw and flew:
//
// - CARVE (`carve-hard`): full steer at 20 m/s on a groomer: a thin curl out of the turn off the
//   outside ski's tail.
// - SKID (`hockey-stop`): full brake at 15 m/s: a sheet thrown the way the skis slide.
// - LANDING (`kicker`): a fifth of a second after he lands from the kicker: a burst round the skis.
// - POWDER (`powder-turns`): linked turns in untouched snow, in a 6 m/s crosswind: fines churned up
//   behind him and the snow cloud over them, hanging and drifting off on the wind (the map's air,
//   plugins/mountain/air/air.hpp, which the frame hands the particles).
//
// Each scene is shot from the chase camera and from the side (--side m to the skier's right,
// looking across at him). Beside each: the grains the arithmetic threw (deterministic) and the
// live particles the pool held at the shot (read back: measured). The lab fails when a scene
// throws nothing. Without an adapter it notes that and draws placeholders, so the digest is the
// same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the spray off a hard carve, a hockey stop, a landing and powder turns
// ge:keywords lab spray particles carve skid hockey-stop landing burst powder fines grains
// ge:keywords live-particles chase-camera side-view fall-line picture scenarios
// ge:skill skier render labs
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "render/particles.hpp"
#include "skier/app/spray.hpp"
#include "skier/scenarios.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"
#include "skier/trees.hpp"

namespace {

using ge::lab::Det;
using ge::skier::SkierState;

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

constexpr double kFrame = 1.0 / 60;

// A scene: a ride scenario and the moment it is shot at.
struct Scene {
  const char* id;
  const char* scenario;
  double at;           // s into the scenario; after the first landing when `after_landing`
  bool after_landing;  // `at` counts from the first landing
  const char* what;
  const char* look;
};

constexpr Scene kScenes[] = {
    {"carve", "carve-hard", 1.6, false, "full steer at 20 m/s on a 15 degree groomer",
     "a thin curl of white grains off the outside ski's tail, thrown out of the turn and falling "
     "back behind him; streaks, not dots, and no square edges"},
    {"skid", "hockey-stop", 0.6, false, "full brake at 15 m/s on a 10 degree groomer",
     "a fan of snow thrown the way the skis slide, thick by the skis and thinning as it rises; "
     "lit like the snow, darker in his shadow"},
    {"landing", "kicker", 0.2, true, "straight over the kicker at 14 m/s, just landed",
     "a burst of grains out round the skis, low and wide, the fines hanging over it"},
    {"powder", "powder-turns", 2.5, false, "linked turns in untouched powder",
     "a haze of fines churned up behind him and big thin puffs of cloud hanging over his line, "
     "drifting off to the left on the crosswind"},
};

// The powder scene's wind at 10 m, m/s.
constexpr double kCrosswind = 6;

struct Frame60 {
  SkierState s;
  int thrown = 0;
};

void run(ge::lab::Lab& lab) {
  const double hour = lab.arg_num("hour");
  const double side = lab.arg_num("side");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (side <= 0 || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
    lab.fail("--side must be over 0 and --width and --height 1..4096",
             "next: pass --side=7 --width=640 --height=360.");
    return;
  }

  // THE SCENES, stepped at 120 Hz, a frame every second step, and the spray the CPU throws.
  std::vector<std::vector<Frame60>> runs;
  std::vector<ge::mountain::mapgen::Level> levels;
  for (const Scene& sc : kScenes) {
    const ge::skier::Scenario* scenario = ge::skier::find_scenario(sc.scenario);
    if (!scenario) {
      lab.fail(std::string("no ride scenario `") + sc.scenario + "`",
               "Name one of plugins/skier/scenarios.cpp's rows in kScenes. Skill: skier.");
      return;
    }
    levels.push_back(ge::skier::scenario_level(*scenario));
    // The powder turns in a crosswind (from +x, blowing toward -x), so the cloud is seen to drift.
    if (std::string_view(sc.id) == "powder") {
      levels.back().weather.wind = kCrosswind;
      levels.back().weather.wind_from = 1.5707963267948966;
    }
    const ge::mountain::mapgen::Level& level = levels.back();
    const ge::skier::TreeIndex trees = ge::skier::build_tree_index(level);
    const ge::skier::World world{&level, &trees};
    SkierState s = ge::skier::stand_skier(world, scenario->x, scenario->z, scenario->heading,
                                          scenario->speed, scenario->lift);
    ge::skier::SprayPen pen = ge::skier::spray_pen(0);
    std::vector<ge::skier::SprayGrain> grains;
    std::vector<Frame60> frames;
    ge::skier::SkierEvents ev;
    double landed = -1, end = scenario->seconds;
    const double dt = 1.0 / 120;
    for (int i = 0; static_cast<double>(i) * dt < end; ++i) {
      const double t = static_cast<double>(i) * dt;
      ge::skier::step_skier(s, scenario->input(t, s), world, ge::skier::kReferenceSkis, dt, ev);
      if (ev.landed && landed < 0) landed = t + dt;
      if (i % 2 == 0) continue;
      grains.clear();
      frames.push_back({s, ge::skier::spray_grains(s, kFrame, pen, grains)});
      const double shot = sc.after_landing ? (landed < 0 ? -1 : landed + sc.at) : sc.at;
      if (shot >= 0 && t + dt >= shot) break;
    }
    if (sc.after_landing && landed < 0) {
      lab.fail(std::string("the `") + sc.scenario + "` scenario never landed",
               "Check the scenario's row in plugins/skier/scenarios.cpp. Skill: skier.");
      return;
    }
    double total = 0;
    for (const Frame60& f : frames) total += f.thrown;
    const SkierState& last = frames.back().s;
    const std::string k = sc.id;
    lab.metric(k + ".t", static_cast<double>(frames.size()) / 60, "s");
    lab.metric(k + ".speed", last.speed, "m/s");
    lab.metric(k + ".edge", std::fabs(last.edge) * 57.29577951308232, "deg");
    lab.metric(k + ".g-lat", std::fabs(last.g_lat), "g");
    lab.metric(k + ".slide", std::fabs(ge::skier::ski_slide(last)), "m/s");
    lab.metric(k + ".packed", last.packed, "share");
    lab.metric(k + ".grains", total, "count");
    lab.metric(k + ".grains-frame", frames.back().thrown, "count");
    if (total <= 0) {
      lab.fail(std::string("the ") + sc.id + " threw no spray",
               "Check its source's rate in plugins/skier/app/spray.cpp (spray_grains). Skill: "
               "skier.");
    }
    runs.push_back(std::move(frames));
  }

  auto shot_of = [&](size_t i, bool beside) {
    const Scene& sc = kScenes[i];
    const SkierState& s = runs[i].back().s;
    const std::string t = fixed(static_cast<double>(runs[i].size()) / 60, 2);
    const std::string id = std::string(sc.id) + (beside ? "-side" : "");
    std::string title = id + " AT " + t + " S";
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const std::string caption =
        std::string(beside ? "From the side, " + fixed(side, 0) + " m off his right, "
                           : "The chase camera, ") +
        t + " s into `" + sc.scenario + "` (" + sc.what + "; " + fixed(s.speed, 1) + " m/s, " +
        fixed(std::fabs(s.g_lat), 2) + " g sideways, sliding " +
        fixed(std::fabs(ge::skier::ski_slide(s)), 1) +
        " m/s), every frame since its start drawn so the spray is the one it threw.";
    return ge::lab::Shot{id,
                         title,
                         caption,
                         sc.look,
                         {{"scenario", sc.scenario},
                          {"view", beside ? "side" : "chase"},
                          {"t", t},
                          {"hour", fixed(hour, 2)},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "spray", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (size_t i = 0; i < runs.size(); ++i) {
      for (bool beside : {false, true}) {
        ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
        ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot(shot_of(i, beside), none, Det::measured);
      }
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  uint32_t errors = 0, late = 0;
  for (size_t i = 0; i < runs.size(); ++i) {
    const ge::mountain::mapgen::Level& level = levels[i];
    // A frame is made once a device (its load ends), so each scene's map has a device of its own.
    if (i > 0) {
      errors += device->errors.load();
      late += device->pipelines_after_load;
      device = ge::gpu::create_device({.label = "spray", .validation = 1});
      if (!device) {
        lab.fail("the GPU device could not be made again",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
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
        *device, "spray.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    for (bool beside : {false, true}) {
      // The scene from its start, frame by frame.
      fall_line::app::forget_tracks(*frame);
      fall_line::app::cut(*frame);
      ge::render::FrameView view;
      fall_line::app::day_view(level, hour, view);
      for (size_t f = 0; f < runs[i].size(); ++f) {
        const SkierState& s = runs[i][f].s;
        if (beside) {
          // Off his right, a metre up, looking back across at him.
          const double rx = ge::core::libm::cos(s.heading), rz = -ge::core::libm::sin(s.heading);
          view.eye[0] = s.x + side * rx;
          view.eye[2] = s.z + side * rz;
          view.eye[1] = ge::core::sample_field(level.ground, view.eye[0], view.eye[2]) + 1.2;
          view.heading = s.heading - 1.5707963267948966;
          view.pitch = -0.05;
        } else {
          fall_line::app::chase_view(level, s, view);
        }
        view.frame = f;
        view.time_s = (static_cast<double>(f) + 1) / 60;
        fall_line::app::draw(*device, *frame, view, {std::span<const SkierState>(&s, 1)},
                             target.view);
      }
      const ge::gpu::Image img = ge::gpu::read_now(*device, target);
      lab.shot(shot_of(i, beside), ge::lab::Picture{width, height, img.rgba}, Det::measured);
      if (beside) continue;
      const std::vector<ge::render::Particle> slots =
          ge::render::read_particles(*device, frame->particles);
      const std::string k = kScenes[i].id;
      lab.metric(k + ".live", ge::render::live_particles(slots), "count", Det::measured);
      frame->recorder.wait(*device);
      for (int p = 0; p < frame->recorder.stats.count; ++p) {
        const auto& ps = frame->recorder.stats.passes[static_cast<size_t>(p)];
        if (std::string_view(ps.name).starts_with("particles") && !std::isnan(ps.gpu_ms)) {
          lab.metric(k + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms", Det::measured);
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
    "spray",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "plugins/skier/app/spray.hpp (what the skis throw) and engine/render/particles.hpp (the "
    "particles that fly and draw it) in Fall Line's frame",
    "skier",
    "Does the spray read as snow thrown off a carve, a skid, a landing and powder turns?",
    {
        {"hour", "11", "the solar hour to light the scenes at"},
        {"side", "7", "how far off the skier's right the side view stands, m"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
