// A READ MAP AS THE SERVER'S WORLD: the 29 and BSP2 fixture maps turned into brush models
// (bsp_world) and traced by the server as any world is: a player dropped onto the hall's floor
// stands 24 above it, walks through the east doorway, stops against a wall where the doorway is
// not, and a large box as wide as the doorway is stopped there; hull 0 answers points; 38 and 46
// are refused, naming the family; both families trace to the same bits, held golden
// (tests/golden/shooter_bsp_world.txt); a mutated map that reads is traced to an end.
//
// A deliberate change to the fixture's hulls or the trace moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/shooter_bsp_world_test` and say why in the PR.
//
// ge:summary Test: fixture maps as the server's world: drops, doorways, walls, golden traces
// ge:keywords test shooter bsp_world trace trace_move hull0 hull1 hull2 doorway golden fixture
// ge:keywords mutate refuse
// ge:skill shooter brush testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/prng.hpp"
#include "golden.hpp"
#include "shooter/bsp_world.hpp"
#include "shooter/server.hpp"
#include "shooter/trace.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;

constexpr const char* kGolden = "tests/golden/shooter_bsp_world.txt";
const sh::Vec3f kZero{0, 0, 0};

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

bool near(float a, float b, float tol = 0.05f) { return std::fabs(a - b) <= tol; }

std::vector<uint8_t> written(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), f, &bytes, &error));
  return bytes;
}

void no_call(void*, sh::Server&, sh::Func) {}

// A server whose world is the fixture map of `family`.
struct MapServer {
  br::Map map;
  sh::BspWorld world;
  sh::Server sv;

  explicit MapServer(br::Family family) {
    std::string error;
    CHECK(br::read_bsp(written(family), &map, &error));
    const bool ok = sh::bsp_world(map, &world, &error);
    if (!ok) std::printf("  %s\n", error.c_str());
    CHECK(ok);
    sh::Program program;
    program.call = &no_call;
    sh::create_server(sv, {}, world.models, program);
  }

  sh::HullTrace trace(sh::Vec3f a, sh::Vec3f mins, sh::Vec3f maxs, sh::Vec3f b) const {
    return sh::trace_move(sv, a, mins, maxs, b, sh::kTraceNormal, -1);
  }
};

const sh::Vec3f kPlayerMins = sh::kPlayerMins, kPlayerMaxs = sh::kPlayerMaxs;
const sh::Vec3f kLargeMins = sh::kLargeMins, kLargeMaxs = sh::kLargeMaxs;

struct Ray {
  sh::Vec3f a, mins, maxs, b;
};

// The golden's rays: drops, walks along and across the doorways, into corners, up to the ceiling.
std::vector<Ray> rays() {
  std::vector<Ray> r;
  const sh::Vec3f boxes[3][2] = {
      {kZero, kZero}, {kPlayerMins, kPlayerMaxs}, {kLargeMins, kLargeMaxs}};
  const sh::Vec3f pairs[][2] = {
      {{256, 192, 200}, {256, 192, -100}}, {{400, 192, 40}, {700, 192, 40}},
      {{400, 300, 40}, {700, 300, 40}},    {{224, 300, 40}, {224, 600, 40}},
      {{100, 100, 100}, {-50, -50, 100}},  {{256, 192, 100}, {256, 192, 400}},
      {{700, 300, 40}, {300, 600, 60}},    {{240, 600, 30}, {600, 700, 30}},
      {{20, 20, 30}, {500, 380, 250}},     {{336, 672, 30}, {336, 672, -30}},
  };
  for (const auto& box : boxes) {
    for (const auto& p : pairs) r.push_back({p[0], box[0], box[1], p[1]});
  }
  return r;
}

uint32_t bits(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, 4);
  return u;
}

std::vector<uint64_t> row_of(const sh::HullTrace& t) {
  return {uint64_t(t.allsolid) | uint64_t(t.startsolid) << 1 | uint64_t(t.inopen) << 2 |
              uint64_t(t.inwater) << 3,
          bits(t.fraction),
          bits(t.endpos.x),
          bits(t.endpos.y),
          bits(t.endpos.z),
          bits(t.plane.normal.x),
          bits(t.plane.normal.y),
          bits(t.plane.normal.z),
          bits(t.plane.dist),
          uint64_t(int64_t(t.ent) + 1)};
}

}  // namespace

TEST(a_player_dropped_in_the_hall_stands_on_its_floor) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    const MapServer s(f);
    CHECK(s.world.models.size() == 1);
    const sh::HullTrace t = s.trace({256, 192, 200}, kPlayerMins, kPlayerMaxs, {256, 192, -100});
    CHECK(t.ent == 0);
    CHECK(near(t.endpos.z, 24));
    CHECK(t.plane.normal.z == 1);
    const sh::HullTrace p = s.trace({256, 192, 200}, kZero, kZero, {256, 192, -100});
    CHECK(near(p.endpos.z, 0));
  }
}

TEST(a_player_walks_through_the_doorway_and_stops_at_the_wall) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    const MapServer s(f);
    const sh::HullTrace door = s.trace({400, 192, 40}, kPlayerMins, kPlayerMaxs, {700, 192, 40});
    CHECK(door.fraction == 1 && door.ent == -1);
    const sh::HullTrace wall = s.trace({400, 300, 40}, kPlayerMins, kPlayerMaxs, {700, 300, 40});
    CHECK(wall.ent == 0);
    CHECK(near(wall.endpos.x, 496));
    CHECK(wall.plane.normal.x == -1);
    // The large box is as wide as the doorway: it stops where its side meets the wall.
    const sh::HullTrace large = s.trace({400, 192, 40}, kLargeMins, kLargeMaxs, {700, 192, 40});
    CHECK(near(large.endpos.x, 480));
    // A point through the north doorway into the corridor.
    const sh::HullTrace north = s.trace({224, 300, 40}, kZero, kZero, {224, 600, 40});
    CHECK(north.fraction == 1);
    CHECK(sh::world_contents(s.sv, {224, 600, 40}) == sh::kContentsEmpty);
    CHECK(sh::world_contents(s.sv, {224, 600, -40}) == sh::kContentsSolid);
  }
}

TEST(a_map_without_clip_hulls_is_refused_naming_its_family) {
  for (br::Family f : {br::Family::v38, br::Family::v46}) {
    br::Map map;
    std::string error;
    CHECK(br::read_bsp(written(f), &map, &error));
    sh::BspWorld w;
    CHECK(!sh::bsp_world(map, &w, &error));
    CHECK(error.find(br::family_name(f)) != std::string::npos);
    CHECK(error.find("next:") != std::string::npos);
  }
}

TEST(both_families_trace_to_the_same_bits_held_golden) {
  const MapServer a(br::Family::v29), b(br::Family::bsp2);
  std::string table = "# flags fraction end.xyz normal.xyz dist ent+1 (shooter_bsp_world_test)\n";
  std::vector<std::vector<uint64_t>> rows;
  for (const Ray& r : rays()) {
    const std::vector<uint64_t> x = row_of(a.trace(r.a, r.mins, r.maxs, r.b));
    CHECK(x == row_of(b.trace(r.a, r.mins, r.maxs, r.b)));
    for (uint64_t v : x) {
      char w[24];
      std::snprintf(w, sizeof w, "%llx ", static_cast<unsigned long long>(v));
      table += w;
    }
    table.back() = '\n';
    rows.push_back(x);
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  const auto golden = ge::test::golden_rows(kGolden);
  CHECK(golden.size() == rows.size());
  for (size_t r = 0; r < golden.size() && r < rows.size(); ++r) CHECK(golden[r] == rows[r]);
}

TEST(a_mutated_map_that_reads_is_traced_to_an_end) {
  const std::vector<uint8_t> base = written(br::Family::v29);
  ge::core::Rng rng = ge::core::make_rng(18);
  const std::vector<Ray> rs = rays();
  int traced = 0;
  for (int n = 0; n < 400; ++n) {
    std::vector<uint8_t> b = base;
    for (int k = 0; k < 4; ++k) {
      const size_t at = size_t(ge::core::next(rng) * double(b.size())) % b.size();
      b[at] = uint8_t(ge::core::next(rng) * 256.0);
    }
    br::Map map;
    std::string error;
    if (!br::read_bsp(b, &map, &error)) continue;
    sh::BspWorld w;
    if (!sh::bsp_world(map, &w, &error)) continue;
    for (const Ray& r : rs) {
      for (int h = 0; h < 3; ++h) sh::trace_hull(w.models[0].hulls[h], r.a, r.b);
    }
    ++traced;
  }
  CHECK(traced > 0);
}
