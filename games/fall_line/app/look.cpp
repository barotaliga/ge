// The ground's albedo bake and the grade's table.
//
// ge:summary The ground's albedo bake
// ge:keywords fall-line albedo bake
// ge:skill render
#include "fall_line/app/look.hpp"

#include <algorithm>
#include <cmath>

#include "render/post.hpp"

namespace fall_line::app {

namespace {

// Linear albedos.
constexpr float kSnow[3] = {0.90f, 0.92f, 0.95f};
constexpr float kPiste[3] = {0.80f, 0.83f, 0.88f};
constexpr float kRock[3] = {0.20f, 0.19f, 0.18f};
// Snow holds below this slope (rise over run) and is gone above the second.
constexpr double kHolds = 1.0;  // 45 degrees
constexpr double kBare = 1.6;   // about 58 degrees

uint8_t encode(float linear) {
  const float c = std::clamp(linear, 0.0f, 1.0f);
  const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
  return static_cast<uint8_t>(std::lround(s * 255.0f));
}

}  // namespace

std::vector<uint8_t> bake_albedo(const ge::mountain::mapgen::Level& level) {
  const auto& g = level.ground;
  std::vector<uint8_t> out(static_cast<size_t>(g.cols) * static_cast<size_t>(g.rows) * 4);
  const bool packed = g.cols == level.packed.cols && g.rows == level.packed.rows;
  for (int r = 0; r < g.rows; ++r) {
    for (int c = 0; c < g.cols; ++c) {
      const size_t i =
          static_cast<size_t>(r) * static_cast<size_t>(g.cols) + static_cast<size_t>(c);
      const auto at = [&](int cc, int rr) {
        cc = std::clamp(cc, 0, g.cols - 1);
        rr = std::clamp(rr, 0, g.rows - 1);
        return static_cast<double>(g.data[static_cast<size_t>(rr) * static_cast<size_t>(g.cols) +
                                          static_cast<size_t>(cc)]);
      };
      const double gx = (at(c + 1, r) - at(c - 1, r)) / (2 * g.cell);
      const double gz = (at(c, r + 1) - at(c, r - 1)) / (2 * g.cell);
      const double slope = std::sqrt(gx * gx + gz * gz);
      const float rock =
          static_cast<float>(std::clamp((slope - kHolds) / (kBare - kHolds), 0.0, 1.0));
      const float pack = packed ? std::clamp(level.packed.data[i], 0.0f, 1.0f) : 0.0f;
      for (int k = 0; k < 3; ++k) {
        const float snow = kSnow[k] + (kPiste[k] - kSnow[k]) * pack;
        out[i * 4 + static_cast<size_t>(k)] = encode(snow + (kRock[k] - snow) * rock);
      }
      out[i * 4 + 3] = 255;
    }
  }
  return out;
}

std::vector<float> bake_grade() {
  std::vector<float> t = ge::render::identity_grade();
  for (size_t k = 0; k < t.size(); k += 3) {
    float e[3] = {t[k], t[k + 1], t[k + 2]};
    // Contrast through the middle: e + a (e - 1/2)(1 - |2e - 1|), rising everywhere, ends held.
    for (float& c : e) c += 0.06f * (c - 0.5f) * (1.0f - std::fabs(2.0f * c - 1.0f));
    const float y = 0.2126f * e[0] + 0.7152f * e[1] + 0.0722f * e[2];
    // Cool shadows, warm highlights.
    const float shade = (1.0f - y) * (1.0f - y), light = y * y;
    constexpr float kCool[3] = {-0.006f, 0.0f, 0.012f};
    constexpr float kWarm[3] = {0.008f, 0.003f, -0.006f};
    for (int c = 0; c < 3; ++c) {
      const float v = y + 1.05f * (e[c] - y) + kCool[c] * shade + kWarm[c] * light;
      t[k + static_cast<size_t>(c)] = std::clamp(v, 0.0f, 1.0f);
    }
  }
  return t;
}

}  // namespace fall_line::app
