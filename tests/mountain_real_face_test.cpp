// The real faces (R25: real_face_crops.hpp, real_face.hpp, the generated real_faces/), held by
// the engine's own checks:
//
//   * THE CROPS: every face keyed by its region and a number, once, named by its place, its
//     middle on the globe; every crop baked.
//   * THE GRID: encoded and decoded back to the same steps; a torn grid refused.
//   * THE GLOBE: a map point to the globe and back to itself.
//   * EVERY FACE: decoded to a mountain over its own floor (its lowest sample within a step of
//     its floor, its ridge band standing over its valley band), sampled bicubically through its
//     own samples.
//   * THE GOLDEN: each face's floor, top, mean and a hash of its decoded heights, in
//     tests/golden/mountain_real_face.txt. A face baked again (a crop moved, the model updated)
//     rewrites the file: `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_real_face_test`, and the
//     PR says why.
//   * LAID 1:1 (massif.hpp): a face's massif stands the real heights over its floor down the
//     upper face, its vertical the real peak's, no dealt side ridges, bowls or headwalls.
//   * A SKI AREA ON A FACE: fell-1 builds a clean map on seed 1, in its region, and the map's
//     digest is held in tests/golden/mountain_real_face_map.txt (rewritten the same way when a
//     generator change moves it on purpose). The real-maps lab sweeps every face.
//
// ge:summary The real faces: the crops, the grid's codec, the globe, every face decoded, a golden
// ge:keywords test real-face face-crop face-grid decode encode globe-at map-at golden r25
// ge:keywords face-lift real-map generate-face
// ge:skill testing mountain
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "golden.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/massif.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace mg = ge::mountain::mapgen;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_real_face.txt";
constexpr const char* kMapGolden = "tests/golden/mountain_real_face_map.txt";

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

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// FNV-1a over the heights' bits.
std::string hash_of(const std::vector<float>& h) {
  uint64_t x = 1469598103934665603ull;
  for (const float v : h) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    for (int k = 0; k < 4; k++) {
      x ^= (bits >> (8 * k)) & 0xff;
      x *= 1099511628211ull;
    }
  }
  char b[20];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(x));
  return b;
}

}  // namespace

TEST(every_face_is_keyed_by_its_region_and_placed) {
  std::set<std::string> ids;
  for (const mg::FaceCrop& c : mg::kFaceCrops) {
    const std::string id = c.id;
    CHECK_MSG(ids.insert(id).second, id + " is listed twice");
    const std::string region = mg::region_name(c.region);
    CHECK_MSG(id.rfind(region + "-", 0) == 0,
              std::string(id).append(" is not keyed by its region ").append(region));
    CHECK_MSG(std::string(c.range).size() >= 2 && std::string(c.area).size() > 0 &&
                  std::string(c.part).size() > 0,
              id + " has no place");
    CHECK(std::fabs(c.lat) < 75 && std::fabs(c.lon) <= 180);
    CHECK(std::fabs(c.east) <= 1800 && std::fabs(c.north) <= 1800);
    CHECK(c.bearing >= -180 && c.bearing < 360);
    CHECK_MSG(mg::face_crop(id) == &c, id + " is not found by its id");
  }
  CHECK(mg::face_crop("nowhere-1") == nullptr);
  CHECK(!mg::real_face("nowhere-1"));
  // Every crop baked, in the crops' order.
  const auto baked = mg::baked_faces();
  CHECK_MSG(baked.size() == mg::kFaceCrops.size(),
            std::to_string(baked.size()) + " faces baked of " +
                std::to_string(mg::kFaceCrops.size()) +
                "; bake the rest with the real-faces lab (--bake --fetch --write)");
  for (size_t i = 0; i < baked.size() && i < mg::kFaceCrops.size(); i++) {
    CHECK(std::string(baked[i]->id) == mg::kFaceCrops[i].id);
  }
}

TEST(the_grid_is_encoded_and_decoded_back) {
  std::vector<int> steps(size_t(mg::kFaceN) * size_t(mg::kFaceN));
  for (size_t i = 0; i < steps.size(); i++) {
    const size_t r = i / size_t(mg::kFaceN);
    const size_t c = i % size_t(mg::kFaceN);
    steps[i] = int((r * 37 + c * c * 3) % 4000) + int(i % 7);
  }
  const std::string data = mg::encode_face_grid(steps);
  const auto back = mg::decode_face_grid(data, 1200);
  CHECK(back.has_value());
  const std::vector<float> heights = back.value_or(std::vector<float>{});
  bool same = heights.size() == steps.size();
  for (size_t i = 0; same && i < steps.size(); i++) {
    same = heights[i] == static_cast<float>(1200 + steps[i] * mg::kFaceStep);
  }
  CHECK_MSG(same, "the grid decoded differs from the steps encoded");
  CHECK(!mg::decode_face_grid(data.substr(0, data.size() / 2), 0));
  CHECK(!mg::decode_face_grid(data + "AAAA", 0));
  CHECK(!mg::decode_face_grid("not base64!", 0));
}

TEST(a_map_point_goes_to_the_globe_and_back) {
  for (const mg::FaceCrop& c : mg::kFaceCrops) {
    for (const double x : {0.0, 1234.5, 4000.0}) {
      for (const double z : {0.0, 2000.0, 3900.0}) {
        const auto [lat, lon] = mg::globe_at(c, x, z);
        const auto [bx, bz] = mg::map_at(c, lat, lon);
        CHECK_MSG(std::fabs(bx - x) < 1e-6 && std::fabs(bz - z) < 1e-6,
                  std::string(c.id) + ": a map point does not come back from the globe");
      }
    }
    // The window's middle stands `east` and `north` off the face's middle.
    const auto [lat, lon] = mg::globe_at(c, mg::kFaceSize / 2, mg::kFaceSize / 2);
    CHECK(std::fabs((lat - c.lat) * 111320 - c.north) < 1e-6);
  }
}

TEST(every_face_decodes_to_a_mountain_over_its_floor) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::string> got;
  constexpr int n = mg::kFaceN;
  for (const mg::FaceData* data : mg::baked_faces()) {
    const std::string id = data->id;
    const auto decoded = mg::real_face(id);
    CHECK_MSG(decoded.has_value(), id + " does not decode");
    if (!decoded) continue;
    const mg::RealFace* face = &*decoded;
    double lo = 1e9;
    double hi = -1e9;
    double sum = 0;
    for (const float h : face->heights) {
      lo = std::min(lo, double(h));
      hi = std::max(hi, double(h));
      sum += h;
    }
    CHECK_MSG(lo >= data->floor && lo <= data->floor + 1, id + ": its lowest is off its floor");
    // A mountain: the ridge band stands well over the valley band across the playable middle.
    const double ridge = mg::face_mean(*face, 700, 3300, 250, 420);
    const double valley = mg::face_mean(*face, 700, 3300, 3450, 4000);
    CHECK_MSG(ridge - valley > 150,
              id + ": its ridge stands " + f1(ridge - valley) + " m over its valley floor");
    // The bicubic sample passes through every sample.
    for (const int r : {0, 7, 60, n - 1}) {
      for (const int c : {0, 33, n - 1}) {
        const double at = mg::face_height(*face, c * mg::kFaceCell, r * mg::kFaceCell);
        CHECK(std::fabs(at - face->heights[size_t(r) * size_t(n) + size_t(c)]) < 1e-4);
      }
    }
    const std::string row =
        cat({id, " floor=", f1(lo), " top=", f1(hi),
             " mean=", f1(sum / double(face->heights.size())), " hash=", hash_of(face->heights)});
    got.push_back(row);
    const size_t at_row = got.size() - 1;
    if (!write && at_row < want.size()) {
      CHECK_MSG(row == want[at_row], cat({id, ": the face moved:\n  got  ", row, "\n  want ",
                                          want[at_row], "\nIf the move is meant, rewrite ", kGolden,
                                          " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
    }
  }
  if (!write) CHECK(got.size() == want.size());
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# face, then its decoded heights' floor, top, mean and hash "
           "(mountain_real_face_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}

TEST(a_real_face_is_laid_one_to_one) {
  for (const char* id : {"fell-1", "alpine-1"}) {
    std::optional<mg::RealFace> decoded = mg::real_face(id);
    CHECK(decoded.has_value());
    if (!decoded) continue;
    const auto face = std::make_shared<const mg::RealFace>(std::move(*decoded));
    const mg::Region& region = mg::region_row(face->crop->region);
    ge::core::Rng rng = ge::core::make_rng(7);
    const mg::TerrainPlan plan = mg::plan_massif(rng, region, face, 0);
    CHECK_MSG(plan.face != nullptr, std::string(id) + ": the plan carries no face");
    if (!plan.face) continue;
    CHECK(plan.altitude == plan.face->floor);
    CHECK(plan.flank == 0 && plan.crests == 0 && plan.bowls.empty() && plan.headwalls.empty());
    CHECK_MSG(mg::within_band(plan.vertical, mg::resort_rules::massif::real::vertical),
              std::string(id) + ": a vertical of " + f1(plan.vertical) + " m");
    // Down the upper face the ground is the real height over the floor, the share of R3's folds
    // laid over it aside.
    const ge::core::Heightfield ground = mg::bake_massif(plan);
    double off = 0;
    int count = 0;
    for (int r = 0; mg::descent_at(plan, plan.summit_z + r * 64) < 0.8; r++) {
      const double z = plan.summit_z + r * 64;
      for (int c = 0; c <= 50; c++) {
        const double x = 400 + c * 64;
        const double real = mg::face_height(*face, x, z) - plan.face->floor;
        off += std::fabs(ge::core::sample_field(ground, x, z) - real);
        count++;
      }
    }
    const double mean = off / count;
    CHECK_MSG(mean < 12, std::string(id) + ": the ground stands " + f1(mean) +
                             " m off the real face on average down its upper face");
  }
}

TEST(a_real_face_builds_a_clean_ski_area) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  mg::GenerateOptions opts;
  opts.face = "fell-1";
  const mg::Level level = mg::generate_level(1, opts);
  CHECK(level.face == std::optional<std::string>("fell-1"));
  CHECK(level.region == mg::RegionId::fell);
  CHECK(level.resort.has_value());
  const std::string row = cat({"fell-1 seed=1 attempt=", std::to_string(level.attempt),
                               " digest=", mg::level_digest(level)});
  if (write) {
    std::ofstream out(ge::test::root_path(kMapGolden));
    out << "# a real face's map: face, seed, the attempt accepted and level_digest "
           "(mountain_real_face_test)\n"
        << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kMapGolden);
  std::string line;
  if (!want.empty()) {
    for (const auto& w : want[0]) line += (line.empty() ? "" : " ") + w;
  }
  CHECK_MSG(row == line, cat({"the fell-1 map moved:\n  got  ", row, "\n  want ", line,
                              "\nIf the move is meant, rewrite ", kMapGolden,
                              " with GE_GOLDEN_WRITE=1 and say why in the PR."}));
  // A face no crop names is refused with the place to look.
  bool refused = false;
  try {
    mg::GenerateOptions bad;
    bad.face = "nowhere-1";
    (void)mg::generate_level(1, bad);
  } catch (const mg::GenerateError&) {
    refused = true;
  }
  CHECK(refused);
}
