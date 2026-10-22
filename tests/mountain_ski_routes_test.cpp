// The ski routes (R42: ski_routes.hpp, route_features.hpp, the analysis in analysis/routes.cpp),
// held by the engine's own checks:
//
//   * THE WINDOW: `steepest_along` reads the steepest window of a line and leaves out the windows
//     a drop excuses.
//   * DETERMINISM: a map with routes built twice is the same map, to the digest.
//   * THE ROUTES: within R42's count and named `SR1…`; each off a chair's or gondola's top onto a
//     run, as long and as deep as R42 asks, steeper than a black and no steeper than a route; a
//     straight one (v9) carries no drop and no jump, a wild one (v10) its jumps `O1…` named by it.
//   * THE DIGEST: the map's `level_digest` and its routes are the row
//     tests/golden/mountain_ski_routes.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_ski_routes_test`, and the PR says why.
//   * THE RULES: the map passes its own analysis.
//
// A debug build (sanitizers) holds the first row; a release build holds them all.
//
// ge:summary The ski routes: the window, determinism, the routes to rule, a golden, analysis
// ge:keywords test ski-route orange-route wild-route route-jump steepest-along golden r42
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
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/ski_routes.hpp"

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_ski_routes.txt";

struct Case {
  double seed;
  int version;
};
constexpr Case kCases[] = {{5, 10}, {38, 10}, {38, 9}, {5, 9}};

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

std::string f0(double v) { return std::to_string(std::lround(v)); }

// The map's row: its attempt and course, each route (from, onto, length, fall, steepest %, drops,
// jumps), the digest.
std::string row_of(const Case& cs, const mg::Level& level) {
  if (!level.resort) return "no ski area";
  const mg::ResortMap& area = *level.resort;
  std::string routes;
  for (const mg::SkiRoute& r : area.routes) {
    size_t jumps = 0;
    for (const mg::Kicker& k : level.kickers) jumps += k.route == r.id ? 1 : 0;
    routes += cat({" ", r.id, "=", r.from, ">", r.into.run, ":", f0(r.length), "m/",
                   f0(r.points.front().y - r.points.back().y), "m/", f0(r.steepest * 100), "%/d",
                   std::to_string(r.drops.size()), "/j", std::to_string(jumps)});
  }
  return cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version),
              " attempt=", std::to_string(level.attempt), " course=", area.course,
              routes.empty() ? " none" : routes, " digest=", mg::level_digest(level)});
}

mg::TrackPoint at(double s, double y) { return {0, s, y, s, 0, 14}; }

}  // namespace

TEST(steepest_along_reads_the_steepest_window_and_leaves_out_a_drops) {
  // A line falling 0.5 m a metre for 100 m, then 10 m sheer over 4 m, then 0.5 again.
  std::vector<mg::TrackPoint> line;
  double y = 500;
  for (int i = 0; i <= 60; i++) {
    const double s = 4.0 * i;
    line.push_back(at(s, y));
    y -= (s >= 100 && s < 104) ? 10 : 2;
  }
  const double plain = mg::steepest_along(line, 20);
  CHECK_MSG(plain > 0.85, "the sheer step reads " + std::to_string(plain) + " over 20 m");
  const mg::SkiRoute::Drop drop{100, 10};
  const double excused = mg::steepest_along(line, 20, std::span(&drop, 1));
  CHECK_MSG(std::fabs(excused - 0.5) < 1e-9,
            "with the drop excused the line reads " + std::to_string(excused) + ", not 0.5");
  CHECK_MSG(mg::steepest_along(line, 1000) == 0, "a window longer than the line reads nothing");
}

TEST(ski_areas_carry_routes_built_to_rule_keep_their_digest_and_pass_their_analysis) {
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
  namespace K = mg::resort_rules::route;
  std::vector<std::string> got;
  for (size_t c = 0; c < kHeld; c++) {
    const Case& cs = kCases[c];
    mg::GenerateOptions opts;
    opts.version = cs.version;
    const mg::Level map = mg::generate_level(cs.seed, opts);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version)});
    CHECK_MSG(map.resort.has_value(), key + ": no ski area");
    if (!map.resort) continue;
    const std::string row = row_of(cs, map);
    CHECK_MSG(row_of(cs, mg::generate_level(cs.seed, opts)) == row,
              key + ": the map built twice differs");
    const auto& routes = map.resort->routes;
    CHECK_MSG(!routes.empty() && double(routes.size()) <= K::count.max,
              key + ": " + std::to_string(routes.size()) + " routes");
    for (size_t i = 0; i < routes.size(); i++) {
      const mg::SkiRoute& r = routes[i];
      CHECK_MSG(r.id == "SR" + std::to_string(i + 1), key + ": " + r.id + " is out of order");
      CHECK_MSG(r.wild == (cs.version >= 10), key + ": " + r.id + " is drawn in the wrong style");
      CHECK_MSG(r.length >= K::length.min && r.length <= K::length.max,
                key + ": " + r.id + " is " + f0(r.length) + " m long");
      CHECK_MSG(r.points.front().y - r.points.back().y >= K::vertical,
                key + ": " + r.id + " falls too little");
      CHECK_MSG(r.steepest > mg::rules::track::max_grade, key + ": " + r.id +
                                                              " is no steeper "
                                                              "than a black");
      for (size_t p = 1; p < r.points.size(); p++) {
        CHECK_MSG(std::fabs(r.points[p].s - r.points[p - 1].s - K::station) < 1e-6,
                  key + ": " + r.id + "'s stations are not every " + f0(K::station) + " m");
      }
      size_t jumps = 0;
      for (const mg::Kicker& k : map.kickers) {
        if (k.route != r.id) continue;
        jumps++;
        CHECK_MSG(!k.on_track && k.id.front() == 'O', key + ": " + k.id + " is no route jump");
      }
      if (!r.wild) {
        CHECK_MSG(jumps == 0 && r.drops.empty(), key + ": " + r.id + " is straight but wild");
      }
    }
    got.push_back(row);
    std::printf("  %s\n", row.c_str());
    const size_t at_row = got.size() - 1;
    if (!write && at_row < want.size()) {
      CHECK_MSG(row == want[at_row], cat({key, ": the routes moved:\n  got  ", row, "\n  want ",
                                          want[at_row], "\nIf the move is meant, rewrite ", kGolden,
                                          " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
    for (const an::Finding& f : an::analyze_level(map).findings) {
      CHECK_MSG(f.severity != an::Severity::error,
                cat({row, ": ", f.rule, " ", f.message,
                     "; read it in `make lab LAB=routes` and fix ski_routes.cpp"}));
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version, then its ski area's routes (mountain_ski_routes_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
