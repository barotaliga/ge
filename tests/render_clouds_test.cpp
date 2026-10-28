// engine/render/clouds.hpp: the noise is the same bytes every time, spans its range and tiles
// (its value at a point is its value a tile on, on every axis); the cut is the noise's own
// quantile, so a layer's column stands over the share of the sky its cover asks; the column is
// carried along the wind by the drift; and the frame block's rows hold each layer's numbers.
//
// ge:summary Tests for the cloud: the noise, the cut, the column's cover and drift, the rows
// ge:keywords test render clouds cloud-noise cloud_cut cloud_column cloud_rows cover drift
// ge:skill render testing
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "check.hpp"
#include "render/clouds.hpp"
#include "render/frame.hpp"

namespace {

namespace gr = ge::render;

gr::CloudLayer heap(double cover) {
  gr::CloudLayer l;
  l.base_m = 1500;
  l.top_m = 2200;
  l.cover = cover;
  l.cell_m = 1000;
  return l;
}

// The share of a square two tiles a side over which the layer's column stands.
double share(const gr::CloudLayer& l) {
  const double span = 2 * gr::kCloudTileCells * l.cell_m * l.stretch;
  int under = 0;
  for (int j = 0; j < 100; ++j)
    for (int i = 0; i < 100; ++i)
      if (gr::cloud_column(l, (i + 0.5) / 100 * span, (j + 0.5) / 100 * span) > 0) ++under;
  return under / 10000.0;
}

}  // namespace

TEST(the_noise_is_the_same_bytes_and_spans_its_range) {
  const auto& n = gr::cloud_noise();
  CHECK(n.size() ==
        static_cast<size_t>(gr::kCloudNoiseSize) * gr::kCloudNoiseSize * gr::kCloudNoiseSize);
  CHECK(*std::min_element(n.begin(), n.end()) == 0);
  CHECK(*std::max_element(n.begin(), n.end()) == 255);
  // A fingerprint of the bytes: they are made by integer hashing and exact arithmetic only.
  uint32_t h = 2166136261u;
  for (uint8_t b : n) h = (h ^ b) * 16777619u;
  uint32_t again = 2166136261u;
  for (uint8_t b : gr::cloud_noise()) again = (again ^ b) * 16777619u;
  CHECK(h == again);
}

TEST(the_noise_tiles) {
  for (double u : {0.013, 0.37, 0.81}) {
    const double v = 0.5 * u + 0.2, w = 0.9 - u;
    const double at = gr::cloud_noise_at(u, v, w);
    CHECK(std::fabs(gr::cloud_noise_at(u + 1, v, w) - at) < 1e-12);
    CHECK(std::fabs(gr::cloud_noise_at(u, v - 1, w) - at) < 1e-12);
    CHECK(std::fabs(gr::cloud_noise_at(u, v, w + 2) - at) < 1e-12);
  }
}

TEST(the_cut_falls_as_the_cover_grows) {
  CHECK(gr::cloud_cut(0) == 1);
  CHECK(gr::cloud_cut(1) == 0);
  double last = 1;
  for (int k = 1; k < 10; ++k) {
    const double cut = gr::cloud_cut(0.1 * k);
    CHECK(cut < last && cut > 0);
    last = cut;
  }
}

TEST(a_layer_covers_the_share_its_cover_asks) {
  for (double cover : {0.15, 0.3, 0.5, 0.85}) {
    const double s = share(heap(cover));
    CHECK(std::fabs(s - cover) < 0.08);
  }
}

TEST(the_drift_carries_the_column_along_the_wind) {
  gr::CloudLayer l = heap(0.4);
  l.heading = 0.7;  // toward +x and +z
  const double along[2] = {std::sin(l.heading), std::cos(l.heading)};
  int same = 0;
  for (int k = 0; k < 40; ++k) {
    const double x = 137.0 * k, z = -91.0 * k;
    const double before = gr::cloud_column(l, x, z);
    gr::CloudLayer moved = l;
    moved.drift_m = 600;
    const double after = gr::cloud_column(moved, x + along[0] * 600, z + along[1] * 600);
    if (std::fabs(before - after) < 1e-6) ++same;
  }
  CHECK(same == 40);
}

TEST(the_rows_hold_each_layers_numbers) {
  gr::Clouds clouds;
  clouds.count = 2;
  clouds.layers[0] = heap(0.3);
  clouds.layers[1] = heap(0.6);
  clouds.layers[1].dome = 0;
  clouds.layers[1].stretch = 4;
  clouds.layers[1].shadow_tau = 0.25;
  clouds.layers[1].anvil = 0.5;
  clouds.layers[1].fibre = 2;
  clouds.layers[1].drop_um = 30;
  clouds.layers[1].halo = 3;
  float info[4];
  float rows[gr::kCloudLayers * gr::kCloudRows][4];
  gr::cloud_rows(clouds, info, rows);
  CHECK(info[0] == 2);
  CHECK(rows[0][0] == 1500 && rows[0][1] == 2200);
  CHECK(std::fabs(rows[0][2] - gr::cloud_cut(0.3)) < 1e-6);
  CHECK(std::fabs(rows[0][3] - 1e-3) < 1e-9);
  // The first layer's form is a heap's: no anvil, the boil's bite, no comb, no patches; its light
  // a water cloud's droplets, no halo, no deck.
  CHECK(rows[3][0] == 0 && rows[3][1] == 0.6f && rows[3][2] == 0 && rows[3][3] == 0);
  CHECK(rows[4][0] == 10 && rows[4][1] == 0.35f && rows[4][2] == 0 && rows[4][3] == 0);
  CHECK(rows[7][1] == 0 && std::fabs(rows[7][0] - 0.25f) < 1e-7 && rows[7][3] == 0.25f);
  // The second's form and light, held to their ranges.
  CHECK(rows[8][0] == 0.5f && rows[8][2] == 1);
  CHECK(rows[9][0] == 30 && rows[9][2] == 1);
  // The third layer's rows are zero.
  for (int r = 10; r < 15; ++r) CHECK(rows[r][0] == 0 && rows[r][3] == 0);
  // The frame block carries them.
  gr::FrameView view;
  view.clouds = clouds;
  gr::FrameUniforms u{};
  gr::fill_uniforms(u, view, 64, 32);
  CHECK(u.cloud_info[0] == 2 && u.clouds[0][1] == 2200);
}

TEST(the_march_is_spread_only_with_the_last_frames_camera) {
  gr::FrameView view;
  view.clouds.count = 1;
  view.frame = 4097;
  gr::FrameHistory history;
  gr::FrameUniforms u{};
  // The first frame has no last camera to reproject by: every half-size pixel marches.
  gr::fill_uniforms(u, view, 64, 32, &history);
  CHECK(u.cloud_info[2] == 0);
  // The stamp counts 1..kCloudStamps, so a cleared target (0) is never the last frame's.
  CHECK(u.cloud_info[3] == 2);
  gr::fill_uniforms(u, view, 64, 32, &history);
  CHECK(u.cloud_info[2] == 1);
  // Not at a quarter, or not asked for: never.
  view.clouds.spread = false;
  gr::fill_uniforms(u, view, 64, 32, &history);
  CHECK(u.cloud_info[2] == 0);
  view.clouds.spread = true;
  view.clouds.quarter = false;
  gr::fill_uniforms(u, view, 64, 32, &history);
  CHECK(u.cloud_info[2] == 0);
}
