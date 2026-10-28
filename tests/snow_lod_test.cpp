// The snow layer's level of detail (engine/snow/lod.hpp): folding a tile and refining it keeps the
// mass to the unit; an imprint far from every focus lands in the coarse layer and wears it; a
// track longer than the tile budget stays inside it with nothing refused, and the far end reads
// worn when a skier comes back; `fold_stale` folds only what no focus is near; a snapshot keeps
// the coarse layer; and the ledger holds with the weather on.
//
// ge:summary Tests: snow level of detail (fold, refine, the coarse press, the budget, snapshots)
// ge:keywords test snow lod fold refine coarse focus budget stale snapshot mass ledger
// ge:skill snow testing
// ge:test tests/snow_lod_test.cpp
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "snow/layer.hpp"
#include "snow/lod.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"
#include "snow/snapshot.hpp"
#include "snow/time.hpp"

namespace sn = ge::snow;

namespace {

sn::LayerSpec strip(double length, uint32_t max_tiles, const sn::Weather* w = nullptr) {
  sn::LayerSpec s;
  s.region = {-8, 0, 8, length};
  s.cover.depth = 0.4;
  s.max_tiles = max_tiles;
  s.near = 12;
  s.stale = 30;
  s.weather = w;
  return s;
}

bool ledger(const sn::Layer& l) { return sn::mass_units(l) + l.spilled == l.mass_in; }

// A skier's carve from z0 to z1 along x = 0, the focus on the ski at every press.
void ski(sn::Layer& layer, double z0, double z1, double t0) {
  const sn::ReferenceContact rc = sn::reference_contact(0);
  const int n = static_cast<int>((z1 - z0) / 0.25);
  for (int i = 0; i < n; i++) {
    const double z = z0 + 0.25 * i, t = t0 + i * 0.05;
    const double focus[2] = {0, z};
    sn::set_focus(layer, focus, t);
    sn::press(layer, sn::place(rc.imprint, 0, z, 0), t);
  }
}

}  // namespace

TEST(folding_and_refining_a_tile_keeps_the_mass_to_the_unit) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(20, 64));
  ski(layer, 1, 5, 0);
  const uint64_t units = sn::mass_units(layer);
  const double before = sn::sample(layer, 0, 3).surface;
  CHECK(ledger(layer) && sn::live_tiles(layer) > 0);
  const int tiles = sn::live_tiles(layer);
  for (size_t i = 0; i < layer.tiles.size(); i++) sn::fold_tile(layer, static_cast<int>(i));
  CHECK(sn::live_tiles(layer) == 0 && layer.folds == static_cast<uint32_t>(tiles));
  CHECK(ledger(layer));
  // The groove is now a worn coarse cell: lower than the cover, by the mean of what it held.
  const sn::Column coarse = sn::sample(layer, 0, 3);
  CHECK(coarse.touched && coarse.drop > 0 && coarse.drop < 0.1);
  // Refined back: every unit the touched coarse cells held is in the fine cells again.
  sn::Cell* cell = sn::cell_for(layer, 80, 30);
  CHECK(cell != nullptr && layer.refines == 1);
  CHECK(ledger(layer));
  CHECK(sn::mass_units(layer) <= units);            // untouched coarse cells went back to the cover
  CHECK(before < sn::sample(layer, 0, 3).surface);  // the groove's detail is now spread
}

TEST(an_imprint_far_from_every_focus_wears_the_coarse_layer) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(200, 64));
  const double focus[2] = {0, 5};
  sn::set_focus(layer, focus, 0);
  const sn::Imprint boot = sn::reference_contact(2).imprint;
  const sn::Pressed p = sn::press(layer, sn::place(boot, 0, 150, 0), 0);
  CHECK(p.coarse >= 1 && sn::live_tiles(layer) == 0);
  CHECK(layer.coarse.size() == 1);
  const sn::Column c = sn::sample(layer, 0, 150);
  CHECK(c.touched && c.drop > 0.001);
  CHECK(ledger(layer));
  CHECK(!layer.records.empty());
  // Near the focus, the same boot keeps its fine detail.
  const sn::Pressed q = sn::press(layer, sn::place(boot, 0, 6, 0), 0);
  CHECK(q.coarse == 0 && q.samples > 0 && sn::live_tiles(layer) == 1);
}

TEST(a_track_longer_than_the_budget_stays_inside_it_and_reads_worn_coming_back) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(64, 3));
  ski(layer, 0.5, 60, 0);
  CHECK(sn::live_tiles(layer) <= 3);
  CHECK(layer.refused == 0 && layer.folds > 0);
  CHECK(ledger(layer));
  // The start of the run was folded; it is worn, not untouched.
  CHECK(sn::cell_at(layer, 80, 20) == nullptr);
  const sn::Column back = sn::sample(layer, 0, 2);
  CHECK(back.touched && back.drop > 0);
  // Skiing back over it refines it and carves again; the books still balance.
  ski(layer, 1, 3, 100);
  CHECK(ledger(layer) && layer.refines > 0);
}

TEST(fold_stale_folds_only_what_no_focus_is_near) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(64, 64));
  ski(layer, 1, 4, 0);    // tile row 0
  ski(layer, 40, 44, 1);  // tile row 6
  const double focus[2] = {0, 42};
  sn::set_focus(layer, focus, 100);
  const int live = sn::live_tiles(layer);
  CHECK(sn::fold_stale(layer, 100) > 0);
  CHECK(sn::live_tiles(layer) < live);
  CHECK(sn::cell_at(layer, 80, 20) == nullptr);   // far and stale: folded
  CHECK(sn::cell_at(layer, 80, 420) != nullptr);  // the focus stands there: kept
  CHECK(sn::fold_stale(layer, 100) == 0);
  CHECK(ledger(layer));
}

TEST(a_snapshot_keeps_the_coarse_layer) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(64, 2));
  ski(layer, 0.5, 40, 0);
  CHECK(!layer.coarse.empty());
  const std::vector<uint8_t> bytes = sn::save(layer);
  sn::SnowWorld other;
  sn::Layer& copy = sn::add_layer(other, strip(64, 2));
  std::string error;
  CHECK_MSG(sn::load(copy, bytes, error), error);
  CHECK(sn::save(copy) == bytes);
  CHECK(sn::mass_units(copy) == sn::mass_units(layer) && ledger(copy));
  for (int i = 0; i < 30; i++) {
    const double z = 0.5 + 1.3 * i;
    CHECK(sn::sample(copy, 0, z).surface == sn::sample(layer, 0, z).surface);
  }
}

TEST(the_ledger_holds_with_the_weather_through_folds) {
  sn::Weather w;
  w.fall = {{0, 600, 1200}, {3.0 / 3600, 0, 2.0 / 3600}};
  w.air = {{0, 900}, {-6, 1}};
  w.wind = {{0}, {8}};
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, strip(64, 2, &w));
  ski(layer, 0.5, 40, 0);
  CHECK(ledger(layer) && layer.folds > 0);
  ski(layer, 1, 6, 1500);
  CHECK(sn::fold_stale(layer, 3000) >= 0);
  CHECK(ledger(layer));
  CHECK(sn::sample(layer, 0, 20, 3000).depth > 0);
}
