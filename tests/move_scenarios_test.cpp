// THE MOVEMENT SCENARIOS, HELD: every scenario (engine/move/scenarios.cpp) run twice to the same
// bits on its preset at 120 Hz, its digest and key numbers held by
// tests/golden/move_scenarios.txt. What each one must show is held in move_step_test.cpp.
//
// A deliberate change to the model moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/move_scenarios_test`, run the `move-trace` and
// `move-strafe` labs before and after (`--tag=before`, `--compare=before`) and put both in the PR.
//
// ge:summary Test: the movement scenarios' digests and numbers held golden, run twice alike
// ge:keywords test move movement scenarios golden digest determinism
// ge:skill move testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "move/scenarios.hpp"

namespace {

namespace mv = ge::move;
constexpr const char* kGolden = "tests/golden/move_scenarios.txt";

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

std::string row_of(const mv::MoveScenario& sc, const mv::MoveRun& r) {
  return std::string(sc.id) + " " + hex(r.digest) + " " + hex(ge::test::bits(r.top_speed)) + " " +
         hex(ge::test::bits(r.distance)) + " " + hex(ge::test::bits(r.apex)) + " " +
         std::to_string(r.jumps) + " " + std::to_string(r.landings) + " " +
         std::to_string(r.steps_up);
}

}  // namespace

TEST(every_scenario_runs_the_same_twice_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::string got;
  for (const mv::MoveScenario& sc : mv::move_scenarios()) {
    const mv::MoveSpec& spec = *mv::find_preset(sc.preset);
    const mv::MoveRun a = mv::run_move_scenario(sc, spec, 120);
    const mv::MoveRun b = mv::run_move_scenario(sc, spec, 120);
    CHECK_MSG(a.digest == b.digest, std::string(sc.id) + " is not a pure function of its staging");
    got += row_of(sc, a) + "\n";
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario digest top-speed-bits distance-bits apex-bits jumps landings steps-up "
           "(move_scenarios_test, 120 Hz)\n"
        << got;
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want += line + "\n";
  }
  CHECK_MSG(got == want, "the movement scenarios moved:\n" + got + "want\n" + want +
                             "If the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the move-trace and move-strafe "
                             "labs' tables before and after in the PR (skill: move).");
}
