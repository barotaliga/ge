// LAB `snow-cost`: what the snow layer costs. A ski's press timed into a fine tile and into the
// coarse layer (far from every focus), `sample` timed over the cover, a fine tile and a coarse
// cell, a tile folded and refined, and a snapshot saved and loaded. Times are measured, never in
// the digest.
//
// ge:summary Bench: the snow layer's press, sample, fold, refine and snapshot costs
// ge:keywords lab bench snow cost press sample fold refine coarse snapshot time perf
// ge:skill snow perf
// ge:test tests/snow_lod_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "snow/layer.hpp"
#include "snow/lod.hpp"
#include "snow/press.hpp"
#include "snow/snapshot.hpp"

namespace {

namespace gl = ge::lab;
namespace sn = ge::snow;

using Clock = std::chrono::steady_clock;

std::string fixed(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

double us_since(Clock::time_point t0, int n) {
  return std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / n;
}

sn::LayerSpec strip() {
  sn::LayerSpec s;
  s.region = {-64, 0, 64, 1024};
  s.cover.depth = 0.4;
  s.near = 32;
  return s;
}

// A ski's step: a 25 cm capsule ending at (x, z).
sn::Imprint ski(double x, double z) {
  sn::Imprint imp;
  imp.shape = sn::capsule(x, z - 0.25, x, z, 0.035);
  imp.sink = 0.08;
  imp.side_push = 0.05;
  imp.motion = sn::Motion::slide;
  return imp;
}

void row(gl::Lab& lab, gl::LabTable& table, const std::string& key, const std::string& what,
         double us) {
  table.rows.push_back({what, fixed(us, 3)});
  lab.metric(key, us, "us", gl::Det::measured);
}

void run(gl::Lab& lab) {
  const int n = static_cast<int>(lab.arg_num("presses"));
  gl::LabTable table{"The snow layer, us a call (the mean over the run)", {"call", "us"}, {}};

  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip());
  const double focus[2] = {0, 10};
  sn::set_focus(layer, focus, 0);

  // Fine: fresh snow within 30 m of the focus, each press on ground no press has crossed.
  const auto x_of = [](int i) { return -20 + 0.33 * (i % 120); };
  const auto z_of = [](int i) { return 0.25 + 0.18 * (i / 120 % 160); };
  auto t0 = Clock::now();
  for (int i = 0; i < n; i++) sn::press(layer, ski(x_of(i), z_of(i)), i / 120.0);
  row(lab, table, "press.fine", "press, a ski into a fine tile", us_since(t0, n));

  // Coarse: the same presses 600 m from the focus.
  t0 = Clock::now();
  for (int i = 0; i < n; i++) sn::press(layer, ski(x_of(i), 600 + z_of(i)), i / 120.0);
  row(lab, table, "press.coarse", "press, a ski into the coarse layer", us_since(t0, n));

  const int reads = 10 * n;
  double sink = 0;
  t0 = Clock::now();
  for (int i = 0; i < reads; i++) sink += sn::sample(layer, 0.01 * (i % 977), 900).surface;
  row(lab, table, "sample.cover", "sample, the cover", us_since(t0, reads));
  t0 = Clock::now();
  for (int i = 0; i < reads; i++) sink += sn::sample(layer, 0.01 * (i % 977), 10).surface;
  row(lab, table, "sample.fine", "sample, a fine tile", us_since(t0, reads));
  t0 = Clock::now();
  for (int i = 0; i < reads; i++) sink += sn::sample(layer, 0.01 * (i % 977), 610).surface;
  row(lab, table, "sample.coarse", "sample, a coarse cell", us_since(t0, reads));

  // A save and a load of what the layer holds now.
  const int saves = 20;
  std::vector<uint8_t> bytes;
  t0 = Clock::now();
  for (int i = 0; i < saves; i++) bytes = sn::save(layer);
  row(lab, table, "snapshot.save", "save, the layer", us_since(t0, saves));
  sn::SnowWorld other;
  sn::Layer& copy = sn::add_layer(other, strip());
  std::string error;
  t0 = Clock::now();
  for (int i = 0; i < saves; i++) sn::load(copy, bytes, error);
  row(lab, table, "snapshot.load", "load, the layer", us_since(t0, saves));

  // Fold every live tile, then refine them back by touching each again.
  std::vector<int> slots;
  for (size_t i = 0; i < layer.tiles.size(); i++) {
    if (layer.tiles[i].tx >= 0) slots.push_back(static_cast<int>(i));
  }
  std::vector<std::pair<int, int>> origins;
  origins.reserve(slots.size());
  for (const int s : slots) origins.push_back({layer.tiles[s].tx, layer.tiles[s].tz});
  t0 = Clock::now();
  for (const int s : slots) sn::fold_tile(layer, s);
  const int folds = slots.empty() ? 1 : static_cast<int>(slots.size());
  row(lab, table, "fold", "fold, a fine tile", us_since(t0, folds));
  t0 = Clock::now();
  for (const auto& [tx, tz] : origins)
    sn::cell_for(layer, tx * sn::kTileCells, tz * sn::kTileCells);
  row(lab, table, "refine", "refine, a fine tile", us_since(t0, folds));

  lab.table(table, gl::Det::measured);
  lab.note("snapshot " + std::to_string(bytes.size()) + " bytes, " + std::to_string(folds) +
           " tiles folded and refined (checksum " + fixed(sink, 1) + ")");
}

const gl::LabSpec kSpec{"snow-cost",
                        gl::LabKind::bench,
                        "engine",
                        "engine/snow/lod.hpp",
                        "snow",
                        "What do a press, a sample, a fold, a refine and a snapshot of snow cost?",
                        {{"presses", "20000", "the presses timed a case"}},
                        "--presses=400",
                        run};
GE_LAB(kSpec);

}  // namespace
