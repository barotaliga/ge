// The local lights' definitions: the flicker curves, the clusters' boxes and the binning, the
// buffer's upload.
//
// ge:summary Local lights' definitions: flicker curves, cluster boxes, binning and the upload
// ge:keywords render lights flicker_level bin_lights cluster_of prepare_lights create_lights
// ge:keywords cluster-box sphere-box view-space nearest-lights
// ge:skill render
// ge:test tests/render_lights_test.cpp
#include "render/lights.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ge::render {

namespace {

constexpr double kPi = 3.14159265358979323846;

// A hash of an integer lattice point and a seed into [0, 1).
double lattice(int64_t i, uint32_t seed) {
  uint64_t x = static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ull ^ (uint64_t{seed} << 32 | seed);
  x ^= x >> 30;
  x *= 0xBF58476D1CE4E5B9ull;
  x ^= x >> 27;
  x *= 0x94D049BB133111EBull;
  x ^= x >> 31;
  return static_cast<double>(x >> 11) * (1.0 / 9007199254740992.0);
}

// Smooth 1D value noise in [0, 1), a lattice point each unit of `x`.
double smooth_noise(double x, uint32_t seed) {
  const double f = std::floor(x);
  const double u = x - f;
  const double s = u * u * (3 - 2 * u);
  const auto i = static_cast<int64_t>(f);
  return lattice(i, seed) + (lattice(i + 1, seed) - lattice(i, seed)) * s;
}

// Slices a doubling of distance past kSliceNear.
double slices_per_doubling() { return (kSlices - 1) / std::log2(kSliceFar / kSliceNear); }

int slice_of(double d) {
  if (d < kSliceNear) return 0;
  const double s = 1 + std::floor(std::log2(d / kSliceNear) * slices_per_doubling());
  return static_cast<int>(std::min(s, static_cast<double>(kSlices - 1)));
}

// The near and far distances of slice `s` (the last runs to a large stand-in for infinity).
void slice_range(uint32_t s, double& d0, double& d1) {
  const double k = slices_per_doubling();
  d0 = s == 0 ? 0.0 : kSliceNear * std::exp2((s - 1) / k);
  d1 = s + 1 >= kSlices ? 1e9 : kSliceNear * std::exp2(s / k);
}

// The camera's x and y spread per metre of distance: x_view = ndc_x * d * sx, y_view = ndc_y * d *
// sy.
void spread(const CameraMatrices& cam, double& sx, double& sy) {
  sx = 1.0 / cam.proj.m[0];
  sy = 1.0 / cam.proj.m[5];
}

// A camera-relative point into view space as (right, up, distance along the view).
void to_view(const CameraMatrices& cam, const double rel[3], double out[3]) {
  const float* m = cam.view.m;
  double v[3];
  for (int r = 0; r < 3; ++r) v[r] = m[r] * rel[0] + m[4 + r] * rel[1] + m[8 + r] * rel[2];
  out[0] = v[0];
  out[1] = v[1];
  out[2] = -v[2];
}

uint32_t tile_px(uint32_t size, uint32_t tiles) { return (size + tiles - 1) / tiles; }

}  // namespace

float flicker_level(const LocalLight& light, double t_s) {
  switch (light.flicker) {
    case Flicker::steady:
      return 1.0f;
    case Flicker::flame: {
      // Two octaves of the waver and a slow breath under them; never below 1 - depth.
      const double x = t_s * light.rate_hz;
      const double n = 0.55 * smooth_noise(x, light.seed) +
                       0.3 * smooth_noise(x * 2.3 + 17.0, light.seed ^ 0x51ed27u) +
                       0.15 * smooth_noise(x * 0.17 + 3.0, light.seed ^ 0xa3c1u);
      return static_cast<float>(1.0 - light.depth * (1.0 - n));
    }
    case Flicker::buzz: {
      // A beat at rate_hz; a beat is dark when its draw falls under the swing's share (in short
      // gaps), else whole with a little hum.
      const double x = t_s * light.rate_hz;
      const auto beat = static_cast<int64_t>(std::floor(x));
      const bool dark = lattice(beat, light.seed) < light.depth * 0.5;
      const double hum = 1.0 - 0.06 * light.depth * (0.5 + 0.5 * std::sin(2 * kPi * x * 12.0));
      return dark ? static_cast<float>(0.08 * lattice(beat, light.seed ^ 0x77u))
                  : static_cast<float>(hum);
    }
    case Flicker::strobe: {
      const double x = t_s * light.rate_hz;
      return x - std::floor(x) < 0.5 ? 1.0f : 0.0f;
    }
    case Flicker::pulse: {
      const double age = t_s - light.born_s;
      if (age < 0) return 0.0f;
      if (age < light.attack_s) return static_cast<float>(age / std::max(light.attack_s, 1e-6f));
      const double fall = (age - light.attack_s) / std::max(light.decay_s, 1e-6f);
      // Gone once it is under a thousandth: the cluster drops it.
      return fall > 6.9 ? 0.0f : static_cast<float>(std::exp(-fall));
    }
    case Flicker::steps: {
      if (!light.steps || !light.steps[0]) return 1.0f;
      const size_t n = std::strlen(light.steps);
      const double x = std::max(t_s, 0.0) * light.rate_hz;
      const char c = light.steps[static_cast<size_t>(std::floor(x)) % n];
      const int k = std::clamp(c - 'a', 0, 25);
      return static_cast<float>(k / 12.0);
    }
  }
  return 1.0f;
}

void reserve_bins(LightBins& bins) {
  bins.lights.assign(kMaxLights, GpuLight{});
  bins.cells.assign(size_t{kClusters} * 2, 0);
  bins.refs.assign(kMaxLightRefs, 0);
  bins.order.reserve(1024);
  bins.order_key.reserve(1024);
}

int cluster_of(const CameraMatrices& cam, double near_m, uint32_t width, uint32_t height,
               const double rel[3]) {
  (void)near_m;
  double v[3];
  to_view(cam, rel, v);
  if (v[2] <= 0) return -1;
  double sx, sy;
  spread(cam, sx, sy);
  const double ndc_x = v[0] / (v[2] * sx), ndc_y = v[1] / (v[2] * sy);
  const double px = (ndc_x * 0.5 + 0.5) * width, py = (0.5 - ndc_y * 0.5) * height;
  if (px < 0 || py < 0 || px >= width || py >= height) return -1;
  const auto tx = static_cast<uint32_t>(px) / tile_px(width, kTilesX);
  const auto ty = static_cast<uint32_t>(py) / tile_px(height, kTilesY);
  const auto s = static_cast<uint32_t>(slice_of(v[2]));
  return static_cast<int>((s * kTilesY + ty) * kTilesX + tx);
}

void bin_lights(LightBins& bins, std::span<const LocalLight> lights, const FrameView& view,
                uint32_t width, uint32_t height) {
  if (bins.lights.size() != kMaxLights) reserve_bins(bins);
  const CameraMatrices cam =
      camera_matrices(view, static_cast<double>(width) / static_cast<double>(height));
  double sx, sy;
  spread(cam, sx, sy);
  const uint32_t tw = tile_px(width, kTilesX), th = tile_px(height, kTilesY);

  // Keep the lit lights whose sphere reaches the view, nearest first.
  bins.order.clear();
  bins.order_key.clear();
  bins.dropped = 0;
  for (size_t i = 0; i < lights.size(); ++i) {
    const LocalLight& l = lights[i];
    if (l.candela <= 0 || l.range_m <= 0 || flicker_level(l, view.time_s) <= 0) continue;
    double rel[3];
    for (int k = 0; k < 3; ++k) rel[k] = l.position[k] - view.eye[k];
    bool outside = false;
    for (const auto& p : cam.planes) {
      if (p[0] * rel[0] + p[1] * rel[1] + p[2] * rel[2] + p[3] < -l.range_m) outside = true;
    }
    if (outside) continue;
    bins.order.push_back(static_cast<uint32_t>(i));
    bins.order_key.push_back(
        static_cast<float>(std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2])));
  }
  // An insertion sort of the two arrays in step, by distance (stable, and few lights a frame).
  {
    const size_t n = bins.order.size();
    for (size_t i = 1; i < n; ++i) {
      const uint32_t o = bins.order[i];
      const float k = bins.order_key[i];
      size_t j = i;
      while (j > 0 && bins.order_key[j - 1] > k) {
        bins.order[j] = bins.order[j - 1];
        bins.order_key[j] = bins.order_key[j - 1];
        --j;
      }
      bins.order[j] = o;
      bins.order_key[j] = k;
    }
  }
  if (bins.order.size() > kMaxLights) {
    bins.dropped += static_cast<uint32_t>(bins.order.size() - kMaxLights);
    bins.order.resize(kMaxLights);
  }
  const auto count = static_cast<uint32_t>(bins.order.size());

  // Each kept light's GPU row and its view-space sphere.
  struct Sphere {
    double c[3];
    double r;
  };
  Sphere spheres[kMaxLights];
  for (uint32_t i = 0; i < count; ++i) {
    const LocalLight& l = lights[bins.order[i]];
    const float level = flicker_level(l, view.time_s);
    GpuLight& g = bins.lights[i];
    double rel[3];
    for (int k = 0; k < 3; ++k) rel[k] = l.position[k] - view.eye[k];
    for (int k = 0; k < 3; ++k) g.at[k] = static_cast<float>(rel[k]);
    g.at[3] = 1.0f / (l.range_m * l.range_m);
    for (int k = 0; k < 3; ++k) g.colour[k] = l.colour[k] * l.candela * level;
    g.colour[3] = l.radius_m * l.radius_m;
    const bool spot = l.outer_deg > 0;
    const double dl = std::sqrt(l.direction[0] * l.direction[0] + l.direction[1] * l.direction[1] +
                                l.direction[2] * l.direction[2]);
    for (int k = 0; k < 3; ++k) g.axis[k] = dl > 0 ? static_cast<float>(l.direction[k] / dl) : 0.0f;
    const double co = std::cos(l.outer_deg * kPi / 180.0);
    const double ci = std::cos(std::min(l.inner_deg, l.outer_deg) * kPi / 180.0);
    g.axis[3] = spot ? static_cast<float>(co) : -2.0f;
    g.cone[0] = spot ? static_cast<float>(1.0 / std::max(ci - co, 1e-4)) : 0.0f;
    g.cone[1] = -1.0f;
    g.cone[2] = g.cone[3] = 0.0f;
    Sphere& s = spheres[i];
    to_view(cam, rel, s.c);
    // A slight margin for the TAA's sub-pixel jitter of the tiles' edges.
    s.r = l.range_m * 1.02;
  }

  // Each cluster's list: walk a light's slices and tiles that its sphere's box covers, test the
  // sphere against each cluster's box. Lists are written cluster by cluster, so the lights stay
  // in nearest-first order within each.
  std::fill(bins.cells.begin(), bins.cells.end(), 0u);
  // First count, then place: two walks over the same boxes.
  struct Range {
    uint32_t s0, s1, x0, x1, y0, y1;
  };
  Range ranges[kMaxLights];
  for (uint32_t i = 0; i < count; ++i) {
    const Sphere& s = spheres[i];
    Range& r = ranges[i];
    const double d0 = std::max(s.c[2] - s.r, 0.0), d1 = s.c[2] + s.r;
    r.s0 = static_cast<uint32_t>(slice_of(d0));
    r.s1 = static_cast<uint32_t>(slice_of(d1));
    // The tiles: where the sphere's box lies on screen at its nearest distance (the widest
    // spread); a sphere round the eye covers all of them.
    if (s.c[2] - s.r <= view.near_m) {
      r.x0 = r.y0 = 0;
      r.x1 = kTilesX - 1;
      r.y1 = kTilesY - 1;
    } else {
      // Every point of the sphere's box has x in [cx - r, cx + r] at a distance in [dn, df],
      // so its x / d is widest at the near face where x is away from the axis and at the far
      // face where it is toward it.
      const double dn = s.c[2] - s.r, df = s.c[2] + s.r;
      auto lo = [&](double v) { return v / (v <= 0 ? dn : df); };
      auto hi = [&](double v) { return v / (v >= 0 ? dn : df); };
      const double xa = (lo(s.c[0] - s.r) / sx * 0.5 + 0.5) * width;
      const double xb = (hi(s.c[0] + s.r) / sx * 0.5 + 0.5) * width;
      const double ya = (0.5 - hi(s.c[1] + s.r) / sy * 0.5) * height;
      const double yb = (0.5 - lo(s.c[1] - s.r) / sy * 0.5) * height;
      auto tile = [](double p, uint32_t t, uint32_t n) {
        return static_cast<uint32_t>(
            std::clamp(std::floor(p / t), 0.0, static_cast<double>(n - 1)));
      };
      if (xb < 0 || xa >= width || yb < 0 || ya >= height) {
        r.s0 = 1;
        r.s1 = 0;  // off screen: no cluster
      }
      r.x0 = tile(xa, tw, kTilesX);
      r.x1 = tile(xb, tw, kTilesX);
      r.y0 = tile(ya, th, kTilesY);
      r.y1 = tile(yb, th, kTilesY);
    }
  }
  auto touches = [&](const Sphere& s, uint32_t sl, uint32_t tx, uint32_t ty) {
    double d0, d1;
    slice_range(sl, d0, d1);
    const double a0 = static_cast<double>(tx * tw) / width * 2 - 1;
    const double a1 = static_cast<double>((tx + 1) * tw) / width * 2 - 1;
    const double b0 = 1 - static_cast<double>((ty + 1) * th) / height * 2;
    const double b1 = 1 - static_cast<double>(ty * th) / height * 2;
    const double lo[3] = {std::min(a0 * d0, a0 * d1) * sx, std::min(b0 * d0, b0 * d1) * sy, d0};
    const double hi[3] = {std::max(a1 * d0, a1 * d1) * sx, std::max(b1 * d0, b1 * d1) * sy, d1};
    double e = 0;
    for (int k = 0; k < 3; ++k) {
      const double q = std::clamp(s.c[k], lo[k], hi[k]) - s.c[k];
      e += q * q;
    }
    return e <= s.r * s.r;
  };
  for (uint32_t i = 0; i < count; ++i) {
    const Range& r = ranges[i];
    for (uint32_t sl = r.s0; sl <= r.s1 && r.s0 <= r.s1; ++sl) {
      for (uint32_t ty = r.y0; ty <= r.y1; ++ty) {
        for (uint32_t tx = r.x0; tx <= r.x1; ++tx) {
          if (touches(spheres[i], sl, tx, ty))
            ++bins.cells[size_t{(sl * kTilesY + ty) * kTilesX + tx} * 2 + 1];
        }
      }
    }
  }
  uint32_t offset = 0;
  for (uint32_t c = 0; c < kClusters; ++c) {
    uint32_t& n = bins.cells[size_t{c} * 2 + 1];
    if (offset + n > kMaxLightRefs) {
      bins.dropped += offset + n - kMaxLightRefs;
      n = kMaxLightRefs - offset;
    }
    bins.cells[size_t{c} * 2] = offset;
    offset += n;
    n = 0;  // refilled as the place walk writes
  }
  bins.ref_count = offset;
  for (uint32_t i = 0; i < count; ++i) {
    const Range& r = ranges[i];
    for (uint32_t sl = r.s0; sl <= r.s1 && r.s0 <= r.s1; ++sl) {
      for (uint32_t ty = r.y0; ty <= r.y1; ++ty) {
        for (uint32_t tx = r.x0; tx <= r.x1; ++tx) {
          if (!touches(spheres[i], sl, tx, ty)) continue;
          const uint32_t c = (sl * kTilesY + ty) * kTilesX + tx;
          const uint32_t at = bins.cells[size_t{c} * 2] + bins.cells[size_t{c} * 2 + 1];
          const uint32_t end = c + 1 < kClusters ? bins.cells[(size_t{c} + 1) * 2] : bins.ref_count;
          if (at < end) {
            bins.refs[at] = i;
            ++bins.cells[size_t{c} * 2 + 1];
          }
        }
      }
    }
  }

  LightHead& h = bins.head;
  h.info[0] = count;
  h.info[1] = kTilesX;
  h.info[2] = kTilesY;
  h.info[3] = kSlices;
  h.grid[0] = static_cast<float>(tw);
  h.grid[1] = static_cast<float>(th);
  h.grid[2] = static_cast<float>(view.near_m);
  h.grid[3] = static_cast<float>(slices_per_doubling());
  h.more[0] = static_cast<float>(kSliceNear);
  h.more[1] = h.more[2] = h.more[3] = 0;
}

double lamp_lux_at(std::span<const LocalLight> lights, const double at[3], double t_s) {
  double e = 0;
  for (const LocalLight& l : lights) {
    double to[3];
    for (int k = 0; k < 3; ++k) to[k] = at[k] - l.position[k];
    const double d2 = to[0] * to[0] + to[1] * to[1] + to[2] * to[2];
    const double r2 = static_cast<double>(l.range_m) * l.range_m;
    if (d2 >= r2) continue;
    const double win = 1 - (d2 * d2) / (r2 * r2);
    double k = win * win * l.candela * flicker_level(l, t_s) /
               (d2 + static_cast<double>(l.radius_m) * l.radius_m);
    if (l.outer_deg > 0) {
      const double d = std::sqrt(std::max(d2, 1e-12));
      const double dl =
          std::sqrt(l.direction[0] * l.direction[0] + l.direction[1] * l.direction[1] +
                    l.direction[2] * l.direction[2]);
      const double c = (to[0] * l.direction[0] + to[1] * l.direction[1] + to[2] * l.direction[2]) /
                       (d * std::max(dl, 1e-12));
      const double co = std::cos(l.outer_deg * kPi / 180.0);
      const double ci = std::cos(std::min(l.inner_deg, l.outer_deg) * kPi / 180.0);
      const double s = std::clamp((c - co) / std::max(ci - co, 1e-4), 0.0, 1.0);
      k *= s * s;
    }
    e += k;
  }
  return e;
}

void create_lights(gpu::Device& device, Lights& lights) {
  lights.buffer = make_buffer(device, "render.lights", kLightBufferBytes,
                              wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  reserve_bins(lights.bins);
  // An empty head until the first prepare: no light is read.
  device.queue.WriteBuffer(lights.buffer, 0, &lights.bins.head, sizeof(LightHead));
}

void prepare_lights(gpu::Device& device, Lights& lights, std::span<const LocalLight> list,
                    const FrameView& view, uint32_t width, uint32_t height) {
  LightBins& b = lights.bins;
  bin_lights(b, list, view, width, height);
  device.queue.WriteBuffer(lights.buffer, 0, &b.head, sizeof(LightHead));
  if (b.head.info[0] > 0) {
    device.queue.WriteBuffer(lights.buffer, kLightsOffset, b.lights.data(),
                             size_t{b.head.info[0]} * sizeof(GpuLight));
    device.queue.WriteBuffer(lights.buffer, kCellsOffset, b.cells.data(),
                             b.cells.size() * sizeof(uint32_t));
    if (b.ref_count > 0) {
      device.queue.WriteBuffer(lights.buffer, kRefsOffset, b.refs.data(),
                               size_t{b.ref_count} * sizeof(uint32_t));
    }
  }
}

}  // namespace ge::render
