// engine/wind/storm.hpp: a cell runs towering, mature, dissipating and gone in order; the updraft
// leads, the rain and the lightning follow it and the downdraft and the gust front come with
// maturity; a stronger cell is taller, rains harder and flashes far more often; the gust front's
// head is the strongest air, shallow, gone past its edge and stronger ahead of the drift; the
// rain falls off from the core; a multicell storm renews on its forward-right flank; and every
// number keeps its bits in tests/golden/wind_storm.txt.
//
// A deliberate change to the storm moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_storm_test`, and put the `storm` lab's tables
// before and after in the PR.
//
// ge:summary Test: thunderstorm cells' life cycle, gust front, rain cores, multicell; golden
// ge:keywords test storm cell life-cycle gust-front outflow rain multicell golden
// ge:skill water testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/storm.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_storm.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

gw::Storm one(double peak, double vx = 0, double vz = 0) {
  gw::Storm s;
  s.vx = vx;
  s.vz = vz;
  s.cells = 1;
  s.cell[0].peak = peak;
  s.cell[0].seed = 7;
  return s;
}

}  // namespace

TEST(a_cell_runs_its_stages_in_order) {
  const gw::Storm s = one(0.5);
  CHECK(gw::cell_at(s, 0, -1).stage == gw::Stage::none);
  CHECK(gw::cell_at(s, 0, 60).stage == gw::Stage::towering);
  CHECK(gw::cell_at(s, 0, gw::kTowering + 10).stage == gw::Stage::mature);
  const double t2 = gw::kTowering + gw::mature_of(0.5);
  CHECK(gw::cell_at(s, 0, t2 + 10).stage == gw::Stage::dissipating);
  CHECK(gw::cell_at(s, 0, gw::life_of(0.5) + 1).stage == gw::Stage::none);
  // The cumulus stage: an updraft, no rain, no lightning, no front.
  const gw::CellAt early = gw::cell_at(s, 0, 300);
  CHECK(early.updraft > 0 && early.rain == 0 && early.flashes == 0 && early.front == 0);
  // Mature: rain, lightning, a downdraft and the gust front out.
  const gw::CellAt mid = gw::cell_at(s, 0, gw::kTowering + 400);
  CHECK(mid.rain > 0.5 && mid.flashes > 2 && mid.downdraft > 4 && mid.front > 1000);
  CHECK(mid.top > 9000 && mid.top < 11000);
  // Dissipating: no updraft, light rain.
  const gw::CellAt late = gw::cell_at(s, 0, t2 + 1200);
  CHECK(late.updraft == 0 && late.rain < 0.3 * mid.rain);
}

TEST(a_stronger_cell_is_taller_wetter_and_far_more_electric) {
  const double t = gw::kTowering + 500;
  const gw::CellAt weak = gw::cell_at(one(0.3), 0, t), ordinary = gw::cell_at(one(0.5), 0, t),
                   severe = gw::cell_at(one(1), 0, t);
  CHECK(weak.top < ordinary.top && ordinary.top < severe.top);
  CHECK(weak.rain < ordinary.rain && ordinary.rain < severe.rain);
  CHECK(severe.flashes > 5 * ordinary.flashes);
  CHECK(severe.updraft > 35 && ordinary.updraft > 18 && ordinary.updraft < 30);
  CHECK(severe.in_cloud > ordinary.in_cloud);
}

TEST(the_first_flashes_stay_in_the_cloud) {
  const gw::Storm s = one(0.6);
  double first = 0;
  for (int step = 0; step < 400; ++step) {
    const double t = step * 5.0;
    if (gw::cell_at(s, 0, t).flashes > 0) {
      first = t;
      break;
    }
  }
  CHECK(first > 400 && first < gw::kTowering + 60);
  CHECK(gw::cell_at(s, 0, first + 30).in_cloud > 0.9);
  const double t2 = gw::kTowering + gw::mature_of(0.6);
  CHECK(gw::cell_at(s, 0, t2 + 800).in_cloud < 0.7);
}

TEST(the_gust_front_head_is_the_strongest_shallow_air) {
  const gw::Storm s = one(0.6, 0, 8);
  const double t = gw::kTowering + 600;
  const gw::CellAt c = gw::cell_at(s, 0, t);
  auto at = [&](double dx, double dz, double above) {
    return gw::outflow_at(s, {c.x + dx, above, c.z + dz, above}, t);
  };
  const double head = at(c.front - 100, 0, 30).speed;
  CHECK(head > at(0.5 * c.front, 0, 30).speed);
  CHECK(at(c.front + 700, 0, 30).speed == 0);
  CHECK(at(c.front - 100, 0, 3000).speed == 0);
  CHECK(at(c.front - 100, 0, 3).speed < head);
  // Ahead of the drift (+z) the outflow and the drift add; behind they oppose.
  CHECK(at(0, c.front - 100, 30).speed > at(0, -(c.front - 100), 30).speed);
  // Outward from the cell, lifting the air at the head.
  const ge::wind::Air a = at(c.front - 100, 0, 30);
  CHECK(a.x > 0 && a.y > 0 && a.gust > 0.5);
  CHECK(gw::outflow_at(s, {c.x, 30, c.z, 30}, 300).speed == 0);
}

TEST(the_rain_falls_off_from_the_core) {
  const gw::Storm s = one(0.7);
  const double t = gw::kTowering + 400;
  const double core = gw::rain_at(s, 0, 0, t);
  CHECK(core > 0.7 && core <= 1);
  CHECK(gw::rain_at(s, 3000, 0, t) < 0.5 * core);
  CHECK(gw::rain_at(s, 20000, 0, t) < 0.01);
}

TEST(a_multicell_storm_renews_ahead_and_to_the_right) {
  gw::StormSpec spec;
  spec.seed = 38;
  spec.renew = 3600;
  spec.severity = 0.6;
  spec.vz = 10;
  const gw::Storm s = gw::lay_storm(spec);
  CHECK(s.cells >= 4 && s.cells <= 7);
  for (int k = 1; k < s.cells; ++k) {
    const gw::Cell &a = s.cell[k - 1], &b = s.cell[k];
    CHECK(b.born - a.born >= 600 && b.born - a.born <= 900);
    // Ahead (+z) and to the right (clockwise from +z is +x) of where the last one has drifted.
    const double gap = b.born - a.born;
    CHECK(b.z > a.z + spec.vz * gap && b.x > a.x);
    CHECK(b.peak > 0 && b.peak <= 1);
  }
  spec.renew = 0;
  CHECK(gw::lay_storm(spec).cells == 1);
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  gw::StormSpec spec;
  spec.seed = 38;
  spec.renew = 2400;
  spec.severity = 0.7;
  spec.vx = 6;
  spec.vz = 4;
  const gw::Storm s = gw::lay_storm(spec);
  got.reserve(s.cells + 4);
  for (int k = 0; k < s.cells; ++k) {
    got.push_back({ge::test::bits(s.cell[k].x), ge::test::bits(s.cell[k].z),
                   ge::test::bits(s.cell[k].born), ge::test::bits(s.cell[k].peak),
                   ge::test::bits(s.cell[k].core), s.cell[k].seed});
  }
  for (const double t : {200.0, 1100.0, 2500.0, 4100.0}) {
    const gw::CellAt c = gw::cell_at(s, 0, t);
    const gw::Air a = gw::outflow_at(s, {c.x + 2500, 20, c.z + 900, 20}, t);
    got.push_back({ge::test::bits(c.updraft), ge::test::bits(c.top), ge::test::bits(c.rain),
                   ge::test::bits(c.flashes), ge::test::bits(c.in_cloud), ge::test::bits(c.front),
                   ge::test::bits(a.x), ge::test::bits(a.z),
                   ge::test::bits(gw::rain_at(s, c.x + 1500, c.z, t))});
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# cells: x z born peak core seed; then per moment of cell 0: updraft top rain "
           "flashes in-cloud front, outflow x z, rain (wind_storm_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t word : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(word));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "the storm moved; if the move is meant, rewrite " + std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the storm "
                             "lab's tables before and after in the PR (skill: water)");
}
