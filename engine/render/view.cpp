// The camera's matrices: the look basis, the reversed-z infinite projection, the inverse and the
// frustum planes pulled from the product's rows.
//
// ge:summary Camera matrices, the 4x4 inverse and the frustum planes
// ge:keywords render camera matrix inverse frustum-planes reverse-z
// ge:skill render
// ge:test tests/render_view_test.cpp
#include "render/view.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace ge::render {

Mat4 mul(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0;
      for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

void transform(const Mat4& a, const float in[4], float out[4]) {
  for (int row = 0; row < 4; ++row) {
    out[row] =
        a.m[row] * in[0] + a.m[4 + row] * in[1] + a.m[8 + row] * in[2] + a.m[12 + row] * in[3];
  }
}

Mat4 inverse(const Mat4& a) {
  // Cofactors in double, so the inverse of a projection with a 0.1 m near plane keeps its bits.
  double m[16], inv[16];
  for (int i = 0; i < 16; ++i) m[i] = a.m[i];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
           m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
           m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
           m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
           m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
           m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
           m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
           m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
           m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  Mat4 r{};
  if (det == 0) {
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
  }
  for (int i = 0; i < 16; ++i) r.m[i] = static_cast<float>(inv[i] / det);
  return r;
}

void look_dir(double heading, double pitch, double out[3]) {
  out[0] = std::sin(heading) * std::cos(pitch);
  out[1] = std::sin(pitch);
  out[2] = std::cos(heading) * std::cos(pitch);
}

void sun_direction(const FrameView& view, double out[3]) {
  look_dir(view.sun_azimuth, view.sun_elevation, out);
}

void real_sun_direction(const FrameView& view, double out[3]) {
  if (view.night.moon_key) {
    look_dir(view.night.sun_azimuth, view.night.sun_elevation, out);
  } else {
    sun_direction(view, out);
  }
}

CameraMatrices camera_matrices(const FrameView& view, double aspect) {
  double f[3];
  look_dir(view.heading, view.pitch, f);
  // right = up x forward (the world is left-handed), up' = forward x right.
  double r[3] = {f[2], 0, -f[0]};
  const double rl = std::sqrt(r[0] * r[0] + r[2] * r[2]);
  if (rl < 1e-9) {
    r[0] = std::cos(view.heading);
    r[2] = -std::sin(view.heading);
  } else {
    r[0] /= rl;
    r[2] /= rl;
  }
  double u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
  if (view.roll != 0) {
    // Rolled right side down: the right swings down toward -up, the up toward +right. At a roll
    // of exactly 0 nothing is touched, so no unrolled picture moves.
    const double c = std::cos(view.roll), s = std::sin(view.roll);
    for (int i = 0; i < 3; ++i) {
      const double ri = r[i], ui = u[i];
      r[i] = ri * c - ui * s;
      u[i] = ui * c + ri * s;
    }
  }
  CameraMatrices c{};
  // Rows of the view rotation: right, up, -forward.
  for (int i = 0; i < 3; ++i) {
    c.view.m[i * 4 + 0] = static_cast<float>(r[i]);
    c.view.m[i * 4 + 1] = static_cast<float>(u[i]);
    c.view.m[i * 4 + 2] = static_cast<float>(-f[i]);
  }
  c.view.m[15] = 1;
  const double g = 1.0 / std::tan(view.fov_y * 0.5);
  c.proj.m[0] = static_cast<float>(g / aspect);
  c.proj.m[5] = static_cast<float>(g);
  c.proj.m[14] = static_cast<float>(view.near_m);  // clip.z = near
  c.proj.m[11] = -1;                               // clip.w = -z_view
  c.view_proj = mul(c.proj, c.view);
  c.inv_view_proj = inverse(c.view_proj);
  // Planes off the rows of the product: w + x, w - x, w + y, w - y, w - z (depth <= 1).
  const float* m = c.view_proj.m;
  auto row = [m](int i, float out[4]) {
    for (int k = 0; k < 4; ++k) out[k] = m[k * 4 + i];
  };
  float rx[4], ry[4], rz[4], rw[4];
  row(0, rx);
  row(1, ry);
  row(2, rz);
  row(3, rw);
  for (int k = 0; k < 4; ++k) {
    c.planes[0][k] = rw[k] + rx[k];
    c.planes[1][k] = rw[k] - rx[k];
    c.planes[2][k] = rw[k] + ry[k];
    c.planes[3][k] = rw[k] - ry[k];
    c.planes[4][k] = rw[k] - rz[k];
  }
  for (auto& p : c.planes) {
    const float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    if (l > 0) {
      for (float& v : p) v /= l;
    }
  }
  return c;
}

double depth_distance(const CameraMatrices& m, double ndc_x, double ndc_y, float depth) {
  if (!(depth > 0)) return std::numeric_limits<double>::infinity();
  const float clip[4] = {static_cast<float>(ndc_x), static_cast<float>(ndc_y), 1.0f, 1.0f};
  float p[4];
  transform(m.inv_view_proj, clip, p);
  const double x = p[0] / p[3], y = p[1] / p[3], z = p[2] / p[3];
  return std::sqrt(x * x + y * y + z * z) / depth;
}

void flash_rows(const Flashes& flashes, double pre, float info[4], float rows[][4]) {
  // The sun's illuminance is 128000 lx a unit (render/night.hpp's kSunLux); a lamp's light rides
  // as its whole flux, 4 pi times its intensity, so flash.wgsl's L / (4 pi r^2) is its lux.
  constexpr double kLux = 128000, kFourPi = 4 * 3.14159265358979;
  const int n = flashes.count < 0 ? 0 : (flashes.count > kFlashLamps ? kFlashLamps : flashes.count);
  info[0] = static_cast<float>(n);
  info[1] = info[2] = info[3] = 0;
  for (int k = 0; k < kFlashLamps; ++k) {
    float* a = rows[static_cast<ptrdiff_t>(2) * k];
    float* b = rows[static_cast<ptrdiff_t>(2) * k + 1];
    for (int c = 0; c < 4; ++c) a[c] = b[c] = 0;
    if (k >= n) continue;
    const FlashLamp& l = flashes.lamp[k];
    for (int c = 0; c < 3; ++c) {
      a[c] = static_cast<float>(l.at[c]);
      b[c] = static_cast<float>(l.tint[c] * l.candela * kFourPi / kLux * pre);
    }
    a[3] = static_cast<float>(l.radius_m);
    b[3] = static_cast<float>(l.out_tau);
  }
}

}  // namespace ge::render
