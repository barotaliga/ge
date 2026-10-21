// The ground's albedo bake.
//
// ge:summary The ground's albedo bake
// ge:keywords fall-line albedo bake
// ge:skill render
#include "fall_line/app/look.hpp"

#include <algorithm>
#include <cmath>

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

}  // namespace fall_line::app
