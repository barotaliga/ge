// ge:summary Shooter's Edge's looks: the switch's options and the palette grade
// ge:keywords look classic modern palette grade quantize r_look r_fx_lights r_fx_glow r_bloom
// ge:keywords r_farland r_farland_warm far-land
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/look.hpp"

#include <cmath>

namespace shooters_edge::look {

namespace ba = ge::brush::app;
namespace gr = ge::render;

void add_cvars(ge::console::Console& con, Picture& picture) {
  namespace gc = ge::console;
  auto& cv = con.cvars();
  cv.bind({.name = "r_look",
           .fallback = "modern",
           .help = "the picture: classic (crisp texels, the lightmaps, the palette) or modern "
                   "(filtered, real-time lights, bloom)",
           .kind = gc::CvarKind::text},
          &picture.look);
  // The modern look's real-time lights, exposure and bloom make a spark or a blast's light at the
  // classic's strength wash a room out, so its defaults turn them down.
  cv.bind({.name = "r_fx_lights",
           .fallback = "0.2",
           .help = "how much light explosions, rockets and shots throw in the modern look, the "
                   "classic's 1",
           .min = 0,
           .max = 4},
          &picture.fx_lights);
  cv.bind({.name = "r_fx_glow",
           .fallback = "0.15",
           .help = "how bright sparks and embers glow in the modern look, the classic's 1",
           .min = 0,
           .max = 4},
          &picture.fx_glow);
  cv.bind({.name = "r_bloom",
           .fallback = "0.08",
           .help = "the modern look's bloom, 0 none to 1; read when the picture is built",
           .min = 0,
           .max = 1},
          &picture.bloom);
  cv.bind({.name = "r_lava_glow",
           .fallback = "0.02",
           .help = "how much the lava lights the walls round it in the modern look, 0 none",
           .min = 0,
           .max = 4},
          &picture.lava_glow);
  cv.bind({.name = "r_lerpmodels",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "the models' animation drawn between their frames, so a figure moves smoothly "
                   "(1), or each frame as the game steps it (0)",
           .kind = gc::CvarKind::boolean},
          &picture.lerp_models);
  cv.bind({.name = "r_lerpmove",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "the models' places in your own game drawn between the server's frames, and a "
                   "monster's strides over their time (1), or where each frame puts them (0)",
           .kind = gc::CvarKind::boolean},
          &picture.lerp_move);
  cv.bind({.name = "r_arrival",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "how strongly your own arrival (a spawn, a teleporter's far end) sweeps the "
                   "whole view in a flash and a warp, with ce_warps on in the modern look; 0 none",
           .min = 0,
           .max = 1},
          &picture.arrival);
  cv.bind({.name = "r_slime_glow",
           .fallback = "1",
           .help = "how much the slime's green glow lights the walls round it in the modern look, "
                   "0 none",
           .min = 0,
           .max = 4},
          &picture.slime_glow);
  cv.bind({.name = "r_glow",
           .fallback = "0.6",
           .flags = gc::kArchive,
           .help = "how bright the glowing parts of the walls (windows, lamps' panels) are in the "
                   "modern look, 1 as the classic draws them",
           .min = 0,
           .max = 2},
          &picture.glow);
  cv.bind({.name = "r_fire_glow",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "how bright the torches', braziers' and bonfires' flames burn in the modern "
                   "look, 1 as made",
           .min = 0,
           .max = 4},
          &picture.fire_glow);
  cv.bind({.name = "r_fire_pace",
           .fallback = "0.4",
           .flags = gc::kArchive,
           .help = "how fast the torches', braziers' and bonfires' flames churn and flicker in the "
                   "modern look, 1 the flame presets' own pace (two or three times a real fire's "
                   "at the maps' scale)",
           .min = 0,
           .max = 2},
          &picture.fire_pace);
  cv.bind({.name = "r_farland",
           .fallback = "auto",
           .help = "the land round the map seen over its walls from high up: auto (one to suit "
                   "the map's sky), off, or alps, fjord, highlands, winter, dolomites, sierra, "
                   "volcano, arctic, xeno, ember, rime",
           .kind = gc::CvarKind::text},
          &picture.far_land);
  cv.bind({.name = "r_farland_warm",
           .fallback = "0",
           .help = "the far land's air warmer than its own by this many degrees C (below 0 "
                   "colder: the snow comes down the mountains and the lakes freeze)",
           .min = -40,
           .max = 40},
          &picture.far_warm);
  cv.bind({.name = "gamma",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "the picture's brightness: 1 as drawn, lower brighter (0.5 the brightest the "
                   "menu offers)",
           .min = 0.3,
           .max = 1.5},
          &picture.gamma);
  cv.bind({.name = "r_beam_style",
           .fallback = "3",
           .flags = gc::kArchive,
           .help = "the arc gun's bolt in the modern look (ce_beams): 1 forked (fine kinks), 2 arc "
                   "(a thick plasma arc), 3 bolt (jagged, forking), 4 braid (a spinning rope of "
                   "three)",
           .min = 1,
           .max = 4},
          &picture.beam_style);
  cv.bind({.name = "r_quality",
           .fallback = "custom",
           .flags = gc::kArchive,
           .help = "the shadows, the water and the skies together: custom (each ce_ switch and "
                   "its ce_<id>_quality as set), low, medium, high, top, auto (the most inside "
                   "r_budget) or 0..1 (cheapest to the top, by measured cost)",
           .kind = gc::CvarKind::text},
          &picture.quality);
  cv.bind({.name = "r_budget",
           .fallback = "8.3",
           .flags = gc::kArchive,
           .help = "the GPU time a frame may take with r_quality auto, ms (8.3: 120 Hz)",
           .min = 1,
           .max = 100},
          &picture.budget_ms);
  ge::classicenhance::bind_switches(cv, &picture.ce);
  // Each dear switch's level when it is on (Stops), beside its ce_ switch.
  const auto level = [&](const char* name, const char* help, std::string* field) {
    cv.bind({.name = name,
             .fallback = "high",
             .flags = gc::kArchive,
             .help = help,
             .kind = gc::CvarKind::text},
            field);
  };
  level("ce_shadows_quality",
        "the live lights' shadows with ce_shadows on: low (the walls' alone, no figure's), medium "
        "(the figures' too, hard-edged) or high (soft by each flame's size, from every light)",
        &picture.shadows_quality);
  level("ce_water_quality",
        "the water with ce_water on: low (a still surface lit by the room, no reflection), "
        "medium (its chop and the room reflected off the screen) or high (the room mirrored "
        "whole, the sun's caustics and shafts)",
        &picture.water_quality);
  level("ce_skies_quality",
        "the sky with ce_skies on: low (our sky, its mean cloud, no cloud layer), medium (the "
        "cloud layer too) or high (and the far land round the map seen from high up)",
        &picture.skies_quality);
  ge::brush::app::add_weather_cvars(cv, picture.weather);
}

Look look_of(const Picture& picture) {
  Look look = Look::modern;
  parse_look(picture.look, &look);
  return look;
}

bool parse_look(std::string_view word, Look* out) {
  if (word == "classic") {
    *out = Look::classic;
    return true;
  }
  if (word == "modern") {
    *out = Look::modern;
    return true;
  }
  return false;
}

const char* look_name(Look look) { return look == Look::classic ? "classic" : "modern"; }

bool parse_level(std::string_view word, Stop* out) {
  if (word == "low")
    *out = Stop::low;
  else if (word == "medium")
    *out = Stop::medium;
  else if (word == "high")
    *out = Stop::high;
  else
    return false;
  return true;
}

const char* stop_name(Stop stop) {
  constexpr const char* kNames[] = {"off", "low", "medium", "high"};
  return kNames[static_cast<size_t>(stop)];
}

Stops custom_stops(const Picture& picture) {
  const auto stop = [](bool on, const std::string& level) {
    Stop s = Stop::high;
    if (!on) return Stop::off;
    parse_level(level, &s);
    return s;
  };
  return {stop(picture.ce.shadows, picture.shadows_quality),
          stop(picture.ce.water, picture.water_quality),
          stop(picture.ce.skies, picture.skies_quality)};
}

namespace {

// THE SHADOWS' STOPS on the renderer: the lights whose cubes the moving casters draw into, their
// edges and the capsules. Low keeps the walls' kept cubes alone (no figure casts); medium the
// figures' from the kCasterLights lights that light them most, hard-edged (one compare: their
// soft edges are nearly all of what the casters cost), with no capsules for the other lights; high
// soft by each light's size, and the capsules.
constexpr int kCasterLightsAt[] = {0, 0, ba::kCasterLights, ba::kCasterLights};
constexpr bool kSoftAt[] = {false, false, false, true};
constexpr bool kCapsulesAt[] = {false, false, false, true};

// The map's lights softened near them over this share of their half reach (bsp_draw.hpp's
// Options::near_soft): a torch's wall a few times its half-reach light, never a white disc.
constexpr double kNearSoft = 0.4;

}  // namespace

ba::Options world_options(Look look, ba::Options base, const Picture& picture) {
  return world_options(look, base, picture, custom_stops(picture));
}

ba::Options world_options(Look look, ba::Options base, const Picture& picture, const Stops& stops) {
  if (look == Look::classic) {
    base.mode = ba::Mode::classic;
    base.lighting = ba::Lighting::lightmaps;
    base.crisp = true;
    base.weather = false;
  } else {
    base.mode = ba::Mode::v2;
    base.lighting = ba::Lighting::lights;
    base.crisp = false;
    base.dynamic_strength = picture.fx_lights;
    base.lava_glow = picture.lava_glow;
    base.slime_glow = picture.slime_glow;
    base.fire_glow = picture.fire_glow;
    base.fire_pace = picture.fire_pace;
    base.glow = picture.glow;
    base.near_soft = kNearSoft;
    base.pack_sky = stops.skies == Stop::off;  // ce_skies off: the pack's own scrolling sky
    const auto at = static_cast<size_t>(stops.shadows);
    if (stops.shadows == Stop::off) base.shadowed = 0;  // ce_shadows off: none cast
    base.caster_lights = kCasterLightsAt[at];
    base.soft_casters = kSoftAt[at];
    base.capsules = kCapsulesAt[at];
    // ce_relief and ce_parallax: the enhanced textures' normal, roughness and height maps.
    base.relief = picture.ce.relief;
    if (!picture.ce.parallax) base.parallax = 0;
    // ce_rebake: the light bounced round the rooms from the cache's rebake, under the live lights.
    base.rebake_bounce = picture.ce.rebake;
  }
  return base;
}

double effects_glow(Look look, const Picture& picture) {
  return look == Look::classic ? 1 : picture.fx_glow;
}

gr::PostSettings post_settings(Look look, const Picture& picture) {
  gr::PostSettings s;
  s.bloom = look == Look::classic ? 0 : picture.bloom;
  return s;
}

std::vector<float> palette_grade(std::span<const uint8_t> palette) {
  std::vector<float> table = gr::identity_grade();
  const size_t colours = palette.size() / 3;
  if (colours == 0) return table;
  for (size_t e = 0; e + 2 < table.size(); e += 3) {
    double best = 1e30;
    size_t pick = 0;
    for (size_t c = 0; c < colours; ++c) {
      double d = 0;
      for (size_t k = 0; k < 3; ++k) {
        const double x = double(table[e + k]) - double(palette[c * 3 + k]) / 255.0;
        d += x * x;
      }
      if (d < best) best = d, pick = c;
    }
    for (size_t k = 0; k < 3; ++k) table[e + k] = float(palette[pick * 3 + k]) / 255.0f;
  }
  return table;
}

std::vector<float> gamma_grade(std::vector<float> table, double gamma) {
  if (gamma == 1) return table;
  for (float& v : table) v = static_cast<float>(std::pow(std::fmax(0.0, double(v)), gamma));
  return table;
}

}  // namespace shooters_edge::look
