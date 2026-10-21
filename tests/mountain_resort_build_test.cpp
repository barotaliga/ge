// The ski-area build (resort_build.hpp), stage by stage, held by the engine's own checks:
//
//   * DETERMINISM: the same attempt built twice gives the same area, to the bit, and the same
//     attempt stream after it.
//   * THE DIGEST: each attempt's area hashes to the row tests/golden/mountain_resort_build.txt
//     keeps, so a change that moves what a seed builds shows here. A deliberate move rewrites
//     the file: `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_resort_build_test`, and the PR
//     says why.
//   * THE RULES, checked as each stage is reached: every run laid is a line down the map (its
//     arc grows, it has a width, it stays on the map), at least `runs_min` pistes stand (R27),
//     once the lanes and drag lifts are laid every piste kept can be skied again (R29), and once
//     graded and pressed every run falls, is packed along its line but in its drifts, and has
//     no kicker over a junction (R9, R17, R27).
//
// A debug build (sanitizers) holds the first rows; a release build holds them all.
//
// ge:summary The ski-area build held by its own checks: determinism, a digest golden, the rules
// ge:keywords test resort ski-area determinism digest golden walk access lanes drag-lift graded
// ge:skill testing mountain
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/mapgen/access_build.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/links.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace mg = ge::mountain::mapgen;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_resort_build.txt";
constexpr std::string_view kTwiceHint =
    "\nSomething reads state outside the attempt (a static, an address, an unordered "
    "container); the first section that differs names the stage.";

// The areas held: a seed in each region, its first attempts.
struct Case {
  double seed;
  mg::RegionId region;
  const char* name;
};
constexpr Case kCases[] = {
    {1, mg::RegionId::maritime, "maritime"}, {2, mg::RegionId::alpine, "alpine"},
    {3, mg::RegionId::fell, "fell"},         {4, mg::RegionId::continental, "continental"},
    {38, mg::RegionId::alpine, "alpine"},    {7, mg::RegionId::maritime, "maritime"},
};
constexpr int kAttempts = 2;

// A byte writer hashed as it goes (FNV-1a 32).
struct Digest {
  uint32_t h = 0x811c9dc5u;
  void byte(uint8_t b) { h = (h ^ b) * 0x01000193u; }
  void f64(double v) {
    uint64_t u;
    std::memcpy(&u, &v, 8);
    for (int i = 0; i < 8; i++) byte(uint8_t(u >> (8 * i)));
  }
  void opt(const std::optional<double>& v) {
    f64(v ? *v : std::numeric_limits<double>::quiet_NaN());
  }
  void str(const std::string& s) {
    for (const char c : s) byte(uint8_t(c));
    byte(0);
  }
  std::string hex() const {
    char out[9];
    std::snprintf(out, sizeof out, "%08x", h);
    return out;
  }
};

// What an attempt built, by section: the ground, the lifts, the pads, every run, the stream.
std::vector<std::pair<std::string, std::string>> sections(const mg::ResortAttempt& a,
                                                          const std::string& stage) {
  std::vector<std::pair<std::string, std::string>> out;
  out.emplace_back("stage", stage);
  Digest d;
  const auto* b = reinterpret_cast<const uint8_t*>(a.ground.data.data());
  for (size_t i = 0; i < a.ground.data.size() * sizeof(float); i++) d.byte(b[i]);
  out.emplace_back("ground", d.hex());
  d = {};
  for (const auto& l : a.lift_plans) {
    d.str(l.id);
    d.str(mg::lift_kind_name(l.kind));
    for (const double v : {l.bottom.x, l.bottom.z, l.top.x, l.top.z}) d.f64(v);
  }
  out.emplace_back("lifts", d.hex());
  d = {};
  for (const auto& p : a.pads)
    for (const double v : {p.x, p.z, p.y, p.dx, p.dz, p.length, p.r, p.deck}) d.f64(v);
  out.emplace_back("pads", d.hex());
  out.emplace_back("runs", std::to_string(a.built.empty() ? a.walked.size() : a.built.size()));
  d = {};
  for (const mg::WalkedRun& r : a.walked) {
    d.str(r.spec.id);
    d.str(r.spec.from);
    d.str(mg::run_kind_name(r.spec.kind));
    for (const mg::TrackPoint& p : r.track.points)
      for (const double v : {p.x, p.z, p.y, p.s, p.heading, p.width}) d.f64(v);
    d.opt(r.into ? std::optional<double>(r.into->run) : std::nullopt);
    d.opt(r.into ? std::optional<double>(r.into->s) : std::nullopt);
  }
  out.emplace_back("walked", d.hex());
  d = {};
  for (const mg::RampRoom& r : a.rooms) {
    for (const double v : {r.from.x, r.from.z, r.to.x, r.to.z}) d.f64(v);
    d.opt(r.half);
  }
  out.emplace_back("rooms", d.hex());
  out.emplace_back("lost", std::to_string(a.access.lost.size()));
  d = {};
  for (const mg::BuiltRun& r : a.built) {
    d.str(r.walked.spec.id);
    d.str(mg::grade_name(r.grade));
    for (const mg::TrackPoint& p : r.walked.track.points)
      for (const double v : {p.x, p.z, p.y, p.width}) d.f64(v);
    for (const mg::Kicker& k : r.kickers)
      for (const double v : {k.x, k.z, k.y, k.height, k.ramp, k.landing}) d.f64(v);
    for (const mg::Cliff& c : r.drops)
      for (const double v : {c.x, c.z, c.y, c.drop, c.shelf}) d.f64(v);
    for (const mg::Drift& f : r.drifts) {
      d.f64(f.from);
      d.f64(f.to);
    }
  }
  out.emplace_back("built", d.hex());
  d = {};
  const auto* pk = reinterpret_cast<const uint8_t*>(a.packed.data.data());
  for (size_t i = 0; i < a.packed.data.size() * sizeof(float); i++) d.byte(pk[i]);
  out.emplace_back("packed", d.hex());
  out.emplace_back("rng", std::to_string(a.rng.a));
  return out;
}

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

// Words joined into one message, with no temporaries.
std::string cat(std::initializer_list<std::string_view> parts) {
  std::string out;
  for (const std::string_view p : parts) out.append(p);
  return out;
}

std::string row_text(const std::string& key,
                     const std::vector<std::pair<std::string, std::string>>& s) {
  std::string out = key;
  for (const auto& [k, v] : s) out.append(" ").append(k).append("=").append(v);
  return out;
}

// R27 and R29 over an area built through its lanes.
void check_links(const mg::ResortAttempt& a, const std::string& where) {
  int pistes = 0;
  for (const mg::WalkedRun& r : a.walked) {
    const std::string at = where + " run " + r.spec.id;
    const auto& pts = r.track.points;
    CHECK_MSG(pts.size() >= 2, at + ": a run of fewer than two points; read walk_run");
    for (size_t i = 0; i < pts.size(); i++) {
      const mg::TrackPoint& p = pts[i];
      const bool on_map = p.x >= 0 && p.z >= 0 && p.x <= a.plan.size && p.z <= a.plan.size;
      CHECK_MSG(on_map, at + ": point " + std::to_string(i) + " is off the map");
      CHECK_MSG(p.width > 0, at + ": point " + std::to_string(i) + " has no width");
      if (i > 0)
        CHECK_MSG(p.s > pts[i - 1].s, at + ": the arc does not grow at point " + std::to_string(i));
    }
    if (r.spec.kind == mg::RunKind::piste) pistes++;
  }
  CHECK_MSG(pistes >= mg::resort_rules::network::runs_min,
            where + ": " + std::to_string(pistes) + " pistes stand, R27 wants " +
                std::to_string(mg::resort_rules::network::runs_min));
  // R29: every piste kept skied again, by the lifts and lanes as laid.
  const std::vector<mg::PlanRun> runs = mg::plan_runs(a.walked);
  std::vector<mg::PlanLift> lifts;
  lifts.reserve(a.lift_plans.size());
  for (const mg::LiftPlan& l : a.lift_plans)
    lifts.push_back({l.id, l.bottom, l.top, l.bottom.z >= a.plan.base_z - 1});
  const mg::PlanAccess v = mg::reckon_access(runs, lifts, 0);
  for (size_t i = 0; i < runs.size(); i++) {
    if (a.access.is_lost(i)) continue;
    CHECK_MSG(v.ok[i], where + " piste " + a.walked[i].spec.id + ": R29 " + v.why[i] +
                           " after the lanes were laid; read lay_access in links.cpp");
  }
}

// R23, R27 over the runs graded and pressed: each falls from its start to its end, is packed
// along its line but in its drifts, and carries no kicker or drop over a junction down it.
void check_graded(const mg::ResortAttempt& a, const std::string& where) {
  int pistes = 0;
  std::vector<std::vector<double>> junctions(a.built.size());
  for (const mg::BuiltRun& b : a.built) {
    if (b.walked.into) junctions[size_t(b.walked.into->run)].push_back(b.walked.into->s);
  }
  for (size_t r = 0; r < a.built.size(); r++) {
    const mg::BuiltRun& b = a.built[r];
    const auto& pts = b.walked.track.points;
    const std::string at = where + " run " + b.walked.spec.id;
    if (b.walked.spec.kind == mg::RunKind::piste) pistes++;
    CHECK_MSG(
        pts.front().y > pts.back().y,
        at + ": ends higher than it starts once pressed; read stamp_run in network_stamp.cpp");
    for (const mg::TrackPoint& p : pts) {
      // A grid cell reads the line a cell's width either side of the point: clear of a drift's
      // fade by that much.
      if (mg::drift_at(b.drifts, p.s - 4) > 0 || mg::drift_at(b.drifts, p.s + 4) > 0) continue;
      const int c = int(std::lround(p.x / a.packed.cell));
      const int q = int(std::lround(p.z / a.packed.cell));
      if (c < 0 || q < 0 || c >= a.packed.cols || q >= a.packed.rows) continue;
      const float v = a.packed.data[size_t(q) * size_t(a.packed.cols) + size_t(c)];
      CHECK_MSG(v > 0.99f, cat({at, ": unpacked on its line at s=", std::to_string(p.s),
                                "; read stamp_run in network_stamp.cpp"}));
    }
    for (const mg::Kicker& k : b.kickers) {
      const double ks = k.s.value_or(0);
      for (const double s : junctions[r]) {
        CHECK_MSG(ks + k.landing < s - mg::resort_rules::network::junction ||
                      ks - k.ramp > s + mg::resort_rules::network::junction,
                  at + ": kicker " + k.id + " over a junction; read the keep-off in resort_graded");
      }
    }
  }
  CHECK_MSG(pistes >= mg::resort_rules::network::runs_min,
            where + ": " + std::to_string(pistes) + " pistes graded, R27 wants " +
                std::to_string(mg::resort_rules::network::runs_min));
}

// Builds one attempt as far as it stands, checking each stage's rules as it is reached when
// `check`; the last stage it reached and its sections.
std::vector<std::pair<std::string, std::string>> build(const Case& c, int attempt,
                                                       mg::ResortAttempt& a, bool check) {
  a = mg::begin_resort(c.seed, attempt, mg::sub_seed(c.seed, attempt), mg::region_row(c.region), 8);
  const std::string key =
      std::to_string(int(c.seed)) + " " + c.name + " " + std::to_string(attempt);
  std::string stage = "mountain";
  if (!mg::resort_mountain(a) && !mg::resort_pads(a)) {
    stage = "pads";
    if (!mg::resort_pistes(a) && !mg::resort_links(a)) {
      stage = "links";
      if (check) check_links(a, key);
      if (!mg::resort_graded(a)) {
        stage = "graded";
        if (check) check_graded(a, key);
      }
    }
  }
  return sections(a, stage);
}

}  // namespace

TEST(resort_builds_are_deterministic_and_keep_their_digest) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want_rows;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want_rows.push_back(line);
  }
  if (!write) CHECK_MSG(!want_rows.empty(), std::string(kGolden) + " is empty; see the header");
#ifdef NDEBUG
  constexpr size_t kHeld = std::size(kCases);
#else
  constexpr size_t kHeld = 1;
#endif
  std::vector<std::string> got_rows;
  int through = 0;
  for (size_t c = 0; c < kHeld; c++) {
    for (int attempt = 0; attempt < kAttempts; attempt++) {
      const Case& cs = kCases[c];
      const std::string key =
          std::to_string(int(cs.seed)) + " " + cs.name + " " + std::to_string(attempt);
      const auto t0 = std::chrono::steady_clock::now();
      mg::ResortAttempt a, b;
      const auto first = build(cs, attempt, a, true);
      const auto again = build(cs, attempt, b, false);
      const std::string row = row_text(key, first);
      const std::string twice = row_text(key, again);
      CHECK_MSG(row == twice, cat({key, ": the same attempt built twice differs:\n  ", row, "\n  ",
                                   twice, kTwiceHint}));
      got_rows.push_back(row);
      const size_t i = got_rows.size() - 1;
      if (!write && i < want_rows.size()) {
        CHECK_MSG(row == want_rows[i],
                  cat({key, ": the area moved:\n  got  ", row, "\n  want ", want_rows[i],
                       "\nIf the move is meant, rewrite ", kGolden,
                       " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
      }
      if (first[0].second == "graded") through++;
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      std::printf("  %s: through %s, %s runs, built twice in %.0f ms\n", key.c_str(),
                  first[0].second.c_str(), first[4].second.c_str(), ms);
    }
  }
  CHECK_MSG(through > 0, "no attempt held was graded; the later stages' rules went unchecked");
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed region attempt, then each section of the area built "
           "(mountain_resort_build_test)\n";
    for (const auto& r : got_rows) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got_rows.size(), kGolden);
  }
}
