// LAB `precip`: what falls (engine/render/precip.hpp) in Fall Line's frame, carried by the map's
// wind (plugins/mountain/air/air.hpp), rain under the snow line, wetting the ground. A seed's map
// is drawn under eight skies in turn, its own weather replaced by each: three of snow (a FLURRY, a
// steady SNOW, a STORM's blizzard), four of rain with the snow line over the summit (a DRIZZLE, a
// RAIN, a DOWNPOUR, a RAINSTORM) and a SLEET with the snow line at the eye. Each from a named view,
// --frames frames at 60 a second up to --at seconds into the run, so the particles in the picture
// have flown on the wind those frames. Then the day's air (M51e): a MILD-DAWN and a
// MILD-AFTERNOON of one day whose mean puts its snow line at the eye mid-morning, so the line
// (wind::snow_level of the day's air) is under the eye at 06:30 and over it at 14:30; and a
// BREATH: a skier standing on the piste on the map's own cold, clear, still morning at 07:30, seen
// side on against the valley, breathing white puffs (skier/app/breath.hpp by wind::breath_of).
// `--skies=` picks some by id; `--hour=22` draws them at night, lit by the piste's floodlights
// (render/lights.hpp through the fall); `--mirror=0` draws the wet ground mirroring the sky alone,
// not the scene (render/mirror.hpp).
//
// Beside each: the fall, its rain share at the eye, the rate and its tier, the visual range, the
// wind, the particles and splashes drawn and the wet ground (deterministic: the air is a pure
// function of the map and the clock), and the two passes' GPU ms (measured). The lab fails when a
// sky draws nothing, or a rain draws no splash or leaves the ground dry. Without an adapter it
// notes that and draws placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1
// fails instead. Built only with GE_GPU.
//
// ge:summary Lab: snow, sleet and rain from a flurry to a rainstorm, on the wind, with splashes
// ge:keywords lab precip snowfall rain sleet drizzle downpour flurries snow storm wind splash wet
// ge:keywords visibility tier fall-line picture snow-line snowline_at mild-day breath cold-morning
// ge:skill render labs
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/air/air.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/weather.hpp"
#include "render/precip.hpp"
#include "skier/app/breath.hpp"
#include "wind/climate.hpp"
#include "wind/cold.hpp"
#include "wind/field.hpp"
#include "wind/precip.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;

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

// The snow line over the summit (rain everywhere), or at the eye (half rain there: a sleet).
constexpr double kAllRain = 1.5;
constexpr double kAtEye = -2;
// The day's mean: the map's own, or the one whose snow line stands at the eye mid-morning (so it
// is under the eye at dawn and over it in the afternoon).
constexpr double kOwnMean = 1000;
constexpr double kMeanAtEye = 999;
constexpr double kMidMorning = 10.5;

struct Sky {
  const char* id;
  mg::WeatherKind kind;
  double snowfall;
  double wind;      // m/s at 10 m
  double snowline;  // SkyPick's share of the mountain; kAtEye at the view's eye; -1 none
  const char* look;
  double mean = kOwnMean;  // the day's mean picked (Weather::mean_c), or kOwnMean / kMeanAtEye
  double hour = -1;        // the sky's solar hour (the map's start); negative: --hour's
  bool breath = false;     // a skier standing close by, breathing, seen from in front
};

constexpr Sky kSkies[] = {
    {"flurries", mg::WeatherKind::flurries, 0.3, 3, -1,
     "a scatter of flakes drifting down, a few near the lens streaked, the far snow clear"},
    {"snow", mg::WeatherKind::snow, 0.6, 5, -1,
     "a steady fall filling the near air, the flakes slanting with the wind and swirling a little"},
    {"storm", mg::WeatherKind::storm, 0.95, 14, -1,
     "a blizzard: dense flakes driven near level by the wind, swirling in sheets, the near air "
     "white"},
    {"drizzle", mg::WeatherKind::snow, 0.15, 2, kAllRain,
     "a few faint grey lines falling straight, the odd splash at the feet, the snow just damp"},
    {"rain", mg::WeatherKind::snow, 0.45, 4, kAllRain,
     "thin streaks falling fast and steep, splashes peppering the near ground, the snow darker "
     "and glossy"},
    {"downpour", mg::WeatherKind::snow, 0.85, 6, kAllRain,
     "a curtain of streaks, the ground alive with splashes, slush pooled on the flats mirroring "
     "the grey"},
    {"rainstorm", mg::WeatherKind::storm, 0.95, 16, kAllRain,
     "rain driven slanting hard by the wind, splashes everywhere, the far slope lost in grey"},
    {"sleet", mg::WeatherKind::snow, 0.6, 5, kAtEye,
     "flakes and streaks falling together, the drops faster, a few splashes, the snow damp"},
    {"mild-dawn", mg::WeatherKind::snow, 0.5, 3, -1,
     "a mild day's dawn: a steady fall of flakes, the snow line under the eye", kMeanAtEye, 6.5},
    {"mild-afternoon", mg::WeatherKind::snow, 0.5, 3, -1,
     "the same day at 14:30: the snow line climbed over the eye, rain streaking down and "
     "splashing, the snow darker",
     kMeanAtEye, 14.5},
    {"breath", mg::WeatherKind::clear, 0, 0, -1,
     "a cold, clear, still morning: a skier standing on the piste seen side on, a white puff of "
     "breath in front of his face, thinning within half a metre",
     kOwnMean, 7.5, true},
};

// The day's mean, C, whose snow line stands at `y` at kMidMorning.
double mean_at(const mg::Level& level, double y) {
  mg::Level l = level;
  double lo = -40, hi = 40;
  for (int k = 0; k < 50; ++k) {
    l.weather.mean_c = 0.5 * (lo + hi);
    (ge::mountain::air::snowline_at(l, kMidMorning) < y ? lo : hi) = l.weather.mean_c;
  }
  return 0.5 * (lo + hi);
}

// A skier standing still on the snow `ahead` m down the view's line from its eye, facing across
// it to the view's left.
ge::skier::SkierState standing(const mg::Level& level, const ge::render::FrameView& v,
                               double ahead) {
  ge::skier::SkierState s;
  s.x = v.eye[0] + ahead * ge::core::libm::sin(v.heading);
  s.z = v.eye[2] + ahead * ge::core::libm::cos(v.heading);
  s.y = level.ground_at(s.x, s.z);
  s.heading = v.heading - 0.5 * 3.14159265358979323846;
  return s;
}

// A view of `s`'s face from behind his left shoulder's side, 2.3 m back along `along` (the view's
// heading, down the mountain), so his breath is seen against the valley and the sky.
void face_view(const ge::skier::SkierState& s, double along, ge::render::FrameView& v) {
  double m[3];
  ge::skier::mouth_of(s, m);
  const double fx = ge::core::libm::sin(s.heading), fz = ge::core::libm::cos(s.heading);
  const double dx = ge::core::libm::sin(along), dz = ge::core::libm::cos(along);
  v.eye[0] = m[0] - 2.2 * dx + 0.7 * fx;
  v.eye[1] = m[1] + 0.05;
  v.eye[2] = m[2] - 2.2 * dz + 0.7 * fz;
  const double tx = m[0] + 0.4 * fx - v.eye[0], tz = m[2] + 0.4 * fz - v.eye[2];
  v.heading = ge::core::libm::atan2(tx, tz);
  v.pitch = ge::core::libm::atan2(m[1] - 0.2 - v.eye[1], ge::core::hypot(tx, tz));
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& view_name = lab.arg("view");
  const double at = lab.arg_num("at");
  const double hour = lab.arg_num("hour");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (at < 0 || frames < 1 || frames > 600 || width <= 0 || height <= 0 || width > 4096 ||
      height > 4096) {
    lab.fail("--at must be 0 or more, --frames 1..600 and --width and --height 1..4096",
             "next: pass --at=30 --frames=60 --width=640 --height=360.");
    return;
  }
  mg::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const mg::Level base = mg::generate_level(seed, opts);
  ge::render::FrameView probe;
  if (!fall_line::app::make_view(base, view_name, probe)) {
    lab.fail("no view `" + view_name + "`",
             "Pass --view= one of games/fall_line/app/views.cpp's names. Skill: render.");
    return;
  }
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const double eye_y = probe.eye[1];

  // THE SKIES picked (all, or --skies' ids).
  std::vector<const Sky*> picked;
  const std::string& want = lab.arg("skies");
  for (const Sky& sky : kSkies) {
    if (want == "all" ||
        ("," + want + ",").find("," + std::string(sky.id) + ",") != std::string::npos) {
      picked.push_back(&sky);
    }
  }
  if (picked.empty()) {
    lab.fail("--skies `" + want + "` names no sky",
             "Pass --skies=all or ids from kSkies in games/fall_line/labs/gpu_precip_lab.cpp, "
             "comma-separated. Skill: render.");
    return;
  }

  // THE SKIES: the map under each, and the air at the shot.
  const fall_line::app::FrameOptions defaults;
  std::vector<mg::Level> levels;
  levels.reserve(picked.size());
  for (const Sky* sky : picked) {
    mg::Level l = base;
    l.weather.kind = sky->kind;
    l.weather.snowfall = sky->snowfall;
    l.weather.wind = sky->wind;
    if (sky->hour >= 0) l.sun.hour = sky->hour - at / 3600;  // the shot's hour at `at`
    if (sky->mean == kMeanAtEye)
      l.weather.mean_c = mean_at(l, eye_y);
    else if (sky->mean != kOwnMean)
      l.weather.mean_c = sky->mean;
    if (sky->snowline == kAtEye) {
      l.weather.snowline = eye_y;
    } else if (sky->snowline >= 0) {
      mg::SkyPick pick;
      pick.snowline = sky->snowline;
      mg::with_sky(l, pick);
    }
    levels.push_back(std::move(l));
    const mg::Level& level = levels.back();
    const ge::mountain::air::Wind w = ge::mountain::air::wind_at(level, at);
    const ge::mountain::air::Fall f = ge::mountain::air::fall_at(level, at, eye_y);
    const ge::wind::Precip p{f.fall, f.rain};
    const std::string k = sky->id;
    lab.metric(k + ".fall", f.fall, "share");
    lab.metric(k + ".rain", f.rain, "share");
    lab.metric(k + ".rate", ge::wind::rate_of(p), "mm/h");
    lab.metric(k + ".tier", static_cast<double>(ge::wind::tier_of(p, w.speed)), "tier");
    lab.metric(k + ".visibility", f.visibility, "m");
    lab.metric(k + ".wind", w.speed, "m/s");
    lab.metric(k + ".gust", w.gust, "share");
    const double hour_k = ge::mountain::air::hour_at(level, at);
    const double over = ge::mountain::air::snowline_at(level, hour_k) - eye_y;
    lab.metric(k + ".snowline", over, "m over the eye");
    if (sky->mean == kMeanAtEye && (sky->hour < kMidMorning) != (over < 0)) {
      lab.fail("the " + k + " sky's snow line stands " + fixed(over, 0) +
                   " m over the eye: it should be under it before mid-morning and over it after",
               "Read snowline_at in plugins/mountain/air/air.cpp and wind::snow_level. Skill: "
               "water.");
    }
    lab.metric(k + ".particles", ge::render::particles_drawn(defaults.fall, f.fall), "count");
    lab.metric(k + ".splashes",
               ge::render::splashes_drawn(defaults.fall, f.rain * std::sqrt(f.fall)), "count");
    if (sky->breath) {
      ge::render::FrameView v = probe;
      const ge::skier::SkierState sk = standing(level, v, 8);
      double m[3];
      ge::skier::mouth_of(sk, m);
      ge::wind::Site site;
      site.y = m[1];
      site.snow = 1;
      const ge::wind::Thermal air =
          ge::wind::air_at(ge::mountain::air::climate_of(level), site, hour_k);
      const double show = ge::wind::breath_of(air);
      lab.metric(k + ".air", air.temp_c, "C");
      lab.metric(k + ".breath", show, "share");
      if (show < 0.3) {
        lab.fail("the " + k + " sky's cold morning shows a breath of only " + fixed(show, 2),
                 "Read breath_of in engine/wind/cold.cpp and climate_of in "
                 "plugins/mountain/air/air.cpp. Skill: water.");
      }
    } else if (ge::render::particles_drawn(defaults.fall, f.fall) == 0) {
      lab.fail("the " + k + " sky draws nothing falling at " + fixed(at, 1) + " s",
               "Check fall_at in plugins/mountain/air/air.cpp (the squalls' breath). Skill: "
               "mountain.");
    }
    if (sky->snowline == kAllRain &&
        (f.rain < 0.99 || ge::render::splashes_drawn(defaults.fall, f.rain) == 0)) {
      lab.fail("the " + k + " sky's fall is not all rain at the eye, or draws no splash",
               "Check fall_at's snow line in plugins/mountain/air/air.cpp and with_sky's in "
               "plugins/mountain/mapgen/weather.cpp. Skill: mountain.");
    }
  }

  auto shot_of = [&](size_t i) {
    const Sky& sky = *picked[i];
    const mg::Level& level = levels[i];
    const ge::mountain::air::Fall f = ge::mountain::air::fall_at(level, at, eye_y);
    const ge::mountain::air::Wind w = ge::mountain::air::wind_at(level, at);
    const double shot_hour = sky.hour >= 0 ? sky.hour : hour;
    const ge::wind::Tier tier = ge::wind::tier_of({f.fall, f.rain}, w.speed);
    std::string title = std::string(sky.id) + ", SEED " + seed_s + " " + view_name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (sky.breath) {
      return ge::lab::Shot{
          sky.id,
          title,
          "Seed " + seed_s + "'s `" + view_name + "` view: a skier standing on the piste at " +
              fixed(shot_hour, 1) +
              " h on the map's own clear day, seen side on against the valley, his breath "
              "drawn as particles as thick as the air at his mouth shows one (wind::breath_of).",
          sky.look,
          {{"seed", seed_s}, {"sky", sky.id}, {"hour", fixed(shot_hour, 2)}}};
    }
    const std::string caption =
        "Seed " + seed_s + "'s `" + view_name + "` view under " + sky.id + ": a " +
        ge::wind::tier_name(tier) + " fall of " + fixed(f.fall, 2) + ", " + fixed(100 * f.rain, 0) +
        " % rain, " + fixed(ge::wind::rate_of({f.fall, f.rain}), 1) + " mm/h, " +
        fixed(f.visibility, 0) + " m visibility, wind " + fixed(w.speed, 1) + " m/s; " +
        std::to_string(frames) + " frames up to " + fixed(at, 1) + " s" +
        (shot_hour >= 0 ? ", at " + fixed(shot_hour, 1) + " h" : "") + "; the snow line " +
        fixed(ge::mountain::air::snowline_at(level, ge::mountain::air::hour_at(level, at)) - eye_y,
              0) +
        " m over the eye.";
    return ge::lab::Shot{sky.id,
                         title,
                         caption,
                         sky.look,
                         {{"seed", seed_s},
                          {"sky", sky.id},
                          {"view", view_name},
                          {"t", fixed(at, 2)},
                          {"hour", shot_hour >= 0 ? fixed(shot_hour, 2) : "the map's"},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "precip", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (size_t i = 0; i < levels.size(); ++i) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(i), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  uint32_t errors = 0, late = 0;
  for (size_t i = 0; i < levels.size(); ++i) {
    // A frame is made once a device (its load ends), so each sky's map has a device of its own.
    if (i > 0) {
      errors += device->errors.load();
      late += device->pipelines_after_load;
      device = ge::gpu::create_device({.label = "precip", .validation = 1});
      if (!device) {
        lab.fail("the GPU device could not be made again",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
    }
    const mg::Level& level = levels[i];
    fall_line::app::FrameOptions options;
    options.mirror = lab.arg_num("mirror") != 0;
    auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height),
                                              wgpu::TextureFormat::RGBA8Unorm, options);
    if (!frame) {
      lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    const ge::gpu::Target target = ge::gpu::create_target(
        *device, "precip.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    ge::render::FrameView view;
    fall_line::app::make_view(level, view_name, view);
    const double sky_hour = picked[i]->hour >= 0 ? picked[i]->hour : hour;
    if (sky_hour >= 0) fall_line::app::sky_view(level, sky_hour, view);
    // The breathing skier, and the view of his face.
    ge::skier::SkierState skier;
    fall_line::app::FrameScene scene;
    if (picked[i]->breath) {
      skier = standing(level, view, 8);
      face_view(skier, view.heading, view);
      scene.skiers = std::span<const ge::skier::SkierState>(&skier, 1);
      double m[3];
      ge::skier::mouth_of(skier, m);
      scene.subject = std::array<double, 3>{m[0], m[1], m[2]};  // the focus on his face
    }
    // A breath needs a breath's frames (the pen starts it somewhere of its own) to show: drawn
    // so the last frame falls a third of the way through his second breath out.
    int drawn = frames;
    if (picked[i]->breath) {
      const double from = ge::skier::breath_pen(0).phase;
      const double turns = 1 + std::fmod(1 + ge::skier::kExhaleShare / 1.5 - from, 1.0);
      drawn = 1 + static_cast<int>(std::lround(turns * ge::skier::kBreathPeriod * 60));
    }
    for (int n = 0; n < drawn; ++n) {
      view.frame = static_cast<uint64_t>(n);
      view.time_s = at - static_cast<double>(drawn - 1 - n) / 60;
      fall_line::app::draw(*device, *frame, view, scene, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    lab.shot(shot_of(i), ge::lab::Picture{width, height, img.rgba}, Det::measured);
    lab.metric(std::string(picked[i]->id) + ".wet", frame->wet.wet, "share", Det::measured);
    lab.metric(std::string(picked[i]->id) + ".puddles", frame->wet.puddles, "share", Det::measured);
    if (picked[i]->snowline == kAllRain && frame->wet.wet <= 0) {
      lab.fail(std::string("the ") + picked[i]->id + " sky leaves the ground dry",
               "Check the wet ground's step in games/fall_line/app/frame.cpp (r_wet). Skill: "
               "render.");
    }
    frame->recorder.wait(*device);
    for (int p = 0; p < frame->recorder.stats.count; ++p) {
      const auto& ps = frame->recorder.stats.passes[static_cast<size_t>(p)];
      if (std::string_view(ps.name).starts_with("precip") && !std::isnan(ps.gpu_ms)) {
        lab.metric(std::string(picked[i]->id) + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms",
                   Det::measured);
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
    "precip",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/precip.hpp (what falls) on plugins/mountain/air/air.hpp's wind and snow line "
    "in Fall Line's frame",
    "render",
    "Does what falls read as a flurry, a snow, a blizzard, a drizzle, a rain, a downpour, a "
    "rainstorm and a sleet, carried on the wind, with splashes and wet ground under the rain?",
    {
        {"seed", "38", "the map's seed"},
        {"skies", "all", "the skies drawn: all, or ids comma-separated (flurries,rain,...)"},
        {"view", "piste", "the named view to shoot from"},
        {"at", "30", "seconds into the run the shot is taken at"},
        {"hour", "-1",
         "the solar hour to light the map at (22: at night, under the piste's floodlights); "
         "negative: the map's own"},
        {"frames", "60", "frames drawn, at 60 a second, up to the shot"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"mirror", "1",
         "1: the scene mirrored in the wet ground (render/mirror.hpp); 0: the sky's light "
         "alone (the before of M38c)"},
    },
    "--frames=4 --width=160 --height=90 --skies=snow,rain",
    run,
};
GE_LAB(kSpec);

}  // namespace
