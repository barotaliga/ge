// The ski jumping hill (R43: ski_jump.hpp, built as R37's straight venue, straight_venue.hpp),
// held by the engine's own checks:
//
//   * THE ELEMENTS: each hill's elements against the construction norm's own arithmetic (h, n,
//     the landing speed and radius, r₁, the table, the slopes at P, the foot and L, e₁, e₂, the
//     out-run, the arcs P–K and K–L), and every hill's choices inside the norm's bands.
//   * THE PROFILE: K h below and n out from the edge; P, K and L at their slopes; the table at
//     its angle and the wall under the edge; the knoll steepening to P; the in-run at its angle
//     at the top gate, the gates no more than the norm's height apart; the level out-run at U;
//     the distance credited reading K at w and L near HS; the point mass off the gates.
//   * THE HILL OVER A MAP: the ground IS the profile down the line; the raised in-run's deck
//     answers on the line and the ground lies far under it beside; no tree on the hill; the
//     gates, the spawn and the take-off; the same hill asked twice.
//   * THE DIGEST: the hill's row in tests/golden/mountain_ski_jump.txt. A deliberate move
//     rewrites the file: `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_ski_jump_test`, and
//     the PR says why.
//
// A debug build (sanitizers) holds the single piste's rows; a release build adds a ski area's.
//
// ge:summary The ski jumping hill: the norm's elements, the profile, the hill over a map, a golden
// ge:keywords test ski-jump jumping-hill hill-of edge-speed design-gate jump-distance golden r43
// ge:skill testing mountain
#include <algorithm>
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
#include "core/heightfield.hpp"
#include "golden.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/ski_jump.hpp"
#include "mountain/mapgen/venue_rules.hpp"

namespace mg = ge::mountain::mapgen;
namespace V = ge::mountain::mapgen::venue_rules::ski_jump;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_ski_jump.txt";
constexpr double kDeg = 180 / 3.141592653589793;

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

std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

bool near(double got, double want, double tol) { return std::fabs(got - want) <= tol; }

#define CHECK_NEAR(got, want, tol, what) \
  CHECK_MSG(near((got), (want), (tol)),  \
            cat({what, ": ", f2(got), " where ", f2(want), " ±", f2(tol), " was wanted"}))

// The profile's slope at `x` m of plan, degrees down.
double slope(const mg::SkiJumpProfile& p, double x) {
  return std::atan2(mg::profile_height_at(p, x - 0.05) - mg::profile_height_at(p, x + 0.05), 0.1) *
         kDeg;
}

struct Case {
  double seed;
  int version;
  mg::JumpHill hill;
};
constexpr Case kCases[] = {
    {38, 1, mg::JumpHill::large},
    {7, 1, mg::JumpHill::normal},
    {38, 9, mg::JumpHill::large},
    {5, 9, mg::JumpHill::normal},
};

}  // namespace

TEST(the_hills_elements_are_the_norms) {
  const mg::HillElements el = mg::hill_of(V::large);
  CHECK_NEAR(el.h, 62.9, 0.05, "large h");
  CHECK_NEAR(el.n, 108.45, 0.05, "large n");
  CHECK_NEAR(el.h / el.n, 0.58, 1e-6, "large steepness");
  CHECK_NEAR(el.landing, 30.26, 0.005, "large landing speed");
  CHECK_NEAR(el.r_l, 300, 0, "large landing radius");
  CHECK_NEAR(el.r1, 103.2, 0.05, "large r1");
  CHECK_NEAR(el.t, 6.79, 0.005, "large table");
  CHECK_NEAR(el.s, 3.15, 0.005, "large table height");
  CHECK_NEAR(el.slope_p * kDeg, 37.0, 1e-6, "large slope at P");
  CHECK_NEAR(el.slope_foot * kDeg, 6.17, 0.005, "large slope at the foot");
  CHECK_NEAR(el.slope_l * kDeg, 31.25, 0.005, "large slope at L");
  CHECK_NEAR(el.e1, 98.8, 0.05, "large e1");
  CHECK_NEAR(el.e2, 80.6, 0.05, "large e2");
  CHECK_NEAR(el.a, 82.6, 0.05, "large out-run");
  CHECK_NEAR(el.l1, 16.2, 0.05, "large P-K");
  CHECK_NEAR(el.l2, 13.9, 0.05, "large K-L");
  const mg::HillElements nl = mg::hill_of(V::normal);
  CHECK_NEAR(nl.h, 45.7, 0.05, "normal h");
  CHECK_NEAR(nl.n, 83.2, 0.05, "normal n");
  CHECK_NEAR(nl.r_l, 210, 0, "normal landing radius");
  CHECK_NEAR(nl.r1, 90.0, 0.05, "normal r1");
  CHECK_NEAR(nl.e1, 84.4, 0.05, "normal e1");
  CHECK_NEAR(nl.e2, 69.3, 0.05, "normal e2");
  CHECK_NEAR(nl.slope_l * kDeg, 31.25, 0.05, "normal slope at L");
  for (const V::Hill* row : {&V::large, &V::normal}) {
    const mg::HillElements e = mg::hill_of(*row);
    CHECK_NEAR(row->w / row->hs, 0.9, 0.005, "w over HS");
    CHECK(row->table >= row->w / 30 + 6.9 - 1e-9 && row->table <= row->w / 30 + 7.9);
    CHECK(row->steepness >= row->w / 800 + 0.4 && row->steepness <= row->w / 1000 + 0.48);
    CHECK(row->in_run <= 37);
    CHECK(e.slope_p * kDeg <= 37 + 1e-9);
    CHECK_MSG(row->speed * row->speed / e.r1 < 0.7 * 9.81, "the transition loads past 0.7 g");
    CHECK(row->r2l <= e.r_l);
  }
}

TEST(the_hills_profile_is_built_to_the_norm) {
  for (const mg::JumpHill hill : {mg::JumpHill::large, mg::JumpHill::normal}) {
    const mg::SkiJumpProfile& p = mg::ski_jump_profile(hill);
    const mg::HillElements& el = p.elements;
    const V::Hill& row = mg::hill_row(hill);
    const std::string name = mg::jump_hill_name(hill);
    // K h below and n out from the edge, P and L on the landing radius.
    CHECK_NEAR(p.k - p.edge, el.n, 1e-6, name + " K out from the edge");
    CHECK_NEAR(mg::profile_height_at(p, p.edge), 0, 1e-6, name + " the edge's height");
    CHECK_NEAR(mg::profile_height_at(p, p.k), -el.h, 0.005, name + " K's height");
    CHECK_NEAR(slope(p, p.k), el.slope * kDeg, 0.05, name + " the slope at K");
    CHECK_NEAR(slope(p, p.p), el.slope_p * kDeg, 0.05, name + " the slope at P");
    CHECK_NEAR(slope(p, p.l), el.slope_l * kDeg, 0.05, name + " the slope at L");
    // The table at its angle, the wall under the edge, the knoll leaving at β₀ and steepening.
    CHECK_NEAR(slope(p, p.edge - 2), row.table, 0.005, name + " the table's angle");
    CHECK(p.edge - p.table > el.t * std::cos(el.table) - 0.1);
    CHECK(mg::profile_height_at(p, p.edge + p.dx) < -el.s + 0.1);
    CHECK_NEAR(slope(p, p.edge + 0.3), el.slope_foot * kDeg, 0.5, name + " the knoll's foot");
    double prev = 0;
    for (int i = 0; p.edge + 0.2 + 0.5 * i < p.p; i++) {
      const double a = slope(p, p.edge + 0.2 + 0.5 * i);
      CHECK_MSG(a >= prev - 1e-6, name + ": the knoll flattens before P");
      prev = a;
    }
    // The in-run at its angle at the top gate, the gates no more than the norm's height apart.
    const mg::SkiJumpHill::Gate& top = p.gates.back();
    CHECK_NEAR(slope(p, top.x), row.in_run, 0.05, name + " the in-run at the top gate");
    CHECK_NEAR(top.e, el.e1, 1e-6, name + " the top gate's e1");
    CHECK_NEAR(p.gates.front().e, el.e2, 1e-6, name + " the lowest gate's e2");
    for (size_t i = 1; i < p.gates.size(); i++) {
      CHECK(p.gates[i].y - p.gates[i - 1].y <= V::gate_height + 1e-9);
      CHECK(p.gates[i].y > p.gates[i - 1].y);
    }
    // Onto the level out-run at U, no more than 88 m below the edge; the profile never climbs.
    CHECK_NEAR(slope(p, p.u + 1), 0, 0.0005, name + " the out-run");
    CHECK(-mg::profile_height_at(p, p.u) <= 88);
    CHECK_NEAR(p.outrun - p.u, el.a, 1e-6, name + " the out-run's length");
    CHECK(p.finish > p.u && p.finish < p.outrun);
    bool climbs = false;
    for (size_t i = 1; i < p.y.size(); i++) climbs = climbs || p.y[i] > p.y[i - 1] + 1e-9;
    CHECK_MSG(!climbs, name + ": the profile climbs somewhere");
    // The distance credited reads K at w and L near HS.
    CHECK_NEAR(mg::jump_distance_at(p, p.k), el.w, 1e-6, name + " the distance at K");
    CHECK_NEAR(mg::jump_distance_at(p, p.l), el.hs, 0.6, name + " the distance at L");
    CHECK_NEAR(mg::jump_distance_at(p, p.p), el.w - el.l1, 0.5, name + " the distance at P");
    CHECK(mg::jump_distance_at(p, p.edge - 5) == 0);
    CHECK(mg::measured_distance(125.74) == 125.5);
    CHECK(mg::measured_distance(126) == 126);
    // A point mass off the top gate past the design speed, off the lowest short of it.
    const int g = mg::design_gate(p);
    CHECK(mg::edge_speed(p, int(p.gates.size()) - 1) >= el.speed);
    CHECK(mg::edge_speed(p, 0) < el.speed - 1);
    CHECK(mg::edge_speed(p, g) >= el.speed);
    if (g > 0) CHECK(mg::edge_speed(p, g - 1) < el.speed);
    CHECK(mg::edge_speed(p, g, 3) < mg::edge_speed(p, g));
  }
  const mg::SkiJumpProfile& large = mg::ski_jump_profile(mg::JumpHill::large);
  CHECK_MSG(double(mg::design_gate(large)) > double(large.gates.size()) / 2,
            "the large hill's design gate stands in the upper half of its gates");
}

TEST(the_hill_is_graded_over_a_map_and_keeps_its_digest) {
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
  constexpr size_t kHeld = 2;
#endif
  std::vector<std::string> got;
  for (size_t c = 0; c < kHeld; c++) {
    const Case& cs = kCases[c];
    mg::GenerateOptions opts;
    opts.version = cs.version;
    const mg::Level base = mg::generate_level(cs.seed, opts);
    const mg::Level level = mg::set_ski_jump(base, cs.hill);
    const std::string key = cat({std::to_string(int(cs.seed)), " v", std::to_string(cs.version),
                                 " ", mg::jump_hill_name(cs.hill)});
    CHECK_MSG(level.ski_jump.has_value() && level.venue != nullptr, key + ": no hill");
    if (!level.ski_jump || !level.venue) continue;
    const mg::SkiJumpHill& hill = *level.ski_jump;
    CHECK(hill.hill == cs.hill);
    CHECK(level.checkpoints.size() == 2);
    CHECK_NEAR(level.checkpoints.front().s, hill.gates.back().x, 1e-6, key + " the start gate");
    CHECK_NEAR(level.checkpoints.back().s, hill.to, 1e-6, key + " the finish line");
    int take_offs = 0;
    for (const mg::Kicker& k : level.kickers) take_offs += k.id == "SJ" && k.on_track ? 1 : 0;
    CHECK_MSG(take_offs == 1, key + ": " + std::to_string(take_offs) + " take-offs");
    CHECK_MSG(hill.vertical > 100, key + ": the hill falls " + f2(hill.vertical) + " m");
    // The ground IS the profile down the line, inside the hill's width.
    const mg::TrackPoint& head = level.track.points.front();
    const double fx = std::sin(head.heading);
    const double fz = std::cos(head.heading);
    const auto at = [&](double d, double off) {
      return level.ground_at(head.x + fx * d + fz * off, head.z + fz * d - fx * off);
    };
    for (const double d : {hill.from, hill.edge - 1, hill.k, hill.u, hill.to}) {
      CHECK_NEAR(at(d, 0), mg::ski_jump_height_at(level, d), 1e-4, key + " the ground on the line");
      const double off = at(d, 8);
      if (d < hill.edge) {
        // THE RAISED IN-RUN: a few metres off the line the ground lies under the structure.
        CHECK_MSG(mg::ski_jump_height_at(level, d) - off > hill.lip - 0.5,
                  key + ": the ground beside the in-run stands up to the deck");
      } else {
        CHECK_NEAR(off, mg::ski_jump_height_at(level, d), 1e-4, key + " the landing hill off it");
      }
    }
    CHECK_MSG(at(hill.from, hill.deck + 0.5) < mg::ski_jump_height_at(level, hill.from) - 10,
              key + ": beside the deck at the top gate the ground is not far under it");
    // The grid under the hill never stands above its surface.
    double over = 0;
    for (int i = 0; i * 0.5 < hill.to; i++) {
      const double d = i * 0.5;
      const double x = head.x + fx * d;
      const double z = head.z + fz * d;
      over = std::max(over, ge::core::sample_field(level.ground, x, z) - level.ground_at(x, z));
    }
    CHECK_MSG(over < 0.05, key + ": the grid stands " + f2(over) + " m over the hill");
    // The spawn stands on the top gate; no tree stands on the hill.
    CHECK_NEAR(level.spawn.x, head.x + fx * hill.from, 1e-6, key + " the spawn");
    int on_hill = 0;
    for (const mg::TreeDef& t : level.trees) {
      const double along = (t.x - head.x) * fx + (t.z - head.z) * fz;
      const double across = std::fabs((t.x - head.x) * fz - (t.z - head.z) * fx);
      if (along > 0 && along < hill.to && across <= hill.width / 2) on_hill++;
    }
    CHECK_MSG(on_hill == 0, key + ": " + std::to_string(on_hill) + " trees stand on the hill");
    // The same hill asked twice; the map under it the base.
    const mg::Level again = mg::set_ski_jump(level, cs.hill);
    CHECK(again.ski_jump && again.ski_jump->base == hill.base);
    const std::string digest = mg::level_digest(level);
    CHECK_MSG(mg::level_digest(mg::set_ski_jump(base, cs.hill)) == digest,
              key + ": the hill set twice differs");
    const std::string row =
        cat({key, " line=", f2(head.x), ",", f2(head.z), " heading=", f2(head.heading * kDeg),
             " vertical=", f2(hill.vertical), " gates=", std::to_string(hill.gates.size()),
             " design=", std::to_string(hill.design + 1), " digest=", digest});
    got.push_back(row);
    std::printf("  %s\n", row.c_str());
    const size_t at_row = got.size() - 1;
    if (!write && at_row < want.size()) {
      CHECK_MSG(row == want[at_row], cat({key, ": the hill moved:\n  got  ", row, "\n  want ",
                                          want[at_row], "\nIf the move is meant, rewrite ", kGolden,
                                          " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# seed version hill, then the hill set over its map (mountain_ski_jump_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
