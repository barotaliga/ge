// The bake's passes: the luxels found on their charts, the direct light, the gathers, the
// smoothing and the fill.
//
// ge:summary Defines bake_scene: luxel setup, direct light, gather passes, filter and dilation
// ge:keywords bake lightmap gather bounce direct-light shadow-ray dilate filter luxel
// ge:skill render perf
// ge:test tests/bake_bake_test.cpp
#include "bake/bake.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

#include "bake/chart.hpp"
#include "bake/trace.hpp"
#include "core/parallel.hpp"
#include "core/prng.hpp"

namespace ge::bake {

namespace {

constexpr double kPi = 3.141592653589793;
constexpr double kBias = 0.01;         // m: rays start this far off their face
constexpr double kInsideShare = 0.25;  // a luxel whose rays meet more backs than this is inside

double lum(const double c[3]) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }
double dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

uint32_t mix(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u + (a << 6) + (a >> 2));
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return h;
}

struct Luxel {
  double p[3];  // the sample point, on the face
  uint32_t chart;
  uint32_t atlas;  // index into the atlas
};

struct Frame {
  double n[3], u[3], v[3];  // unit
};

// The nearest point of triangle (a, b, c) to q, all in a chart's (s, t), and whether q was inside.
bool nearest_2d(const double a[2], const double b[2], const double c[2], const double q[2],
                double out[2]) {
  auto cross = [](const double o[2], const double x[2], const double y[2]) {
    return (x[0] - o[0]) * (y[1] - o[1]) - (x[1] - o[1]) * (y[0] - o[0]);
  };
  const double area = cross(a, b, c);
  const double w0 = cross(b, c, q), w1 = cross(c, a, q), w2 = cross(a, b, q);
  const bool inside = area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
  if (inside) {
    out[0] = q[0];
    out[1] = q[1];
    return true;
  }
  double best = 1e300;
  const double* e[3][2] = {{a, b}, {b, c}, {c, a}};
  for (const auto& ed : e) {
    const double dx = ed[1][0] - ed[0][0], dy = ed[1][1] - ed[0][1];
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((q[0] - ed[0][0]) * dx + (q[1] - ed[0][1]) * dy) / len2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const double px = ed[0][0] + dx * t, py = ed[0][1] + dy * t;
    const double d2 = (q[0] - px) * (q[0] - px) + (q[1] - py) * (q[1] - py);
    if (d2 < best) {
      best = d2;
      out[0] = px;
      out[1] = py;
    }
  }
  return false;
}

// A cosine-weighted direction about the frame's normal (a disc point lifted onto the hemisphere;
// the disc drawn by rejection, so no transcendental is needed).
void cosine_dir(core::Rng& rng, const Frame& f, double out[3]) {
  double x = 0, y = 0;
  do {
    x = core::next(rng) * 2 - 1;
    y = core::next(rng) * 2 - 1;
  } while (x * x + y * y >= 1);
  const double z = std::sqrt(std::max(0.0, 1 - x * x - y * y));
  for (int k = 0; k < 3; k++) out[k] = f.u[k] * x + f.v[k] * y + f.n[k] * z;
}

// A point in the ball of radius r (rejection from the cube).
void ball_point(core::Rng& rng, double r, double out[3]) {
  double x = 0, y = 0, z = 0;
  do {
    x = core::next(rng) * 2 - 1;
    y = core::next(rng) * 2 - 1;
    z = core::next(rng) * 2 - 1;
  } while (x * x + y * y + z * z > 1);
  out[0] = x * r;
  out[1] = y * r;
  out[2] = z * r;
}

Ray make_ray(const double o[3], const double d[3], double t_max) {
  Ray r;
  for (int k = 0; k < 3; k++) {
    r.o[k] = static_cast<float>(o[k]);
    r.d[k] = static_cast<float>(d[k]);
  }
  r.t_max = static_cast<float>(t_max);
  return r;
}

double smooth(double lo, double hi, double x) {
  const double t = std::clamp((x - lo) / (hi - lo), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

// The direct light at luxel `l` (irradiance, and the luminance-weighted direction sum).
void direct_at(const Scene& scene, const Bvh& bvh, const Luxel& l, const Frame& f, const Options& o,
               core::Rng& rng, double rgb[3], double dirsum[3], uint64_t* rays) {
  double from[3];
  for (int k = 0; k < 3; k++) from[k] = l.p[k] + f.n[k] * kBias;
  for (const Light& light : scene.lights) {
    double to[3], dist = 0, cosn = 0, scale = 0;
    if (light.kind == LightKind::sun) {
      for (int k = 0; k < 3; k++) to[k] = -light.dir[k];
      cosn = dot3(to, f.n);
      if (cosn <= 0) continue;
      // Visibility: the share of rays toward points of the sun's disc that reach the sky.
      const int n = light.radius > 0 ? std::max(1, o.light_samples) : 1;
      int seen = 0;
      for (int s = 0; s < n; s++) {
        double d[3] = {to[0], to[1], to[2]};
        if (n > 1) {
          double j[3];
          ball_point(rng, light.radius, j);
          for (int k = 0; k < 3; k++) d[k] += j[k];
        }
        (*rays)++;
        const Hit h = closest_hit(bvh, make_ray(from, d, 1e30));
        if (!h.hit() || scene.materials[scene.tri_material[h.tri]].sky) seen++;
      }
      scale = cosn * seen / n;
    } else {
      for (int k = 0; k < 3; k++) to[k] = light.pos[k] - l.p[k];
      dist = std::sqrt(dot3(to, to));
      if (dist <= 0 || (light.reach > 0 && dist >= light.reach)) continue;
      for (int k = 0; k < 3; k++) to[k] /= dist;
      cosn = dot3(to, f.n);
      if (cosn <= 0) continue;
      double fall = cosn / (dist * dist + light.radius * light.radius);
      if (light.reach > 0) {
        const double q = dist / light.reach, w = 1 - q * q * q * q;
        fall *= w * w;
      }
      if (light.kind == LightKind::spot) {
        const double c = -dot3(to, light.dir);
        fall *= smooth(light.cone_cos[1], light.cone_cos[0], c);
      }
      if (fall <= 0) continue;
      // Visibility: the share of rays to points over the light's ball that arrive.
      const int n = std::max(1, o.light_samples);
      int seen = 0;
      for (int s = 0; s < n; s++) {
        double target[3] = {light.pos[0], light.pos[1], light.pos[2]};
        if (n > 1 && light.radius > 0) {
          double j[3];
          ball_point(rng, light.radius, j);
          for (int k = 0; k < 3; k++) target[k] += j[k];
        }
        double d[3] = {target[0] - from[0], target[1] - from[1], target[2] - from[2]};
        const double len = std::sqrt(dot3(d, d));
        if (len <= 0) continue;
        for (int k = 0; k < 3; k++) d[k] /= len;
        (*rays)++;
        if (!occluded(bvh, make_ray(from, d, len * (1 - 1e-3)))) seen++;
      }
      scale = fall * seen / n;
    }
    if (scale <= 0) continue;
    const double c[3] = {light.colour[0] * scale, light.colour[1] * scale, light.colour[2] * scale};
    const double w = lum(c);
    for (int k = 0; k < 3; k++) {
      rgb[k] += c[k];
      dirsum[k] += to[k] * w;
    }
  }
}

}  // namespace

void sample_lightmap(const Lightmap& map, double x, double y, float out[3]) {
  const double fx = x - 0.5, fy = y - 0.5;
  const double x0f = std::floor(fx), y0f = std::floor(fy);
  const double tx = fx - x0f, ty = fy - y0f;
  const auto w = static_cast<int64_t>(map.width), h = static_cast<int64_t>(map.height);
  auto at = [&](int64_t xi, int64_t yi, int k) {
    xi = std::clamp<int64_t>(xi, 0, w - 1);
    yi = std::clamp<int64_t>(yi, 0, h - 1);
    return static_cast<double>(map.rgb[static_cast<size_t>(yi * w + xi) * 3 + k]);
  };
  const auto x0 = static_cast<int64_t>(x0f), y0 = static_cast<int64_t>(y0f);
  for (int k = 0; k < 3; k++) {
    const double a = at(x0, y0, k) * (1 - tx) + at(x0 + 1, y0, k) * tx;
    const double b = at(x0, y0 + 1, k) * (1 - tx) + at(x0 + 1, y0 + 1, k) * tx;
    out[k] = static_cast<float>(a * (1 - ty) + b * ty);
  }
}

Lightmap bake_scene(const Scene& scene, const Options& options, Stats* stats) {
  Lightmap out;
  out.width = scene.atlas_width;
  out.height = scene.atlas_height;
  const size_t texels = size_t{out.width} * out.height;
  out.rgb.assign(texels * 3, 0.0f);
  out.dir.assign(texels * 4, 0.0f);
  out.direct.assign(texels * 3, 0.0f);
  if (texels == 0) return out;
  const size_t workers = options.workers ? options.workers : core::default_workers();

  // Each chart's frame and triangles; then its luxels, found on its triangles.
  std::vector<Frame> frames(scene.charts.size());
  std::vector<std::vector<uint32_t>> chart_tris(scene.charts.size());
  for (size_t c = 0; c < scene.charts.size(); c++) {
    const Chart& ch = scene.charts[c];
    const double lu = std::sqrt(dot3(ch.u, ch.u)), lv = std::sqrt(dot3(ch.v, ch.v));
    for (int k = 0; k < 3; k++) {
      frames[c].n[k] = ch.normal[k];
      frames[c].u[k] = ch.u[k] / lu;
      frames[c].v[k] = ch.v[k] / lv;
    }
  }
  for (uint32_t t = 0; t < scene.triangles(); t++) {
    if (scene.tri_chart[t] != kNoChart) chart_tris[scene.tri_chart[t]].push_back(t);
  }
  std::vector<uint8_t> state(texels, 0);  // 0 outside every chart's faces, 1 covered, 2 inside
  std::vector<Luxel> luxels;
  for (uint32_t c = 0; c < scene.charts.size(); c++) {
    const Chart& ch = scene.charts[c];
    std::vector<double> st;  // 6 a triangle: its corners on the chart
    for (const uint32_t t : chart_tris[c]) {
      for (int k = 0; k < 3; k++) {
        double p[3];
        for (int a = 0; a < 3; a++)
          p[a] = scene.positions[size_t{scene.indices[t * 3 + k]} * 3 + a];
        double s = 0, tt = 0;
        chart_point(ch, p, &s, &tt);
        st.push_back(s);
        st.push_back(tt);
      }
    }
    for (uint32_t j = 0; j < ch.height; j++) {
      for (uint32_t i = 0; i < ch.width; i++) {
        const double q[2] = {i + 0.5, j + 0.5};
        double best[2] = {0, 0}, best_d2 = 1e300, centre[2] = {0, 0};
        bool inside = false;
        for (size_t k = 0; k + 6 <= st.size() && !inside; k += 6) {
          double near[2] = {0, 0};
          inside = nearest_2d(&st[k], &st[k + 2], &st[k + 4], q, near);
          const double d2 =
              (near[0] - q[0]) * (near[0] - q[0]) + (near[1] - q[1]) * (near[1] - q[1]);
          if (inside || d2 < best_d2) {
            best_d2 = inside ? 0 : d2;
            best[0] = near[0];
            best[1] = near[1];
            centre[0] = (st[k] + st[k + 2] + st[k + 4]) / 3;
            centre[1] = (st[k + 1] + st[k + 3] + st[k + 5]) / 3;
          }
        }
        // A luxel within a luxel of a face samples the face's nearest point, drawn a tenth of a
        // luxel in toward its triangle's middle so its rays do not start on a neighbour's edge.
        if (best_d2 > 1.0) continue;
        if (!inside) {
          const double dx = centre[0] - best[0], dy = centre[1] - best[1];
          const double len = std::sqrt(dx * dx + dy * dy);
          if (len > 0.1) {
            best[0] += dx / len * 0.1;
            best[1] += dy / len * 0.1;
          }
        }
        Luxel l{};
        for (int k = 0; k < 3; k++) l.p[k] = ch.origin[k] + ch.u[k] * best[0] + ch.v[k] * best[1];
        l.chart = c;
        l.atlas = (ch.y + j) * out.width + ch.x + i;
        state[l.atlas] = 1;
        luxels.push_back(l);
      }
    }
  }

  const Bvh bvh = build_bvh(scene);
  std::atomic<uint64_t> rays{0};
  const size_t count = luxels.size();
  std::vector<double> dsum(count * 3, 0.0), dlum(count, 0.0);  // the direct light's direction

  // DIRECT.
  core::parallel_bands(count, workers, [&](size_t lo, size_t hi) {
    uint64_t r = 0;
    for (size_t i = lo; i < hi; i++) {
      core::Rng rng = core::make_rng(mix(mix(options.seed, static_cast<uint32_t>(i)), 0));
      double rgb[3] = {0, 0, 0};
      direct_at(scene, bvh, luxels[i], frames[luxels[i].chart], options, rng, rgb, &dsum[i * 3],
                &r);
      for (int k = 0; k < 3; k++)
        out.direct[size_t{luxels[i].atlas} * 3 + k] = static_cast<float>(rgb[k]);
      dlum[i] = lum(rgb);
    }
    rays += r;
  });

  // Fill every luxel not covered (or inside) from its chart's good neighbours, rounds of one
  // luxel, so bilinear reads at a chart's edge see the edge's light.
  auto dilate = [&](std::vector<float>& map, int channels) {
    for (uint32_t c = 0; c < scene.charts.size(); c++) {
      const Chart& ch = scene.charts[c];
      std::vector<uint8_t> good(size_t{ch.width} * ch.height);
      bool any = false;
      for (uint32_t j = 0; j < ch.height; j++) {
        for (uint32_t i = 0; i < ch.width; i++) {
          good[j * ch.width + i] = state[(ch.y + j) * out.width + ch.x + i] == 1;
          any = any || good[j * ch.width + i];
        }
      }
      if (!any) continue;
      for (int round = 0; round < static_cast<int>(ch.width + ch.height); round++) {
        std::vector<uint8_t> next = good;
        bool changed = false, missing = false;
        for (uint32_t j = 0; j < ch.height; j++) {
          for (uint32_t i = 0; i < ch.width; i++) {
            if (good[j * ch.width + i]) continue;
            double acc[4] = {0, 0, 0, 0};
            int n = 0;
            for (int dj = -1; dj <= 1; dj++) {
              for (int di = -1; di <= 1; di++) {
                const int x = static_cast<int>(i) + di, y = static_cast<int>(j) + dj;
                if (x < 0 || y < 0 || x >= static_cast<int>(ch.width) ||
                    y >= static_cast<int>(ch.height))
                  continue;
                if (!good[static_cast<size_t>(y) * ch.width + x]) continue;
                const size_t a =
                    (ch.y + static_cast<uint32_t>(y)) * out.width + ch.x + static_cast<uint32_t>(x);
                for (int k = 0; k < channels; k++) acc[k] += map[a * channels + k];
                n++;
              }
            }
            if (n == 0) {
              missing = true;
              continue;
            }
            const size_t a = (ch.y + j) * out.width + ch.x + i;
            for (int k = 0; k < channels; k++)
              map[a * channels + k] = static_cast<float>(acc[k] / n);
            next[j * ch.width + i] = 1;
            changed = true;
          }
        }
        good.swap(next);
        if (!changed || !missing) break;
      }
    }
  };

  // GATHER passes over the last pass's irradiance (the direct light first).
  std::vector<float> last = out.direct;
  dilate(last, 3);
  std::vector<double> ind(count * 3, 0.0), isum(count * 3, 0.0), ilum(count, 0.0);
  std::vector<uint8_t> inside(count, 0);
  const int samples = std::max(1, options.samples);
  for (int pass = 0; pass < options.bounces; pass++) {
    core::parallel_bands(count, workers, [&](size_t lo, size_t hi) {
      uint64_t r = 0;
      for (size_t i = lo; i < hi; i++) {
        const Luxel& l = luxels[i];
        const Frame& f = frames[l.chart];
        core::Rng rng = core::make_rng(
            mix(mix(options.seed, static_cast<uint32_t>(i)), static_cast<uint32_t>(pass + 1)));
        double from[3];
        for (int k = 0; k < 3; k++) from[k] = l.p[k] + f.n[k] * kBias;
        double sum[3] = {0, 0, 0}, dir[3] = {0, 0, 0};
        int backs = 0;
        for (int s = 0; s < samples; s++) {
          double d[3];
          cosine_dir(rng, f, d);
          const Hit h = closest_hit(bvh, make_ray(from, d, 1e30));
          r++;
          double L[3] = {0, 0, 0};
          if (!h.hit() || scene.materials[scene.tri_material[h.tri]].sky) {
            for (int k = 0; k < 3; k++) L[k] = scene.sky[k] / kPi;
          } else if (!h.front) {
            backs++;
            continue;
          } else {
            const Material& m = scene.materials[scene.tri_material[h.tri]];
            float e[3] = {0, 0, 0};
            const uint32_t c = scene.tri_chart[h.tri];
            if (c != kNoChart) {
              const Chart& ch = scene.charts[c];
              double p[3], s0 = 0, t0 = 0;
              for (int k = 0; k < 3; k++) p[k] = from[k] + d[k] * h.t;
              chart_point(ch, p, &s0, &t0);
              s0 = std::clamp(s0, 0.5, ch.width - 0.5);
              t0 = std::clamp(t0, 0.5, ch.height - 0.5);
              // Bilinear over `last`, inside the chart's rectangle (clamped above).
              const double fx = ch.x + s0 - 0.5, fy = ch.y + t0 - 0.5;
              const double x0f = std::floor(fx), y0f = std::floor(fy);
              const double tx = fx - x0f, ty = fy - y0f;
              const auto x0 = static_cast<size_t>(x0f), y0 = static_cast<size_t>(y0f);
              const size_t x1 = std::min<size_t>(x0 + 1, ch.x + ch.width - 1);
              const size_t y1 = std::min<size_t>(y0 + 1, ch.y + ch.height - 1);
              for (int k = 0; k < 3; k++) {
                const double a = last[(y0 * out.width + x0) * 3 + k] * (1 - tx) +
                                 last[(y0 * out.width + x1) * 3 + k] * tx;
                const double b = last[(y1 * out.width + x0) * 3 + k] * (1 - tx) +
                                 last[(y1 * out.width + x1) * 3 + k] * tx;
                e[k] = static_cast<float>(a * (1 - ty) + b * ty);
              }
            }
            for (int k = 0; k < 3; k++) L[k] = (m.albedo[k] * e[k] + m.emit[k]) / kPi;
          }
          const double w = lum(L);
          for (int k = 0; k < 3; k++) {
            sum[k] += L[k];
            dir[k] += d[k] * w;
          }
        }
        if (pass == 0) inside[i] = backs > kInsideShare * samples ? 1 : 0;
        const double scale = kPi / samples;
        for (int k = 0; k < 3; k++) {
          ind[i * 3 + k] = sum[k] * scale;
          isum[i * 3 + k] = dir[k] * scale;
        }
        double tmp[3] = {ind[i * 3], ind[i * 3 + 1], ind[i * 3 + 2]};
        ilum[i] = lum(tmp);
      }
      rays += r;
    });
    if (pass == 0) {
      for (size_t i = 0; i < count; i++) {
        if (inside[i]) state[luxels[i].atlas] = 2;
      }
    }
    // Smooth the gathered light over each chart's good luxels.
    if (options.filter > 0) {
      std::vector<int64_t> index_of(texels, -1);
      for (size_t i = 0; i < count; i++) {
        if (state[luxels[i].atlas] == 1) index_of[luxels[i].atlas] = static_cast<int64_t>(i);
      }
      std::vector<double> fi(count * 3), fs(count * 3), fl(count);
      const int r = options.filter;
      for (size_t i = 0; i < count; i++) {
        const Chart& ch = scene.charts[luxels[i].chart];
        const uint32_t ax = luxels[i].atlas % out.width, ay = luxels[i].atlas / out.width;
        double a[7] = {0, 0, 0, 0, 0, 0, 0};
        int n = 0;
        for (int dy = -r; dy <= r; dy++) {
          for (int dx = -r; dx <= r; dx++) {
            const int64_t x = static_cast<int64_t>(ax) + dx, y = static_cast<int64_t>(ay) + dy;
            if (x < ch.x || y < ch.y || x >= ch.x + ch.width || y >= ch.y + ch.height) continue;
            const int64_t j = index_of[static_cast<size_t>(y) * out.width + static_cast<size_t>(x)];
            if (j < 0) continue;
            for (int k = 0; k < 3; k++) {
              a[k] += ind[static_cast<size_t>(j) * 3 + k];
              a[3 + k] += isum[static_cast<size_t>(j) * 3 + k];
            }
            a[6] += ilum[static_cast<size_t>(j)];
            n++;
          }
        }
        for (int k = 0; k < 3; k++) {
          fi[i * 3 + k] = n ? a[k] / n : ind[i * 3 + k];
          fs[i * 3 + k] = n ? a[3 + k] / n : isum[i * 3 + k];
        }
        fl[i] = n ? a[6] / n : ilum[i];
      }
      ind.swap(fi);
      isum.swap(fs);
      ilum.swap(fl);
    }
    for (size_t i = 0; i < count; i++) {
      const size_t a = luxels[i].atlas;
      for (int k = 0; k < 3; k++)
        last[a * 3 + k] = static_cast<float>(out.direct[a * 3 + k] + ind[i * 3 + k]);
    }
    dilate(last, 3);
  }

  // The result: the last pass's irradiance and the light's mean direction.
  for (size_t i = 0; i < count; i++) {
    const size_t a = luxels[i].atlas;
    const double total = dlum[i] + (options.bounces > 0 ? ilum[i] : 0);
    double v[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++) {
      out.rgb[a * 3 + k] =
          static_cast<float>(out.direct[a * 3 + k] + (options.bounces > 0 ? ind[i * 3 + k] : 0));
      v[k] = dsum[i * 3 + k] + (options.bounces > 0 ? isum[i * 3 + k] : 0);
    }
    if (total > 0) {
      for (int k = 0; k < 3; k++) out.dir[a * 4 + k] = static_cast<float>(v[k] / total);
    }
    out.dir[a * 4 + 3] = state[a] == 1 ? 1.0f : 0.0f;
  }
  dilate(out.rgb, 3);
  dilate(out.direct, 3);
  {
    // The direction's fill keeps w (coverage) at 0 where it was filled.
    std::vector<float> keep(texels);
    for (size_t t = 0; t < texels; t++) keep[t] = out.dir[t * 4 + 3];
    dilate(out.dir, 4);
    for (size_t t = 0; t < texels; t++) out.dir[t * 4 + 3] = keep[t];
  }
  if (stats) {
    stats->luxels = 0;
    stats->filled = 0;
    for (size_t i = 0; i < count; i++)
      (state[luxels[i].atlas] == 1 ? stats->luxels : stats->filled)++;
    stats->rays = rays.load();
  }
  return out;
}

}  // namespace ge::bake
