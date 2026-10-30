// LAB `storm-pass`: A THUNDERSTORM PASSING OVER A SEED'S MOUNTAIN in Fall Line (docs/storms.md §
// Fall Line's storms): the map's sky picked by hand with a storm of --storm (sv_storm), its cells
// laid off the map's seed (mountain/air/air.hpp's `storm_of`), and what the run gets of it.
//
// - THE PASS (`pass`, a chart, deterministic): at the mountain's middle, every 15 s through the
//   first half hour, the wind at 10 m with the gust front in it, the fall (the storm's rain on the
//   dial), the share of it that is rain at the base and at the summit, how far the freezing level
//   has fallen under the downdraft, and the flashes a minute within 15 km (the strikes apart).
// - THE GAME'S FRAMES (`approach`, `front`, `downpour`, `strike`, `after`): the named view drawn
//   by the game's own frame, which lays the storm, chooses the lightning's lamps and channels and
//   the towers itself (app/frame.hpp): the storm coming in, the gust front at the eye, the rain
//   core, the first strike 2 to 6 km out in sight at its first stroke, seen from outside the rain
//   with the view turned to it, and the sky clearing behind it. Their moments are worked out at the
//   view's eye (the view is presentation, so measured).
//
// The lab fails when the gust front blows no harder than the map's own wind and 8 m/s more, the
// core rains less than 0.3 on the dial, nothing strikes within 10 km, or the summer's rain is not
// rain at the base. Without an adapter it notes that and draws placeholders, so the digest is the
// same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: a thunderstorm passing over seed 38: gusts, rain, flashes, the game's frames
// ge:keywords lab storm thunderstorm pass gust-front downpour strike sv_storm storm_of fall-line
// ge:keywords picture chart
// ge:skill water render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/air/air.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/weather.hpp"
#include "wind/lightning.hpp"
#include "wind/precip.hpp"
#include "wind/storm.hpp"

namespace {

using ge::lab::Det;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace ma = ge::mountain::air;
namespace gw = ge::wind;
namespace r = ge::render;

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v != nullptr && std::strcmp(v, "1") == 0;
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
constexpr double kSpan = 1800;         // s, the half hour charted
constexpr double kStep = 15;           // s
constexpr double kNear = 15000;        // m: the flashes counted
constexpr double kStrikeNear = 10000;  // m: a strike the lab asks for
constexpr double kStrikeFar = 6000;    // m: the farthest strike drawn

double mean_luma(const gl::Picture& p) {
  double sum = 0;
  const size_t n = static_cast<size_t>(p.width) * static_cast<size_t>(p.height);
  for (size_t i = 0; i < n; ++i)
    sum += 0.2126 * p.rgba[i * 4] + 0.7152 * p.rgba[i * 4 + 1] + 0.0722 * p.rgba[i * 4 + 2];
  return n ? sum / static_cast<double>(n) : 0;
}

std::vector<gw::Flash> flashes_of(const gw::Storm& s, double t0, double t1) {
  std::vector<gw::Flash> out;
  for (int k = 0; k < s.cells; ++k) {
    for (auto n = static_cast<int64_t>(std::floor(t0)); n < static_cast<int64_t>(t1); ++n) {
      gw::Flash f;
      if (gw::flash_in_slot(s, k, n, f) && f.t0 >= t0 && f.t0 < t1) out.push_back(f);
    }
  }
  std::sort(out.begin(), out.end(),
            [](const gw::Flash& a, const gw::Flash& b) { return a.t0 < b.t0; });
  return out;
}

double plan_distance(const gw::Flash& f, double x, double z) {
  return ge::core::hypot(f.ex - x, f.ez - z);
}

// Whether the strike's lowest 300 m stand clear of the ground seen from `eye`.
bool in_sight(const mg::Level& level, const double eye[3], const gw::Flash& f) {
  for (int i = 1; i < 64; ++i) {
    const double u = i / 64.0;
    const double x = eye[0] + (f.ex - eye[0]) * u, z = eye[2] + (f.ez - eye[2]) * u;
    const double y = eye[1] + (f.ey + 300 - eye[1]) * u;
    if (level.ground_at(x, z) > y) return false;
  }
  return true;
}

void chart_shot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                const std::string& caption, const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {{"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double strength = lab.arg_num("storm");
  const double hour = lab.arg_num("hour");
  const std::string view_name = lab.arg("view");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (strength <= 0 || strength > 1 || frames < 1 || frames > 120 || width < 16 || height < 16 ||
      width > 4096 || height > 4096) {
    lab.fail("--storm must be over 0 and at most 1, --frames 1..120, --width and --height 16..4096",
             "next: pass --storm=0.7 --frames=4 --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  mg::Level level = mg::generate_level(seed, opts);
  mg::SkyPick pick;
  pick.thunder = strength;
  mg::with_sky(level, pick);
  const gw::Storm storm = ma::storm_of(level);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const double mx = 0.5 * (level.mountain.summit.x + level.mountain.base.x);
  const double mz = 0.5 * (level.mountain.summit.z + level.mountain.base.z);
  lab.metric("storm.cells", storm.cells, "count");
  lab.metric("storm.freezing", level.weather.snowline, "m");

  // THE PASS at the mountain's middle.
  gl::LineChart line;
  line.frame = {"A STORM OF " + fixed(strength, 1) + " PASSING OVER SEED " + seed_s,
                "at the mountain's middle: the first cell overhead " +
                    fixed(ma::kOverhead / 60, 0) + " min in, new cells on its right flank",
                "t (min)", "value"};
  line.spikes = false;
  gl::Trace wind{"wind m/s", {}, {}}, rain{"fall x20", {}, {}},
      base{"rain share at base x10", {}, {}}, top{"rain share at summit x10", {}, {}},
      cool{"freezing level fallen, m / 50", {}, {}}, flashes{"flashes/min within 15 km", {}, {}},
      strikes{"strikes/min within 15 km", {}, {}};
  const std::vector<gw::Flash> all = flashes_of(storm, 0, kSpan);
  double gust = 0, wettest = 0, base_rain = 1;
  for (int k = 0; k * kStep <= kSpan; ++k) {
    const double t = k * kStep, m = t / 60;
    const ma::Wind w = ma::wind_at(level, storm, t, mx, mz);
    const ma::Fall fb = ma::fall_at(level, storm, t, mx, level.mountain.base.y, mz);
    const ma::Fall ft = ma::fall_at(level, storm, t, mx, level.mountain.summit.y, mz);
    gust = std::max(gust, w.speed);
    if (fb.fall > wettest) {
      wettest = fb.fall;
      base_rain = fb.rain;
    }
    wind.x.push_back(m), wind.y.push_back(w.speed);
    rain.x.push_back(m), rain.y.push_back(20 * fb.fall);
    base.x.push_back(m), base.y.push_back(10 * fb.rain);
    top.x.push_back(m), top.y.push_back(10 * ft.rain);
    cool.x.push_back(m), cool.y.push_back(ma::cooled_at(storm, t, mx, mz) / 50);
  }
  int near_strikes = 0;
  for (int minute = 0; minute * 60 < kSpan; ++minute) {
    double n = 0, g = 0;
    for (const gw::Flash& f : all) {
      if (f.t0 < minute * 60 || f.t0 >= (minute + 1) * 60) continue;
      if (plan_distance(f, mx, mz) > kNear) continue;
      n += 1;
      if (f.kind == gw::FlashKind::ground) {
        g += 1;
        if (plan_distance(f, mx, mz) < kStrikeNear) ++near_strikes;
      }
    }
    flashes.x.push_back(minute + 0.5), flashes.y.push_back(n);
    strikes.x.push_back(minute + 0.5), strikes.y.push_back(g);
  }
  line.traces = {wind, rain, base, top, cool, flashes, strikes};
  line.marks.push_back({ma::kOverhead / 60, "OVERHEAD"});
  chart_shot(lab, "pass", gl::line_chart(line),
             "The storm at the mountain's middle through the first half hour. The gust front "
             "arrives ahead of the rain and blows hardest just behind its edge; the rain peaks as "
             "the core passes over, all of it rain at the base under the summer's freezing level, "
             "which falls under the downdraft so the summit sees sleet; the lightning pulses with "
             "the cells and most of it stays in the cloud.",
             "the wind jumping before the rain; the fall peaking near the OVERHEAD mark; the base "
             "all rain; the freezing level dipping with the core; flashes all through, strikes "
             "among them");
  lab.metric("pass.gust", gust, "m/s");
  lab.metric("pass.map-wind", level.weather.wind, "m/s");
  lab.metric("pass.wettest", wettest, "dial");
  lab.metric("pass.rain-at-base", base_rain, "share");
  lab.metric("pass.flashes", static_cast<double>(all.size()), "flashes");
  lab.metric("pass.near-strikes", near_strikes, "strikes");
  if (gust <= level.weather.wind + 8) {
    lab.fail("the gust front blew " + fixed(gust, 1) +
                 " m/s at the middle, no more than the map's " + fixed(level.weather.wind, 1) +
                 " m/s and 8",
             "Check air.cpp's wind_at with a storm and storm_of's track (kOverhead), then run "
             "`make lab LAB=storm-pass` (skill: water).");
  }
  if (wettest < 0.3) {
    lab.fail("the core rained only " + fixed(wettest, 2) + " on the dial at the middle",
             "Check air.cpp's fall_at with a storm, then run `make lab LAB=storm-pass` (skill: "
             "water).");
  }
  if (base_rain < 0.99) {
    lab.fail("the downpour at the base was only " + fixed(base_rain * 100, 0) + " % rain",
             "Check with_sky's summer freezing level (kSummerFreezing) and cooled_at, then run "
             "`make lab LAB=storm-pass` (skill: water).");
  }
  if (near_strikes == 0) {
    lab.fail("nothing struck within 10 km of the middle in the half hour",
             "Check storm_of's severity and track, then run `make lab LAB=storm-pass` (skill: "
             "water).");
  }

  // THE GAME'S FRAMES, their moments worked out at the view's eye.
  r::FrameView view;
  if (!fall_line::app::make_view(level, view_name, view)) {
    lab.fail("no view `" + view_name + "`",
             "Pass --view= one of games/fall_line/app/views.cpp's names. Skill: render.");
    return;
  }
  fall_line::app::sky_view(level, hour, view);
  const double ex = view.eye[0], ez = view.eye[2];
  double t_front = 0, t_rain = 0, most_wind = 0, most_rain = 0;
  for (int k = 0; k * 5 <= kSpan; ++k) {
    const double t = k * 5.0;
    const double w = ma::wind_at(level, storm, t, ex, ez).speed;
    const double f = gw::rain_at(storm, ex, ez, t);
    if (w > most_wind) most_wind = w, t_front = t;
    if (f > most_rain) most_rain = f, t_rain = t;
  }
  // The first strike 2 to 6 km out seen from outside the rain (as a strike is photographed:
  // under the cloud's base, ahead of the downpour or behind it); its shot turns the view to it.
  const gw::Flash* strike = nullptr;
  for (const gw::Flash& f : all) {
    const double d = plan_distance(f, ex, ez);
    if (f.kind == gw::FlashKind::ground && d > 2000 && d < kStrikeFar &&
        gw::rain_at(storm, ex, ez, f.t0) < 0.15 && in_sight(level, view.eye, f)) {
      strike = &f;
      break;
    }
  }
  const double t_strike = strike ? strike->t0 + strike->stroke[0].t + 0.5 * kFrame : t_rain;
  lab.metric("eye.front", t_front, "s", Det::measured);
  lab.metric("eye.rain", t_rain, "s", Det::measured);
  lab.metric("eye.strike", t_strike, "s", Det::measured);
  struct Moment {
    const char* id;
    const char* title;
    double t;
    const char* look;
  };
  const Moment moments[] = {
      {"approach", "THE STORM COMING IN", 0, "the towers over the mountain, the light going"},
      {"front", "THE GUST FRONT AT THE EYE", t_front,
       "the falling rain driven sideways by the outflow, the sky dark ahead"},
      {"downpour", "THE RAIN CORE OVERHEAD", t_rain,
       "rain to the summit, the view closed in, the ground wet and dark"},
      {"strike", "A STRIKE AHEAD OF THE RAIN", t_strike,
       "the channel to the ground under the cloud's base, the cloud and the snow lit by it"},
      {"after", "THE STORM GONE BY", kSpan, "the rain thinning, the towers moving off"},
  };
  std::vector<gl::Shot> shots;
  for (const Moment& m : moments) {
    std::string caption = "Seed " + seed_s;
    caption += "'s `" + view_name + "` view at " + fixed(hour, 1) + " h under a storm of ";
    caption += fixed(strength, 1) + ", " + fixed(m.t / 60, 1);
    caption += " min into the run, drawn by the game's own frame.";
    shots.push_back({m.id,
                     std::string(m.title) + ", SEED " + seed_s,
                     caption,
                     m.look,
                     {{"seed", seed_s}, {"view", view_name}, {"storm", fixed(strength, 1)}}});
  }
  auto device = ge::gpu::create_device({.label = "storm-pass", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    // The digest hashes the shots and the metrics in call order: the device path's order.
    for (size_t i = 0; i < std::size(moments); ++i) {
      gl::Picture none = gl::create_picture(width, height, {40, 40, 48});
      gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shots[i], none, Det::measured);
      const std::string id = moments[i].id;
      lab.metric(id + ".luma", 0, "", Det::measured);
      lab.metric(id + ".lamps", 0, "count", Det::measured);
      lab.metric(id + ".channel", 0, "segments", Det::measured);
    }
    lab.metric("gpu-errors", 0, "count", Det::measured);
    return;
  }
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, {});
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "storm-pass.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  uint64_t number = 0;
  for (size_t i = 0; i < std::size(moments); ++i) {
    fall_line::app::cut(*frame);
    for (int n = 0; n < frames; ++n) {
      r::FrameView v = view;
      if (strike != nullptr && std::string(moments[i].id) == "strike") {
        v.heading = ge::core::libm::atan2(strike->ex - ex, strike->ez - ez);
        v.pitch = 0.12;
      }
      v.time_s = moments[i].t - static_cast<double>(frames - 1 - n) * kFrame;
      v.frame = number++;
      fall_line::app::draw(*device, *frame, v, {}, target.view);
    }
    const gl::Picture pic{width, height, ge::gpu::read_now(*device, target).rgba};
    lab.shot(shots[i], pic, Det::measured);
    lab.metric(std::string(moments[i].id) + ".luma", mean_luma(pic), "", Det::measured);
    lab.metric(std::string(moments[i].id) + ".lamps", frame->lightning.lamps.count, "count",
               Det::measured);
    lab.metric(std::string(moments[i].id) + ".channel",
               static_cast<double>(frame->lightning.bolts.size()), "segments", Det::measured);
  }
  const uint32_t errors = device->errors.load();
  lab.metric("gpu-errors", errors, "count", Det::measured);
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "storm-pass",
    gl::LabKind::picture,
    "game:fall_line",
    "mountain/air/air.hpp's storm_of, wind_at and fall_at with a storm, and app/frame.hpp's "
    "lightning, towers and rain, over seed 38",
    "water",
    "Does a storm pass over the mountain as one: its gust front before the rain, the downpour "
    "rain to the summit, strikes near the skier lit in the game's own frame, and the sky clearing "
    "after?",
    {
        {"seed", "38", "the map's seed"},
        {"storm", "0.7", "the storm's strength (sv_storm), over 0 .. 1"},
        {"view", "piste", "the named view to shoot from"},
        {"hour", "16", "the hour of the afternoon"},
        {"frames", "4", "frames drawn up to each shot, at 60 a second"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=1 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
