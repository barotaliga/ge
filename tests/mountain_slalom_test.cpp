// A slalom set over a built map (R31: slalom.hpp, course_prep.hpp, the analysis in
// analysis/race.hpp), held by the engine's own checks:
//
//   * DETERMINISM: the same map and run set twice is the same course, to the digest; a run set
//     over a map that already carries the other run's course is the same course again, on the
//     same base.
//   * THE TWO RUNS: on the same stretch, their gates set afresh.
//   * THE DIGEST: each course's `level_digest`, stretch and combinations are the row
//     tests/golden/mountain_slalom.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_slalom_test`, and the PR says why.
//   * THE RULES: every course passes its own analysis (R31), and the map under it is the map it
//     was set over, its woods only thinned and its kickers only levelled.
//
// A debug build (sanitizers) holds the single-piste rows; a release build adds a ski area's red
// course.
//
// ge:summary A slalom set over a map: determinism, the two runs, a digest golden, its analysis
// ge:keywords test slalom set-slalom race-course pole-gate two-runs golden r31
// ge:skill testing mountain
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/slalom.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_slalom.txt";

struct Case {
  double seed;
  int version;
  mg::RegionId region;
  const char* name;
};
constexpr Case kCases[] = {
    {38, 1, mg::RegionId::alpine, "alpine"},
    {1, 1, mg::RegionId::maritime, "maritime"},
    {3, 1, mg::RegionId::fell, "fell"},
    {38, 8, mg::RegionId::alpine, "alpine"},
};

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

std::string cat(std::initializer_list<std::string_view> parts) {
  std::string out;
  for (const std::string_view p : parts) out.append(p);
  return out;
}

// The course a set map names; a map with none fails the test.
const mg::RaceCourse& race_of(const mg::Level& level) {
  if (!level.race) throw std::runtime_error("set_slalom returned a map with no course");
  return *level.race;
}

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The course's row: the stretch, the gates, the combinations, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  const mg::RaceCourse& r = race_of(level);
  return cat({std::to_string(int(cs.seed)),
              " v",
              std::to_string(cs.version),
              " ",
              cs.name,
              " run=",
              std::to_string(r.run),
              " from=",
              f0(r.from),
              " to=",
              f0(r.to),
              " gates=",
              std::to_string(level.checkpoints.size() - 2),
              " hairpins=",
              std::to_string(r.hairpins),
              " verticals=",
              std::to_string(r.verticals),
              " delays=",
              std::to_string(r.delays),
              " digest=",
              mg::level_digest(level)});
}

}  // namespace

TEST(slalom_courses_are_deterministic_keep_their_digest_and_pass_their_analysis) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
#ifdef NDEBUG
  constexpr size_t kHeld = std::size(kCases);
#else
  constexpr size_t kHeld = 3;
#endif
  std::vector<std::string> got;
  for (size_t c = 0; c < kHeld; c++) {
    const Case& cs = kCases[c];
    mg::GenerateOptions opts;
    opts.version = cs.version;
    opts.region = cs.region;
    if (cs.version != 1) opts.grade = mg::PisteGrade::red;
    const mg::Level map = mg::generate_level(cs.seed, opts);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    const mg::Level run1 = mg::set_slalom(map, 1);
    const mg::Level run2 = mg::set_slalom(map, 2);
    // Determinism, and a run set over the other run's map: the same course on the same base.
    const mg::Level again = mg::set_slalom(run2, 1);
    CHECK_MSG(row_of(cs, again) == row_of(cs, run1),
              cat({key, ": run 1 set over run 2's map is another course:\n  ", row_of(cs, again),
                   "\n  ", row_of(cs, run1)}));
    CHECK_MSG(race_of(again).base == race_of(run2).base,
              key + ": run 1 set over run 2's map stands on another base map");
    CHECK_MSG(mg::level_digest(*race_of(run1).base) == mg::level_digest(map),
              key + ": the course's base is not the map it was set over");
    // The two runs: one stretch, two courses.
    CHECK_MSG(race_of(run1).from == race_of(run2).from && race_of(run1).to == race_of(run2).to,
              key + ": the two runs stand on different stretches");
    CHECK_MSG(mg::level_digest(run1) != mg::level_digest(run2),
              key + ": the second run is the first run's course again");
    for (const mg::Level* level : {&run1, &run2}) {
      const std::string row = row_of(cs, *level);
      const size_t at = got.size();
      got.push_back(row);
      if (!write && at < want.size()) {
        CHECK_MSG(row == want[at], cat({key, ": the course moved:\n  got  ", row, "\n  want ",
                                        want[at], "\nIf the move is meant, rewrite ", kGolden,
                                        " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
      }
      for (const an::Finding& f : an::analyze_level(*level).findings) {
        CHECK_MSG(f.severity != an::Severity::error,
                  cat({key, " run ", std::to_string(race_of(*level).run), ": ", f.rule, " ",
                       f.message, "; read it in `make lab LAB=slalom` and fix slalom.cpp"}));
      }
      // The map under the course: its woods only thinned, its kickers only levelled.
      CHECK_MSG(
          level->trees.size() <= map.trees.size() && level->kickers.size() <= map.kickers.size(),
          key + ": the course grew trees or kickers");
      CHECK_MSG(level->sun.hour == map.sun.hour && level->weather.kind == map.weather.kind,
                key + ": the course moved the day it was set under");
    }
    std::printf("  %s\n  %s\n", got[got.size() - 2].c_str(), got.back().c_str());
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version region, then each run's course (mountain_slalom_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
