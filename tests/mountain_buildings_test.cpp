// plugins/mountain/village/buildings.hpp: on the reference seeds the village stands its base's
// buildings and a town; every footprint keeps off the streets, the open places, the runs and the
// lifts and off every other building, on ground that falls no more than a plinth takes; the front
// row turns its back on the main street; the trees on a footprint are felled; and each seed's
// buildings are golden.
//
// ge:summary Tests: the village's buildings on their lots, their fit, the trees felled, golden
// ge:keywords test village buildings place-buildings lots fit felled church lodge golden
// ge:skill mountain testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/math.hpp"
#include "golden.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/village/buildings.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/streets.hpp"

namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_buildings.txt";
constexpr int kVersion = 10;
constexpr double kSeeds[] = {38, 7, 101};

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

struct Village {
  std::unique_ptr<mg::Level> level;
  vg::Streets plan;
  vg::GroundMask mask;
  std::vector<vg::Building> buildings;
};

const Village& village_of(double seed) {
  static std::map<double, Village> built;
  auto it = built.find(seed);
  if (it == built.end()) {
    Village v;
    mg::GenerateOptions opts;
    opts.version = kVersion;
    v.level = std::make_unique<mg::Level>(mg::generate_level(seed, opts));
    const std::optional<vg::Streets> plan = vg::plan_streets(*v.level);
    CHECK_MSG(plan.has_value(), "seed " + std::to_string(seed) + " laid no village");
    if (plan) {
      v.plan = *plan;
      v.mask = vg::bake_ground(v.plan);
      v.buildings = vg::place_buildings(*v.level, v.plan, v.mask, vg::clearance_of(*v.level));
    }
    it = built.emplace(seed, std::move(v)).first;
  }
  return it->second;
}

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

int count_of(const std::vector<vg::Building>& bs, vg::BuildingKind k) {
  int n = 0;
  for (const vg::Building& b : bs) n += b.kind == k ? 1 : 0;
  return n;
}

}  // namespace

TEST(each_seed_stands_its_base_and_a_town) {
  for (const double seed : kSeeds) {
    const Village& v = village_of(seed);
    std::printf("seed %g: %zu buildings\n", seed, v.buildings.size());
    CHECK_MSG(v.buildings.size() >= 12, "seed " + std::to_string(seed) + " stood " +
                                            std::to_string(v.buildings.size()) + " buildings");
    CHECK(v.buildings.size() <= static_cast<size_t>(vg::buildings::kMost));
    CHECK(count_of(v.buildings, vg::BuildingKind::lodge) == 1);
    CHECK(count_of(v.buildings, vg::BuildingKind::church) <= 1);
    CHECK(count_of(v.buildings, vg::BuildingKind::hotel) <= vg::buildings::kHotels);
    CHECK(count_of(v.buildings, vg::BuildingKind::shop) <= vg::buildings::kShops);
    CHECK(count_of(v.buildings, vg::BuildingKind::house) > 0);
  }
}

TEST(every_footprint_keeps_off_the_streets_and_every_other) {
  for (const double seed : kSeeds) {
    const Village& v = village_of(seed);
    const vg::Clear clear = vg::clearance_of(*v.level);
    for (const vg::Building& b : v.buildings) {
      const double fx = std::sin(b.heading);
      const double fz = std::cos(b.heading);
      for (int i = -3; i <= 3; i++) {
        for (int j = -3; j <= 3; j++) {
          const double u = b.width * i / 6;
          const double w = b.depth * j / 6;
          const double x = b.x + u * fz + w * fx;
          const double z = b.z - u * fx + w * fz;
          CHECK(!vg::on_street(v.mask, x, z));
          CHECK(clear(x, z, vg::streets::kClearHub));
          CHECK(vg::in_building(b, x, z, 1e-6));
          // Heights between the metre the placer reads are the snow's own bilinear sag.
          CHECK(v.level->ground_at(x, z) <= b.y + 0.05);
          CHECK(v.level->ground_at(x, z) >= b.low - 0.05);
          for (const vg::Building& o : v.buildings) {
            if (o.id != b.id) CHECK(!vg::in_building(o, x, z));
          }
        }
      }
      CHECK(b.y - b.low <= vg::buildings::kFall + 1e-9);
    }
  }
}

TEST(the_front_row_faces_the_hub_and_the_town_faces_its_street) {
  const Village& v = village_of(38);
  for (const vg::Building& b : v.buildings) {
    // The street it fronts: its nearest point lies behind the front row, before the rest.
    const vg::Street& st = v.plan.streets[static_cast<size_t>(b.street)];
    double best = 1e300;
    double sx = 0;
    double sz = 0;
    for (const vg::StreetPoint& p : st.points) {
      const double d = std::hypot(p.x - b.x, p.z - b.z);
      if (d < best) best = d, sx = p.x, sz = p.z;
    }
    const double toward = (sx - b.x) * std::sin(b.heading) + (sz - b.z) * std::cos(b.heading);
    const bool front_row =
        b.kind == vg::BuildingKind::lodge || b.kind == vg::BuildingKind::ticket ||
        b.kind == vg::BuildingKind::rental || b.kind == vg::BuildingKind::school ||
        b.kind == vg::BuildingKind::first_aid;
    if (front_row) {
      CHECK(toward < 0);
    } else if (b.kind != vg::BuildingKind::hotel) {
      CHECK(toward > 0);
    }
  }
}

TEST(the_trees_on_a_footprint_are_felled) {
  const Village& v = village_of(38);
  std::vector<uint8_t> felled = vg::felled_trees(*v.level, v.mask);
  int before = 0;
  for (const uint8_t f : felled) before += f;
  vg::fell_for_buildings(*v.level, v.buildings, felled);
  int after = 0;
  for (const uint8_t f : felled) after += f;
  CHECK(after >= before);
  for (size_t i = 0; i < v.level->trees.size(); i++) {
    for (const vg::Building& b : v.buildings) {
      if (vg::in_building(b, v.level->trees[i].x, v.level->trees[i].z, 1)) CHECK(felled[i] == 1);
    }
  }
}

TEST(every_seed_keeps_its_golden) {
  std::vector<std::string> got;
  for (const double seed : kSeeds) {
    const Village& v = village_of(seed);
    std::string kinds;
    for (int k = 0; k < vg::kBuildingKinds; k++) {
      kinds += " " + std::to_string(count_of(v.buildings, static_cast<vg::BuildingKind>(k)));
    }
    got.push_back(std::to_string(static_cast<long long>(seed)) + " " +
                  hex(vg::buildings_hash(v.buildings, 0xcbf29ce484222325ull)) + kinds);
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each seed's village buildings at generator v10: the seed, their hash and how many "
           "of each kind (mountain_buildings_test)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the buildings moved:\n  got  " + got[r] + "\n  want " + line +
                                  "\nIf the move is meant, rewrite " + kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the village lab's pictures "
                                  "before and after in the PR (skill: mountain).");
  }
}
