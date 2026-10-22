// The speed events set over a built map (R32 the downhill, R33 the super-G, R36 the giant slalom:
// speed_events.hpp, speed_course.hpp, course_prep.hpp, the analysis in analysis/race_checks.hpp),
// held by the engine's own checks:
//
//   * DETERMINISM: each course set twice is the same course, to the digest; a course set over a
//     map that already carries another is set over the map under it, on the same base, and a map
//     already carrying the course asked for is that map.
//   * THE GIANT SLALOM'S TWO RUNS: on the same stretch, their gates set afresh.
//   * THE DIGEST: each course's `level_digest`, stretch and gates are the row
//     tests/golden/mountain_speed_events.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_speed_events_test`, and the PR says why.
//   * THE RULES: every course passes its own analysis, and the map under it keeps its day, its
//     woods only thinned and its kickers only levelled.
//
// On a ski area each is set over the course its rule races. A debug build (sanitizers) holds the
// single-piste rows; a release build adds a ski area's.
//
// ge:summary The speed events set over a map: determinism, two runs, a digest golden, analysis
// ge:keywords test downhill super-g giant-slalom speed-event race-course golden r32 r33 r36
// ge:skill testing mountain
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/speed_events.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_speed_events.txt";

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
  if (!level.race) throw std::runtime_error("a speed event returned a map with no course");
  return *level.race;
}

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The course's row: the event, the stretch, the gates, the jumps, the trap, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  const mg::RaceCourse& r = race_of(level);
  return cat({std::to_string(int(cs.seed)),
              " v",
              std::to_string(cs.version),
              " ",
              cs.name,
              " ",
              mg::discipline_name(r.discipline),
              " run=",
              std::to_string(r.run),
              " from=",
              f0(r.from),
              " to=",
              f0(r.to),
              " vertical=",
              f0(r.vertical),
              " gates=",
              std::to_string(level.checkpoints.size() - 2),
              " jumps=",
              std::to_string(r.jumps.size()),
              " trap=",
              r.trap ? f0(r.trap->s) : std::string("-"),
              " digest=",
              mg::level_digest(level)});
}

// The map a case's event is set over: on a ski area, rebuilt down the course its rule races.
mg::Level map_for(const Case& cs, const mg::Level& plain, const std::optional<std::string>& id) {
  if (cs.version == 1 || !id) return plain;
  mg::GenerateOptions opts;
  opts.version = cs.version;
  opts.region = cs.region;
  opts.course = *id;
  return mg::generate_level(cs.seed, opts);
}

}  // namespace

TEST(speed_events_are_deterministic_keep_their_digest_and_pass_their_analysis) {
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
    const mg::Level plain = mg::generate_level(cs.seed, opts);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    const mg::Level dh_map = map_for(cs, plain, mg::downhill_course_of(plain));
    const mg::Level turn_map = map_for(cs, plain, mg::most_vertical_course_of(plain));
    const mg::Level downhill = mg::set_downhill(dh_map);
    const mg::Level super_g = mg::set_super_g(turn_map);
    const mg::Level gs1 = mg::set_giant_slalom(turn_map, 1);
    const mg::Level gs2 = mg::set_giant_slalom(turn_map, 2);
    // Determinism, and a course set over another course's map: the same course on the same base.
    CHECK_MSG(row_of(cs, mg::set_downhill(downhill)) == row_of(cs, downhill),
              key + ": a downhill set over a downhill is another course");
    const mg::Level gs_over_sg = mg::set_giant_slalom(super_g, 1);
    CHECK_MSG(row_of(cs, gs_over_sg) == row_of(cs, gs1),
              cat({key, ": a giant slalom set over a super-G is another course:\n  ",
                   row_of(cs, gs_over_sg), "\n  ", row_of(cs, gs1)}));
    CHECK_MSG(race_of(gs_over_sg).base == race_of(super_g).base,
              key + ": a giant slalom set over a super-G stands on another base map");
    CHECK_MSG(row_of(cs, mg::set_super_g(gs2)) == row_of(cs, super_g),
              key + ": a super-G set over a giant slalom is another course");
    CHECK_MSG(mg::level_digest(*race_of(downhill).base) == mg::level_digest(dh_map),
              key + ": the downhill's base is not the map it was set over");
    // The giant slalom's two runs: one stretch, two courses.
    CHECK_MSG(race_of(gs1).from == race_of(gs2).from && race_of(gs1).to == race_of(gs2).to,
              key + ": the giant slalom's two runs stand on different stretches");
    CHECK_MSG(mg::level_digest(gs1) != mg::level_digest(gs2),
              key + ": the giant slalom's second run is the first run's course again");
    for (const mg::Level* level : {&downhill, &super_g, &gs1, &gs2}) {
      const std::string row = row_of(cs, *level);
      const size_t at = got.size();
      got.push_back(row);
      std::printf("  %s\n", row.c_str());
      if (!write && at < want.size()) {
        CHECK_MSG(row == want[at], cat({key, ": the course moved:\n  got  ", row, "\n  want ",
                                        want[at], "\nIf the move is meant, rewrite ", kGolden,
                                        " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
      }
      for (const an::Finding& f : an::analyze_level(*level).findings) {
        CHECK_MSG(f.severity != an::Severity::error,
                  cat({row, ": ", f.rule, " ", f.message,
                       "; read it in `make lab LAB=speed-events` and fix speed_events.cpp"}));
      }
      const mg::Level& under = *race_of(*level).base;
      CHECK_MSG(level->trees.size() <= under.trees.size() &&
                    level->kickers.size() <= under.kickers.size(),
                key + ": the course grew trees or kickers");
      CHECK_MSG(level->sun.hour == under.sun.hour && level->weather.kind == under.weather.kind,
                key + ": the course moved the day it was set under");
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version region, then each speed event's course (mountain_speed_events_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
