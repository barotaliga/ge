// THE BODY, HELD: the blow and the risk curve against their numbers, the severity score, the
// frailty draw, mending, and every ride scenario's body (plugins/skier/scenarios.cpp) held bit
// for bit by tests/golden/skier_body.txt, with the crash scenarios held to what they must do:
// a clean ride hurts nothing, a trunk at speed breaks him, a high drop breaks his legs and back.
//
// A deliberate change to the body or its catalog moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/skier_body_test`, run the `injuries` lab before and
// after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the body's blows, risks, severity and death, and every scenario's injuries
// ge:keywords test body injuries blow risk severity iss mortal death golden scenario
// ge:skill skier testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "skier/anatomy.hpp"
#include "skier/body.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"

namespace {

namespace sk = ge::skier;
constexpr const char* kGolden = "tests/golden/skier_body.txt";

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

// What a scenario's body took: its digest, its severity and every injury by part and name.
std::string row_of(const sk::Scenario& sc, const sk::ScenarioRun& r) {
  std::string s = std::string(sc.id) + " " + hex(r.body_digest) +
                  " iss=" + std::to_string(sk::severity_of(r.body)) +
                  (r.death_time >= 0    ? " dead"
                   : r.mortal_time >= 0 ? " mortal"
                                        : "");
  for (int32_t k = 0; k < r.body.count && k < sk::kMostInjuries; k++) {
    const sk::Injury& j = r.body.injuries[static_cast<size_t>(k)];
    s += std::string(" ") + sk::part_name(j.part) + ":" + sk::injury_def(j.kind).name;
  }
  return s;
}

sk::ScenarioRun ride(const char* id) {
  const sk::Scenario* sc = sk::find_scenario(id);
  CHECK_MSG(sc != nullptr, std::string("no scenario ") + id);
  return sk::run_scenario(*sc, sk::kReferenceSkis);
}

bool has(const sk::BodyState& b, sk::Part part, const char* name) {
  const int32_t kind = sk::find_injury(name);
  for (int32_t k = 0; k < b.count && k < sk::kMostInjuries; k++) {
    const sk::Injury& j = b.injuries[static_cast<size_t>(k)];
    if (j.part == part && j.kind == kind) return true;
  }
  return false;
}

}  // namespace

TEST(the_catalog_names_every_part_and_each_ladder_is_whole) {
  const auto cat = sk::injuries_catalog();
  CHECK(cat.size() == 98);
  std::vector<int> per(sk::kBases, 0);
  for (size_t k = 0; k < cat.size(); k++) {
    const sk::InjuryDef& d = cat[k];
    CHECK_MSG(d.ais >= 1 && d.ais <= 5 && d.at > 0,
              std::string(d.name) + ": an AIS of 1 to 5 and a positive dose");
    CHECK_MSG(sk::find_injury(d.name) == static_cast<int32_t>(k),
              std::string(d.name) + " is named twice in anatomy.cpp");
    per[static_cast<size_t>(d.base)]++;
  }
  for (int b = 0; b < sk::kBases; b++) CHECK_MSG(per[static_cast<size_t>(b)] > 0, "a bare part");
  for (int p = 0; p < sk::kParts; p++) {
    const auto part = static_cast<sk::Part>(p);
    CHECK(sk::sided(sk::base_of(part), sk::side_of(part)) == part);
  }
}

TEST(a_blow_is_a_stop_and_a_dose_a_chance) {
  // The headform's drop: 6.1 m/s over the head's 6 mm and the groomer's 2.2 cm peaks at about
  // 106 g.
  const double g = sk::blow_of(6.1, 0.006 + 0.022);
  CHECK_MSG(g > 100 && g < 112, "the headform on hard snow: " + std::to_string(g) + " g");
  CHECK(sk::risk_of(10, 10) == 0.5);
  CHECK(sk::risk_of(4.9, 10) == 0);
  CHECK(sk::risk_of(12.5, 10) > 0.85 && sk::risk_of(7.5, 10) < 0.1);
  CHECK(sk::energy_over(sk::Mech::twist, 20, 10) == 4);
  CHECK(sk::energy_over(sk::Mech::blunt, 20, 10) == 2);
}

TEST(the_severity_score_sums_the_three_worst_regions) {
  sk::BodyState b;
  CHECK(sk::severity_of(b) == 0);
  b.worst[static_cast<size_t>(sk::Part::head)] = 3;
  b.worst[static_cast<size_t>(sk::Part::neck)] = 2;  // the same region as the head
  b.worst[static_cast<size_t>(sk::Part::chest)] = 2;
  b.worst[static_cast<size_t>(sk::Part::abdomen)] = 1;
  b.worst[static_cast<size_t>(sk::Part::knee_l)] = 2;
  CHECK(sk::severity_of(b) == 9 + 4 + 4);
  CHECK(!sk::dies_of(1, 15));
  int dead = 0;
  for (uint32_t seed = 0; seed < 1000; seed++) dead += sk::dies_of(seed, 46) ? 1 : 0;
  CHECK_MSG(dead > 420 && dead < 580, "the even chance at 46 killed " + std::to_string(dead));
  sk::mend_body(b);
  CHECK(sk::severity_of(b) == 0 && b.count == 0);
}

TEST(every_scenario_hurts_the_same_twice_and_keeps_its_golden) {
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
    CHECK_MSG(a.body_digest == b.body_digest,
              std::string(sc.id) +
                  ": the same scenario hurt twice differs; the body keeps state "
                  "between runs or draws off something but its hash "
                  "(plugins/skier/body.cpp, skill: determinism)");
    got.push_back(row_of(sc, a));
    const size_t i = got.size() - 1;
    if (!write) {
      CHECK_MSG(i < want.size() && got[i] == want[i],
                "the body moved:\n  got  " + got[i] + "\n  want " +
                    (i < want.size() ? want[i] : std::string("(no row)")) +
                    "\nIf the move is meant, rewrite " + kGolden +
                    " with GE_GOLDEN_WRITE=1, and put the injuries lab's --compare in the PR.");
    }
  }
  if (!write) {
    CHECK_MSG(got.size() == want.size(), "a scenario was added or removed; rewrite the golden");
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario body-digest iss [mortal|dead] part:injury... "
           "(skier_body_test, reference skis, 120 Hz)\n";
    for (const auto& r : got) out << r << "\n";
  }
}

TEST(a_clean_ride_hurts_nothing) {
  // The synthetic kicker lands at 7.7 g and its pop at 11.4: past a clean landing's 6, where a
  // strained back is a chance. Every ride below lands under it.
  for (const char* id : {"tuck-schuss", "carve-hard", "turns", "pop", "rollers", "drop"}) {
    const auto r = ride(id);
    CHECK_MSG(r.body.count == 0,
              std::string(id) + " hurt him: " + std::to_string(r.body.count) + " injuries");
  }
}

TEST(a_trunk_at_speed_breaks_him_and_a_glance_bruises_his_side) {
  const auto fast = ride("tree-fast");
  CHECK_MSG(sk::severity_of(fast.body) >= 16,
            "tree-fast: a trunk at 72 km/h is major trauma; ISS " +
                std::to_string(sk::severity_of(fast.body)));
  const auto side = ride("tree-side");
  CHECK(side.body.count > 0);
  for (int32_t k = 0; k < side.body.count; k++) {
    const sk::Part p = side.body.injuries[static_cast<size_t>(k)].part;
    // The trunk stands on his left (he passes it half a metre to its right).
    CHECK_MSG(sk::side_of(p) <= 0, std::string("tree-side hurt his right: ") + sk::part_name(p));
  }
}

TEST(a_high_drop_breaks_the_legs_and_the_back_and_a_crooked_one_the_knee) {
  const auto high = ride("drop-high");
  CHECK(high.body.count > 0);
  CHECK(high.body.worst[static_cast<size_t>(sk::Part::back)] >= 2);
  CHECK(high.body.worst[static_cast<size_t>(sk::Part::foot_l)] >= 1 ||
        high.body.worst[static_cast<size_t>(sk::Part::shin_l)] >= 1);
  const auto across = ride("drop-across");
  CHECK(across.cause == sk::Wipeout::across);
  // Spun right in the air he lands with his way to the left of his skis: the left ski leads
  // the slide and catches.
  CHECK(has(across.body, sk::Part::knee_l, "torn_mcl") ||
        has(across.body, sk::Part::knee_l, "torn_acl_twist") ||
        has(across.body, sk::Part::knee_l, "torn_meniscus"));
}
