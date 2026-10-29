// LAB `post`: THE LENS AND THE FILM over Fall Line's frame (render/post.hpp, render/lens.hpp,
// render/grade.hpp; docs/rendering.md § Post). Two scenes of seed --seed at --hour: SUN, the eye
// before a wood looking into the low sun through the trees (the sun's rays, the flare, the dirt),
// and START, the start gate down the piste (the sharpening, the vignette, the fringes, the grain,
// the grade). Each is shot with every effect off (the frame as it was before them), with Fall
// Line's defaults, and with each effect alone at full strength, so a picture shows what the effect
// does and the defaults show how much of it the game keeps.
//
// THE COST of each is timed on the SUN scene: the median GPU frame over --frames frames after the
// TAA history settles, every variant --rounds times in turn keeping its least, against the frame
// with every effect off; the post and the lens passes' own ms beside it. Times are measured (a
// software adapter's say little of a GPU's: any full-screen pass costs it about the same).
//
// Without an adapter it notes that and draws placeholders, so the digest is the same on every
// row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the post effects (sun rays, flare, sharpen, vignette, fringes, grain, dirt,
// grade) each alone and at the defaults, pictured and timed
// ge:keywords lab post post-effects lens sun-rays god-rays flare sharpen vignette chromatic
// ge:keywords film-grain lens-dirt grade fall-line cost
// ge:skill render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/taa.hpp"

namespace {

namespace fa = fall_line::app;
namespace gr = ge::render;
namespace mg = ge::mountain::mapgen;
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

double median(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

// Every effect off: the frame as it was drawn before the lens and the film.
fa::FrameOptions bare() {
  fa::FrameOptions o;
  o.look = {};
  o.lens = {};
  o.grade = {};
  o.ao = {};
  o.dof = {};
  return o;
}

struct Variant {
  std::string name;
  std::string what;      // the caption's sentence
  std::string look_for;  // what the picture must show
  std::function<void(fa::FrameOptions&)> set;
  bool sun = true, start = true;  // the scenes it is shot in
  bool moving = false;            // the eye moves through the frames (kStride a frame)
};

// The moving eye: 0.35 m a frame along its heading (21 m/s at 60 frames a second, a skier's
// cruising speed) turning 0.25 degrees a frame, its height over the ground kept.
constexpr double kStride = 0.35, kTurn = 0.0044;

std::vector<Variant> variants() {
  return {
      {"off", "Every effect off: the frame as it was before the lens and the film.",
       "the reference the others are judged against", [](fa::FrameOptions& o) { o = bare(); }},
      {"defaults",
       "Fall Line's defaults: the sun's rays at 0.5, sharpening at 0.3, the vignette at 0.25, "
       "ambient occlusion at 0.6; the flare, the fringes, the grain, the dirt and motion blur off.",
       "close to the reference at a glance: a little crisper, the corners a touch darker, the sun "
       "streaming softly through the trees, the trees' feet a shade darker; nothing that reads as "
       "an effect",
       [](fa::FrameOptions& o) { o = fa::FrameOptions{}; }},
      {"sunrays", "The sun's rays alone at full strength (r_sunrays 1).",
       "beams fanning from the sun through the gaps between the trunks and crowns, dark behind "
       "each trunk; nothing on the snow away from the sun; no banding or blotches",
       [](fa::FrameOptions& o) {
         o = bare();
         o.lens.rays = 1;
       },
       true, false},
      {"flare", "The lens flare alone at full strength (r_flare 1).",
       "a few faint coloured ghosts on the line from the sun through the middle, some hexagonal, "
       "and a faint ring round the sun; dimmer where trees hide the sun",
       [](fa::FrameOptions& o) {
         o = bare();
         o.lens.flare = 1;
       },
       true, false},
      {"sharpen", "Contrast-adaptive sharpening alone at full strength (r_sharpen 1).",
       "the snow's grain, the trees' needles and the far ridges crisper; no bright or dark halo "
       "along an edge against the sky",
       [](fa::FrameOptions& o) {
         o = bare();
         o.look.sharpen = 1;
       }},
      {"vignette", "The vignette alone at full strength (r_vignette 1).",
       "the corners darker, the middle untouched, the fall-off smooth with no ring",
       [](fa::FrameOptions& o) {
         o = bare();
         o.look.vignette = 1;
       }},
      {"chromatic", "The lens's colour fringes alone at full strength (r_chromatic 1).",
       "red and blue edges on the trunks and ridges toward the corners, none in the middle",
       [](fa::FrameOptions& o) {
         o = bare();
         o.look.chromatic = 1;
       }},
      {"grain", "Film grain alone at full strength (r_grain 1).",
       "a fine even grain in the middle tones, little in the white snow and the black trunks",
       [](fa::FrameOptions& o) {
         o = bare();
         o.look.grain = 1;
       }},
      {"lensdirt", "Dirt on the lens at full strength (r_lensdirt 1), lit by the sun's rays.",
       "smudges and dried drops showing only near the sun and the bright glow, invisible over "
       "the shaded snow",
       [](fa::FrameOptions& o) {
         o = bare();
         o.lens.rays = 0.5;
         o.look.dirt = 1;
       },
       true, false},
      {"moving",
       "Every effect off with the eye moving at 21 m/s and turning, the reference for "
       "motion blur.",
       "the frame as sharp as a still one: TAA keeps the moving picture clean",
       [](fa::FrameOptions& o) { o = bare(); }, true, true, true},
      {"motionblur",
       "Motion blur at a film camera's shutter (r_motionblur 0.5), the eye moving at 21 m/s and "
       "turning.",
       "the snow and the trees streaked along their motion, more toward the edges and the near "
       "ground; the middle of the picture, where the eye is heading, nearly sharp; no halo round "
       "the trunks and no seam between tiles",
       [](fa::FrameOptions& o) {
         o = bare();
         o.look.motion_blur = 0.5;
       },
       true, true, true},
      {"ao", "Ambient occlusion alone at full strength (r_ao 1, r_ao_radius 1.5).",
       "the snow darker in the creases: round each trunk's foot, under the crowns' skirts, in "
       "the skier's lee and the start hut's corners; open snow and the far slopes untouched; no "
       "dark halo round a trunk against the snow behind it",
       [](fa::FrameOptions& o) {
         o = bare();
         o.ao.strength = 1;
       }},
      {"dof", "Depth of field at full strength (r_dof 1) focused 30 m out (r_dof_focus 30).",
       "the trees about 30 m out sharp, the near snow and the far ridges blurred into soft shapes; "
       "a sharp edge round each trunk in focus, no halo of the blurred background over it",
       [](fa::FrameOptions& o) {
         o = bare();
         o.dof = {.strength = 1, .focus_m = 30};
       }},
      {"grade",
       "The grade's knobs turned: r_contrast 1.2, r_saturation 1.15, r_temperature 0.3 (warm).",
       "a warmer, punchier picture with the white snow still white and the black still black",
       [](fa::FrameOptions& o) {
         o = bare();
         o.grade = {.contrast = 1.2, .saturation = 1.15, .temperature = 0.3};
       }},
  };
}

// True when the ground hides no part of the sun's ray from (x, y, z): 96 samples toward the sun,
// growing from 4 m to about 10 km.
bool sees_sun(const mg::Level& level, double x, double y, double z, const gr::FrameView& v) {
  const double rise = ge::core::libm::tan(v.sun_elevation);
  const double fx = ge::core::libm::sin(v.sun_azimuth), fz = ge::core::libm::cos(v.sun_azimuth);
  double d = 4;
  for (int i = 0; i < 96; ++i, d *= 1.085) {
    if (level.ground_at(x + d * fx, z + d * fz) > y + d * rise) return false;
  }
  return true;
}

// The eye before a wood with the low sun ahead through the trees: of every fifth tree, the spot
// 40 m behind it from the sun, no tree within 20 m, whose corridor toward the sun (20 to 100 m
// along, 15 m either side) holds the number of trees nearest to `trees` and whose sun the
// mountain does not hide, looking past the sun's left, raised so it sits high in the frame.
bool sun_view(const mg::Level& level, double trees, gr::FrameView& v) {
  const double flat_x = ge::core::libm::sin(v.sun_azimuth),
               flat_z = ge::core::libm::cos(v.sun_azimuth);
  double best = 1e9;
  bool found = false;
  for (size_t i = 0; i < level.trees.size(); i += 5) {
    const double ex = level.trees[i].x - 40 * flat_x, ez = level.trees[i].z - 40 * flat_z;
    int count = 0;
    bool close = false;
    for (const auto& t : level.trees) {
      const double dx = t.x - ex, dz = t.z - ez;
      const double along = dx * flat_x + dz * flat_z, side = dx * flat_z - dz * flat_x;
      if (dx * dx + dz * dz < 400) close = true;
      if (along > 20 && along < 100 && std::fabs(side) < 15) ++count;
    }
    const double score = std::fabs(count - trees);
    if (!close && score < best && sees_sun(level, ex, level.ground_at(ex, ez) + 1.7, ez, v)) {
      best = score;
      v.eye[0] = ex;
      v.eye[2] = ez;
      found = true;
    }
  }
  if (!found) return false;
  v.eye[1] = level.ground_at(v.eye[0], v.eye[2]) + 1.7;
  // The sun off the middle, so the flare's ghosts spread along their line.
  v.heading = v.sun_azimuth + 0.25;
  v.pitch = v.sun_elevation * 0.6;
  return true;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double hour = lab.arg_num("hour");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = std::max(1, static_cast<int>(lab.arg_num("frames")));
  const int rounds = std::max(1, static_cast<int>(lab.arg_num("rounds")));
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
    lab.fail("--width and --height must be 1..4096", "next: pass --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fa::kMapVersion;
  const mg::Level level = mg::generate_level(seed, opts);
  gr::FrameView sun;
  fa::day_view(level, hour, sun);
  const bool sun_found = sun_view(level, lab.arg_num("trees"), sun);
  gr::FrameView start;
  fa::day_view(level, hour, start);
  fa::make_view(level, "start", start);
  if (!sun_found) {
    lab.fail("no spot in a wood looking into the sun on seed " + fixed(seed, 0),
             "next: try another --seed or --hour (the sun must be up).");
    return;
  }
  std::vector<Variant> all = variants();
  const std::string only = lab.arg("only");
  if (!only.empty()) {
    std::erase_if(all, [&](const Variant& v) {
      return v.name != "off" && ("," + only + ",").find("," + v.name + ",") == std::string::npos;
    });
  }
  const std::string seed_s = fixed(seed, 0), hour_s = fixed(hour, 1);
  auto shot_of = [&](const Variant& var, bool in_sun) {
    const std::string scene = in_sun ? "sun" : "start";
    return ge::lab::Shot{
        scene + "-" + var.name,
        (in_sun ? "INTO THE SUN THROUGH THE TREES, " : "THE START, ") + std::string("SEED ") +
            seed_s + " AT " + hour_s + " H: " + var.name,
        var.what,
        var.look_for,
        {{"seed", seed_s}, {"hour", hour_s}, {"scene", scene}, {"variant", var.name}}};
  };

  auto device = ge::gpu::create_device({.label = "post", .validation = 0});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Variant& var : all) {
      for (bool in_sun : {true, false}) {
        if (in_sun ? !var.sun : !var.start) continue;
        ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
        ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot(shot_of(var, in_sun), none, Det::measured);
      }
    }
    lab.metric("variants", static_cast<double>(all.size()), "count");
    lab.metric("gpu-errors", 0, "count");
    return;
  }
  const int settle = gr::taa_phases(1.0);

  // A variant's frame: settled on a scene, shot, then timed on the sun scene. Each variant is its
  // own frame on its own device (the switches are compiled in, and a device's pipelines are all
  // made before its first frame).
  const std::string adapter = device->report.name;
  uint32_t errors = 0;
  struct Timing {
    double total = 1e18, post = 1e18, lens = 1e18, tiles = 1e18, ao = 1e18, dof = 1e18;
  };
  std::vector<Timing> timing(all.size());
  for (int round = 0; round < rounds; ++round) {
    for (size_t i = 0; i < all.size(); ++i) {
      fa::FrameOptions options;
      all[i].set(options);
      device.reset();
      device = ge::gpu::create_device({.label = "post", .validation = 0});
      if (!device) {
        lab.fail("the GPU device could not be made again for `" + all[i].name + "`",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
      const ge::gpu::Target target = ge::gpu::create_target(
          *device, "post.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
      auto frame =
          fa::create_frame(*device, level, static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height), wgpu::TextureFormat::RGBA8Unorm, options);
      if (!frame) {
        lab.fail("Fall Line's frame did not build for `" + all[i].name + "`",
                 "Read the gpu: lines above. Skill: render.");
        return;
      }
      // A scene's frames: still, or with the eye moving (`advance`) a frame at a time.
      auto advance = [&](gr::FrameView& view) {
        if (!all[i].moving) return;
        const double over = view.eye[1] - level.ground_at(view.eye[0], view.eye[2]);
        view.eye[0] += kStride * ge::core::libm::sin(view.heading);
        view.eye[2] += kStride * ge::core::libm::cos(view.heading);
        view.eye[1] = level.ground_at(view.eye[0], view.eye[2]) + over;
        view.heading += kTurn;
      };
      auto still = [&](gr::FrameView& view) {
        fa::cut(*frame);
        for (int k = 0; k < settle; ++k) {
          view.frame = static_cast<uint64_t>(k);
          advance(view);
          fa::draw(*device, *frame, view, {}, target.view);
        }
      };
      if (round == 0) {
        for (bool in_sun : {true, false}) {
          if (in_sun ? !all[i].sun : !all[i].start) continue;
          gr::FrameView view = in_sun ? sun : start;
          still(view);
          lab.shot(shot_of(all[i], in_sun),
                   ge::lab::Picture{width, height, ge::gpu::read_now(*device, target).rgba},
                   Det::measured);
        }
      }
      gr::FrameView view = sun;
      still(view);
      std::vector<double> total, post, lens, tiles, ao, dof;
      for (int k = 0; k < frames; ++k) {
        view.frame = static_cast<uint64_t>(settle) + static_cast<uint64_t>(k);
        advance(view);
        fa::draw(*device, *frame, view, {}, target.view);
        frame->recorder.wait(*device);
        const gr::FrameStats& st = frame->recorder.stats;
        double sum = 0;
        for (int p = 0; p < st.count; ++p) {
          const gr::PassStat& pass = st.passes[static_cast<size_t>(p)];
          if (!std::isfinite(pass.gpu_ms)) continue;
          sum += pass.gpu_ms;
          if (pass.name == "post") post.push_back(pass.gpu_ms);
          if (pass.name == "lens") lens.push_back(pass.gpu_ms);
          if (pass.name == "motion_blur.tiles") tiles.push_back(pass.gpu_ms);
          if (pass.name == "ao") ao.push_back(pass.gpu_ms);
          if (pass.name == "dof") dof.push_back(pass.gpu_ms);
        }
        total.push_back(sum);
      }
      timing[i].total = std::min(timing[i].total, median(total));
      timing[i].post = std::min(timing[i].post, median(post));
      timing[i].lens = std::min(timing[i].lens, median(lens));
      timing[i].tiles = std::min(timing[i].tiles, median(tiles));
      timing[i].ao = std::min(timing[i].ao, median(ao));
      timing[i].dof = std::min(timing[i].dof, median(dof));
      frame.reset();
      errors += device->errors.load();
    }
  }
  ge::lab::LabTable table{
      "Each variant's GPU frame on the sun scene against every effect off, " + adapter + " at " +
          std::to_string(width) + "x" + std::to_string(height),
      {"variant", "frame ms", "over off ms", "post ms", "lens ms", "tiles ms", "ao ms", "dof ms"},
      {}};
  for (size_t i = 0; i < all.size(); ++i) {
    table.rows.push_back({all[i].name, fixed(timing[i].total, 2),
                          fixed(timing[i].total - timing[0].total, 2), fixed(timing[i].post, 2),
                          fixed(timing[i].lens, 2), fixed(timing[i].tiles, 2),
                          fixed(timing[i].ao, 2), fixed(timing[i].dof, 2)});
    lab.metric("cost." + all[i].name + "-ms", timing[i].total - timing[0].total, "ms",
               Det::measured);
  }
  lab.table(std::move(table), Det::measured);
  lab.metric("variants", static_cast<double>(all.size()), "count");
  lab.metric("gpu-errors", errors, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "post",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render post, lens and grade over Fall Line's frame",
    "render",
    "What does each post effect do to the picture, what do the defaults keep, and what does each "
    "cost?",
    {
        {"seed", "38", "the map's seed"},
        {"hour", "12", "the hour (the sun must be in view over the trees)"},
        {"trees", "2", "the trees between the eye and the sun the sun scene looks for"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"frames", "6", "frames timed a variant, after the history settles"},
        {"rounds", "1", "times every variant is timed in turn, keeping its least"},
        {"only", "", "the variants to run, by name, comma-separated (off always; empty: all)"},
    },
    "--width=160 --height=90 --frames=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
