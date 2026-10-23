// LAB `weather`: the mountain's skies in the picture (plugins/mountain/app/weather_look.hpp over
// render/view.hpp's `Weather`): a seed's map drawn under each of R19's eight kinds in turn, its
// own weather replaced (a CLEAR day, FAIR heaps, a FLURRY, a HIGH veil, an OVERCAST, a steady
// SNOW, a STORM and a valley FOG), from a named view, --frames frames at 60 a second up to --at
// seconds into the run.
//
// Beside each: the look's numbers (the sun's pass, the deck's cover, the visual range, the deck
// overhead hidden: deterministic, pure functions of the map and the clock) and, measured off the
// picture, its mean luma and its contrast (the luma's spread: the flat light is a small one).
// The lab fails when a lid's picture is no flatter than the clear day's, or a fog's far half is
// not closer to the sky than the clear day's. Without an adapter it notes that and draws
// placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built
// only with GE_GPU.
//
// ge:summary Lab: a map under each weather kind: the flat light, the deck, the fog
// ge:keywords lab weather overcast deck flat-light fog visibility storm sky fall-line picture
// ge:skill render labs
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/air/air.hpp"
#include "mountain/app/weather_look.hpp"
#include "mountain/mapgen/generate.hpp"

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

struct Sky {
  const char* id;
  mg::WeatherKind kind;
  double snowfall;
  double fog;
  const char* look;
};

constexpr Sky kSkies[] = {
    {"clear", mg::WeatherKind::clear, 0, 0,
     "a blue sky, the sun's shadows hard on the snow, the far ridges blue in the clear air"},
    {"fair", mg::WeatherKind::fair, 0, 0, "the clear day with the blue a touch paler"},
    {"flurries", mg::WeatherKind::flurries, 0.3, 0,
     "a scatter of flakes, the far mountain softened into the sky some hundreds of metres off"},
    {"high", mg::WeatherKind::high, 0, 0,
     "a milky blue sky, the sun's shadows softer and fainter than the clear day's"},
    {"overcast", mg::WeatherKind::overcast, 0, 0,
     "a grey deck, darker overhead; flat light with no shadows; the far ridges grey"},
    {"snow", mg::WeatherKind::snow, 0.6, 0,
     "a steady fall under a grey deck: flat light, everything past a hundred metres gone grey"},
    {"storm", mg::WeatherKind::storm, 0.95, 0,
     "a blizzard: one grey from the snow to the sky, the near air full of driven flakes"},
    {"fog", mg::WeatherKind::fog, 0, 0.5,
     "a valley fog: a pale even grey, the trees fading out a couple of hundred metres off"},
};

// The picture's luma (0..255) over its rows from `y0` to `y1`: the mean and the spread.
void luma_stats(const ge::lab::Picture& p, int y0, int y1, double& mean, double& spread) {
  double sum = 0, sq = 0;
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = 0; x < p.width; ++x) {
      const size_t i =
          (static_cast<size_t>(y) * static_cast<size_t>(p.width) + static_cast<size_t>(x)) * 4;
      const double l = 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
      sum += l;
      sq += l * l;
      ++n;
    }
  }
  mean = n ? sum / n : 0;
  spread = n ? std::sqrt(std::max(sq / n - mean * mean, 0.0)) : 0;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string& view_name = lab.arg("view");
  const double at = lab.arg_num("at");
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (at < 0 || frames < 1 || frames > 600 || width <= 0 || height <= 1 || width > 4096 ||
      height > 4096) {
    lab.fail("--at must be 0 or more, --frames 1..600, --width 1..4096 and --height 2..4096",
             "next: pass --at=30 --frames=8 --width=640 --height=360.");
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

  // THE SKIES: the map under each, and the look at the shot.
  std::vector<mg::Level> levels;
  levels.reserve(std::size(kSkies));
  for (const Sky& sky : kSkies) {
    mg::Level l = base;
    l.weather.kind = sky.kind;
    l.weather.snowfall = sky.snowfall;
    l.weather.fog = sky.fog;
    levels.push_back(std::move(l));
    const ge::render::Weather w = ge::mountain::app::weather_at(levels.back(), at);
    const std::string k = sky.id;
    lab.metric(k + ".sun-pass", w.sun_pass, "share");
    lab.metric(k + ".cover", w.cover, "share");
    lab.metric(k + ".visibility", w.visibility_m, "m");
    lab.metric(k + ".hidden", w.hidden, "share");
  }

  auto shot_of = [&](size_t i) {
    const Sky& sky = kSkies[i];
    const ge::render::Weather w = ge::mountain::app::weather_at(levels[i], at);
    std::string title = std::string(sky.id) + ", SEED " + seed_s + " " + view_name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const std::string range =
        w.visibility_m > 0 ? fixed(w.visibility_m, 0) + " m visual range" : "the clear air's range";
    const std::string caption = "Seed " + seed_s + "'s `" + view_name + "` view under " + sky.id +
                                " (sun " + fixed(w.sun_pass * 100, 0) + " % through, deck over " +
                                fixed(w.cover * 100, 0) + " % of the sky, " + range + "), " +
                                std::to_string(frames) + " frames up to " + fixed(at, 1) + " s.";
    return ge::lab::Shot{sky.id,
                         title,
                         caption,
                         sky.look,
                         {{"seed", seed_s},
                          {"sky", sky.id},
                          {"view", view_name},
                          {"t", fixed(at, 2)},
                          {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  auto device = ge::gpu::create_device({.label = "weather", .validation = 1});
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
  std::vector<double> spread(levels.size()), far(levels.size());
  for (size_t i = 0; i < levels.size(); ++i) {
    // A frame is made once a device (its load ends), so each sky's map has a device of its own.
    if (i > 0) {
      errors += device->errors.load();
      late += device->pipelines_after_load;
      device = ge::gpu::create_device({.label = "weather", .validation = 1});
      if (!device) {
        lab.fail("the GPU device could not be made again",
                 "Read the gpu: lines above. Skill: gpu.");
        return;
      }
    }
    const mg::Level& level = levels[i];
    fall_line::app::FrameOptions options;
    auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height),
                                              wgpu::TextureFormat::RGBA8Unorm, options);
    if (!frame) {
      lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    const ge::gpu::Target target = ge::gpu::create_target(
        *device, "weather.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    ge::render::FrameView view;
    fall_line::app::make_view(level, view_name, view);
    for (int n = 0; n < frames; ++n) {
      view.frame = static_cast<uint64_t>(n);
      view.time_s = at - static_cast<double>(frames - 1 - n) / 60;
      fall_line::app::draw(*device, *frame, view, {}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    const ge::lab::Picture pic{width, height, img.rgba};
    lab.shot(shot_of(i), pic, Det::measured);
    double mean = 0, top_mean = 0, top_spread = 0;
    luma_stats(pic, 0, height, mean, spread[i]);
    // The upper half: the far ground and the sky, where the fog closes the view.
    luma_stats(pic, 0, height / 2, top_mean, top_spread);
    far[i] = top_spread;
    const std::string k = kSkies[i].id;
    lab.metric(k + ".luma", mean, "level", Det::measured);
    lab.metric(k + ".contrast", spread[i], "level", Det::measured);
    lab.metric(k + ".far-contrast", top_spread, "level", Det::measured);
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
  // The flat light: an overcast's picture has less contrast than the clear day's; the fog's far
  // half less than the clear day's.
  if (spread[4] >= spread[0]) {
    lab.fail("the overcast's picture is no flatter than the clear day's (" + fixed(spread[4], 1) +
                 " against " + fixed(spread[0], 1) + " levels of spread)",
             "Check deck_luminance and sky_irradiance in engine/render/wgsl/frame.wgsl and the "
             "overcast's row in plugins/mountain/app/weather_look.cpp. Skill: render.");
  }
  if (far[7] >= far[0]) {
    lab.fail("the fog's far half is no flatter than the clear day's (" + fixed(far[7], 1) +
                 " against " + fixed(far[0], 1) + " levels)",
             "Check haze's fog in engine/render/wgsl/frame.wgsl. Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "weather",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "plugins/mountain/app/weather_look.hpp (the mountain's skies) over render/view.hpp's Weather "
    "and engine/render/wgsl/frame.wgsl's light, sky and haze, in Fall Line's frame",
    "render",
    "Does each sky read as itself: the clear day's hard light, a lid's flat grey, the fall's and "
    "the fog's closing view?",
    {
        {"seed", "38", "the map's seed"},
        {"view", "piste", "the named view to shoot from"},
        {"at", "30", "seconds into the run the shot is taken at"},
        {"frames", "8", "frames drawn, at 60 a second, up to the shot"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=2 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
