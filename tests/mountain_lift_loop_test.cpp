// THE LIFTS RUN, HELD (plugins/mountain/lifts/lift_loop.hpp) on three hand-built lifts, one of
// each kind: the loop closes on itself, a carrier never leaps, the carriers come round on the
// loop's period, a detachable's carriers slow through its terminals and bunch there, exactly one
// carrier passes the load point a headway, the load zone takes a skier only slow and facing up
// the line, and every carrier's place at three moments is held bit for bit by
// tests/golden/mountain_lift_loop.txt.
//
// A deliberate change to the loop moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/mountain_lift_loop_test`, and put the `lift-ride` lab's
// table before and after in the PR.
//
// ge:summary Test: the lift loop: closure, no leaps, the period, terminals, load zone; golden
// ge:keywords test lift-loop carrier_at carrier_place carrier_passing load-zone golden
// ge:skill mountain testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/math.hpp"
#include "golden.hpp"
#include "mountain/lifts/lift_loop.hpp"

namespace {

namespace ml = ge::mountain::lifts;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/mountain_lift_loop.txt";

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

// A lift of `kind` from (100, 900) up to the north-east, `length` m of plan.
ml::LiftPlan lift(mg::LiftKind kind, double length, int32_t index) {
  mg::Lift l;
  l.id = "L" + std::to_string(index);
  l.kind = kind;
  l.bottom = {100, 0, 900};
  l.top = {100 + length * 0.6, 300, 900 - length * 0.8};
  return ml::plan_lift(l, index);
}

std::vector<ml::LiftPlan> lifts() {
  return {lift(mg::LiftKind::gondola, 1200, 0), lift(mg::LiftKind::chair, 800, 1),
          lift(mg::LiftKind::drag, 300, 2)};
}

double distance(const ml::Place& a, const ml::Place& b) {
  return ge::core::hypot(a.x - b.x, a.z - b.z);
}

}  // namespace

TEST(a_carrier_goes_round_its_loop_without_a_leap_and_comes_round_on_its_period) {
  for (const ml::LiftPlan& p : lifts()) {
    CHECK(p.carriers == static_cast<int32_t>(2 * p.length / p.ride.every));
    const double span = p.clock.empty() ? p.loop : p.span;
    const double period = span / p.ride.speed;
    const double dt = 1.0 / 120;
    ml::Place last = ml::carrier_place(p, ml::carrier_at(p, 0, 0));
    double most = 0;
    const auto steps = static_cast<int>(period / dt);
    for (int i = 1; i <= steps; i++) {
      const ml::Place at = ml::carrier_place(p, ml::carrier_at(p, 0, i * dt));
      most = ge::core::num::max(most, distance(at, last));
      last = at;
    }
    CHECK_MSG(most < p.ride.speed * dt * 1.05,
              "a carrier leaped " + std::to_string(most) +
                  " m in a step; read loop_spot and "
                  "loop_at in plugins/mountain/lifts/lift_loop.cpp (skill: mountain)");
    for (const double t : {3.0, 97.5}) {
      CHECK(distance(ml::carrier_place(p, ml::carrier_at(p, 1, t)),
                     ml::carrier_place(p, ml::carrier_at(p, 1, t + period))) < 1e-6);
    }
  }
}

TEST(a_detachables_carriers_slow_through_its_terminals_and_bunch_there) {
  const ml::LiftPlan g = lift(mg::LiftKind::gondola, 1200, 0);
  CHECK(!g.clock.empty() && g.span > g.loop);
  CHECK(ml::carrier_speed_at(g, ml::load_at(g), 0) == g.ride.slow);
  CHECK(ml::carrier_speed_at(g, g.length / 2, 0) == g.ride.speed);
  CHECK(ml::carrier_speed_at(g, ml::off_at(g), 0) == g.ride.slow);
  // A carrier spends far longer in its first 5 m than in 5 m out on the line's middle.
  const auto dwell = [&g](double from, double to) {
    double t = 0;
    for (int i = 0; from + i * 0.1 < to; i++) t += 0.1 / ml::carrier_speed_at(g, from + i * 0.1, 0);
    return t;
  };
  CHECK(dwell(ml::load_at(g), ml::load_at(g) + 5) > 2.5 * dwell(600, 605));
  const ml::LiftPlan d = lift(mg::LiftKind::drag, 300, 2);
  CHECK(d.clock.empty() && ml::carrier_speed_at(d, 10, 0) == d.ride.speed);
}

TEST(one_carrier_passes_the_load_point_a_headway) {
  for (const ml::LiftPlan& p : lifts()) {
    const double span = p.clock.empty() ? p.loop : p.span;
    const double headway = span / p.carriers / p.ride.speed;
    const double dt = 1.0 / 120;
    int passed = 0;
    int32_t last = -1;
    const auto steps = static_cast<int>(10 * headway / dt);
    for (int i = 0; i < steps; i++) {
      const int32_t k = ml::carrier_passing(p, ml::load_at(p), i * dt, dt);
      if (k < 0) continue;
      CHECK(k != last);
      last = k;
      passed++;
    }
    CHECK_MSG(passed >= 9 && passed <= 11,
              std::to_string(passed) + " carriers passed the load point in ten headways");
  }
}

TEST(the_load_zone_takes_a_skier_slow_and_facing_up_the_line) {
  for (const ml::LiftPlan& p : lifts()) {
    const ml::Place at = ml::load_spot(p);
    CHECK(ml::in_load_zone(p, at.x, at.z, p.heading, 0));
    CHECK(ml::in_load_zone(p, at.x, at.z, p.heading, p.ride.entry.fastest));
    CHECK(!ml::in_load_zone(p, at.x, at.z, p.heading, p.ride.entry.fastest + 0.1));
    CHECK(!ml::in_load_zone(p, at.x, at.z, p.heading + ge::core::kPi, 0));
    CHECK(!ml::in_load_zone(p, at.x + p.dz * 10, at.z - p.dx * 10, p.heading, 0));
    const mg::LinePoint f = ml::line_frame(p, at.x, at.z);
    CHECK(std::abs(f.u - p.ride.entry.at) < 1e-9 && std::abs(f.v - p.ride.entry.side) < 1e-9);
  }
  CHECK(ml::off_at(lift(mg::LiftKind::gondola, 1200, 0)) == 1200 - mg::kGondolaDoor);
}

TEST(every_carriers_place_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  for (const ml::LiftPlan& p : lifts()) {
    got.push_back(
        {static_cast<uint64_t>(p.carriers), ge::test::bits(p.loop), ge::test::bits(p.span)});
    for (const double t : {0.0, 41.7, 523.3}) {
      for (int32_t k = 0; k < p.carriers; k += p.carriers / 3) {
        const ml::LoopSpot s = ml::carrier_at(p, k, t);
        const ml::Place at = ml::carrier_place(p, s);
        got.push_back({ge::test::bits(s.u), static_cast<uint64_t>(s.side), ge::test::bits(s.turn),
                       ge::test::bits(at.x), ge::test::bits(at.z), ge::test::bits(at.heading)});
      }
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# per lift: carriers, loop bits, span bits; then per moment and carrier: u, side, "
           "turn, x, z, heading (mountain_lift_loop_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t w : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(w));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "a carrier moved; if the move is meant, rewrite " + std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the lift-ride "
                             "lab's table before and after in the PR (skill: mountain)");
}
