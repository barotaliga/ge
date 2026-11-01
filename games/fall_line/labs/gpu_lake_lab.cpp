// LAB `lake`: water in Fall Line's own frame (render/water.hpp's passes in
// games/fall_line/app/frame.cpp's table): a tarn laid on a seed's map, drawn with everything else
// the game draws (the snow, the trails, the forest, the sky, the cloud, TAA-U, the bloom, the
// grade), from the shore, from over it, into the sun, in a fresh wind, at the day's end, and
// from under its surface (render/underwater.hpp); and at DAWN under the air's own fog (M51c:
// mountain/app/mist_look.hpp's `water_mist`, render/mist_fog.hpp): a spring morning's air
// (`--spring`, the day's mean, C) at `--dawn`, still and clear, then in an 8 m/s wind. The still
// dawn must fog the tarn and the windy one must not: each is drawn with the air's fog and without
// it, and the lab fails when the still dawn's fog changes under 10 % of the picture or the windy
// one's over 3 %. Their mists' kinds, extinctions and depths are deterministic rows.
//
// FROZEN (M51f: mountain/air/marks.hpp's `lake_ice`, app/frame.hpp's `freeze_water`): the tarn
// after `--days` of winter at `--winter` C (the day's mean at the mountain's base, lapsed to the
// tarn), its ice drawn in place of the water (mountain/app/ice_look.hpp: snow on it, the wind's
// swept patches bare), and after one night at -4 C, young black ice. The lab fails when the
// winter's ice does not bear a skier (`kSkier` kg, Gold's formula), when the one night's does,
// or when either sheet changes under 5 % of the shore's picture against the open water.
//
// THE TARN is the lab's own until the generator lays lakes (docs/goals.md M24l): the flattest
// open ground of the map's lower half, away from the piste, is dug into a bowl `--radius` m
// across its shore, its rim made good where the ground falls away, the trees in it taken out,
// and a lake (water/world.hpp's `add_lake`, the bed the dug ground) filled to the rim. The
// wind is the map's own (mountain/air's `field_of`), its waves grown over the lake's fetch.
//
// Each shot is drawn twice, with the water and without it, and the share of pixels the water
// changed is measured: the lab fails when a view meant to show the lake shows none of it, on a
// GPU error, or on a pipeline made after the frame's load. Without an adapter it notes that and
// draws placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead.
// Built only with GE_GPU.
//
// ge:summary Lab: a lake in Fall Line's frame: shore, aerial, into the sun, wind, evening, dawn fog
// ge:keywords lab lake tarn water fall-line picture in-game reflection glitter shore frame
// ge:keywords dawn-fog lake-fog steam-fog radiation-fog water_mist
// ge:keywords lake-ice frozen-lake black-ice freeze_water lake_ice ice_bears
// ge:skill render water labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/air/air.hpp"
#include "mountain/air/marks.hpp"
#include "mountain/app/mist_look.hpp"
#include "mountain/mapgen/generate.hpp"
#include "water/optics.hpp"
#include "water/world.hpp"
#include "wind/cold.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;
namespace lm = ge::core::libm;
namespace gw = ge::water;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSkier = 90;  // kg: a skier and his gear, what the ice must bear

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

double ground(const mg::Level& level, double x, double z) {
  return ge::core::sample_field(level.ground, x, z);
}

struct Tarn {
  double x = 0, z = 0;  // the centre
  double radius = 0;    // the shore's, m
  double level = 0;     // the surface, m
  double relief = 0;    // the ground's spread over the site before it was dug, m
  double deepest = 0;   // the dug bowl's deepest water, m
};

// The flattest open site in the map's lower half, `radius` m round, its whole box clear of the
// piste by 25 m.
bool find_site(const mg::Level& level, double radius, Tarn& out) {
  const double edge = 2.2 * radius, reach = 1.5 * radius;
  const double low = level.mountain.base.y, high = level.mountain.summit.y;
  const double half = low + 0.5 * (high - low);
  double best = 1e30;
  const int steps = static_cast<int>((level.size - 2 * edge) / 16);
  for (int row = 0; row <= steps; ++row) {
    const double z = edge + 16.0 * row;
    for (int col = 0; col <= steps; ++col) {
      const double x = edge + 16.0 * col;
      const double y0 = ground(level, x, z);
      if (y0 > half) continue;
      bool clear = true;
      for (const mg::TrackPoint& p : level.track.points) {
        const double dx = p.x - x, dz = p.z - z;
        if (dx * dx + dz * dz < (reach + 25) * (reach + 25)) {
          clear = false;
          break;
        }
      }
      if (!clear) continue;
      double lo = 1e30, hi = -1e30;
      for (int j = -4; j <= 4; ++j) {
        for (int i = -4; i <= 4; ++i) {
          const double y = ground(level, x + reach * i / 4.0, z + reach * j / 4.0);
          lo = std::min(lo, y);
          hi = std::max(hi, y);
        }
      }
      // The flattest first; of two as flat, the lower (a tarn lies in the valley).
      const double score = (hi - lo) + 0.002 * (y0 - low);
      if (score < best) {
        best = score;
        out.x = x;
        out.z = z;
        out.relief = hi - lo;
      }
    }
  }
  out.radius = radius;
  return best < 1e29;
}

// Dig the tarn: the ground inside the shore a bowl `depth` m deep under the rim's lowest point
// (its floor shelving up over the outer 0.7 of the radius),
// the ground outside raised to just over the water out to its box's corners and blended back
// beyond, and the trees in the water taken out (those on the blend set on the new ground).
void dig(mg::Level& level, Tarn& t, double depth) {
  double rim = 1e30;
  for (int i = 0; i < 64; ++i) {
    const double a = 2 * kPi * i / 64;
    rim = std::min(rim, ground(level, t.x + t.radius * lm::cos(a), t.z + t.radius * lm::sin(a)));
  }
  t.level = rim;
  ge::core::Heightfield& g = level.ground;
  double deepest = 0;
  for (int row = 0; row < g.rows; ++row) {
    for (int col = 0; col < g.cols; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      const double r = std::sqrt((x - t.x) * (x - t.x) + (z - t.z) * (z - t.z)) / t.radius;
      if (r > 2) continue;
      float& y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      const double rimmed = t.level + 0.3 + 0.02 * t.radius * std::max(r - 1, 0.0);
      if (r < 1) {
        // A smooth floor: flat in the middle, easing up to the rim, so the shore shelves.
        const double s = std::clamp((1 - r) / 0.7, 0.0, 1.0);
        const double bowl = s * s * (3 - 2 * s);
        y = static_cast<float>(std::min<double>(y, rimmed - (0.3 + depth) * bowl));
        deepest = std::max(deepest, t.level - y);
      } else if (y < rimmed) {
        // Wholly raised to the box's corners (r = 1.42), blended back by r = 2.
        const double s = std::clamp((2 - r) / (2 - 1.45), 0.0, 1.0);
        const double w = s * s * (3 - 2 * s);
        y = static_cast<float>(y + (rimmed - y) * w);
      }
    }
  }
  t.deepest = deepest;
  std::erase_if(level.trees, [&](const mg::TreeDef& tree) {
    const double dx = tree.x - t.x, dz = tree.z - t.z;
    return dx * dx + dz * dz < (t.radius + 4) * (t.radius + 4);
  });
  for (mg::TreeDef& tree : level.trees) tree.y = ground(level, tree.x, tree.z);
}

double bed_height(const void* context, double x, double z) {
  return ground(*static_cast<const mg::Level*>(context), x, z);
}

gw::World tarn_world(const mg::Level& level, const Tarn& t, double wind, const std::string& look) {
  ge::wind::Field field = ge::mountain::air::field_of(level);
  if (wind >= 0) field.mean = wind;
  gw::World world(field);
  std::vector<gw::Point> shore;
  for (int i = 0; i < 64; ++i) {
    const double a = 2 * kPi * i / 64;
    shore.push_back({t.x + t.radius * lm::cos(a), t.z + t.radius * lm::sin(a)});
  }
  gw::add_lake(world, shore, t.level, {.look = look, .temperature = 4}, {bed_height, &level});
  return world;
}

// The share of a picture's pixels that differ from another's by more than a few levels.
double changed(const ge::lab::Picture& a, const ge::lab::Picture& b) {
  size_t n = 0;
  for (size_t i = 0; i < a.rgba.size(); i += 4) {
    int most = 0;
    for (size_t c = 0; c < 3; ++c)
      most = std::max(most, std::abs(int{a.rgba[i + c]} - int{b.rgba[i + c]}));
    n += most > 6;
  }
  return static_cast<double>(n) * 4 / static_cast<double>(a.rgba.size());
}

struct Shot {
  std::string id, title, caption, look_for;
  double hour;  // the day's hour (negative: the map's own)
  double wind;  // m/s (negative: the map's own)
  // 0 the shore facing the summit, 1 high over it, 2 the shore facing the sun, 3 under the
  // surface off the near shore, 4 under the middle looking up.
  int stand;
  double min_share;  // the water's least share of the picture
  // The air's fog over the tarn at a dawn (`--spring`'s day): 0 none drawn, 1 it must fog the
  // picture, 2 it must not.
  int air = 0;
  // The ice: `ice_days` of winter at `ice_mean` C (none where days is negative); 1 it must bear a
  // skier, 2 it must not.
  double ice_days = -1, ice_mean = 0;
  int bears = 0;
};

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double radius = lab.arg_num("radius");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const std::string& look = lab.arg("look");
  if (!gw::find_look(look)) {
    lab.fail("no water look `" + look + "`",
             "next: pass --look= one of water/optics.hpp's looks (adriatic, nordic-clear ...).");
    return;
  }
  if (radius < 20 || radius > 400 || width < 16 || height < 16 || width > 4096 || height > 4096 ||
      frames < 1 || frames > 120) {
    lab.fail("--radius must be 20..400 m, --width and --height 16..4096, --frames 1..120",
             "next: pass --radius=90 --width=640 --height=360 --frames=8.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  mg::Level level = mg::generate_level(seed, opts);
  level.weather.kind = mg::WeatherKind::clear;
  level.weather.snowfall = 0;
  level.weather.fog = 0;
  Tarn tarn;
  if (!find_site(level, radius, tarn)) {
    lab.fail("no open site " + fixed(radius, 0) + " m round clear of the piste on seed " +
                 fixed(seed, 0),
             "next: pass a smaller --radius, or another --seed.");
    return;
  }
  dig(level, tarn, 8);
  lab.metric("tarn.x", tarn.x, "m");
  lab.metric("tarn.z", tarn.z, "m");
  lab.metric("tarn.level", tarn.level, "m");
  lab.metric("tarn.relief", tarn.relief, "m");
  lab.metric("tarn.deepest", tarn.deepest, "m");
  const ge::wind::Field own = ge::mountain::air::field_of(level);
  lab.metric("wind.mean", own.mean, "m/s");

  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::string where = "a tarn " + fixed(2 * radius, 0) + " m across dug into seed " + seed_s +
                            "'s lower slopes (" + look + " water, 4 C)";
  const std::vector<Shot> shots = {
      {"shore", "FROM THE SHORE",
       "From 1.7 m over the near shore of " + where +
           ", looking across it to the summit, under "
           "the map's own wind (" +
           fixed(own.mean, 1) + " m/s) at the map's hour.",
       "the far shore and the mountain mirrored, broken by the wind's ripples; the near water "
       "seen into, turquoise over the dug bed; no hard edge where the snow meets the water",
       -1, -1, 0, 0.1},
      {"aerial", "FROM OVER THE SHORE",
       "From 45 m over the ground behind the near shore, looking down over the tarn and on up "
       "the mountain.",
       "the whole tarn sitting in the snow, its colour deepening toward the middle, the sky's "
       "light across it",
       -1, -1, 1, 0.05},
      {"into-sun", "INTO THE SUN",
       "From the shore opposite the sun, looking across the tarn toward it.",
       "a path of glitter on the water under the sun, the far water a sheet of light, the "
       "snow on the far shore in shadow",
       -1, -1, 2, 0.1},
      {"fresh", "A FRESH WIND",
       "The shore view under 12 m/s: the short sea that wind grows over the tarn's fetch.",
       "the mirror broken into short choppy waves, darker where their faces turn from the sky", -1,
       12, 0, 0.1},
      {"evening", "THE DAY'S END", "The shore view at 16:30, the sun low over the mountains.",
       "the warm low light on the snow, the water darker and bluer, the mirror of the lit "
       "slopes",
       16.5, -1, 0, 0.1},
      {"under", "UNDER THE SURFACE",
       "From 2 m under the tarn's surface over the near shelf, looking out across "
       "its dug bed.",
       "the water's own colour closing in by its visibility, the shelving bed "
       "fading into it, the surface's mirror overhead",
       -1, -1, 3, 0.3},
      {"under-up", "THE SURFACE FROM BELOW",
       "From 2.5 m down in the middle of the tarn, looking up toward the far shore.",
       "Snell's window: the sky and the snowy slopes seen through a bright circle, the "
       "water mirrored past it",
       -1, -1, 4, 0.3},
      {"dawn-still", "A STILL, CLEAR DAWN",
       "The shore view at dawn on a spring morning, still and clear, under the fog the air lays "
       "over the tarn.",
       "a bank of mist lying on the water, the far shore faint through it, the slopes above it "
       "clear in the low sun",
       lab.arg_num("dawn"), 0.3, 0, 0.05, 1},
      {"dawn-windy", "A WINDY DAWN", "The same dawn in an 8 m/s wind: the air's fog over the tarn.",
       "no fog: the wind mixes it away, the far shore sharp across the chop", lab.arg_num("dawn"),
       8, 0, 0.05, 2},
      {"frozen", "FROZEN",
       "The shore view after " + fixed(lab.arg_num("days"), 0) + " days of winter at " +
           fixed(lab.arg_num("winter"), 0) + " C: the tarn under its ice.",
       "a flat white sheet where the water was, the snow lying on it, patches of bare glassy "
       "ice the wind swept catching the light; no mirror of the slopes, no ripples",
       -1, -1, 0, 0.05, 0, lab.arg_num("days"), lab.arg_num("winter"), 1},
      {"thin-ice", "ONE COLD NIGHT",
       "The shore view after a single night at -4 C: young ice, too thin to bear a skier.",
       "black, glassy ice where the water was, the sky's light sliding across it; no snow on "
       "it, no ripples",
       -1, -1, 0, 0.05, 0, 0.5, -4, 2},
  };

  auto shot_of = [&](const Shot& s) {
    return ge::lab::Shot{
        s.id,
        s.title,
        s.caption,
        s.look_for,
        {{"seed", seed_s},
         {"radius", fixed(radius, 0) + " m"},
         {"wind", s.wind < 0 ? fixed(own.mean, 1) + " m/s (the map's)" : fixed(s.wind, 1) + " m/s"},
         {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  // The dawn shots' fog off the air (deterministic rows, with a GPU or without).
  std::vector<ge::mountain::app::WaterMist> mists(shots.size());
  for (size_t k = 0; k < shots.size(); ++k) {
    const Shot& s = shots[k];
    if (s.air == 0) continue;
    ge::wind::Climate c = ge::mountain::air::climate_of(level);
    c.mean_c = lab.arg_num("spring");
    c.wind = s.wind;
    c.cloud = 0.05;
    ge::mountain::app::MapWater w;
    w.centre[0] = tarn.x;
    w.centre[1] = tarn.z;
    w.half[0] = w.half[1] = tarn.radius;
    w.level = tarn.level;
    w.water_c = 4;
    const ge::mountain::app::WaterMist& wm = mists[k] =
        ge::mountain::app::water_mist(level, c, s.hour, w);
    lab.metric(s.id + ".mist", static_cast<double>(wm.mist.kind), "kind");
    lab.metric(s.id + ".mist-density", wm.area.density, "1/m");
    lab.metric(s.id + ".mist-depth", 2 * wm.area.half[1], "m");
    lab.note(s.id + ": " + ge::wind::mist_name(wm.mist.kind) + " fog, " +
             fixed(wm.mist.visibility, 0) + " m visibility, " + fixed(wm.mist.depth, 1) +
             " m deep");
  }

  // The ice on the frozen shots (deterministic rows, with a GPU or without).
  std::vector<double> ice(shots.size(), 0);
  for (size_t k = 0; k < shots.size(); ++k) {
    const Shot& s = shots[k];
    if (s.ice_days < 0) continue;
    mg::Level winter = level;
    winter.weather.mean_c = s.ice_mean;
    ice[k] = ge::mountain::air::lake_ice(winter, tarn.level, s.ice_days);
    const double bears = ge::wind::ice_bears(ice[k]);
    lab.metric(s.id + ".ice", ice[k], "m");
    lab.metric(s.id + ".bears", bears, "kg");
    lab.note(s.id + ": " + fixed(ice[k] * 100, 1) + " cm of ice, bears " + fixed(bears, 0) + " kg");
    if (s.bears == 1 && bears < kSkier) {
      lab.fail(s.id + "'s ice (" + fixed(ice[k] * 100, 1) + " cm) does not bear a skier",
               "Check wind/cold.cpp's ice_grown and mountain/air/marks.cpp's lake_ice. Skill: "
               "mountain.");
    }
    if (s.bears == 2 && bears >= kSkier) {
      lab.fail(s.id + "'s one night of ice (" + fixed(ice[k] * 100, 1) + " cm) bears a skier",
               "Check wind/cold.cpp's ice_grown. Skill: mountain.");
    }
  }

  auto device = ge::gpu::create_device({.label = "lake", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Shot& s : shots) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(s), none, Det::measured);
    }
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  auto frame =
      fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height), wgpu::TextureFormat::RGBA8Unorm);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "lake.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  const mg::Mountain& m = level.mountain;
  // Away from the summit across the tarn, and the sun's bearing at the map's hour.
  const double up = lm::atan2(m.summit.x - tarn.x, m.summit.z - tarn.z);
  ge::render::FrameView day;
  fall_line::app::day_view(level, -1, day);
  int body = -1;
  double wind_now = -2;
  auto draw = [&](const ge::render::FrameView& v, const fall_line::app::FrameScene& scene) {
    fall_line::app::cut(*frame);
    ge::render::FrameView at = v;
    for (int n = 0; n < frames; ++n) {
      at.frame = static_cast<uint64_t>(n);
      at.time_s = 30 - static_cast<double>(frames - 1 - n) / 60;
      fall_line::app::draw(*device, *frame, at, scene, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    return ge::lab::Picture{width, height, img.rgba};
  };
  for (const Shot& s : shots) {
    if (s.wind != wind_now) {
      const gw::World world = tarn_world(level, tarn, s.wind, look);
      if (body < 0) {
        body = fall_line::app::add_water(*device, *frame, world.bodies[0]);
      } else {
        fall_line::app::set_water(*device, *frame, body, world.bodies[0]);
      }
      wind_now = s.wind;
    }
    ge::render::FrameView v;
    fall_line::app::day_view(level, s.hour, v);
    double toward = up;
    double back = tarn.radius + 3, over = 1.7;
    if (s.stand == 1) {
      back = tarn.radius * 1.9;
      over = 45;
    } else if (s.stand == 2) {
      toward = v.sun_azimuth;
    }
    if (s.stand == 3) {
      back = tarn.radius * 0.6;
    } else if (s.stand == 4) {
      back = 0;
    }
    const double ex = tarn.x - back * lm::sin(toward), ez = tarn.z - back * lm::cos(toward);
    v.eye[0] = ex;
    v.eye[2] = ez;
    v.eye[1] = std::max(ground(level, ex, ez), tarn.level) + over;
    v.heading = toward;
    v.pitch = s.stand == 1 ? -lm::atan2(v.eye[1] - tarn.level, back) * 0.75 : -0.06;
    if (s.stand == 3) {
      v.eye[1] = tarn.level - 2;
      v.pitch = -0.1;
    } else if (s.stand == 4) {
      v.eye[1] = tarn.level - 2.5;
      v.pitch = 1.0;
    }
    // The air's fog over the tarn at a dawn; any other shot draws the frame's own.
    fall_line::app::FrameScene scene;
    std::vector<ge::render::FogVolume> fog;
    if (s.air != 0) {
      const ge::mountain::app::WaterMist& wm = mists[&s - shots.data()];
      // Without it: the same volume emptied, so the frame deals none of the map's own either.
      fog = {wm.area};
      fog[0].density = 0;
      scene.fog = fog;
      scene.hour = s.hour;
      const ge::lab::Picture clear = draw(v, scene);
      fog[0].density = wm.area.density;
      const ge::lab::Picture fogged = draw(v, scene);
      const double share = changed(fogged, clear);
      lab.metric(s.id + ".fog-share", share, "share", Det::measured);
      if (s.air == 1 && share < 0.1) {
        lab.fail(
            "the still dawn's fog changed " + fixed(share * 100, 1) + "% of the picture, under 10%",
            "Check wind/mist.cpp's radiation fog by water and render/mist_fog.cpp's bank. "
            "Skill: render.");
      }
      if (s.air == 2 && share > 0.03) {
        lab.fail(
            "the windy dawn's fog changed " + fixed(share * 100, 1) + "% of the picture, over 3%",
            "Check wind/mist.cpp's mixing in the wind and the forming fog's range. "
            "Skill: render.");
      }
    }
    if (s.ice_days >= 0) {
      // The ice against the open water from the same stand.
      const gw::World world = tarn_world(level, tarn, s.wind, look);
      const ge::lab::Picture open = draw(v, scene);
      fall_line::app::freeze_water(*device, *frame, body, world.bodies[0], ice[&s - shots.data()]);
      const ge::lab::Picture frozen = draw(v, scene);
      fall_line::app::freeze_water(*device, *frame, body, world.bodies[0], 0);
      const double share = changed(frozen, open);
      lab.metric(s.id + ".ice-share", share, "share", Det::measured);
      if (share < s.min_share) {
        lab.fail("the " + s.id + " view's ice changed " + fixed(share * 100, 1) +
                     "% of the picture against the open water",
                 "Check mountain/app/ice_look.cpp's sheet and app/frame.cpp's freeze_water. "
                 "Skill: render.");
      }
      lab.shot(shot_of(s), frozen, Det::measured);
      continue;
    }
    const ge::lab::Picture pic = draw(v, scene);
    frame->water.bodies[static_cast<size_t>(body)].hidden = true;
    const ge::lab::Picture dry = draw(v, scene);
    frame->water.bodies[static_cast<size_t>(body)].hidden = false;
    const double share = changed(pic, dry);
    lab.metric(s.id + ".water-share", share, "share", Det::measured);
    if (share < s.min_share) {
      lab.fail("the " + s.id + " view shows little of the tarn (" + fixed(share * 100, 1) +
                   "% of the picture changed by the water)",
               "Check prepare_water's levels and the body's box (engine/render/water.cpp) and "
               "the view's stand in games/fall_line/labs/gpu_lake_lab.cpp. Skill: render.");
    }
    lab.shot(shot_of(s), pic, Det::measured);
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
    "lake",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/water.hpp's surface passes in games/fall_line/app/frame.cpp's pass table, "
    "over a tarn laid on a seed's map (water/world.hpp's add_lake)",
    "render",
    "Does water read as water in the game's own picture: the mirror, the colour seen into, the "
    "shore in the snow, the glitter, the wind's waves, the evening light?",
    {
        {"seed", "38", "the map's seed"},
        {"radius", "90", "the tarn's radius, m"},
        {"look", "adriatic", "the water's look (water/optics.hpp): a clear tarn"},
        {"frames", "8", "frames drawn, at 60 a second, before each shot (TAA-U settles)"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"spring", "3", "the dawn shots' day: its mean temperature, C (the tarn at 4 C)"},
        {"dawn", "7.6", "the dawn shots' solar hour"},
        {"winter", "-8", "the frozen shot's winter: the day's mean at the mountain's base, C"},
        {"days", "60", "the frozen shot's winter: its days"},
    },
    "--frames=2 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
