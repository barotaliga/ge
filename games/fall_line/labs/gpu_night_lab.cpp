// LAB `night`: the night in the picture (render/night.hpp over the moon on the map's date,
// plugins/mountain/app/night_look.hpp): a seed's map from a named view's eye at dusk, as the sun
// goes 3, 8 and 14 degrees under on its own day, looking toward it, and the minute either side of
// the moon taking the key light from the twilight; then under a FULL moon, a
// first QUARTER and a NEW moon (the nearest days to the map's own with the moon so, each at the
// hour after dark its moon stands highest; the new moon's at midnight), once as the view looks,
// over the snow, and once at the sky: at the moon through a long lens (its phase, its bright
// limb toward the sun, its earthlit dark part), or, with no moon, up into the south.
//
// Beside each: where the sun and the moon stand and how much of the moon is lit (deterministic,
// core/solar.hpp's), and, measured, the moon's light on the ground and the twilight's (which of
// them keys the picture), the snow's mean luma and the sky's, and the stars seen (the sky's
// pixels well over every pixel round them). The lab fails when the full moon's snow is no brighter
// than the quarter's, the quarter's no brighter than the new moon's, the dusk does not darken,
// the snow jumps more than a fifth as the moon takes the key, or the new moon's sky shows no
// stars. Without an adapter it notes that and draws
// placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built
// only with GE_GPU.
//
// ge:summary Lab: a map at dusk and under a full, a quarter and a new moon: the moon, the stars
// ge:keywords lab night moon moonlight full-moon new-moon quarter dusk twilight stars key-light
// ge:keywords fall-line picture night-sky adaptation
// ge:skill render labs
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/solar.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/night_look.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/sun.hpp"
#include "render/night.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;

constexpr double kDeg = ge::core::kPi / 180.0;

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

// Where a shot looks from the view's eye: as the view looks (down over the snow), toward the
// sun under the horizon (the dusk's glow), at the moon through a long lens, or up into the south.
enum class Aim : std::uint8_t { view, sun, moon, up };

// One shot: the map's day of the year and the solar hour (past 24: the small hours after).
struct Night {
  std::string id;
  double day = 0;
  double hour = 0;
  Aim aim = Aim::view;
  const char* look = "";
};

double sun_elevation(const mg::Level& level, double day, double hour) {
  return ge::core::sun_at(hour, level.sun.latitude, mg::declination_of(day)).elevation;
}

// The nearest day to `day` whose moon at midnight is lit as `want` says (and waxing, when
// `waxing`).
double moon_day(const mg::Level& level, double day, bool (*want)(double lit, bool waxing)) {
  for (int k = 0; k < 60; ++k) {
    for (int sgn : {1, -1}) {
      const double d = day + sgn * k;
      if (d < 1 || d > 365) continue;
      mg::Level l = level;
      l.sun.day_of_year = d;
      const double lit = ge::mountain::app::moon_of(l, 24).lit;
      l.sun.day_of_year = d + 1;
      const double next = ge::mountain::app::moon_of(l, 24).lit;
      if (want(lit, next > lit)) return d;
    }
  }
  return day;
}

// The hour after dark (the sun 12 degrees under) its moon stands highest, every quarter hour
// from 17 to 31.
double moon_hour(const mg::Level& level, double day) {
  mg::Level l = level;
  l.sun.day_of_year = day;
  double best = 24, high = -10;
  for (int k = 0; k <= 56; ++k) {
    const double h = 17 + k * 0.25;
    if (sun_elevation(level, day, h) > -12 * kDeg) continue;
    const double e = ge::mountain::app::moon_of(l, h).elevation;
    if (e > high) {
      high = e;
      best = h;
    }
  }
  return best;
}

// A pixel's luma (0..255).
double luma_at(const ge::lab::Picture& p, int x, int y) {
  const size_t i =
      (static_cast<size_t>(y) * static_cast<size_t>(p.width) + static_cast<size_t>(x)) * 4;
  return 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
}

// The picture's mean luma (0..255) over its rows from `y0` to `y1`, and the stars seen there:
// pixels 25 levels or more over every pixel two away.
void luma_stats(const ge::lab::Picture& p, int y0, int y1, double& mean, int& stars) {
  double sum = 0;
  int n = 0;
  stars = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = 0; x < p.width; ++x) {
      const double l = luma_at(p, x, y);
      sum += l;
      ++n;
      if (x < 2 || y < 2 || x + 2 >= p.width || y + 2 >= p.height) continue;
      const double around = std::max(std::max(luma_at(p, x - 2, y), luma_at(p, x + 2, y)),
                                     std::max(luma_at(p, x, y - 2), luma_at(p, x, y + 2)));
      if (l > around + 25) ++stars;
    }
  }
  mean = n ? sum / n : 0;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& view_name = lab.arg("view");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (frames < 1 || frames > 600 || width <= 0 || height <= 4 || width > 4096 || height > 4096) {
    lab.fail("--frames must be 1..600, --width 1..4096 and --height 5..4096",
             "next: pass --frames=8 --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level level = mg::generate_level(seed, opts);
  ge::render::FrameView probe;
  fall_line::app::day_view(level, -1, probe);
  if (!fall_line::app::make_view(level, view_name, probe)) {
    lab.fail("no view `" + view_name + "`",
             "Pass --view= one of games/fall_line/app/views.cpp's names. Skill: render.");
    return;
  }
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  // THE NIGHTS: the dusk on the map's own day, then the three moons.
  const double day = level.sun.day_of_year;
  const double lat = level.sun.latitude;
  const double dec = mg::declination_of(day);
  std::vector<Night> nights;
  for (double under : {3.0, 8.0, 14.0}) {
    const std::optional<double> h = ge::core::hour_of_elevation(-under * kDeg, false, lat, dec);
    nights.push_back({"dusk-" + std::to_string(static_cast<int>(under)), day, h ? *h : 18.0,
                      Aim::sun,
                      under < 5    ? "the afterglow: a bright west, the snow still lit by the sky"
                      : under < 10 ? "the twilight's last: a deep blue sky, the snow dimming"
                                   : "night falling: the snow grey, the stars coming out"});
  }
  // THE HANDOVER: the minute the moon takes the key from the twilight on the map's day (render/
  // night.hpp decides it, in floating point, so its hour is measured), shot the minute before
  // and the minute after. With no moon up through the dusk, both at the sun 10 degrees under.
  double before = -1;
  {
    const std::optional<double> from = ge::core::hour_of_elevation(-3 * kDeg, false, lat, dec);
    const double start = from ? *from : 17.0;
    for (int k = 0; k < 180; ++k) {
      const double h = start + k / 60.0;
      ge::render::FrameView v;
      fall_line::app::day_view(level, h + 1.0 / 60, v);
      if (v.night.moon_key) {
        before = h;
        break;
      }
    }
    if (before < 0) {
      const std::optional<double> h = ge::core::hour_of_elevation(-10 * kDeg, false, lat, dec);
      before = h ? *h : 18.0;
    }
  }
  nights.push_back(
      {"handover-before", day, before, Aim::sun, "the last minute the twilight keys the picture"});
  nights.push_back({"handover-after", day, before + 1.0 / 60, Aim::sun,
                    "the first minute the moon keys it: the same picture, no jump"});
  const double full = moon_day(level, day, [](double lit, bool) { return lit > 0.97; });
  const double quarter = moon_day(
      level, day, [](double lit, bool waxing) { return waxing && lit > 0.45 && lit < 0.55; });
  const double none = moon_day(level, day, [](double lit, bool) { return lit < 0.03; });
  const double full_h = moon_hour(level, full), quarter_h = moon_hour(level, quarter);
  nights.push_back({"full", full, full_h, Aim::view,
                    "a full moon high: the snow a pale blue grey with the moon's shadows"});
  nights.push_back({"quarter", quarter, quarter_h, Aim::view,
                    "a half moon: the snow darker than under the full moon"});
  nights.push_back({"new", none, 24.0, Aim::view, "no moon: the snow a dim grey, starlit"});
  nights.push_back({"full-sky", full, full_h, Aim::moon,
                    "the full moon's round disc, its grey seas, bright to its edge"});
  nights.push_back({"quarter-sky", quarter, quarter_h, Aim::moon,
                    "the half moon, its bright limb toward the set sun, its dark half faintly "
                    "earthlit"});
  nights.push_back({"new-sky", none, 24.0, Aim::up,
                    "a sky full of stars, brighter and fewer toward the horizon, and the galaxy's "
                    "band"});

  std::vector<mg::Level> levels;
  for (const Night& n : nights) {
    mg::Level l = level;
    l.sun.day_of_year = n.day;
    const ge::mountain::app::MapMoon m = ge::mountain::app::moon_of(l, n.hour);
    const Det det = n.id.rfind("handover", 0) == 0 ? Det::measured : Det::deterministic;
    lab.metric(n.id + ".day", n.day, "day");
    lab.metric(n.id + ".hour", n.hour, "h", det);
    lab.metric(n.id + ".sun-elevation", sun_elevation(l, n.day, n.hour) / kDeg, "deg", det);
    lab.metric(n.id + ".moon-elevation", m.elevation / kDeg, "deg", det);
    lab.metric(n.id + ".moon-lit", m.lit, "share", det);
    levels.push_back(std::move(l));
  }

  // From the view's eye, turned toward the light: the dusk toward the sun under the horizon, a
  // moon toward it with its disc in the top of the frame, the new moon's sky to the south.
  auto view_of = [&](size_t i) {
    ge::render::FrameView v;
    fall_line::app::day_view(levels[i], nights[i].hour, v);
    fall_line::app::make_view(levels[i], view_name, v);
    const ge::render::Night& n = v.night;
    if (nights[i].aim == Aim::up) {
      v.heading = ge::core::kSouth;
      v.pitch = 20 * kDeg;
    } else if (nights[i].aim == Aim::sun) {
      v.heading = n.moon_key ? n.sun_azimuth : v.sun_azimuth;
      v.pitch = 2 * kDeg;
    } else if (nights[i].aim == Aim::moon) {
      // A long lens on the moon: its disc some 40 px across in a 640 x 360 picture.
      v.heading = n.moon_azimuth;
      v.pitch = n.moon_elevation;
      v.fov_y = 5 * kDeg;
    }
    return v;
  };
  auto shot_of = [&](size_t i) {
    const Night& n = nights[i];
    const ge::render::FrameView v = view_of(i);
    std::string title = n.id + ", SEED " + seed_s + " " + view_name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const double sun = v.night.moon_key ? v.night.sun_elevation : v.sun_elevation;
    const std::string caption =
        "Seed " + seed_s + "'s `" + view_name + "` view on day " + fixed(n.day, 0) + " at " +
        fixed(n.hour, 2) + " h: the sun " + fixed(-sun / kDeg, 1) + " degrees under, the moon " +
        fixed(v.night.moon_elevation / kDeg, 1) + " degrees up and " +
        fixed(levels.size() ? ge::mountain::app::moon_of(levels[i], n.hour).lit * 100 : 0, 0) +
        " % lit; the key light the " + (v.night.moon_key ? "moon" : "sun") + ".";
    return ge::lab::Shot{n.id,
                         title,
                         caption,
                         n.look,
                         {{"seed", seed_s},
                          {"night", n.id},
                          {"view", view_name},
                          {"day", fixed(n.day, 0)},
                          {"hour", fixed(n.hour, 2)},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "night", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (size_t i = 0; i < nights.size(); ++i) {
      ge::lab::Picture blank = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(blank, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(i), blank, Det::measured);
    }
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
      *device, "night.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  std::vector<double> snow(nights.size());
  std::vector<int> seen(nights.size());
  for (size_t i = 0; i < nights.size(); ++i) {
    ge::render::FrameView v = view_of(i);
    for (int n = 0; n < frames; ++n) {
      v.frame = static_cast<uint64_t>(i * 1000 + static_cast<size_t>(n));
      v.time_s = static_cast<double>(n) / 60;
      fall_line::app::draw(*device, *frame, v, {}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    const ge::lab::Picture pic{width, height, img.rgba};
    lab.shot(shot_of(i), pic, Det::measured);
    double sky = 0;
    int stars = 0, ground_stars = 0;
    // The sky is the top third of the frame; the snow its bottom fifth.
    luma_stats(pic, 0, height / 3, sky, stars);
    luma_stats(pic, height * 4 / 5, height, snow[i], ground_stars);
    seen[i] = stars;
    const std::string k = nights[i].id;
    const ge::render::Night& night = v.night;
    const double moon_lux = ge::render::moonlight_on_ground(
        ge::render::moon_lux(
            ge::render::phase_angle(ge::mountain::app::moon_of(levels[i], nights[i].hour).lit),
            ge::mountain::app::moon_of(levels[i], nights[i].hour).distance_km),
        night.moon_elevation);
    const double sun = night.moon_key ? night.sun_elevation : v.sun_elevation;
    lab.metric(k + ".moon-lux", moon_lux, "lx", Det::measured);
    lab.metric(k + ".twilight-lux", ge::render::twilight_lux(sun), "lx", Det::measured);
    lab.metric(k + ".moon-key", night.moon_key ? 1 : 0, "flag", Det::measured);
    lab.metric(k + ".snow-luma", snow[i], "level", Det::measured);
    lab.metric(k + ".sky-luma", sky, "level", Det::measured);
    lab.metric(k + ".stars", stars, "count", Det::measured);
  }
  // THE TWILIGHT: the sky tables' own light on a level face with the sun a degree at a time
  // under the horizon (no moon), against render/night.hpp's twilight_lux, which the
  // pre-exposure and the haze's choice read.
  ge::lab::LabTable twilight{
      "twilight: the light on a level face, the sky tables' and twilight_lux's",
      {"sun-deg", "tables-lx", "model-lx", "ratio"},
      {}};
  int off = 0;
  for (int deg = 1; deg <= 15; ++deg) {
    ge::render::FrameView v = view_of(0);
    v.night = {};
    v.sun_elevation = -deg * kDeg;
    v.sun_illuminance = 1;
    v.frame = 0;
    fall_line::app::draw(*device, *frame, v, {}, target.view);
    const ge::render::SkyLightReading r = ge::render::read_sky_light(*device, frame->sky);
    const double tables =
        (0.2126 * r.irradiance_up[0] + 0.7152 * r.irradiance_up[1] + 0.0722 * r.irradiance_up[2]) *
        ge::render::kSunLux;
    const double model = ge::render::twilight_lux(v.sun_elevation);
    const double ratio = tables / model;
    twilight.rows.push_back(
        {std::to_string(-deg), fixed(tables, 5), fixed(model, 5), fixed(ratio, 2)});
    if (model > 1e-3 && (ratio < 0.67 || ratio > 1.5)) ++off;
  }
  lab.table(twilight, Det::measured);
  if (off > 0) {
    lab.fail(std::to_string(off) +
                 " degrees of twilight where the model is off the sky tables' "
                 "light by more than half",
             "Refit kLog in engine/render/night.cpp's twilight_lux to the `twilight` table's "
             "tables-lx column. Skill: render.");
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
  // The dusk darkens; the full moon's snow outshines the quarter's, the quarter's the new moon's;
  // a moonless sky shows its stars.
  // The handover: the snow within a fifth of its brightness across the minute.
  const double jump = std::fabs(snow[4] - snow[3]) / std::max(snow[3], 1.0);
  lab.metric("handover.jump", jump, "share", Det::measured);
  if (jump > 0.2) {
    lab.fail("the picture jumps " + fixed(jump * 100, 0) + " % as the moon takes the key (snow " +
                 fixed(snow[3], 1) + " then " + fixed(snow[4], 1) + ")",
             "Check twilight_lux in engine/render/night.cpp against the sky table's twilight. "
             "Skill: render.");
  }
  if (!(snow[0] > snow[1] && snow[1] > snow[2])) {
    lab.fail("the dusk does not darken (snow " + fixed(snow[0], 1) + ", " + fixed(snow[1], 1) +
                 ", " + fixed(snow[2], 1) + ")",
             "Check exposure's adaptation in engine/render/wgsl/frame.wgsl. Skill: render.");
  }
  if (!(snow[5] > snow[6] && snow[6] > snow[7])) {
    lab.fail("the moons do not order the snow's brightness (full " + fixed(snow[5], 1) +
                 ", quarter " + fixed(snow[6], 1) + ", new " + fixed(snow[7], 1) + ")",
             "Check set_night in engine/render/night.cpp and exposure in "
             "engine/render/wgsl/frame.wgsl. Skill: render.");
  }
  if (seen[10] == 0 && height >= 90) {
    lab.fail("the new moon's sky shows no stars",
             "Check stars in engine/render/sky.wgsl. Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "night",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/night.hpp (the moon, the key light, the dark sky) and sky.wgsl's moon and "
    "stars, over the moon on the map's date (plugins/mountain/app/night_look.hpp), in Fall "
    "Line's frame",
    "render",
    "Does the night read as night: the dusk darkening, a full moon's pale snow and shadows, a "
    "moonless night's dim snow under its stars?",
    {
        {"seed", "38", "the map's seed"},
        {"view", "vista", "the named view to shoot from"},
        {"frames", "8", "frames drawn, at 60 a second, for each shot"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=2 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
