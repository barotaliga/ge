// engine/render/snow_mirror.hpp on the GPU (the software adapter on CI), held to the layer:
//
//   * THE RAW CELLS: every sample of a layer crossed by skis, boots and a track, some tiles
//     folded into the coarse layer and some live outside the fine window, reads on the GPU
//     exactly what `snow_mirror_expect` works out on the CPU: fine cells in the window, the
//     coarse view (folded blocks and the summaries of live tiles) outside it, the cover elsewhere.
//   * THE COLUMN: `snow_column` at points over the tracks reads the physics' `snow::sample`
//     depth and drop within a tenth of a millimetre; an untouched layer reads its cover.
//   * THE UPLOADS: a second update with nothing pressed sends no tile and no block; one press
//     sends its tile again and nothing else.
//
// ge:summary Tests for the snow mirror: raw cells to the bit, the column, uploads only on change
// ge:keywords test render snow snow-mirror gpu readback atlas page-table coarse-view lavapipe
// ge:skill render snow testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "gpu_support.hpp"
#include "render/snow_mirror.hpp"
#include "snow/layer.hpp"
#include "snow/lod.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"

namespace {

namespace r = ge::render;
namespace sn = ge::snow;

sn::LayerSpec spec() {
  sn::LayerSpec s;
  s.region = {0, 0, 40, 40};
  s.cover.depth = 0.4;
  s.near = 8;
  s.stale = 5;
  return s;
}

// A carve along x = `x` from z0 to z1, the skier the focus, starting at `t0`.
void ski(sn::Layer& layer, double x, double z0, double z1, double t0) {
  const sn::ReferenceContact rc = sn::reference_contact(0);
  const int n = static_cast<int>((z1 - z0) / 0.25);
  for (int i = 0; i < n; i++) {
    const double z = z0 + 0.25 * i, t = t0 + i * 0.05;
    const double focus[2] = {x, z};
    sn::set_focus(layer, focus, t);
    sn::press(layer, sn::place(rc.imprint, x, z, 0), t);
  }
}

// The layer the tests share: a carve folded behind the skier, boots far from him in the coarse
// layer, and the skier's own end of the track live.
void traffic(sn::Layer& layer) {
  ski(layer, 6, 1, 38, 0);
  sn::fold_stale(layer, 20);
  const sn::Imprint boot = sn::reference_contact(2).imprint;
  for (int k = 0; k < 12; k++)
    sn::press(layer, sn::place(boot, 30 + 0.3 * (k % 2), 3 + 0.7 * k, 0), 20);
  ski(layer, 20, 30, 39, 21);
}

bool same(const r::SnowRaw& a, const r::SnowRaw& b) {
  return a.depth == b.depth && a.mass == b.mass && a.bits == b.bits && a.source == b.source &&
         (a.source != 2 || a.samples == b.samples);
}

}  // namespace

TEST(the_gpu_reads_every_raw_cell_the_cpu_expects) {
  auto device = ge::test::test_device();
  if (!device) return;
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, spec());
  traffic(layer);
  CHECK(!layer.coarse.empty() && sn::live_tiles(layer) > 1);
  r::SnowMirror mirror;
  r::create_snow_mirror(*device, mirror, layer, {.window = 2});
  r::update_snow_mirror(*device, mirror, layer, 20, 36);
  const std::vector<r::SnowRaw> gpu =
      r::read_snow_raw(*device, mirror, 0, 0, layer.cols, layer.rows);
  int wrong = 0, sources[3] = {0, 0, 0};
  for (int rr = 0; rr < layer.rows; rr++) {
    for (int c = 0; c < layer.cols; c++) {
      const r::SnowRaw want = r::snow_mirror_expect(mirror, layer, c, rr);
      const r::SnowRaw& got = gpu[static_cast<size_t>(rr) * layer.cols + c];
      if (!same(want, got)) wrong++;
      sources[want.source]++;
    }
  }
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " samples differ");
  CHECK(sources[0] > 0 && sources[1] > 0 && sources[2] > 0);
  CHECK(device->errors == 0);
}

TEST(the_gpu_column_reads_the_physics_column) {
  auto device = ge::test::test_device();
  if (!device) return;
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, spec());
  r::SnowMirror mirror;
  r::create_snow_mirror(*device, mirror, layer, {.window = 8});
  r::update_snow_mirror(*device, mirror, layer, 20, 20);
  const float untouched[2] = {12.3f, 17.9f};
  const r::SnowColumnRead u = r::read_snow_columns(*device, mirror, untouched)[0];
  CHECK(u.touched == 0 && std::fabs(u.depth - 0.4f) < 1e-6f && u.drop == 0);

  traffic(layer);
  r::update_snow_mirror(*device, mirror, layer, 20, 20);
  std::vector<float> xz;
  for (int k = 0; k < 400; k++) {
    xz.push_back(6.0f + 0.013f * static_cast<float>(k % 20) - 0.12f);
    xz.push_back(2.0f + 0.091f * static_cast<float>(k));
  }
  const std::vector<r::SnowColumnRead> got = r::read_snow_columns(*device, mirror, xz);
  int touched = 0;
  double worst = 0;
  for (size_t k = 0; k < got.size(); k++) {
    const sn::Column want = sn::sample(layer, xz[2 * k], xz[2 * k + 1]);
    worst = std::fmax(worst, std::fabs(want.depth - got[k].depth));
    worst = std::fmax(worst, std::fabs(want.drop - got[k].drop));
    if (want.touched != (got[k].touched > 0)) worst = 1;
    touched += want.touched ? 1 : 0;
  }
  CHECK(touched > 100);
  CHECK_MSG(worst < 1e-4, "worst gap " + std::to_string(worst) + " m");
}

TEST(only_what_changed_is_sent_again) {
  auto device = ge::test::test_device();
  if (!device) return;
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, spec());
  traffic(layer);
  r::SnowMirror mirror;
  r::create_snow_mirror(*device, mirror, layer, {.window = 4});
  r::update_snow_mirror(*device, mirror, layer, 20, 36);
  CHECK(mirror.last.tiles > 0 && mirror.last.blocks > 0);
  r::update_snow_mirror(*device, mirror, layer, 20, 36);
  CHECK(mirror.last.tiles == 0 && mirror.last.blocks == 0 && mirror.last.bytes == 0);
  sn::press(layer, sn::place(sn::reference_contact(2).imprint, 21, 37, 0), 30);
  r::update_snow_mirror(*device, mirror, layer, 20, 36);
  CHECK(mirror.last.tiles == 1 && mirror.last.blocks == 1);
}
