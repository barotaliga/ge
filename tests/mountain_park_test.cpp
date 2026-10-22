// The terrain park on a map built for a tricks run (R20: park.hpp, the analysis in
// analysis/park.cpp), held by the engine's own checks:
//
//   * DETERMINISM: the tricks map built twice is the same map, to the digest.
//   * THE PARK: within R20's count, every kicker built to its size, the sizes laid in turn, the
//     kickers in the order skied and named `T1…`, no gate on any of them.
//   * THE DIGEST: the map's `level_digest` and its park are the row
//     tests/golden/mountain_park.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_park_test`, and the PR says why.
//   * THE RULES: the tricks map passes its own analysis.
//
// A debug build (sanitizers) holds the single-piste rows; a release build adds a ski area's.
//
// ge:summary The terrain park on a tricks map: determinism, its kickers, a golden, analysis
// ge:keywords test terrain-park park trick-kicker built-landing golden r20
// ge:skill testing mountain
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/rules.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_park.txt";

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

// The park's kickers in the order skied.
std::vector<const mg::Kicker*> park_of(const mg::Level& level) {
  std::vector<const mg::Kicker*> out;
  for (const mg::Kicker& k : level.kickers) {
    if (k.trick) out.push_back(&k);
  }
  return out;
}

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The map's row: its attempt, its line's length, the park's sizes in order and its first and last
// lips, the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  const std::vector<const mg::Kicker*> park = park_of(level);
  std::string sizes;
  for (const mg::Kicker* k : park)
    sizes.push_back(mg::trick_size_name(k->trick.value_or(mg::TrickSize::low))[0]);
  const std::string first = park.empty() ? "-" : f0(park.front()->s.value_or(0));
  const std::string last = park.empty() ? "-" : f0(park.back()->s.value_or(0));
  const std::string course = level.resort ? level.resort->course : std::string("-");
  return cat({std::to_string(int(cs.seed)),
              " v",
              std::to_string(cs.version),
              " ",
              cs.name,
              " attempt=",
              std::to_string(level.attempt),
              " course=",
              course,
              " length=",
              f0(level.track.length),
              " park=",
              sizes,
              " lips=",
              first,
              "-",
              last,
              " gates=",
              std::to_string(level.checkpoints.size()),
              " digest=",
              mg::level_digest(level)});
}

}  // namespace

TEST(tricks_maps_carry_a_park_built_to_rule_keep_their_digest_and_pass_their_analysis) {
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
  namespace T = mg::rules::trick;
  std::vector<std::string> got;
  for (size_t c = 0; c < kHeld; c++) {
    const Case& cs = kCases[c];
    mg::GenerateOptions opts;
    opts.version = cs.version;
    opts.region = cs.region;
    opts.tricks = true;
    const mg::Level map = mg::generate_level(cs.seed, opts);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    const std::string row = row_of(cs, map);
    CHECK_MSG(row_of(cs, mg::generate_level(cs.seed, opts)) == row,
              key + ": the tricks map built twice differs");
    const std::vector<const mg::Kicker*> park = park_of(map);
    CHECK_MSG(double(park.size()) >= T::count.min && double(park.size()) <= T::count.max,
              key + ": the park carries " + std::to_string(park.size()) + " kickers");
    for (size_t i = 0; i < park.size(); i++) {
      const mg::Kicker& k = *park[i];
      CHECK_MSG(k.id == "T" + std::to_string(i + 1), key + ": " + k.id + " is out of order");
      CHECK_MSG(k.on_track && k.shape, key + ": " + k.id + " is not a built park kicker");
      if (i > 0) {
        CHECK_MSG(k.s.value_or(0) > park[i - 1]->s.value_or(0),
                  key + ": " + k.id + " stands before " + park[i - 1]->id);
      }
      const double from = k.s.value_or(0) - k.ramp;
      const double to = k.s.value_or(0) + k.landing;
      for (const mg::Checkpoint& g : map.checkpoints) {
        CHECK_MSG(g.s < from || g.s > to, key + ": a gate stands on " + k.id);
      }
    }
    // The ladder: a low kicker first, and every size laid.
    bool sized[3] = {false, false, false};
    for (const mg::Kicker* k : park) {
      sized[static_cast<int>(k->trick.value_or(mg::TrickSize::low))] = true;
    }
    CHECK_MSG(!park.empty() && park.front()->trick == mg::TrickSize::low,
              key + ": the park does not open on a low kicker");
    CHECK_MSG(sized[0] && sized[1] && sized[2], key + ": the park lays not every size");
    got.push_back(row);
    std::printf("  %s\n", row.c_str());
    const size_t at = got.size() - 1;
    if (!write && at < want.size()) {
      CHECK_MSG(row == want[at], cat({key, ": the park moved:\n  got  ", row, "\n  want ", want[at],
                                      "\nIf the move is meant, rewrite ", kGolden,
                                      " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
    for (const an::Finding& f : an::analyze_level(map).findings) {
      CHECK_MSG(f.severity != an::Severity::error,
                cat({row, ": ", f.rule, " ", f.message,
                     "; read it in `make lab LAB=park` and fix park.cpp"}));
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version region, then its tricks map and its park (mountain_park_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
