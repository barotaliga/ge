// A ski area raced as a map (generator version 8: resort_level.hpp, course_gates.hpp,
// course_day.hpp, the area's analysis in analysis/resort.hpp), held by the engine's own checks:
//
//   * DETERMINISM: the same seed built twice is the same map, to the digest and every section.
//   * THE DIGEST: each map's `level_digest` and the course it races are the row
//     tests/golden/mountain_resort_level.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_resort_level_test`, and the PR says why.
//   * THE RULES: the map passes its own analysis (the course and the area, R25–R30); the course
//     raced is one of the area's; its slalom gates stand inside the piste where they stand, in
//     order, spaced in the band but the last; its sun stands within the bearing the face was
//     turned to (R15); asking for a course by id races that course on the same snow, and asking
//     for a colour races the nearest colour the area has (R28).
//   * THE DEALT COLOUR: `deal_grade` keeps its odds over a thousand seeds.
//   * THE VERSIONS: a version that lays the ski routes (9, 10) is refused by name.
//
// A debug build (sanitizers) holds the first row; a release build holds them all, and the courses
// asked for by id and by colour.
//
// ge:summary A ski area raced as a map: determinism, a digest golden, its analysis and the gates
// ge:keywords test resort-level course-map race-map gates course-day choose-course deal-grade
// ge:keywords version-8 golden
// ge:skill testing mountain
#include <array>
#include <chrono>
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
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/sun.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_resort_level.txt";

struct Case {
  double seed;
  mg::RegionId region;
  const char* name;
};
constexpr Case kCases[] = {
    {38, mg::RegionId::alpine, "alpine"},
    {1, mg::RegionId::maritime, "maritime"},
    {3, mg::RegionId::fell, "fell"},
    {4, mg::RegionId::continental, "continental"},
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

// Words joined into one message, with no temporaries.
std::string cat(std::initializer_list<std::string_view> parts) {
  std::string out;
  for (const std::string_view p : parts) out.append(p);
  return out;
}

// The area a v8 map publishes, and the colour it races; a map with neither fails the test.
const mg::ResortMap& area_of(const mg::Level& level) {
  if (!level.resort) throw std::runtime_error("the map publishes no ski area");
  return *level.resort;
}
mg::PisteGrade grade_of(const mg::Level& level) {
  if (!level.grade) throw std::runtime_error("the map names no colour");
  return *level.grade;
}

mg::Level build(const Case& cs, std::optional<std::string> course = std::nullopt,
                std::optional<mg::PisteGrade> grade = std::nullopt) {
  mg::GenerateOptions opts;
  opts.version = 8;
  opts.region = cs.region;
  opts.course = std::move(course);
  opts.grade = grade;
  return mg::generate_level(cs.seed, opts);
}

// The map's row: the attempt, the course raced and its colour, the gates, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  return cat(
      {std::to_string(int(cs.seed)), " ", cs.name, " attempt=", std::to_string(level.attempt),
       " course=", area_of(level).course, " grade=", mg::grade_name(grade_of(level)),
       " courses=", std::to_string(area_of(level).courses.size()),
       " gates=", std::to_string(level.checkpoints.size()), " digest=", mg::level_digest(level)});
}

// The rules a race map keeps, beyond its analysis.
void check_map(const mg::Level& level, const std::string& key) {
  const an::LevelAnalysis checked = an::analyze_level(level);
  for (const an::Finding& f : checked.findings) {
    CHECK_MSG(f.severity != an::Severity::error,
              cat({key, ": the map was accepted with ", f.rule, " ", f.message,
                   "; generate.cpp's accept test must refuse what the analysis errors on"}));
  }
  const mg::ResortMap& area = area_of(level);
  bool listed = false;
  for (const mg::Course& c : area.courses) listed = listed || c.id == area.course;
  CHECK_MSG(listed, cat({key, ": course ", area.course, " is not one the area offers"}));
  // R11, R28 — the gates: inside the piste, in order, spaced in the band but the last.
  const auto& gates = level.checkpoints;
  CHECK_MSG(gates.size() >= 3, cat({key, ": fewer than three gates"}));
  namespace G = mg::resort_rules::course::gates;
  for (size_t i = 1; i < gates.size(); i++) {
    const double gap = gates[i].s - gates[i - 1].s;
    const std::string at = cat({key, ": gate ", std::to_string(i)});
    CHECK_MSG(gap > 0, at + " stands up the course from the one before");
    if (i + 1 < gates.size()) {
      CHECK_MSG(gap >= G::spacing.min - 0.5 && gap <= G::spacing.max + 0.5,
                at + " stands " + std::to_string(gap) + " m after the one before");
      CHECK_MSG(gates[i].offset && gates[i].span, at + " is a slalom gate with no offset");
      const double inside = gates[i].span.value_or(0) / 2 - std::abs(gates[i].offset.value_or(0)) -
                            gates[i].width / 2;
      CHECK_MSG(inside >= G::margin - 0.5,
                at + "'s panels stand " + std::to_string(inside) + " m inside the piste");
    } else {
      CHECK_MSG(gap <= G::spacing.max + 0.5, at + ", the finish, stands too far down");
    }
  }
  // R15 — the course's sun turned to the face.
  CHECK_MSG(level.sun.facing.has_value(), cat({key, ": the sun names no face"}));
  CHECK_MSG(mg::sun_off_face(level.sun) <= mg::rules::sun::facing + 1e-9,
            cat({key, ": the sun stands off the face's bearing"}));
}

}  // namespace

TEST(resort_maps_are_deterministic_keep_their_digest_and_pass_their_analysis) {
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
  constexpr size_t kHeld = 1;
#endif
  std::vector<std::string> got;
  for (size_t c = 0; c < kHeld; c++) {
    const Case& cs = kCases[c];
    const auto t0 = std::chrono::steady_clock::now();
    const mg::Level level = build(cs);
    const mg::Level again = build(cs);
    const std::string row = row_of(cs, level);
    const std::string key = cat({std::to_string(int(cs.seed)), " ", cs.name});
    CHECK_MSG(row == row_of(cs, again),
              cat({key, ": the same seed built twice differs:\n  ", row, "\n  ", row_of(cs, again),
                   "\nSomething reads state outside the attempt (a static, an address)."}));
    got.push_back(row);
    if (!write && c < want.size()) {
      CHECK_MSG(row == want[c], cat({key, ": the map moved:\n  got  ", row, "\n  want ", want[c],
                                     "\nIf the move is meant, rewrite ", kGolden,
                                     " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
    check_map(level, key);
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %s: built twice in %.0f ms\n", row.c_str(), ms);
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed region, then the map raced (mountain_resort_level_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}

TEST(a_course_asked_for_is_raced_on_the_same_snow) {
#ifndef NDEBUG
  std::printf(
      "  held by a release build: three ski areas built under the sanitizers take minutes\n");
  return;
#endif
  const Case& cs = kCases[0];
  const mg::Level dealt = build(cs);
  const std::vector<mg::Course>& courses = area_of(dealt).courses;
  CHECK_MSG(courses.size() >= 2, "seed 38 offers one course; pick a seed that offers more");
  if (courses.size() < 2) return;
  // By id: another course, the same area and woods.
  const mg::Course& other = courses[0].id == area_of(dealt).course ? courses[1] : courses[0];
  const mg::Level asked = build(cs, other.id);
  CHECK_MSG(area_of(asked).course == other.id,
            cat({"asked for course ", other.id, ", raced ", area_of(asked).course}));
  CHECK_MSG(asked.attempt == dealt.attempt && asked.trees.size() == dealt.trees.size() &&
                asked.ground.data == dealt.ground.data,
            "a course asked for by id raced on other snow");
  CHECK_MSG(std::abs(asked.track.length - other.length) < 1e-9,
            "the track is not the course asked for");
  check_map(asked, "seed 38, course " + other.id);
  // By colour: the nearest colour the area has.
  const mg::PisteGrade want = other.grade;
  const mg::Level coloured = build(cs, std::nullopt, want);
  CHECK_MSG(grade_of(coloured) == want,
            cat({"asked for a ", mg::grade_name(want), " course, raced a ",
                 mg::grade_name(grade_of(coloured)), " though the area has one"}));
}

TEST(the_dealt_colour_keeps_its_odds) {
  std::array<int, 4> counts{};
  constexpr int kSeeds = 1000;
  for (int s = 1; s <= kSeeds; s++) counts[static_cast<size_t>(mg::deal_grade(s))]++;
  for (size_t g = 0; g < counts.size(); g++) {
    const double share = double(counts[g]) / kSeeds;
    CHECK_MSG(
        std::abs(share - mg::rules::grade::odds[g]) < 0.05,
        cat({mg::grade_name(static_cast<mg::PisteGrade>(g)), " dealt at ", std::to_string(share),
             ", its odds ", std::to_string(mg::rules::grade::odds[g])}));
  }
  CHECK(mg::deal_grade(38) == mg::deal_grade(38));
}

TEST(the_versions_that_lay_routes_are_refused_by_name) {
  for (const int version : {9, 10}) {
    mg::GenerateOptions opts;
    opts.version = version;
    bool refused = false;
    try {
      mg::generate_level(38, opts);
    } catch (const mg::GenerateError& e) {
      refused = std::string(e.what()).find("M4c") != std::string::npos;
    }
    CHECK_MSG(refused, cat({"version ", std::to_string(version),
                            " was not refused naming the milestone that builds it"}));
  }
}
