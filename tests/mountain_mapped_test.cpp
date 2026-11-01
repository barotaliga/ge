// plugins/mountain/tarn/mapped.hpp: a real face's mapped lakes are laid on its map. Each is its
// face's shore as the hints keep it, set into the ground under it; frozen to bear a skier, the
// shore is filled flat at the surface; open, it is a bowl no deeper than `depth_of`; either way
// the bank stands over the water out to its lip and grades to the ground round it, and no tree
// stands in the water. A dealt map has none.
//
// ge:summary Test: a real face's mapped lakes laid on its map: the shore, the basin, the banks
// ge:keywords test mapped-lake lay_mapped_lakes mapped_lakes lay_mapped depth_of shore on_lake
// ge:keywords banks lip bank-slope
// ge:skill mountain testing
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/tarn/mapped.hpp"
#include "mountain/tarn/tarn.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

const mg::Level& alpine1() {
  static const std::unique_ptr<mg::Level> built = [] {
    mg::GenerateOptions opts;
    opts.face = "alpine-1";
    return std::make_unique<mg::Level>(mg::generate_level(38, opts));
  }();
  return *built;
}

// The plan distance from (x, z) to the shore's nearest edge, m.
double to_shore(const std::vector<mg::PlanPoint>& s, double x, double z) {
  double best = 1e30;
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
    const double len2 = ex * ex + ez * ez;
    const double t = std::clamp(((x - s[j].x) * ex + (z - s[j].z) * ez) / len2, 0.0, 1.0);
    best = std::min(best, std::hypot(x - s[j].x - t * ex, z - s[j].z - t * ez));
  }
  return best;
}

// Every node of the ground's grid within `reach` m of the lake's shore's box: its place, its
// height, whether it is inside the shore and how far from it.
struct Node {
  double x, z, y, d;
  bool in;
};
std::vector<Node> nodes(const mg::Level& level, const mg::MapLake& lake, double reach) {
  double x0 = 1e30, x1 = -1e30, z0 = 1e30, z1 = -1e30;
  for (const mg::PlanPoint& p : lake.shore) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  const ge::core::Heightfield& g = level.ground;
  std::vector<Node> out;
  for (int row = 0; row < g.rows; ++row) {
    const double z = g.origin_z + row * g.cell;
    if (z < z0 - reach || z > z1 + reach) continue;
    for (int col = 0; col < g.cols; ++col) {
      const double x = g.origin_x + col * g.cell;
      if (x < x0 - reach || x > x1 + reach) continue;
      const float y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      out.push_back({x, z, y, to_shore(lake.shore, x, z), tn::inside_shore(lake.shore, x, z)});
    }
  }
  return out;
}

// The map with only its first lake laid, on day `day` of the year.
mg::Level with_first_lake(double day) {
  mg::Level level = alpine1();
  level.sun.day_of_year = day;
  std::vector<mg::MapLake> lakes = tn::mapped_lakes(level);
  if (!lakes.empty()) tn::lay_mapped(level, lakes[0]);
  return level;
}

// Every bank node: just over the water within the lip, then graded no steeper than the slope.
void check_banks(const mg::Level& level, const mg::MapLake& lake, const std::string& what) {
  const double rimmed = lake.level + 0.3;
  int lip = 0, graded = 0;
  for (const Node& n : nodes(level, lake, tn::kLip + tn::kBankReach)) {
    if (n.in || n.d > tn::kLip + tn::kBankReach) continue;
    const double off = std::max(0.0, n.d - tn::kLip);
    const bool ok = std::fabs(n.y - rimmed) <= tn::kBankSlope * off + 1e-3;
    if (n.d < tn::kLip)
      lip += ok ? 0 : 1;
    else
      graded += ok ? 0 : 1;
  }
  CHECK_MSG(lip == 0,
            what + ": " + std::to_string(lip) + " nodes of the lip off the water's height");
  CHECK_MSG(graded == 0,
            what + ": " + std::to_string(graded) + " bank nodes steeper than the slope");
}

}  // namespace

TEST(a_real_face_s_lakes_are_its_mapped_shores) {
  const mg::Level& level = alpine1();
  const std::vector<mg::MapLake> lakes = tn::mapped_lakes(level);
  const auto hints = mg::real_hints("alpine-1");
  CHECK(hints.has_value());
  CHECK_MSG(!lakes.empty(), "alpine-1's map holds no mapped lake: bake its lakes (real-hints)");
  if (!hints || lakes.empty()) return;
  CHECK(lakes.size() <= hints->lakes.size());
  for (const mg::MapLake& l : lakes) {
    CHECK(l.shore.size() >= 3 && l.radius > 0 && l.ice == 0);
    // Its shore is its face's: the hints' points, and what the water is drawn and swum in.
    const auto same = std::find_if(hints->lakes.begin(), hints->lakes.end(), [&](const auto& h) {
      return h.shore.size() == l.shore.size() && h.shore[0].x == l.shore[0].x &&
             h.shore[0].z == l.shore[0].z;
    });
    CHECK(same != hints->lakes.end());
    CHECK(tn::shore_of(l).size() == l.shore.size());
    // A ray from the middle meets the shore where the shore is.
    const double reach = tn::reach_of(l, 0.3);
    CHECK(reach > 0 && reach < 4 * l.radius);
  }
  // Every lake keeps the same surface built twice.
  const std::vector<mg::MapLake> again = tn::mapped_lakes(level);
  CHECK(again.size() == lakes.size() && again[0].level == lakes[0].level);
  // A dealt map has no face and so no mapped lake.
  mg::GenerateOptions dealt;
  dealt.version = 10;
  CHECK(tn::mapped_lakes(mg::generate_level(38, dealt)).empty());
}

TEST(ice_that_bears_a_skier_fills_the_shore_flat) {
  const mg::Level level = with_first_lake(40);  // midwinter
  CHECK(level.lakes.size() == 1);
  if (level.lakes.empty()) return;
  const mg::MapLake& lake = level.lakes[0];
  CHECK_MSG(tn::bears(lake), "midwinter's ice on alpine-1's lake does not bear a skier");
  CHECK(lake.depth == 0 && lake.thaw == 0);
  int off = 0, inside = 0;
  for (const Node& n : nodes(level, lake, 0)) {
    if (!n.in) continue;
    ++inside;
    if (std::fabs(n.y - lake.level) > 1e-3) ++off;
  }
  CHECK(inside > 100);
  CHECK_MSG(off == 0, std::to_string(off) + " nodes inside the frozen shore off its surface");
  check_banks(level, lake, "frozen");
  for (const mg::TreeDef& t : level.trees) CHECK(!tn::on_lake(lake, t.x, t.z));
}

TEST(open_water_lies_in_a_bowl_banked_to_its_shore) {
  const mg::Level level = with_first_lake(200);  // midsummer
  CHECK(level.lakes.size() == 1);
  if (level.lakes.empty()) return;
  const mg::MapLake& lake = level.lakes[0];
  CHECK(lake.ice == 0 && lake.thaw == 0);
  // The bowl is dug `depth_of` under the surface; where the ground under the shore lay lower
  // still it is left as it was.
  double lowest = 1e30;
  for (const Node& n : nodes(alpine1(), lake, 0)) {
    if (n.in) lowest = std::min(lowest, n.y);
  }
  CHECK_MSG(
      lake.depth > 0.5 && lake.depth <= std::max(tn::depth_of(lake), lake.level - lowest) + 1e-3,
      "its deepest water " + std::to_string(lake.depth) + " m");
  int dry = 0;
  for (const Node& n : nodes(level, lake, 0)) {
    if (n.in && n.y > lake.level + 0.3 + 1e-3) ++dry;
  }
  CHECK_MSG(dry == 0, std::to_string(dry) + " nodes inside the shore stand over the water");
  check_banks(level, lake, "open");
  for (const mg::TreeDef& t : level.trees) CHECK(!tn::on_lake(lake, t.x, t.z));
  // The surface is set low in the ground the generator left inside the shore.
  const mg::MapLake before = tn::mapped_lakes(alpine1())[0];
  CHECK(before.level == lake.level);
}

TEST(a_lake_s_depth_grows_with_its_size_between_the_bounds) {
  mg::MapLake pond, tarn, sea;
  pond.radius = 5;
  tarn.radius = 60;
  sea.radius = 2000;
  CHECK(tn::depth_of(pond) == tn::kMappedShallow);
  CHECK(std::fabs(tn::depth_of(tarn) - 6) < 1e-12);
  CHECK(tn::depth_of(sea) == tn::kMappedDeep);
}
