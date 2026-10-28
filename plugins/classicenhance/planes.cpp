// Wrapping planes: the box blur as a running sum along rows then columns, the percentile by a
// sorted copy, the sRGB curve through a table both ways.
//
// ge:summary The planes' blur, percentile and sRGB tables
// ge:keywords plane blur box running-sum percentile srgb table
// ge:skill classicenhance
// ge:test tests/classicenhance_texture_test.cpp
#include "classicenhance/planes.hpp"

#include <algorithm>
#include <array>

#include "core/libm/libm.hpp"

namespace ge::classicenhance {

namespace {

Plane box(const Plane& p, int r, bool rows) {
  Plane out(p.w, p.h);
  const int n = rows ? p.w : p.h;
  const int lines = rows ? p.h : p.w;
  const float inv = 1.0f / float(2 * r + 1);
  for (int line = 0; line < lines; line++) {
    auto get = [&](int k) { return rows ? p.wrap(k, line) : p.wrap(line, k); };
    double sum = 0;
    for (int k = -r; k <= r; k++) sum += get(k);
    for (int k = 0; k < n; k++) {
      (rows ? out.at(k, line) : out.at(line, k)) = float(sum) * inv;
      sum += get(k + r + 1) - get(k - r);
    }
  }
  return out;
}

const std::array<float, 256>& to_linear_table() {
  static const std::array<float, 256> t = [] {
    std::array<float, 256> a{};
    for (int i = 0; i < 256; i++) {
      const double c = i / 255.0;
      a[size_t(i)] = float(c <= 0.04045 ? c / 12.92 : core::libm::pow((c + 0.055) / 1.055, 2.4));
    }
    return a;
  }();
  return t;
}

// sRGB bytes for linear light quantised to 4096 steps.
const std::array<uint8_t, 4097>& to_srgb_table() {
  static const std::array<uint8_t, 4097> t = [] {
    std::array<uint8_t, 4097> a{};
    for (int i = 0; i <= 4096; i++) {
      const double c = i / 4096.0;
      const double s = c <= 0.0031308 ? c * 12.92 : 1.055 * core::libm::pow(c, 1 / 2.4) - 0.055;
      a[size_t(i)] = uint8_t(std::clamp(s * 255.0 + 0.5, 0.0, 255.0));
    }
    return a;
  }();
  return t;
}

}  // namespace

Plane blur(const Plane& p, int r, int passes) {
  if (r <= 0 || p.w == 0) return p;
  Plane out = p;
  for (int i = 0; i < passes; i++) out = box(box(out, r, true), r, false);
  return out;
}

float percentile(const Plane& p, double share) {
  if (p.v.empty()) return 0;
  std::vector<float> s = p.v;
  const size_t k = std::min(s.size() - 1, size_t(share * double(s.size() - 1) + 0.5));
  std::nth_element(s.begin(), s.begin() + long(k), s.end());
  return s[k];
}

uint8_t linear_to_srgb8(float c) {
  const int i = int(std::clamp(c, 0.0f, 1.0f) * 4096.0f + 0.5f);
  return to_srgb_table()[size_t(i)];
}

float srgb8_to_linear(uint8_t c) { return to_linear_table()[c]; }

}  // namespace ge::classicenhance
