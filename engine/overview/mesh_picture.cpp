// A mesh drawn small on the CPU (mesh_picture.hpp).
//
// ge:summary A textured mesh rastered small on the CPU: a cut-out icon or a figure at its size
// ge:keywords overview figure icon raster cut-out
// ge:skill map-design ui
#include "overview/mesh_picture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "overview/plate.hpp"

namespace ge::overview {

namespace ui = ge::ui;

namespace {

// Each vertex's normal: the area-weighted sum of the normals of the faces that meet at its place
// (positions equal to the bit), normalised; three a vertex.
std::vector<double> smooth_normals(const Mesh& m, const std::vector<float>& wx,
                                   const std::vector<float>& wy, const std::vector<float>& wz) {
  const size_t nv = wx.size();
  std::vector<double> at(nv * 3, 0);
  for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
    const uint32_t v[3] = {m.idx[t], m.idx[t + 1], m.idx[t + 2]};
    const double e1[3] = {double(wx[v[1]]) - wx[v[0]], double(wy[v[1]]) - wy[v[0]],
                          double(wz[v[1]]) - wz[v[0]]};
    const double e2[3] = {double(wx[v[2]]) - wx[v[0]], double(wy[v[2]]) - wy[v[0]],
                          double(wz[v[2]]) - wz[v[0]]};
    const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                         e1[0] * e2[1] - e1[1] * e2[0]};
    for (const uint32_t k : v)
      for (size_t q = 0; q < 3; ++q) at[size_t(k) * 3 + q] += n[q];
  }
  // A seam's copies of one place summed together.
  std::map<std::array<float, 3>, std::array<double, 3>> by_place;
  for (size_t i = 0; i < nv; ++i) {
    std::array<double, 3>& s = by_place[{wx[i], wy[i], wz[i]}];
    for (size_t q = 0; q < 3; ++q) s[q] += at[i * 3 + q];
  }
  for (size_t i = 0; i < nv; ++i) {
    const std::array<double, 3>& s = by_place[{wx[i], wy[i], wz[i]}];
    const double l = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    for (size_t q = 0; q < 3; ++q) at[i * 3 + q] = l > 1e-12 ? s[q] / l : 0;
  }
  return at;
}

// How a picture is laid out and shaded beyond its view: its size, where the origin falls (a
// fraction of the width and of the height; fitted pictures ignore it), the dark rim, and smooth
// shading (each vertex's normal the mean of its faces' at its place) in place of flat.
struct PictureFrame {
  int width = 0, height = 0;
  double origin_x = 0.5, origin_y = 0.5;
  bool rim = true;
  bool smooth = false;
};

// `m` seen from `azimuth_deg` round z and `elevation_deg` up, `kSS` samples a pixel each way:
// fitted to the picture when `px_per_unit` is 0, else at that size with the origin where `f` puts
// it; turned to `yaw_deg` first.
ui::Image raster(const Mesh& m, const PictureFrame& f, double azimuth_deg, double elevation_deg,
                 double yaw_deg, double pitch_deg, double px_per_unit, int kSS, float bright = 1) {
  const int size = f.width, size_h = f.height;
  const int S = size * kSS, SH = size_h * kSS;
  const double az = azimuth_deg * 3.14159265358979 / 180,
               el = elevation_deg * 3.14159265358979 / 180;
  const double d[3] = {ge::core::libm::cos(az) * ge::core::libm::cos(el),
                       ge::core::libm::sin(az) * ge::core::libm::cos(el),
                       ge::core::libm::sin(el)};  // to the eye
  double r[3] = {-d[1], d[0], 0};
  const double rl = ge::core::hypot(r[0], r[1]);
  r[0] /= rl;
  r[1] /= rl;
  const double u[3] = {d[1] * r[2] - d[2] * r[1], d[2] * r[0] - d[0] * r[2],
                       d[0] * r[1] - d[1] * r[0]};
  const size_t nv = m.pos.size() / 3;
  std::vector<double> sx(nv), sy(nv), sz(nv);
  std::vector<float> wx(nv), wy(nv), wz(nv);
  double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
  const double yc = ge::core::libm::cos(yaw_deg * 3.14159265358979 / 180),
               ys = ge::core::libm::sin(yaw_deg * 3.14159265358979 / 180);
  const double pcos = ge::core::libm::cos(pitch_deg * 3.14159265358979 / 180),
               psin = ge::core::libm::sin(pitch_deg * 3.14159265358979 / 180);
  for (size_t i = 0; i < nv; ++i) {
    float turned[3];
    const float* p = &m.pos[i * 3];
    if (yaw_deg != 0 || pitch_deg != 0) {
      // The nose up by the pitch (round the model's y), then turned round z by the yaw.
      const double x = pcos * p[0] - psin * p[2], z = psin * p[0] + pcos * p[2];
      turned[0] = float(yc * x - ys * p[1]);
      turned[1] = float(ys * x + yc * p[1]);
      turned[2] = float(z);
      p = turned;
    }
    wx[i] = p[0];
    wy[i] = p[1];
    wz[i] = p[2];
    sx[i] = p[0] * r[0] + p[1] * r[1] + p[2] * r[2];
    sy[i] = -(p[0] * u[0] + p[1] * u[1] + p[2] * u[2]);
    sz[i] = p[0] * d[0] + p[1] * d[1] + p[2] * d[2];
    lo[0] = std::min(lo[0], sx[i]);
    hi[0] = std::max(hi[0], sx[i]);
    lo[1] = std::min(lo[1], sy[i]);
    hi[1] = std::max(hi[1], sy[i]);
  }
  const double fit = px_per_unit > 0 ? px_per_unit * kSS
                                     : 0.84 * S / std::max({hi[0] - lo[0], hi[1] - lo[1], 1e-6});
  const double cx = px_per_unit > 0 ? 0 : (lo[0] + hi[0]) / 2,
               cy = px_per_unit > 0 ? 0 : (lo[1] + hi[1]) / 2;
  const double ox = px_per_unit > 0 ? S * f.origin_x : S / 2.0,
               oy = px_per_unit > 0 ? SH * f.origin_y : SH / 2.0;
  for (size_t i = 0; i < nv; ++i) {
    sx[i] = ox + (sx[i] - cx) * fit;
    sy[i] = oy + (sy[i] - cy) * fit;
  }
  std::vector<double> depth(size_t(S) * size_t(SH), -1e30);
  std::vector<float> col(size_t(S) * size_t(SH) * 3, 0);
  const double key[3] = {-0.35, 0.45, 0.82};
  // Smooth shading: each vertex's normal the sum of the faces' (by area) meeting at its place, so
  // a seam's copies of a point share one.
  std::vector<double> vn;
  if (f.smooth) vn = smooth_normals(m, wx, wy, wz);
  for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
    const uint32_t a = m.idx[t], b = m.idx[t + 1], c = m.idx[t + 2];
    const float pa[3] = {wx[a], wy[a], wz[a]};
    const float pb[3] = {wx[b], wy[b], wz[b]};
    const float pc[3] = {wx[c], wy[c], wz[c]};
    double e1[3], e2[3], n[3];
    for (int q = 0; q < 3; ++q) e1[q] = pb[q] - pa[q], e2[q] = pc[q] - pa[q];
    n[0] = e1[1] * e2[2] - e1[2] * e2[1];
    n[1] = e1[2] * e2[0] - e1[0] * e2[2];
    n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (nl < 1e-12) continue;
    for (double& q : n) q /= nl;
    if (n[0] * d[0] + n[1] * d[1] + n[2] * d[2] < 0)
      for (double& q : n) q = -q;
    const auto shade_of = [&](const double* nn) {
      const double lambert = std::max(0.0, nn[0] * key[0] + nn[1] * key[1] + nn[2] * key[2]);
      const double rim =
          ge::core::libm::pow(1 - std::fabs(nn[0] * d[0] + nn[1] * d[1] + nn[2] * d[2]), 3.0);
      return float(0.5 + 0.62 * lambert + 0.25 * rim) * bright;
    };
    const float flat = shade_of(n);
    const uint32_t vv[3] = {a, b, c};
    const double area = (sx[b] - sx[a]) * (sy[c] - sy[a]) - (sx[c] - sx[a]) * (sy[b] - sy[a]);
    if (std::fabs(area) < 1e-12) continue;
    const int x0 = std::max(0, int(std::min({sx[a], sx[b], sx[c]})));
    const int x1 = std::min(S - 1, int(std::max({sx[a], sx[b], sx[c]})) + 1);
    const int y0 = std::max(0, int(std::min({sy[a], sy[b], sy[c]})));
    const int y1 = std::min(SH - 1, int(std::max({sy[a], sy[b], sy[c]})) + 1);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const double px = x + 0.5, py = y + 0.5;
        double w[3];
        w[0] = ((sx[b] - px) * (sy[c] - py) - (sx[c] - px) * (sy[b] - py)) / area;
        w[1] = ((sx[c] - px) * (sy[a] - py) - (sx[a] - px) * (sy[c] - py)) / area;
        w[2] = 1 - w[0] - w[1];
        if (w[0] < 0 || w[1] < 0 || w[2] < 0) continue;
        const double z = w[0] * sz[a] + w[1] * sz[b] + w[2] * sz[c];
        const size_t at = size_t(y) * size_t(S) + size_t(x);
        if (z <= depth[at]) continue;
        depth[at] = z;
        float tc[3] = {0.6f, 0.6f, 0.6f};
        if (m.tw > 0) {
          double s = 0, tt = 0;
          for (int q = 0; q < 3; ++q)
            s += w[q] * m.st[size_t(vv[q]) * 2], tt += w[q] * m.st[size_t(vv[q]) * 2 + 1];
          const float* rc = &m.rect[size_t(a) * 4];
          const auto X = int(rc[0] + wrap(s, std::max(1.0f, rc[2])));
          const auto Y = int(rc[1] + wrap(tt, std::max(1.0f, rc[3])));
          const uint8_t* p = &m.tex[(size_t(std::clamp(Y, 0, m.th - 1)) * size_t(m.tw) +
                                     size_t(std::clamp(X, 0, m.tw - 1))) *
                                    4];
          tc[0] = p[0] / 255.0f, tc[1] = p[1] / 255.0f, tc[2] = p[2] / 255.0f;
        }
        float shade = flat;
        if (f.smooth) {  // the vertices' normals across the face, turned to the eye's side
          double nn[3];
          for (int q = 0; q < 3; ++q)
            nn[q] = w[0] * vn[size_t(a) * 3 + size_t(q)] + w[1] * vn[size_t(b) * 3 + size_t(q)] +
                    w[2] * vn[size_t(c) * 3 + size_t(q)];
          const double l = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
          if (l > 1e-12) {
            for (double& q : nn) q /= l;
            if (nn[0] * d[0] + nn[1] * d[1] + nn[2] * d[2] < 0)
              for (double& q : nn) q = -q;
            shade = shade_of(nn);
          }
        }
        for (int q = 0; q < 3; ++q) col[at * 3 + size_t(q)] = std::min(1.0f, tc[q] * shade);
      }
    }
  }
  // Down to `size`, coverage as alpha; then a dark rim two pixels round the cut-out.
  ui::Image img = ui::make_image(size, size_h, {0, 0, 0, 0});
  std::vector<float> alpha(size_t(size) * size_t(size_h), 0);
  for (int Y = 0; Y < size_h; ++Y)
    for (int X = 0; X < size; ++X) {
      float acc[3] = {0, 0, 0};
      int cover = 0;
      for (int j = 0; j < kSS; ++j)
        for (int i = 0; i < kSS; ++i) {
          const size_t at = size_t(Y * kSS + j) * size_t(S) + size_t(X * kSS + i);
          if (depth[at] < -1e29) continue;
          ++cover;
          for (int q = 0; q < 3; ++q) acc[q] += col[at * 3 + size_t(q)];
        }
      const size_t o = size_t(Y) * size_t(size) + size_t(X);
      alpha[o] = float(cover) / float(kSS * kSS);
      if (cover > 0)
        for (int q = 0; q < 3; ++q)
          img.rgba[o * 4 + size_t(q)] = uint8_t(std::lround(acc[q] / float(cover) * 255));
    }
  if (!f.rim) {  // coverage alone
    for (size_t o = 0; o < alpha.size(); ++o)
      img.rgba[o * 4 + 3] = uint8_t(std::lround(std::clamp(alpha[o], 0.0f, 1.0f) * 255));
    return img;
  }
  for (int Y = 0; Y < size_h; ++Y)
    for (int X = 0; X < size; ++X) {
      const size_t o = size_t(Y) * size_t(size) + size_t(X);
      float rim = 0;
      for (int j = -2; j <= 2; ++j)
        for (int i = -2; i <= 2; ++i) {
          if (i * i + j * j > 5) continue;
          const int x = X + i, y = Y + j;
          if (x >= 0 && y >= 0 && x < size && y < size_h)
            rim = std::max(rim, alpha[size_t(y) * size_t(size) + size_t(x)]);
        }
      const float a = alpha[o];
      // The model over its rim: the rim a near-black 85 %.
      for (int q = 0; q < 3; ++q) {
        const float mc = img.rgba[o * 4 + size_t(q)] / 255.0f;
        const float out_a = a + rim * 0.85f * (1 - a);
        const float v = out_a > 0 ? (mc * a + 0.04f * rim * 0.85f * (1 - a)) / out_a : 0;
        img.rgba[o * 4 + size_t(q)] = uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255));
      }
      img.rgba[o * 4 + 3] =
          uint8_t(std::lround(std::clamp(a + rim * 0.85f * (1 - a), 0.0f, 1.0f) * 255));
    }
  return img;
}

}  // namespace

ui::Image draw_icon(const Mesh& m, int size, double azimuth_deg, double elevation_deg) {
  return raster(m, PictureFrame{.width = size, .height = size}, azimuth_deg, elevation_deg, 0, 0, 0,
                3);
}

ui::Image draw_figure(const Mesh& m, int size, double yaw_deg, double pitch_deg,
                      double elevation_deg, double px_per_unit) {
  return raster(m, PictureFrame{.width = size, .height = size}, -90, elevation_deg, yaw_deg,
                pitch_deg, std::max(px_per_unit, 1e-3), 2, 1.4f);
}

ui::Image draw_turned(const Mesh& m, const TurnView& v) {
  if (v.width <= 0 || v.height <= 0) return {};
  const PictureFrame f{.width = v.width,
                       .height = v.height,
                       .origin_x = v.origin_x,
                       .origin_y = v.origin_y,
                       .rim = false,
                       .smooth = true};
  return raster(m, f, -90, v.elevation_deg, v.yaw_deg, 0, std::max(v.px_per_unit, 1e-3),
                std::clamp(v.supersample, 1, 4), v.bright);
}

}  // namespace ge::overview
