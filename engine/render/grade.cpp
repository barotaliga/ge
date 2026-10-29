// The grade's knobs on a colour and over a table.
//
// ge:summary The grade's knobs applied to an sRGB-encoded colour and over the post LUT
// ge:keywords render grade knobs contrast saturation white-balance gamma lut
// ge:skill render
// ge:test tests/render_grade_test.cpp
#include "render/grade.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

namespace {

float to_linear(float e) {
  return e <= 0.04045f ? e / 12.92f : std::pow((e + 0.055f) / 1.055f, 2.4f);
}

float to_encoded(float l) {
  return l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
}

float luminance(const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

}  // namespace

bool grade_neutral(const GradeKnobs& k) {
  return k.contrast == 1 && k.saturation == 1 && k.temperature == 0 && k.tint == 0 && k.gamma == 1;
}

void grade_colour(const GradeKnobs& k, float rgb[3]) {
  float l[3];
  for (int c = 0; c < 3; ++c) l[c] = to_linear(std::clamp(rgb[c], 0.0f, 1.0f));
  // White balance: warm lifts red and lowers blue, the tint lowers green; then the luminance
  // the colour had is given back, so only the hue moves.
  const float y0 = luminance(l);
  const auto t = static_cast<float>(std::clamp(k.temperature, -1.0, 1.0));
  const auto m = static_cast<float>(std::clamp(k.tint, -1.0, 1.0));
  l[0] *= 1.0f + 0.18f * t;
  l[2] *= 1.0f - 0.18f * t;
  l[1] *= 1.0f - 0.12f * m;
  const float y1 = luminance(l);
  if (y1 > 0) {
    for (float& v : l) v *= y0 / y1;
  }
  // Saturation about the luminance.
  const auto s = static_cast<float>(std::max(k.saturation, 0.0));
  for (float& v : l) v = std::max(y0 + s * (v - y0), 0.0f);
  // Contrast: an S-curve about 0.5 in the encoded colour, ends and middle held.
  const auto p = static_cast<float>(std::max(k.contrast, 0.05));
  const auto g = static_cast<float>(1.0 / std::max(k.gamma, 0.05));
  for (int c = 0; c < 3; ++c) {
    float e = std::clamp(to_encoded(l[c]), 0.0f, 1.0f);
    e = e < 0.5f ? 0.5f * std::pow(2.0f * e, p) : 1.0f - 0.5f * std::pow(2.0f - 2.0f * e, p);
    rgb[c] = std::clamp(std::pow(e, g), 0.0f, 1.0f);
  }
}

void apply_grade_knobs(std::span<float> table, const GradeKnobs& k) {
  if (grade_neutral(k)) return;
  for (size_t i = 0; i + 2 < table.size(); i += 3) grade_colour(k, &table[i]);
}

}  // namespace ge::render
