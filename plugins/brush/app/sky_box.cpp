// plugins/brush/app/sky_box.cpp: a map's own sky box: six pictures resampled into the scene's
// atlas in the shader's face order.
//
// ge:summary The sky box's six pictures resampled into one atlas, three faces wide and two high
// ge:keywords sky-box skybox build_sky_box resample bilinear cube-map
// ge:skill brush render
// ge:test tests/brush_sky_box_test.cpp
#include "brush/app/sky_box.hpp"

#include <algorithm>
#include <cstddef>

namespace ge::brush::app {
namespace {

double dot(const int a[3], const double d[3]) { return a[0] * d[0] + a[1] * d[1] + a[2] * d[2]; }

// The picture's colour at (u, v) in 0..1 across and down, bilinear between its texel centres,
// clamped at its edges.
void read_bilinear(const assets::Image& p, double u, double v, uint8_t out[4]) {
  const double x = std::clamp(u * p.width - 0.5, 0.0, double(p.width - 1));
  const double y = std::clamp(v * p.height - 0.5, 0.0, double(p.height - 1));
  const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
  const int x1 = std::min(x0 + 1, p.width - 1), y1 = std::min(y0 + 1, p.height - 1);
  const double fx = x - x0, fy = y - y0;
  const auto at = [&](int xx, int yy, int c) {
    return double(p.rgba[(size_t(yy) * size_t(p.width) + size_t(xx)) * 4 + size_t(c)]);
  };
  for (int c = 0; c < 3; ++c) {
    const double top = at(x0, y0, c) + (at(x1, y0, c) - at(x0, y0, c)) * fx;
    const double bottom = at(x0, y1, c) + (at(x1, y1, c) - at(x0, y1, c)) * fx;
    out[c] = static_cast<uint8_t>(std::clamp(top + (bottom - top) * fy + 0.5, 0.0, 255.0));
  }
  out[3] = 255;
}

}  // namespace

bool build_sky_box(const std::array<assets::Image, 6>& pictures, Scene* scene, std::string* why) {
  scene->sky_box = {};
  scene->sky_box_face = 0;
  for (int i = 0; i < 6; ++i) {
    const assets::Image& p = pictures[size_t(i)];
    if (p.width <= 0 || p.height <= 0 ||
        p.rgba.size() < size_t(p.width) * size_t(p.height) * 4) {
      if (why != nullptr) {
        *why = std::string("the sky box's ") + kSkyBoxSuffixes[i] + " picture is empty";
      }
      return false;
    }
  }
  const int n = std::min({pictures[0].width, pictures[0].height, kSkyBoxMaxFace});
  Atlas atlas;
  atlas.width = uint32_t(n) * 3;
  atlas.height = uint32_t(n) * 2;
  atlas.levels.emplace_back(size_t(atlas.width) * atlas.height * 4, uint8_t{0});
  std::vector<uint8_t>& texels = atlas.levels[0];
  for (int f = 0; f < 6; ++f) {
    const int* look = kSkyBoxLook[f];
    const int* up = kSkyBoxUp[f];
    // right = cross(up, look)
    const int right[3] = {up[1] * look[2] - up[2] * look[1], up[2] * look[0] - up[0] * look[2],
                          up[0] * look[1] - up[1] * look[0]};
    // The picture that looks the same way.
    int from = 0;
    for (int i = 0; i < 6; ++i) {
      const SkyBoxPicture& q = kSkyBoxPictures[i];
      if (q.look[0] == look[0] && q.look[1] == look[1] && q.look[2] == look[2]) from = i;
    }
    const SkyBoxPicture& q = kSkyBoxPictures[from];
    const assets::Image& p = pictures[size_t(from)];
    const uint32_t ox = uint32_t(f % 3) * uint32_t(n), oy = uint32_t(f / 3) * uint32_t(n);
    for (int j = 0; j < n; ++j) {
      const double t = 1.0 - (j + 0.5) * 2.0 / n;
      for (int i = 0; i < n; ++i) {
        const double s = (i + 0.5) * 2.0 / n - 1.0;
        const double d[3] = {look[0] + s * right[0] + t * up[0], look[1] + s * right[1] + t * up[1],
                             look[2] + s * right[2] + t * up[2]};
        // d meets the picture's plane at dot(d, look) = 1 already (the same axis).
        const double ps = dot(q.right, d), pt = dot(q.top, d);
        uint8_t* out = &texels[((size_t(oy) + size_t(j)) * atlas.width + ox + size_t(i)) * 4];
        read_bilinear(p, ps * 0.5 + 0.5, 0.5 - pt * 0.5, out);
      }
    }
  }
  scene->sky_box = std::move(atlas);
  scene->sky_box_face = uint32_t(n);
  return true;
}

}  // namespace ge::brush::app
