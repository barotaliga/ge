// ge:summary Shooter's Edge's looks: the switch's options and the palette grade
// ge:keywords look classic modern palette grade quantize r_look r_fx_lights r_fx_glow r_bloom
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/look.hpp"

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
           .fallback = "0.3",
           .help = "the modern look's bloom, 0 none to 1; read when the picture is built",
           .min = 0,
           .max = 1},
          &picture.bloom);
  cv.bind({.name = "r_lava_glow",
           .fallback = "0.2",
           .help = "how much the lava lights the walls round it in the modern look, 0 none",
           .min = 0,
           .max = 4},
          &picture.lava_glow);
  cv.bind({.name = "r_slime_glow",
           .fallback = "1",
           .help = "how much the slime's green glow lights the walls round it in the modern look, "
                   "0 none",
           .min = 0,
           .max = 4},
          &picture.slime_glow);
  ge::classicenhance::bind_switches(cv, &picture.ce);
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

ba::Options world_options(Look look, ba::Options base, const Picture& picture) {
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
    base.pack_sky = !picture.ce.skies;  // ce_skies off: the pack's own scrolling sky
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

}  // namespace shooters_edge::look
