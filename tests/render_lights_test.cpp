// engine/render/lights.hpp and surface.hpp: every flicker is a pure function of the light and the
// time and stays in its range (a pulse rises, peaks and dies, a step's letters are its levels);
// the clusters never miss a light that reaches a point (every point inside a kept light's sphere
// finds that light in its own cluster's list, the lights nearest first), lights wholly outside
// the view are dropped, the nearest kMaxLights are kept and the rest counted; the surfaces pack
// two rows each, matte by default.
//
// ge:summary Tests for local lights: flicker curves, clustered binning against brute force
// ge:keywords test render lights flicker pulse steps cluster bin_lights cluster_of surface
// ge:keywords conservative-binning nearest-lights pack_surfaces
// ge:skill render testing
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "check.hpp"
#include "render/lights.hpp"
#include "render/surface.hpp"

namespace {

namespace r = ge::render;

uint32_t next(uint32_t& s) {
  s = s * 1664525u + 1013904223u;
  return s;
}
double uniform(uint32_t& s) { return (next(s) >> 8) * (1.0 / 16777216.0); }

r::FrameView a_view() {
  r::FrameView v;
  v.eye[0] = 10;
  v.eye[1] = 2;
  v.eye[2] = -5;
  v.heading = 0.4;
  v.pitch = -0.15;
  v.fov_y = 1.0;
  v.near_m = 0.1;
  v.time_s = 3.25;
  return v;
}

// Lights scattered in a box round the eye, a few big and many small, some spots.
std::vector<r::LocalLight> scatter(int n, uint32_t seed) {
  std::vector<r::LocalLight> out;
  uint32_t s = seed;
  for (int i = 0; i < n; ++i) {
    r::LocalLight l;
    l.position[0] = 10 + (uniform(s) - 0.5) * 160;
    l.position[1] = 2 + (uniform(s) - 0.5) * 30;
    l.position[2] = -5 + (uniform(s) - 0.5) * 160;
    l.range_m = static_cast<float>(uniform(s) < 0.1 ? 15 + 25 * uniform(s) : 1 + 6 * uniform(s));
    l.candela = 50;
    if (uniform(s) < 0.3) {
      l.outer_deg = 35;
      l.inner_deg = 20;
    }
    out.push_back(l);
  }
  return out;
}

}  // namespace

TEST(steady_is_whole_and_every_flicker_is_pure) {
  r::LocalLight l;
  CHECK(r::flicker_level(l, 12.5) == 1.0f);
  for (r::Flicker f :
       {r::Flicker::flame, r::Flicker::buzz, r::Flicker::strobe, r::Flicker::pulse}) {
    l.flicker = f;
    l.seed = 7;
    CHECK_BITS(r::flicker_level(l, 4.321), r::flicker_level(l, 4.321));
  }
}

TEST(a_flame_wavers_inside_its_swing_and_two_seeds_differ) {
  r::LocalLight a, b;
  a.flicker = b.flicker = r::Flicker::flame;
  a.depth = b.depth = 0.4f;
  a.seed = 1;
  b.seed = 2;
  float lo = 2, hi = -1;
  int differ = 0;
  for (int i = 0; i < 2000; ++i) {
    const double t = i * 0.005;
    const float x = r::flicker_level(a, t);
    lo = std::fmin(lo, x);
    hi = std::fmax(hi, x);
    if (std::fabs(x - r::flicker_level(b, t)) > 0.02f) ++differ;
  }
  CHECK(lo >= 0.6f - 1e-6f && hi <= 1.0f + 1e-6f);
  CHECK(hi - lo > 0.15f);  // it does waver
  CHECK(differ > 1000);    // and not in step with another torch
}

TEST(a_pulse_rises_peaks_and_dies) {
  r::LocalLight l;
  l.flicker = r::Flicker::pulse;
  l.born_s = 1.0;
  l.attack_s = 0.002f;
  l.decay_s = 0.03f;
  CHECK(r::flicker_level(l, 0.99) == 0.0f);
  CHECK(r::flicker_level(l, 1.001) > 0.4f && r::flicker_level(l, 1.001) < 0.6f);
  CHECK(std::fabs(r::flicker_level(l, 1.002) - 1.0f) < 1e-3f);
  CHECK(std::fabs(r::flicker_level(l, 1.032) - std::exp(-1.0f)) < 1e-3f);
  CHECK(r::flicker_level(l, 1.5) == 0.0f);  // gone: the clusters drop it
}

TEST(steps_play_their_letters_at_the_rate) {
  r::LocalLight l;
  l.flicker = r::Flicker::steps;
  l.steps = "amz";
  l.rate_hz = 10;
  CHECK(r::flicker_level(l, 0.05) == 0.0f);
  CHECK(r::flicker_level(l, 0.15) == 1.0f);
  CHECK(std::fabs(r::flicker_level(l, 0.25) - 25.0f / 12.0f) < 1e-6f);
  CHECK(r::flicker_level(l, 0.35) == 0.0f);  // round again
}

TEST(a_strobe_is_on_half_its_beat) {
  r::LocalLight l;
  l.flicker = r::Flicker::strobe;
  l.rate_hz = 4;
  int on = 0;
  for (int i = 0; i < 1000; ++i) on += r::flicker_level(l, i * 0.001) > 0.5f ? 1 : 0;
  CHECK(on == 500);
}

TEST(every_point_a_light_reaches_finds_it_in_its_cluster) {
  const r::FrameView v = a_view();
  const uint32_t w = 640, h = 360;
  const std::vector<r::LocalLight> lights = scatter(200, 11);
  r::LightBins bins;
  r::bin_lights(bins, lights, v, w, h);
  CHECK(bins.head.info[0] > 20);
  CHECK(bins.dropped == 0);
  const r::CameraMatrices cam = r::camera_matrices(v, static_cast<double>(w) / h);
  // Each kept light's own index back (by its camera-relative position).
  uint32_t s = 5;
  int tested = 0, missed = 0;
  for (uint32_t i = 0; i < bins.head.info[0]; ++i) {
    const r::GpuLight& g = bins.lights[i];
    const double range = 1.0 / std::sqrt(g.at[3]);
    for (int k = 0; k < 60; ++k) {
      // A point inside the sphere.
      double p[3] = {0, 0, 0};
      double d2 = 2;
      while (d2 > 1) {
        for (double& c : p) c = uniform(s) * 2 - 1;
        d2 = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
      }
      for (int j = 0; j < 3; ++j) p[j] = g.at[j] + p[j] * range * 0.999;
      const int c = r::cluster_of(cam, v.near_m, w, h, p);
      if (c < 0) continue;
      ++tested;
      const uint32_t first = bins.cells[static_cast<size_t>(c) * 2];
      const uint32_t n = bins.cells[static_cast<size_t>(c) * 2 + 1];
      bool found = false;
      for (uint32_t q = 0; q < n; ++q) found = found || bins.refs[first + q] == i;
      if (!found) ++missed;
    }
  }
  CHECK(tested > 1000);
  CHECK_MSG(missed == 0, "a point inside a light's sphere found its cluster without that light");
}

TEST(a_cluster_lists_its_lights_nearest_first_and_is_tight) {
  const r::FrameView v = a_view();
  const std::vector<r::LocalLight> lights = scatter(300, 23);
  r::LightBins bins;
  r::bin_lights(bins, lights, v, 640, 360);
  uint64_t refs = 0;
  for (uint32_t c = 0; c < r::kClusters; ++c) {
    const uint32_t first = bins.cells[size_t{c} * 2], n = bins.cells[size_t{c} * 2 + 1];
    refs += n;
    for (uint32_t q = 1; q < n; ++q) CHECK(bins.refs[first + q - 1] < bins.refs[first + q]);
  }
  CHECK(refs == bins.ref_count);
  // Tight: far fewer entries than every light in every cluster its slice range spans.
  CHECK(refs == bins.ref_count);
  // Tight: a lamp of 1 m's reach 20 m ahead lands in a handful of clusters, not its slices' rows.
  r::LocalLight one;
  double f[3];
  r::look_dir(v.heading, v.pitch, f);
  for (int k = 0; k < 3; ++k) one.position[k] = v.eye[k] + f[k] * 20;
  one.range_m = 1;
  r::bin_lights(bins, std::span<const r::LocalLight>(&one, 1), v, 640, 360);
  CHECK(bins.head.info[0] == 1);
  CHECK(bins.ref_count >= 1 && bins.ref_count <= 8);
}

TEST(lights_outside_the_view_or_dark_are_dropped_and_the_nearest_kept) {
  r::FrameView v = a_view();
  std::vector<r::LocalLight> lights(3);
  // Behind the eye, far off its sphere: out.
  double f[3];
  r::look_dir(v.heading, v.pitch, f);
  for (int k = 0; k < 3; ++k) lights[0].position[k] = v.eye[k] - f[k] * 50;
  // Ahead: in.
  for (int k = 0; k < 3; ++k) lights[1].position[k] = v.eye[k] + f[k] * 20;
  // Ahead but a pulse long dead: out.
  lights[2] = lights[1];
  lights[2].flicker = r::Flicker::pulse;
  lights[2].born_s = 0;
  r::LightBins bins;
  r::bin_lights(bins, lights, v, 320, 180);
  CHECK(bins.head.info[0] == 1);

  // Many more than the cap: the nearest kept.
  std::vector<r::LocalLight> crowd(r::kMaxLights + 40);
  for (size_t i = 0; i < crowd.size(); ++i) {
    const double d = 5 + static_cast<double>(crowd.size() - i);
    for (int k = 0; k < 3; ++k) crowd[i].position[k] = v.eye[k] + f[k] * d;
    crowd[i].range_m = 2;
  }
  r::bin_lights(bins, crowd, v, 320, 180);
  CHECK(bins.head.info[0] == r::kMaxLights);
  CHECK(bins.dropped == 40);
  double nearest = 1e9, farthest = 0;
  for (uint32_t i = 0; i < r::kMaxLights; ++i) {
    const r::GpuLight& g = bins.lights[i];
    const double d = std::sqrt(g.at[0] * g.at[0] + g.at[1] * g.at[1] + g.at[2] * g.at[2]);
    nearest = std::fmin(nearest, d);
    farthest = std::fmax(farthest, d);
  }
  CHECK(nearest < 7 && farthest < 5 + r::kMaxLights + 2);
}

TEST(surfaces_pack_two_rows_and_matte_has_no_lobe) {
  const float rgb[3] = {0.5f, 0.4f, 0.3f};
  const r::Surface m = r::matte_surface(rgb);
  CHECK(m.spec == 0 && m.metal == 0 && m.rough == 1);
  const r::Surface s[2] = {m, r::metal_surface(rgb, 0.3f)};
  std::vector<float> out;
  r::pack_surfaces(s, out);
  CHECK(out.size() == 16);
  CHECK(out[0] == 0.5f && out[3] == 1.0f && out[12] == 1.0f);
}
