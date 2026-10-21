// A speed-skiing track set over a built map (R34: speed_ski.hpp, the analysis in
// analysis/race_checks.hpp), held by the engine's own checks:
//
//   * DETERMINISM: the track set twice is the same track, to the digest; a run set over the
//     other run's map is the same run as one set over the map, on the same base.
//   * THE TWO RUNS: one track; the qualification from a start lowered below the final's, some
//     12 km/h slower through the zone.
//   * THE DIGEST: each run's `level_digest`, the track's arcs and the speeds are the row
//     tests/golden/mountain_speed_ski.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_speed_ski_test`, and the PR says why.
//   * THE RULES: both runs pass their own analysis, and the map under the track keeps its day,
//     its woods only thinned and its kickers only fewer.
//
// A debug build (sanitizers) holds the single-piste rows; a release build adds a ski area's.
//
// ge:summary A speed-skiing track set over a map: determinism, two runs, a golden, its analysis
// ge:keywords test speed-skiing speed-track timing-zone qualification final golden r34
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
#include "mountain/mapgen/speed_ski.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_speed_ski.txt";

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

// The track a set map names; a map with none fails the test.
const mg::RaceCourse& race_of(const mg::Level& level) {
  if (!level.race || !level.race->zone) {
    throw std::runtime_error("set_speed_ski returned a map with no speed track");
  }
  return *level.race;
}

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The run's row: the arcs down the track, the zone speed, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  const mg::RaceCourse& r = race_of(level);
  const mg::RaceCourse::Zone zone = r.zone.value_or(mg::RaceCourse::Zone{0, 0, 0});
  return cat({std::to_string(int(cs.seed)),
              " v",
              std::to_string(cs.version),
              " ",
              cs.name,
              " run=",
              std::to_string(r.run),
              " length=",
              f0(level.track.length),
              " from=",
              f0(r.from),
              " zone=",
              f0(zone.from),
              "-",
              f0(zone.to),
              " stop=",
              f0(r.stop),
              " kmh=",
              f0(mg::speed_ski_aim(level)),
              " digest=",
              mg::level_digest(level)});
}

}  // namespace

TEST(speed_ski_tracks_are_deterministic_keep_their_digest_and_pass_their_analysis) {
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
    const mg::Level map = mg::generate_level(cs.seed, opts);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    const mg::Level final_run = mg::set_speed_ski(map, 2);
    const mg::Level qualify = mg::set_speed_ski(map, 1);
    // Determinism, and a run set over the other run's map: the same run on the same base.
    const mg::Level again = mg::set_speed_ski(final_run, 1);
    CHECK_MSG(row_of(cs, again) == row_of(cs, qualify),
              cat({key, ": the qualification set over the final's map is another run:\n  ",
                   row_of(cs, again), "\n  ", row_of(cs, qualify)}));
    CHECK_MSG(race_of(again).base == race_of(final_run).base,
              key + ": the qualification set over the final's map stands on another base");
    CHECK_MSG(mg::level_digest(*race_of(final_run).base) == mg::level_digest(map),
              key + ": the track's base is not the map it was set over");
    // The two runs: one track, the qualification lower and slower.
    CHECK_MSG(race_of(qualify).from > race_of(final_run).from,
              key + ": the qualification does not start below the final");
    const double drop = mg::speed_ski_aim(final_run) - mg::speed_ski_aim(qualify);
    CHECK_MSG(drop > 10 && drop < 15, cat({key, ": the qualification is ", std::to_string(drop),
                                           " km/h under the final, not some 12"}));
    for (const mg::Level* level : {&qualify, &final_run}) {
      const std::string row = row_of(cs, *level);
      const size_t at = got.size();
      got.push_back(row);
      std::printf("  %s\n", row.c_str());
      if (!write && at < want.size()) {
        CHECK_MSG(row == want[at], cat({key, ": the track moved:\n  got  ", row, "\n  want ",
                                        want[at], "\nIf the move is meant, rewrite ", kGolden,
                                        " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
      }
      for (const an::Finding& f : an::analyze_level(*level).findings) {
        CHECK_MSG(f.severity != an::Severity::error,
                  cat({row, ": ", f.rule, " ", f.message,
                       "; read it in `make lab LAB=speed-ski` and fix speed_ski.cpp"}));
      }
      CHECK_MSG(
          level->trees.size() <= map.trees.size() && level->kickers.size() <= map.kickers.size(),
          key + ": the track grew trees or kickers");
      CHECK_MSG(level->sun.hour == map.sun.hour && level->weather.kind == map.weather.kind,
                key + ": the track moved the day it was set under");
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version region, then each run of its speed track (mountain_speed_ski_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
