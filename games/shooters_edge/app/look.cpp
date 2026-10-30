// ge:summary Shooter's Edge's looks: the switch's options and the palette grade
// ge:keywords look classic modern palette grade quantize
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/look.hpp"

namespace shooters_edge::look {

namespace ba = ge::brush::app;
namespace gr = ge::render;

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

ba::Options world_options(Look look, ba::Options base) {
  if (look == Look::classic) {
    base.mode = ba::Mode::classic;
    base.lighting = ba::Lighting::lightmaps;
    base.crisp = true;
    base.weather = false;
  } else {
    base.mode = ba::Mode::v2;
    base.lighting = ba::Lighting::lights;
    base.crisp = false;
  }
  return base;
}

gr::PostSettings post_settings(Look look) {
  gr::PostSettings s;
  s.bloom = look == Look::classic ? 0 : 0.3;
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
