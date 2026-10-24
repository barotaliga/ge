// THE SKIER, HELD: every ride scenario (plugins/skier/scenarios.cpp) run on the reference pair,
// its trajectory's digest and its key numbers held bit for bit by
// tests/golden/skier_scenarios.txt, and each scenario that has one right answer held to it: the
// schusses meet `terminal_speed`, the hard carve holds no more than the edge's grip, the stop
// stops, the kicker flies, the trunk and the harsh drop wipe him out, a trunk dead ahead is met
// at the tips and one met slowly at them turns the skis aside.
//
// A deliberate change to the model moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/skier_scenarios_test`, run the `ride` lab before and
// after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the ride scenarios' digests and their expectations, on the reference pair
// ge:keywords test skier scenarios ride golden digest terminal carve stop kicker tree drop
// ge:skill skier testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "skier/limits.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"
#include "skier/snow.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"
#include "skier/tuning.hpp"

namespace {

namespace sk = ge::skier;
constexpr const char* kGolden = "tests/golden/skier_scenarios.txt";

// Whether an environment variable is set (MSVC deprecates std::getenv in favour of its own).
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

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

std::string row_of(const sk::Scenario& sc, const sk::ScenarioRun& r) {
  return std::string(sc.id) + " " + hex(r.digest) + " " + hex(ge::test::bits(r.top_speed)) + " " +
         hex(ge::test::bits(r.distance)) + " " + std::to_string(r.flights) + " " +
         sk::wipeout_name(r.cause);
}

sk::ScenarioRun ride(const char* id) {
  const sk::Scenario* sc = sk::find_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  return sk::run_scenario(*sc, sk::kReferenceSkis);
}

}  // namespace

TEST(every_scenario_rides_the_same_twice_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::string> got;
  for (const sk::Scenario& sc : sk::scenarios()) {
    const sk::ScenarioRun a = sk::run_scenario(sc, sk::kReferenceSkis);
    const sk::ScenarioRun b = sk::run_scenario(sc, sk::kReferenceSkis);
    CHECK_MSG(a.digest == b.digest, std::string(sc.id) +
                                        ": the same scenario ridden twice differs; something in "
                                        "plugins/skier keeps state between runs (skill: "
                                        "determinism)");
    got.push_back(row_of(sc, a));
    const size_t i = got.size() - 1;
    if (!write) {
      CHECK_MSG(i < want.size() && got[i] == want[i],
                "the ride moved:\n  got  " + got[i] + "\n  want " +
                    (i < want.size() ? want[i] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1, and put the ride lab's --compare in the PR.");
    }
  }
  if (!write)
    CHECK_MSG(got.size() == want.size(), "a scenario was added or removed; rewrite the golden");
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario digest top-speed-bits distance-bits flights wipeout "
           "(skier_scenarios_test, reference skis, 120 Hz)\n";
    for (const auto& r : got) out << r << "\n";
  }
}

TEST(a_straight_run_meets_the_terminal_speed_limits_states) {
  for (const sk::Scenario& sc : sk::scenarios()) {
    if (!sc.schuss) continue;
    const sk::ScenarioRun r = sk::run_scenario(sc, sk::kReferenceSkis);
    const double cda =
        sc.input(0, r.end).tuck > 0 ? sk::kReferenceSkis.cda_tuck : sk::kReferenceSkis.cda_upright;
    const double want = sk::terminal_speed(sk::kReferenceSkis, sc.pitch, cda, sc.packed);
    CHECK_MSG(r.top_speed > want * 0.99 && r.top_speed < want * 1.01,
              std::string(sc.id) + ": top " + std::to_string(r.top_speed) + " m/s against the " +
                  std::to_string(want) +
                  " terminal_speed expects; read skier.cpp's free forces "
                  "and contact against limits.hpp");
  }
  const double top = sk::top_speed(sk::kReferenceSkis) * 3.6;
  CHECK_MSG(top > 110 && top < 125, "the reference pair's top speed left the all-mountain band: " +
                                        std::to_string(top) + " km/h");
}

TEST(the_tuck_beats_standing_tall_and_the_groomer_beats_powder) {
  const auto tuck = ride("tuck-schuss");
  CHECK(tuck.top_speed > ride("tall-schuss").top_speed);
  CHECK(tuck.top_speed > ride("powder-schuss").top_speed);
  CHECK(ride("turns").end.speed > ride("powder-turns").end.speed);
}

TEST(a_carve_holds_no_more_than_the_edge_grips) {
  const auto r = ride("carve-hard");
  const double most = sk::side_grip(sk::kReferenceSkis, 1, sk::kReferenceSkis.edge_max);
  CHECK_MSG(r.max_g_lat > 0.8 && r.max_g_lat <= most * 1.01,
            "carve-hard pulled " + std::to_string(r.max_g_lat) + " g; the edge holds " +
                std::to_string(most));
  CHECK(r.cause == sk::Wipeout::none);
  CHECK(ride("carve").min_radius > 0);
}

TEST(the_brake_stops_him_and_holds_him) {
  const auto stop = ride("hockey-stop");
  CHECK_MSG(stop.stop_time > 0 && stop.distance < 30,
            "hockey-stop: " + std::to_string(stop.distance) + " m to stop");
  CHECK_MSG(ride("stand-still").distance < 0.05, "stand-still slid under a held brake");
  const auto skate = ride("skate-flat");
  CHECK_MSG(skate.end.speed * 3.6 > 10 && skate.end.speed * 3.6 < 30,
            "skate-flat: " + std::to_string(skate.end.speed * 3.6) + " km/h after 12 s");
}

TEST(a_kicker_throws_him_and_a_pop_throws_him_further) {
  const auto plain = ride("kicker");
  const auto popped = ride("kicker-pop");
  CHECK(plain.flights >= 1 && plain.longest_flight > 0.5 && plain.cause == sk::Wipeout::none);
  CHECK(popped.longest_flight > plain.longest_flight && popped.cause == sk::Wipeout::none);
  CHECK(ride("pop").flights == 1);
  CHECK(ride("rollers").cause == sk::Wipeout::none);
}

TEST(a_trunk_and_a_harsh_drop_wipe_him_out) {
  CHECK(ride("tree").cause == sk::Wipeout::tree);
  CHECK(ride("drop").cause == sk::Wipeout::none);
  const auto harsh = ride("drop-harsh");
  CHECK(harsh.cause == sk::Wipeout::landing && harsh.end.down && harsh.stop_time > 0);
}

namespace {

// A packed flat with one trunk at its middle, and the skier skiing straight down +z at `v` m/s
// from 3 m short of it, `across` m to its right, until he wipes out or `seconds` pass.
struct AtATrunk {
  sk::SkierState s;
  sk::SkierEvents first;  // the first step's events that met the trunk
  double trunk_z = 0;
};
AtATrunk ski_at_a_trunk(double v, double across, double seconds) {
  ge::mountain::mapgen::Level level = sk::flat_strip(0, 1);
  const double m = level.size / 2;
  level.trees.push_back(ge::mountain::mapgen::TreeDef{m, m, level.ground_at(m, m), 18, 0.2, 60, 2.5,
                                                      ge::mountain::mapgen::TreeKind::spruce, -1});
  const sk::TreeIndex trees = sk::build_tree_index(level);
  const sk::World world{&level, &trees};
  AtATrunk out;
  out.trunk_z = m;
  out.s = sk::stand_skier(world, m + across, m - 3, 0, v);
  bool met = false;
  for (int64_t i = 0; i < static_cast<int64_t>(seconds * 120); i++) {
    sk::SkierEvents ev;
    sk::step_skier(out.s, sk::SkierInput{}, world, sk::kReferenceSkis, 1.0 / 120, ev);
    if (!met && ev.tree >= 0) {
      met = true;
      out.first = ev;
      if (ev.wiped_out) break;
    }
  }
  return out;
}

}  // namespace

TEST(a_trunk_dead_ahead_stops_the_tips_before_his_body_reaches_it) {
  namespace w = sk::tuning::wipeout;
  const AtATrunk r = ski_at_a_trunk(12, 0, 1);
  CHECK(r.first.wiped_out && r.first.cause == sk::Wipeout::tree);
  // Down at the trunk's front with the tips on it: his boots a ski's fore-length short of it.
  const double short_of = r.trunk_z - r.s.z;
  const double tips = sk::kReferenceSkis.length * w::kTipShare;
  CHECK_MSG(short_of > tips && short_of < tips + w::kTipRadius + 0.2 + 0.05,
            "the trunk met " + std::to_string(short_of) + " m ahead of his boots");
}

TEST(a_trunk_met_at_the_tips_slowly_turns_the_skis_aside) {
  // Slow and a little to the trunk's right: his tips glance off it, and the skis turn away from
  // it (to his right, a heading past 0) rather than his body meeting it square.
  const AtATrunk r = ski_at_a_trunk(2.5, 0.25, 2);
  CHECK(r.first.glanced && !r.first.wiped_out);
  CHECK_MSG(r.s.heading > 0.05, "the skis turned to " + std::to_string(r.s.heading) + " rad");
  CHECK(!r.s.down);
}
