// A ski-cross course built over a map (R35: ski_cross.hpp, the analysis in
// analysis/race_checks.hpp), held by the engine's own checks:
//
//   * DETERMINISM: the course built twice is the same course, to the digest; one built over a
//     map already carrying another course is the one built over the map under it.
//   * THE COURSE: a start straight, berms with a turning gate on each, features with a corridor
//     gate before each, every jump the course's own kicker, four lanes at the start.
//   * THE DIGEST: the course's `level_digest`, its stretch and its features are the row
//     tests/golden/mountain_ski_cross.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_ski_cross_test`, and the PR says why.
//   * THE RULES: the course passes its own analysis, and the map under it keeps its day, its
//     woods only thinned.
//
// A debug build (sanitizers) holds the single-piste rows; a release build adds a ski area's.
//
// ge:summary A ski-cross course built over a map: determinism, its features, a golden, analysis
// ge:keywords test ski-cross berm rollers jump step-down corridor-gate golden r35
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
#include "mountain/mapgen/ski_cross.hpp"
#include "mountain/mapgen/speed_ski.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_ski_cross.txt";

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

// The course a built map names; a map with none fails the test.
const mg::RaceCourse& race_of(const mg::Level& level) {
  if (!level.race || level.race->discipline != mg::Discipline::ski_cross || !level.race->stretch) {
    throw std::runtime_error("set_ski_cross returned a map with no ski-cross course");
  }
  return *level.race;
}

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The course's row: its stretch, its length and drop, its features by kind, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  const mg::RaceCourse& r = race_of(level);
  const mg::RaceCourse::Stretch st = r.stretch.value_or(mg::RaceCourse::Stretch{0, 0});
  std::string kinds;
  for (const mg::RaceCourse::Feature& f : r.features) {
    kinds.push_back(mg::cross_feature_name(f.kind)[0]);
  }
  return cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version), " ", cs.name,
              " stretch=", f0(st.from), "-", f0(st.to), " length=", f0(r.to - r.from),
              " drop=", f0(r.vertical), " gates=", std::to_string(level.checkpoints.size()),
              " features=", kinds, " digest=", mg::level_digest(level)});
}

}  // namespace

TEST(ski_cross_courses_are_deterministic_keep_their_digest_and_pass_their_analysis) {
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
    mg::Level map = mg::generate_level(cs.seed, opts);
    // A ski area builds its ski cross down the course the rule picks.
    if (const auto id = mg::ski_cross_course_of(map)) {
      opts.course = id;
      map = mg::generate_level(cs.seed, opts);
    }
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    const mg::Level course = mg::set_ski_cross(map);
    const std::string row = row_of(cs, course);
    // Determinism, and a course built over a map carrying another: the one over the map.
    CHECK_MSG(row_of(cs, mg::set_ski_cross(map)) == row, key + ": the course built twice differs");
    CHECK_MSG(row_of(cs, mg::set_ski_cross(course)) == row,
              key + ": a course built over itself is another course");
    CHECK_MSG(row_of(cs, mg::set_ski_cross(mg::set_speed_ski(map))) == row,
              key + ": a course built over a speed track is not the one built over its map");
    CHECK_MSG(mg::level_digest(*race_of(course).base) == mg::level_digest(map),
              key + ": the course's base is not the map it was built over");
    // The course: berms each with a turning gate, features, the jumps its own kickers, four lanes.
    const mg::RaceCourse& r = race_of(course);
    size_t berms = 0;
    size_t jumps = 0;
    for (const mg::RaceCourse::Feature& f : r.features) {
      if (f.kind == mg::CrossFeature::berm) berms++;
      if (f.kind == mg::CrossFeature::jump || f.kind == mg::CrossFeature::step) jumps++;
    }
    CHECK_MSG(berms >= 3, key + ": the course has fewer than three berms");
    CHECK_MSG(berms < r.features.size(), key + ": the course has no feature but its berms");
    size_t own = 0;
    for (const mg::Kicker& k : course.kickers) own += k.on_track ? 1 : 0;
    CHECK_MSG(own == jumps && r.jumps.size() == jumps,
              key + ": the course's jumps are not its own kickers");
    CHECK_MSG(course.grid.size() == 4, key + ": the start gate is not four lanes");
    got.push_back(row);
    std::printf("  %s\n", row.c_str());
    const size_t at = got.size() - 1;
    if (!write && at < want.size()) {
      CHECK_MSG(row == want[at], cat({key, ": the course moved:\n  got  ", row, "\n  want ",
                                      want[at], "\nIf the move is meant, rewrite ", kGolden,
                                      " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
    for (const an::Finding& f : an::analyze_level(course).findings) {
      CHECK_MSG(f.severity != an::Severity::error,
                cat({row, ": ", f.rule, " ", f.message,
                     "; read it in `make lab LAB=ski-cross` and fix ski_cross.cpp"}));
    }
    CHECK_MSG(course.trees.size() <= map.trees.size(), key + ": the course grew trees");
    CHECK_MSG(course.sun.hour == map.sun.hour && course.weather.kind == map.weather.kind,
              key + ": the course moved the day it was built under");
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version region, then its ski-cross course (mountain_ski_cross_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
