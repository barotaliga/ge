// LAB `storm-sky`: the thunderstorm DRAWN (docs/storms.md § The storm drawn) in Fall Line's frame:
// a seed's map at night under a stormy sky of towers (cumulonimbus, --cover of the sky), one
// storm cell laid in front of the view where a tower stands, and its flashes as the frame sees
// them: render/view.hpp's `Flashes` (each flash's light as a lamp, lighting the cloud from inside
// through render/wgsl/flash.wgsl, the air round it and the ground) and render/bolts.hpp's channel.
//
// The shots: the night with no flash (`dark`), a cloud-to-ground STRIKE at its first return
// stroke (`strike`), a flash inside the cloud (`sheet`: sheet lightning, the tower lit from
// within and no channel seen), a BOLT FROM THE BLUE out of the anvil far ahead of the rain
// (`blue`), and a `filmstrip` of the strike's frames at 60 a second, from its leader to the dark
// after it (the flicker: a frame for each stroke, darker frames between). Beside each, the
// picture's mean luma; the lab fails when a flash's picture is no brighter than the dark one, or
// the filmstrip's brightest frame is not the first stroke's. Every flash is picked off
// engine/wind/lightning.hpp's schedule (deterministic, with or without a GPU). Without an adapter
// it notes that and draws placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1
// fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the storm drawn: a strike, sheet lightning, a bolt from the blue, the flicker
// ge:keywords lab storm lightning strike sheet-lightning bolt-from-the-blue flicker filmstrip
// ge:keywords flash-light bolts night fall-line picture
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
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/cloud_field.hpp"
#include "mountain/app/storm_look.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/bolts.hpp"
#include "render/cloud_kinds.hpp"
#include "render/clouds.hpp"
#include "wind/lightning.hpp"
#include "wind/storm.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;
namespace gw = ge::wind;
namespace r = ge::render;
namespace libm = ge::core::libm;

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
constexpr int kStrip = 8;  // the filmstrip's frames

double mean_luma(const ge::lab::Picture& p) {
  double sum = 0;
  const size_t n = static_cast<size_t>(p.width) * static_cast<size_t>(p.height);
  for (size_t i = 0; i < n; ++i)
    sum += 0.2126 * p.rgba[i * 4] + 0.7152 * p.rgba[i * 4 + 1] + 0.0722 * p.rgba[i * 4 + 2];
  return n ? sum / static_cast<double>(n) : 0;
}

// The first flash of `kind` (and `blue`) the cell makes once it is mature, of `strokes` or more,
// that `keep` keeps (none: any).
template <typename Keep>
bool pick(const gw::Storm& s, gw::FlashKind kind, bool blue, int strokes, gw::Flash& out,
          Keep keep) {
  const double end = s.cell[0].born + gw::life_of(s.cell[0].peak);
  for (auto n = static_cast<int64_t>(s.cell[0].born + gw::kTowering + 300);
       n < static_cast<int64_t>(end); ++n) {
    gw::Flash f;
    if (!gw::flash_in_slot(s, 0, n, f)) continue;
    if (f.kind == kind && f.blue == blue && f.strokes >= strokes && keep(f)) {
      out = f;
      return true;
    }
  }
  return false;
}

struct Pick {
  const char* id;
  const char* title;
  const char* look;
  gw::FlashKind kind;
  bool blue;
  int strokes;
  double lift;  // degrees over the view's pitch, to frame the flash
};

constexpr Pick kPicks[] = {
    {"strike", "A STRIKE AT ITS FIRST STROKE",
     "the channel bright and branched from the cloud's base to the ground, the base over it lit, "
     "the snow and the trees under it lit as if by a moment of day",
     gw::FlashKind::ground, false, 3, 0},
    {"sheet", "SHEET LIGHTNING",
     "no channel: the tower lit from within, brightest round the flash and fading through its "
     "body, its base a pale sheet, the ground faintly lit",
     gw::FlashKind::cloud, false, 3, 22},
    {"blue", "A BOLT FROM THE BLUE",
     "a long channel out of the anvil's side far from the tower's core, striking the clear ground "
     "ahead of the storm",
     gw::FlashKind::ground, true, 1, 0},
};

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& view_name = lab.arg("view");
  const double hour = lab.arg_num("hour");
  const double cover = lab.arg_num("cover");
  const double ahead = lab.arg_num("ahead");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (frames < 1 || frames > 120 || width < 16 || height < 16 || width > 4096 || height > 4096 ||
      cover < 0 || cover > 1 || ahead < 1000) {
    lab.fail(
        "--frames must be 1..120, --width and --height 16..4096, --cover 0..1, --ahead "
        "1000 m or more",
        "next: pass --frames=6 --width=640 --height=360 --cover=0.5 --ahead=7000.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level level = mg::generate_level(seed, opts);
  r::FrameView base;
  if (!fall_line::app::make_view(level, view_name, base)) {
    lab.fail("no view `" + view_name + "`",
             "Pass --view= one of games/fall_line/app/views.cpp's names. Skill: render.");
    return;
  }
  fall_line::app::sky_view(level, hour, base);
  base.pitch += 8 * 3.14159265358979 / 180;
  const std::string seed_s = std::to_string(static_cast<long long>(seed));

  // THE CELL, its flashes picked off the schedule (their times its seed's alone, wherever it
  // stands); every shot is drawn under the sky of the strike's moment, `sky_t`, each flash's light
  // taken at its own.
  gw::Storm storm;
  storm.ground = level.mountain.base.y;
  storm.cells = 1;
  storm.cell[0].peak = 0.9;
  storm.cell[0].core = 1800 + 1800 * 0.9;
  storm.cell[0].seed = static_cast<uint32_t>(seed) * 2654435761u + 17;
  gw::Flash first_strike;
  const auto any = [](const gw::Flash&) { return true; };
  if (!pick(storm, kPicks[0].kind, kPicks[0].blue, kPicks[0].strokes, first_strike, any)) {
    lab.fail("no strike in the cell's mature life",
             "Widen pick() in games/fall_line/labs/gpu_storm_lab.cpp, then run `make lab "
             "LAB=storm-sky` (skill: water).");
    return;
  }
  const double sky_t = first_strike.t0;

  // THE SKY: towers over the map, the share --cover of it.
  const r::CloudKind* kinds[r::kCloudLayers] = {r::cloud_kind_named("cumulonimbus")};
  r::Clouds clouds = ge::mountain::app::cloud_layers_of(
      kinds, 1, level.weather, level.seed, level.mountain.base.y, level.sun.latitude, sky_t);
  const double k = clouds.layers[0].cover > 0 ? cover / clouds.layers[0].cover : 1;
  clouds.layers[0].cover = std::min(1.0, clouds.layers[0].cover * k);
  const r::CloudLayer& tower = clouds.layers[0];

  // THE CELL: --ahead m out along the view (a third either way), within 12 degrees of its
  // middle, under the thickest column of tower there.
  double cx = 0, cz = 0, best = -1;
  for (int j = -8; j <= 8; ++j) {
    for (int i = -6; i <= 6; ++i) {
      const double d = ahead * (1 + j / 24.0), h = base.heading + i * 0.035;
      const double x = base.eye[0] + d * libm::sin(h), z = base.eye[2] + d * libm::cos(h);
      const double c = r::cloud_column(tower, x, z);
      if (c > best) {
        best = c;
        cx = x;
        cz = z;
      }
    }
  }
  storm.base = tower.base_m - storm.ground;
  storm.cell[0].x = cx;
  storm.cell[0].z = cz;
  // Where the cell stands follows the view and the clouds, which the presentation works out with
  // the platform's own transcendentals: measured, so a machine whose libm rounds otherwise digests
  // the same. The flashes' times are the cell's seed's alone, and hashed.
  lab.metric("cell.x", cx, "m", Det::measured);
  lab.metric("cell.z", cz, "m", Det::measured);
  lab.metric("cell.column", best, "share", Det::measured);

  gw::Flash picked[std::size(kPicks)];
  bool found[std::size(kPicks)] = {};
  for (size_t i = 0; i < std::size(kPicks); ++i) {
    // A flash in the cloud is picked in the frame's middle third and in the tower, where its glow
    // is seen.
    const auto seen = [&](const gw::Flash& f) {
      if (f.kind != gw::FlashKind::cloud) return true;
      const double mx = 0.5 * (f.x + f.ex) - base.eye[0], mz = 0.5 * (f.z + f.ez) - base.eye[2];
      double off = libm::atan2(mx, mz) - base.heading;
      off = std::remainder(off, 2 * 3.14159265358979);
      return std::fabs(off) < 0.35 && 0.5 * (f.y + f.ey) - storm.ground < 8000 &&
             r::cloud_column(tower, 0.5 * (f.x + f.ex), 0.5 * (f.z + f.ez)) > 0.4;
    };
    found[i] = pick(storm, kPicks[i].kind, kPicks[i].blue, kPicks[i].strokes, picked[i], seen);
    if (!found[i]) {
      lab.fail(std::string("no ") + kPicks[i].id + " flash in the cell's mature life",
               "Widen pick() in games/fall_line/labs/gpu_storm_lab.cpp, then run `make lab "
               "LAB=storm-sky` (skill: water).");
      return;
    }
    const gw::Flash& f = picked[i];
    const std::string id = kPicks[i].id;
    // A flash in the cloud is picked by where it is seen, so by the view: measured.
    const Det seen_det = f.kind == gw::FlashKind::ground ? Det::deterministic : Det::measured;
    lab.metric(id + ".t", f.t0, "s", seen_det);
    lab.metric(id + ".strokes", f.strokes, "count", seen_det);
    lab.metric(id + ".distance",
               ge::core::hypot(ge::core::hypot(f.ex - base.eye[0], f.ey - base.eye[1]),
                               f.ez - base.eye[2]),
               "m", Det::measured);
  }
  static gw::Channel channel[std::size(kPicks)];
  for (size_t i = 0; i < std::size(kPicks); ++i) gw::build_channel(picked[i], channel[i]);

  auto device = ge::gpu::create_device({.label = "storm-sky", .validation = 1});
  const auto shot = [&](const std::string& id, const std::string& title, const std::string& look,
                        const std::string& what) {
    return ge::lab::Shot{id,
                         title + ", SEED " + seed_s,
                         "Seed " + seed_s + "'s `" + view_name + "` view at " + fixed(hour, 1) +
                             " h under towers over " + fixed(cover * 100, 0) +
                             " % of the sky: " + what + ".",
                         look,
                         {{"seed", seed_s}, {"view", view_name}, {"hour", fixed(hour, 1)}}};
  };
  // Every shot's spec, the same with a device or without one.
  std::vector<ge::lab::Shot> shots;
  shots.push_back(shot("dark", "THE STORM'S NIGHT, NO FLASH",
                       "the towers dark against the night sky, the snow dim", "no flash"));
  for (size_t i = 0; i < std::size(kPicks); ++i) {
    const gw::Flash& f = picked[i];
    const double d = ge::core::hypot(f.ex - base.eye[0], f.ez - base.eye[2]);
    shots.push_back(
        shot(kPicks[i].id, kPicks[i].title, kPicks[i].look,
             std::string(f.kind == gw::FlashKind::ground ? "a ground flash" : "a cloud flash") +
                 " of " + std::to_string(f.strokes) + " strokes, " + fixed(d / 1000, 1) +
                 " km off, the frame of its first stroke"));
  }
  shots.push_back(shot("filmstrip", "THE STRIKE FRAME BY FRAME, 60 A SECOND",
                       "a frame bright for each return stroke, darker between, the channel there "
                       "only while a stroke or its glow lasts, the dark after it",
                       "the strike's first eight frames from its leader, each labelled with the "
                       "light that fell in it (the unit)"));
  // The frame of the strike's first stroke.
  const int first = static_cast<int>((picked[0].stroke[0].t) / kFrame);
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    // The digest hashes the shots and the metrics in call order, so the placeholders carry the
    // device path's shots and its one deterministic metric, in its order.
    for (const ge::lab::Shot& s : shots) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(s, none, Det::measured);
    }
    lab.metric("filmstrip.first-stroke", first + 1, "frame");
    lab.metric("gpu-errors", 0, "count", Det::measured);
    return;
  }

  fall_line::app::FrameOptions options;
  options.cloud_kinds[0] = kinds[0];
  options.cloud_count = 1;
  options.cover = cover;
  options.precip = false;
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "storm-sky.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  uint64_t number = 0;
  // `frames` frames, the last the one over [t - 1/60, t) of `flash`'s light, under the sky at
  // `sky_t`.
  const auto draw_to = [&](double t, const gw::Flash* flash, const gw::Channel* ch, double lift) {
    fall_line::app::cut(*frame);
    for (int n = 0; n < frames; ++n) {
      const double back = static_cast<double>(frames - 1 - n) * kFrame;
      r::FrameView view = base;
      view.pitch += lift * 3.14159265358979 / 180;
      view.time_s = sky_t - back;
      view.frame = number++;
      const double a = t - back - kFrame, b = t - back;
      view.flashes = ge::mountain::app::lamps_of(flash, a, b);
      std::vector<r::BoltSegment> segs;
      if (ch != nullptr && flash != nullptr)
        ge::mountain::app::bolts_of(*flash, *ch, a, b, tower.base_m, segs);
      fall_line::app::FrameScene scene;
      scene.bolts = segs;
      fall_line::app::draw(*device, *frame, view, scene, target.view);
    }
    return ge::lab::Picture{width, height, ge::gpu::read_now(*device, target).rgba};
  };

  const ge::lab::Picture dark = draw_to(sky_t, nullptr, nullptr, 0);
  const double dark_luma = mean_luma(dark);
  lab.shot(shots[0], dark, Det::measured);
  lab.metric("dark.luma", dark_luma, "level", Det::measured);
  for (size_t i = 0; i < std::size(kPicks); ++i) {
    const gw::Flash& f = picked[i];
    // The frame that ends just past the brightest stroke (a cloud flash's first pulse).
    const double at = f.t0 + f.stroke[0].t + 0.5 * kFrame;
    const ge::lab::Picture pic = draw_to(at, &f, &channel[i], kPicks[i].lift);
    const std::string id = kPicks[i].id;
    lab.shot(shots[i + 1], pic, Det::measured);
    const double luma = mean_luma(pic);
    // The night with no flash, framed as this shot is.
    const double unlit = kPicks[i].lift == 0
                             ? dark_luma
                             : mean_luma(draw_to(sky_t, nullptr, nullptr, kPicks[i].lift));
    lab.metric(id + ".luma", luma, "level", Det::measured);
    lab.metric(id + ".dark-luma", unlit, "level", Det::measured);
    if (luma <= unlit + 1) {
      lab.fail(id + "'s picture is no brighter than the dark night's (" + fixed(luma, 1) +
                   " against " + fixed(unlit, 1) + " levels)",
               "Check flash_rows in engine/render/view.cpp and render/wgsl/flash.wgsl, then run "
               "`make lab LAB=storm-sky` (skill: water).");
    }
  }

  // THE FILMSTRIP: the strike's frames, one at a time, from its leader on.
  const gw::Flash& strike = picked[0];
  const int tw = width / 2, th = height / 2;
  ge::lab::Picture strip = ge::lab::create_picture(tw * 4, th * 2, {0, 0, 0});
  std::vector<double> lumas;
  lumas.reserve(kStrip);
  fall_line::app::cut(*frame);
  for (int n = 0; n < kStrip; ++n) {
    r::FrameView view = base;
    view.time_s = sky_t + (n + 1) * kFrame;
    view.frame = number++;
    const double a = strike.t0 + n * kFrame, b = a + kFrame;
    view.flashes = ge::mountain::app::lamps_of(&strike, a, b);
    std::vector<r::BoltSegment> segs;
    ge::mountain::app::bolts_of(strike, channel[0], a, b, tower.base_m, segs);
    fall_line::app::FrameScene scene;
    scene.bolts = segs;
    fall_line::app::draw(*device, *frame, view, scene, target.view);
    const ge::lab::Picture pic{width, height, ge::gpu::read_now(*device, target).rgba};
    lumas.push_back(mean_luma(pic));
    ge::lab::Picture small = ge::lab::resized(pic, tw, th);
    ge::lab::draw_text(small, 3, 3,
                       std::to_string(n + 1) + "  " + fixed(gw::light_over(strike, a, b), 3),
                       {255, 255, 120});
    ge::lab::blit(strip, small, (n % 4) * tw, (n / 4) * th);
    lab.metric("filmstrip." + std::to_string(n + 1) + ".luma", lumas.back(), "level",
               Det::measured);
  }
  lab.shot(shots.back(), strip, Det::measured);
  // The brightest frame is the first stroke's.
  const auto most = std::max_element(lumas.begin(), lumas.end()) - lumas.begin();
  double best_light = -1;
  int lit = 0;
  for (int n = 0; n < kStrip; ++n) {
    const double l = gw::light_over(strike, strike.t0 + n * kFrame, strike.t0 + (n + 1) * kFrame);
    if (l > best_light) {
      best_light = l;
      lit = n;
    }
  }
  lab.metric("filmstrip.brightest", static_cast<double>(most + 1), "frame", Det::measured);
  lab.metric("filmstrip.first-stroke", first + 1, "frame");
  if (most != lit) {
    lab.fail("the filmstrip's brightest frame is " + std::to_string(most + 1) +
                 ", not the one the most light fell in (" + std::to_string(lit + 1) + ")",
             "Check bolts.wgsl's light and flash.wgsl, then run `make lab LAB=storm-sky` "
             "(skill: water).");
  }
  const uint32_t errors = device->errors.load();
  lab.metric("gpu-errors", errors, "count", Det::measured);
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "storm-sky",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "render/view.hpp's Flashes, render/wgsl/flash.wgsl and render/bolts.hpp over "
    "engine/wind/lightning.hpp's flashes, in Fall Line's frame",
    "water",
    "Does the storm read as one at night: a strike's channel and the ground lit under it, sheet "
    "lightning glowing in the tower, a bolt from the blue, and the flicker frame by frame?",
    {
        {"seed", "38", "the map's seed"},
        {"view", "vista", "the named view to shoot from"},
        {"hour", "22", "the hour of the night"},
        {"cover", "0.5", "the share of the sky the towers cover"},
        {"ahead", "7000", "how far ahead of the view the storm's cell stands, m"},
        {"frames", "4", "frames drawn up to each shot, at 60 a second"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=1 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
