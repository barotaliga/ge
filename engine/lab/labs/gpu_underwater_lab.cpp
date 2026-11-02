// LAB `gpu-underwater`: THE VIEW UNDER WATER (render/underwater.hpp), on the software adapter,
// in a small lake (a sandy bowl 9 m deep, its surface at 100 m) with white markers standing on
// its bed 2, 4, 8, 16 and 32 m ahead of an eye 6 m down (3 m over the bed). Named views: every
// named look from that eye (the fog in each water's own colour, the markers fading out by its
// visibility); the four presets (eyes open, a mask, murky, the classic tint and warp); the surface
// from below, looking straight up from 5 m down, where Snell's window is measured off the picture
// (more than 3 degrees from 48.6 fails); the caustics on the bed and the light shafts, each off
// beside on under a breeze (each must raise the picture's contrast); and a dive from 1 m over the
// surface to 3 m under it. It
// fails on a GPU error or a pipeline made after the load. Without an adapter it notes that, with no
// pictures.
//
// ge:summary Lab: under water: every look, the presets, Snell's window, caustics, shafts, a dive
// ge:keywords lab gpu underwater fog visibility snell window presets mask murky classic dive
// ge:keywords picture software-adapter caustics light-shafts god-rays
// ge:skill render water labs
// ge:test tests/render_water_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/underwater.hpp"
#include "water/optics.hpp"
#include "water/world.hpp"
#include "wind/field.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace gw = ge::water;
namespace lm = ge::core::libm;
using ge::lab::water_scene::draw;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::make_scene;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using ge::lab::water_scene::Scene;
using ge::lab::water_scene::view_at;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;

// THE POOL: a bowl 256 m across on 1 m cells, its bed at 91 m out to 90 m from the middle,
// the shore at 105 m; the markers on the bed ahead of the eye at (128, 60).
constexpr double kLevel = 100, kMid = 128, kEyeZ = 60;
constexpr double kMarkers[] = {2, 4, 8, 16, 32};

bool on_marker(double x, double z) {
  for (int i = 0; i < 5; ++i) {
    const double mx = kMid + (i % 2 == 0 ? -1.5 : 1.5), mz = kEyeZ + kMarkers[i];
    if (std::fabs(x - mx) < 0.6 && std::fabs(z - mz) < 0.6) return true;
  }
  return false;
}

float pool_ground(double x, double z) {
  const double dx = x - kMid, dz = z - kMid, r = std::sqrt(dx * dx + dz * dz);
  const double bed = r < 90 ? 91 : 91 + (r - 90) * 0.6;
  return static_cast<float>(bed + 0.3 * lm::sin(x / 7) * lm::cos(z / 11) +
                            (on_marker(x, z) ? 3.0 : 0.0));
}

gw::World pool_world(const std::string& look, double wind = 2) {
  ge::wind::Field field;
  field.mean = wind;
  field.from = 1.5 * kPi;
  gw::World world(field);
  std::vector<gw::Point> shore;
  for (int i = 0; i < 48; ++i) {
    const double a = 2 * kPi * i / 48;
    shore.push_back({kMid + 105 * lm::cos(a), kMid + 105 * lm::sin(a)});
  }
  gw::add_lake(world, shore, kLevel, {.look = look}, {}, 9);
  return world;
}

// White markers over the sand and grass.
void paint(ge::lab::water_scene::Ground& g) {
  const ge::core::Heightfield& f = g.field;
  for (int r = 0; r < f.rows; ++r) {
    for (int c = 0; c < f.cols; ++c) {
      if (!on_marker(c * f.cell, r * f.cell)) continue;
      const size_t i =
          (static_cast<size_t>(r) * static_cast<size_t>(f.cols) + static_cast<size_t>(c)) * 4;
      g.albedo[i] = g.albedo[i + 1] = g.albedo[i + 2] = 240;
    }
  }
}

double luma_at(const gl::Picture& p, int x, int y) {
  const size_t i =
      (static_cast<size_t>(y) * static_cast<size_t>(p.width) + static_cast<size_t>(x)) * 4;
  return 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
}

// The spread of the luma over a box of the picture against its mean (its standard deviation
// over the mean): how much a light pattern varies across it.
double contrast(const gl::Picture& p, int x0, int y0, int x1, int y1) {
  double sum = 0, sq = 0;
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const double l = luma_at(p, x, y);
      sum += l;
      sq += l * l;
      ++n;
    }
  }
  const double mean = sum / std::max(n, 1);
  return std::sqrt(std::max(sq / std::max(n, 1) - mean * mean, 0.0)) / std::max(mean, 1e-6);
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=640.");
    return;
  }
  for (const gw::NamedLook& l : gw::looks()) {
    lab.metric(std::string(l.name) + ".visibility", gw::optics(l.water).visibility, "m");
  }
  auto device = ge::gpu::create_device({.label = "gpu-underwater", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"looks", "presets", "window", "caustics", "shafts", "dive"}, w, h);
    return;
  }
  Scene s;
  if (!make_scene(*device, s, pool_ground, kLevel, 1, w, h, paint)) {
    lab.fail("the water's pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const gr::FrameView eye = view_at(kMid, kLevel - 6, kEyeZ, 0, -0.05, 0.8, 2.4, 2.0);
  const int tw = static_cast<int>(w / 2), th = static_cast<int>(h / 2);
  auto half = [&](const gl::Picture& p) {
    gl::Picture out = gl::create_picture(tw, th, {0, 0, 0});
    for (int y = 0; y < th; ++y) {
      for (int x = 0; x < tw; ++x) {
        for (int c = 0; c < 4; ++c) {
          int sum = 0;
          for (int k = 0; k < 4; ++k) {
            const int sx = 2 * x + (k & 1), sy = 2 * y + (k >> 1);
            sum += p.rgba[(static_cast<size_t>(sy) * static_cast<size_t>(p.width) +
                           static_cast<size_t>(sx)) *
                              4 +
                          static_cast<size_t>(c)];
          }
          out.rgba[(static_cast<size_t>(y) * static_cast<size_t>(tw) + static_cast<size_t>(x)) * 4 +
                   static_cast<size_t>(c)] = static_cast<uint8_t>(sum / 4);
        }
      }
    }
    return out;
  };
  auto set_look = [&](const std::string& look, double wind = 2) {
    gw::World world = pool_world(look, wind);
    if (s.water.bodies.empty()) {
      gr::add_water_body(*device, s.water, world.bodies[0]);
    } else {
      gr::set_water_body(*device, s.water, 0, world.bodies[0]);
    }
  };

  // THE LOOKS, eyes open.
  {
    const std::vector<std::string> names = {"nordic-brown", "nordic-clear",      "baltic",
                                            "adriatic",     "tropical-shallows", "glacial",
                                            "algal-pond",   "muddy-river",       "pool"};
    gl::Picture sheet = gl::create_picture(3 * tw + 40, 3 * (th + 14) + 10, {250, 250, 248});
    for (size_t i = 0; i < names.size(); ++i) {
      set_look(names[i]);
      const gl::Picture pic = draw(*device, s, eye);
      const int ox = 10 + static_cast<int>(i % 3) * (tw + 10),
                oy = 4 + static_cast<int>(i / 3) * (th + 14);
      const gw::Optics o = gw::optics(*gw::find_look(names[i]));
      gl::draw_text(sheet, ox, oy, names[i] + "  " + fixed(o.visibility, 1) + " M", {24, 26, 32});
      paste(sheet, half(pic), ox, oy + 10);
    }
    lab.shot({"looks",
              "UNDER THE SURFACE IN NINE LOOKS",
              "From 6 m down in a sandy bowl (3 m over its bed), white markers on the bed 2, 4, 8, "
              "16 and 32 m "
              "ahead, in each named look with the eyes open; the figure is the look's "
              "visibility (a black target lost).",
              "each water's own colour closing in: the brown lake amber and dark within a few "
              "metres, the clear lakes and the sea blue-green with the far markers fading, the "
              "tropical water clear to 32 m, the glacial milky, the mud blind; red gone first",
              {}},
             sheet, Det::measured);
  }

  // THE PRESETS, in the clear lake.
  {
    set_look("nordic-clear");
    const std::vector<std::string> presets = {"eyes-open", "mask", "murky", "classic"};
    gl::Picture sheet = gl::create_picture(2 * tw + 30, 2 * (th + 14) + 10, {250, 250, 248});
    for (size_t i = 0; i < presets.size(); ++i) {
      gr::underwater_look(presets[i], s.under.look);
      const gl::Picture pic = draw(*device, s, eye);
      const int ox = 10 + static_cast<int>(i % 2) * (tw + 10),
                oy = 4 + static_cast<int>(i / 2) * (th + 14);
      gl::draw_text(sheet, ox, oy, presets[i], {24, 26, 32});
      paste(sheet, half(pic), ox, oy + 10);
    }
    gr::underwater_look("eyes-open", s.under.look);
    lab.shot({"presets",
              "THE FOUR PRESETS",
              "The clear lake from the same eye: eyes open (the bare eye's blur by distance), a "
              "mask (sharp, a third larger), murky (three times the fog), and the classic "
              "stylised look (a flat tint and a screen warp, no blur, a long view).",
              "the near markers sharp and the far ones soft with the eyes open; all sharp and "
              "nearer with the mask; the view closing in at a few metres when murky; a flat "
              "green-blue tint and a wobble in the classic look",
              {}},
             sheet, Det::measured);
  }

  // SNELL'S WINDOW: straight up from 5 m down, the window's edge measured as the radius where the
  // mean luma round a ring about the centre crosses halfway between the window's and the mirror's
  // (the waves break the rim into bright and dark fragments, so one column would stop early).
  {
    set_look("tropical-shallows");
    gr::UnderwaterLook still = s.under.look;
    still.warp = 0;
    still.blur = 0;
    still.caustics = 0;  // the window's edge measured alone (the shafts and the net move its luma)
    still.shafts = 0;
    s.under.look = still;
    gr::FrameView up = view_at(kMid, kLevel - 5, kMid, 0, 1.5607, 0.9, 2.4, 2.0);
    up.fov_y = 2.2;
    const gl::Picture pic = draw(*device, s, up);
    const double cx = 0.5 * w, cy = 0.5 * h;
    const auto ring = [&](double r) {
      double sum = 0;
      for (int k = 0; k < 96; ++k) {
        const double a = 2 * kPi * k / 96;
        const int x = std::clamp(static_cast<int>(cx + r * lm::cos(a)), 0, static_cast<int>(w) - 1);
        const int y = std::clamp(static_cast<int>(cy + r * lm::sin(a)), 0, static_cast<int>(h) - 1);
        sum += luma_at(pic, x, y);
      }
      return sum / 96;
    };
    const double inside = ring(0.05 * h);
    const double outside = ring(0.49 * h);
    const double mid = 0.5 * (inside + outside);
    double half_px = 0.05 * h;
    while (half_px < 0.49 * h && (ring(half_px) - mid) * (inside - mid) > 0) half_px += 0.5;
    const double focal = 0.5 * h / lm::tan(0.5 * up.fov_y);
    const double angle = lm::atan(half_px / focal) * 180 / kPi;
    lab.metric("window.angle", angle, "deg", Det::measured);
    if (std::fabs(angle - 48.6) > 3) {
      lab.fail("Snell's window spans " + fixed(angle, 1) + " degrees from straight up, not 48.6",
               "Check the refraction out through the surface in engine/render/water.wgsl's "
               "fs_below (n = 1.333, the critical angle 48.6). Skill: render.");
    }
    gr::underwater_look("eyes-open", s.under.look);
    lab.shot({"window",
              "SNELL'S WINDOW",
              "Looking straight up from 5 m down in tropical water, a 126 degree lens: the sky "
              "and the world above seen only within 48.6 degrees of straight up; past it the "
              "surface mirrors the water below. Measured: " +
                  fixed(angle, 1) + " degrees.",
              "a bright round window, the shore's hills squeezed round its rim, the dark "
              "mirror of the water outside it",
              {{"angle", fixed(angle, 1) + " deg"}}},
             pic, Det::measured);
  }

  // THE CAUSTICS AND THE SHAFTS, each off beside on, in tropical water under a fresh breeze.
  const auto off_on = [&](const std::string& id, const gr::FrameView& v, bool shafts_view,
                          const std::string& title, const std::string& caption,
                          const std::string& look_for) {
    set_look("tropical-shallows", 6);
    gr::underwater_look("eyes-open", s.under.look);
    gl::Picture sheet = gl::create_picture(2 * tw + 30, th + 24, {250, 250, 248});
    double c[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
      s.under.look.caustics = shafts_view ? 0 : i;
      s.under.look.shafts = shafts_view ? i : 0;
      const gl::Picture pic = draw(*device, s, v);
      c[i] = contrast(pic, static_cast<int>(w / 4), static_cast<int>(h / 4),
                      static_cast<int>(3 * w / 4), static_cast<int>(3 * h / 4));
      const int ox = 10 + i * (tw + 10);
      gl::draw_text(sheet, ox, 4, i == 0 ? "OFF" : "ON", {24, 26, 32});
      paste(sheet, half(pic), ox, 14);
    }
    gr::underwater_look("eyes-open", s.under.look);
    lab.metric(id + ".contrast_off", c[0], "", Det::measured);
    lab.metric(id + ".contrast_on", c[1], "", Det::measured);
    if (!(c[1] > c[0] * 1.15)) {
      lab.fail("the " + id + " add no pattern: the picture's contrast " + fixed(c[1], 3) +
                   " with them against " + fixed(c[0], 3) + " without",
               "Check caustic_at in engine/render/wgsl/water_body.wgsl and its callers in "
               "engine/render/underwater.wgsl. Skill: render.");
    }
    lab.shot({id,
              title,
              caption + " Contrast " + fixed(c[0], 3) + " off, " + fixed(c[1], 3) + " on.",
              look_for,
              {}},
             sheet, Det::measured);
  };
  off_on("caustics", view_at(kMid, 93, kEyeZ, 0, -1.25, 1.0, 2.4, 2.0), false, "THE CAUSTICS",
         "Looking down at the sand from 2 m over it, 9 m under a 6 m/s breeze's waves in tropical "
         "water, the sun 57 degrees up: the light the waves focus on the bed, off and on.",
         "a net of bright wavering lines over the sand where the crests gather the sun, the "
         "darker sand between; the same sand evenly lit with them off");
  off_on("shafts", view_at(kMid, kLevel - 4, kEyeZ, 2.4, 0.55, 0.6, 2.4, 2.0), true, "THE SHAFTS",
         "From 4 m down looking up toward the sun (34 degrees up, so its way through the water "
         "leans), the same breeze: the light the waves focus scattered toward the eye, off and on.",
         "bright shafts fanning down from the surface toward the sun's side and the darker "
         "water between them; an even veil with them off");

  // THE DIVE: from 1 m over the surface to 3 m under it.
  {
    set_look("adriatic");
    const double heights[] = {1.0, -0.3, -1.0, -3.0};
    gl::Picture sheet = gl::create_picture(4 * tw + 50, th + 24, {250, 250, 248});
    for (int i = 0; i < 4; ++i) {
      const gr::FrameView v = view_at(kMid, kLevel + heights[i], kEyeZ, 0, -0.12, 0.8, 2.4, 2.0);
      const gl::Picture pic = draw(*device, s, v);
      const int ox = 10 + i * (tw + 10);
      gl::draw_text(sheet, ox, 4, fixed(heights[i], 1) + " M", {24, 26, 32});
      paste(sheet, half(pic), ox, 14);
    }
    lab.shot({"dive",
              "THE DIVE",
              "The same eye from 1 m over the sea's surface to 0.3, 1 and 3 m under it.",
              "the surface from above, then the world under it: the markers through the blue, "
              "the veil darker the deeper the eye",
              {}},
             sheet, Det::measured);
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
             "Add it to create_underwater's manifest. Skill: gpu, § Pipelines.");
  }
}

const gl::LabSpec kSpec{
    "gpu-underwater",
    gl::LabKind::picture,
    "engine",
    "engine/render/underwater.hpp",
    "render",
    "Does the view under water read as each water's own: its colour closing in by its "
    "visibility, the blur, Snell's window, the presets, the dive through the surface?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
    },
    "--width=128 --height=72",
    run,
};
GE_LAB(kSpec);

}  // namespace
