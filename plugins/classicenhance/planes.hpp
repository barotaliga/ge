// The float planes the texture enhancer works in: one channel of a picture that wraps at its
// edges (a texture tiles), with a box blur whose repeat approximates a Gaussian, linear-light
// conversions, and the 8-bit encoding of a plane.
//
// ge:summary Wrapping float planes, their blur and their 8-bit encodings, for texture work
// ge:keywords plane float channel wrap tile blur box gaussian linear srgb encode
// ge:skill classicenhance
// ge:test tests/classicenhance_texture_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace ge::classicenhance {

struct Plane {
  int w = 0, h = 0;
  std::vector<float> v;

  Plane() = default;
  Plane(int width, int height, float fill = 0) : w(width), h(height), v(size_t(width) * size_t(height), fill) {}
  float& at(int x, int y) { return v[size_t(y) * size_t(w) + size_t(x)]; }
  float at(int x, int y) const { return v[size_t(y) * size_t(w) + size_t(x)]; }
  // The texel at (x, y) with both coordinates wrapped into the plane.
  float wrap(int x, int y) const {
    x %= w;
    y %= h;
    return at(x < 0 ? x + w : x, y < 0 ? y + h : y);
  }
};

// The plane blurred by a box of radius `r` (a side of 2r + 1), `passes` times, wrapping.
Plane blur(const Plane& p, int r, int passes = 2);
// The value below which `share` (0..1) of the plane's texels lie.
float percentile(const Plane& p, double share);

// Linear light (0..1) to an sRGB byte, and back.
uint8_t linear_to_srgb8(float c);
float srgb8_to_linear(uint8_t c);

}  // namespace ge::classicenhance
