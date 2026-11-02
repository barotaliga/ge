// The real faces' hints (R25: real_hints.hpp, packed.hpp, the generated real_hints/), held by
// the engine's own checks:
//
//   * THE PACKING: whole numbers written as varints and read back, to 64 bits either side of
//     nought; base64 both ways; a torn stream refused.
//   * THE CODEC: hints on the grain encoded and decoded back to the same lifts, pistes and woods;
//     torn data and data running on past its end refused.
//   * EVERY FACE: hints baked for every crop, in the crops' order; each decodes, every lift and
//     piste bend on the map, every lift's bottom lower on the face than its top and every piste
//     falling from its first bend to its last; the woods' tree line within the face's relief.
//   * THE GOLDEN: each face's counts and a hash of its data, in
//     tests/golden/mountain_real_hints.txt. The hints baked again (the map data moved, the bake
//     changed) rewrite the file: `GE_GOLDEN_WRITE=1 build/<preset>/tests/mountain_real_hints_test`,
//     and the PR says why.
//
// ge:summary The real faces' hints: the packing, the codec, every face decoded, a golden
// ge:keywords test real-hints packed varint base64 decode-hints encode-hints face-woods golden
// ge:skill testing mountain
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/mapgen/packed.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/real_hints.hpp"

namespace mg = ge::mountain::mapgen;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_real_hints.txt";

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

std::string f0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}

// FNV-1a over the data's characters.
std::string hash_of(const char* s) {
  uint64_t x = 1469598103934665603ull;
  for (; *s; s++) {
    x ^= uint8_t(*s);
    x *= 1099511628211ull;
  }
  char b[20];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(x));
  return b;
}

bool on_map(const mg::PlanPoint& p) {
  return p.x >= 0 && p.x <= mg::kFaceSize && p.z >= 0 && p.z <= mg::kFaceSize;
}

}  // namespace

TEST(whole_numbers_are_packed_and_read_back) {
  const std::vector<int64_t> values{0,
                                    1,
                                    -1,
                                    63,
                                    -64,
                                    64,
                                    127,
                                    -128,
                                    300,
                                    -70000,
                                    1ll << 40,
                                    -(1ll << 40),
                                    std::numeric_limits<int64_t>::max(),
                                    std::numeric_limits<int64_t>::min()};
  mg::packed::Writer out;
  for (const int64_t v : values) out.put(v);
  // Nought, one and minus one are a byte each.
  CHECK(out.bytes[0] == 0 && out.bytes[1] == 2 && out.bytes[2] == 1);
  const std::string text = mg::packed::to_base64(out.bytes);
  const auto bytes = mg::packed::from_base64(text);
  CHECK(bytes.has_value() && *bytes == out.bytes);
  if (!bytes) return;
  mg::packed::Reader in{*bytes};
  for (const int64_t v : values) CHECK_MSG(in.get() == v, "a value came back changed");
  CHECK(in.ok && in.done());
  in.get();
  CHECK_MSG(!in.ok, "reading past the end is not refused");
  CHECK(!mg::packed::from_base64("ab$d"));
  CHECK(!mg::packed::from_base64("ab=c"));
}

TEST(hints_are_encoded_and_decoded_back) {
  mg::RealHints h;
  h.lifts = {{mg::HintLiftKind::gondola, {100, 3800}, {1200, 900}},
             {mg::HintLiftKind::drag, {2002, 3500}, {2400, 3100}}};
  h.pistes = {{mg::HintGrade::red, {{1200, 904}, {1180, 1500}, {900, 3700}}, {40, 0, 22}},
              {mg::HintGrade::route, {{3000, 100}, {3100, 400}}, {0, 0}}};
  h.woods = mg::HintWoods{100.0 / 255, {0, 1, 51.0 / 255, 0, 0, 0, 0, 1}};
  const std::string data = mg::encode_hints(h);
  const auto back = mg::decode_hints(data);
  CHECK(back.has_value());
  if (!back) return;
  CHECK(back->lifts.size() == 2 && back->pistes.size() == 2 && back->woods.has_value());
  CHECK(back->lifts[0].kind == mg::HintLiftKind::gondola && back->lifts[1].bottom.x == 2002);
  CHECK(back->pistes[0].grade == mg::HintGrade::red && back->pistes[0].points.size() == 3);
  CHECK(back->pistes[0].points[2].x == 900 && back->pistes[0].widths[0] == 40);
  CHECK(back->pistes[1].grade == mg::HintGrade::route);
  CHECK(back->woods && back->woods->line == h.woods->line && back->woods->bands == h.woods->bands);
  CHECK_MSG(mg::encode_hints(*back) == data, "the hints decoded do not encode the same");
  CHECK(!mg::decode_hints(data.substr(0, data.size() / 2)));
  CHECK(!mg::decode_hints(data + "AAAA"));
  CHECK(!mg::decode_hints("not base64!"));
  mg::RealHints bare;
  const auto none = mg::decode_hints(mg::encode_hints(bare));
  CHECK(none && none->lifts.empty() && none->pistes.empty() && !none->woods && none->lakes.empty());
}

TEST(a_face_s_lakes_ride_after_its_woods) {
  mg::RealHints h;
  h.woods = mg::HintWoods{0.5, {}};
  const std::string dry = mg::encode_hints(h);
  h.lakes = {{{{100, 200}, {300, 210}, {280, 400}, {120, 380}}},
             {{{2000, 2000}, {2050, 2004}, {2010, 2080}}}};
  const std::string wet = mg::encode_hints(h);
  // Hints with no lakes are the bytes they were before lakes were kept; with them, more follow.
  const auto dry_bytes = mg::packed::from_base64(dry);
  const auto wet_bytes = mg::packed::from_base64(wet);
  CHECK(dry_bytes && wet_bytes && wet_bytes->size() > dry_bytes->size());
  if (dry_bytes && wet_bytes) {
    CHECK(std::equal(dry_bytes->begin(), dry_bytes->end(), wet_bytes->begin()));
  }
  const auto back = mg::decode_hints(wet);
  CHECK(back && back->lakes.size() == 2 && back->woods);
  if (!back || back->lakes.size() != 2) return;
  CHECK(back->lakes[0].shore.size() == 4 && back->lakes[1].shore.size() == 3);
  CHECK(back->lakes[0].shore[2].x == 280 && back->lakes[0].shore[2].z == 400);
  CHECK(back->lakes[1].shore[1].x == 2050 && back->lakes[1].shore[1].z == 2004);
  CHECK(mg::encode_hints(*back) == wet);
  CHECK(!mg::decode_hints(wet.substr(0, wet.size() - 8)));
}

TEST(a_face_s_woods_are_read_by_its_height) {
  mg::FaceWoods w{1000, 1800, 1500, {0.8, 0.8, 0.6, 0.4, 0.2, 0, 0, 0}};
  // A band's middle reads the band; the ends hold.
  CHECK(std::fabs(mg::face_cover_at(w, 1050) - 0.8) < 1e-12);
  CHECK(std::fabs(mg::face_cover_at(w, 1250) - 0.6) < 1e-12);
  CHECK(std::fabs(mg::face_cover_at(w, 1300) - 0.5) < 1e-12);
  CHECK(mg::face_cover_at(w, 500) == 0.8 && mg::face_cover_at(w, 2500) == 0);
}

TEST(every_face_has_hints_that_stand_on_its_map) {
  const auto baked = mg::baked_hints();
  CHECK_MSG(baked.size() == mg::kFaceCrops.size(),
            std::to_string(baked.size()) + " faces' hints baked of " +
                std::to_string(mg::kFaceCrops.size()) +
                "; bake the rest with the real-hints lab (--bake --fetch --write)");
  for (size_t i = 0; i < baked.size() && i < mg::kFaceCrops.size(); i++) {
    CHECK(std::string(baked[i]->id) == mg::kFaceCrops[i].id);
  }
  CHECK(!mg::real_hints("nowhere-1"));
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::string> got;
  int wooded = 0;
  int lakes = 0;
  for (const mg::HintData* data : baked) {
    const std::string id = data->id;
    const auto h = mg::real_hints(id);
    const auto face = mg::real_face(id);
    CHECK_MSG(h.has_value() && face.has_value(), id + ": its hints or its heights do not decode");
    if (!h || !face) continue;
    for (const mg::HintLift& l : h->lifts) {
      CHECK_MSG(on_map(l.bottom) && on_map(l.top), id + ": a lift stands off the map");
      CHECK_MSG(mg::face_height(*face, l.bottom.x, l.bottom.z) <=
                    mg::face_height(*face, l.top.x, l.top.z) + 1,
                id + ": a lift's bottom stands over its top");
    }
    for (const mg::HintPiste& p : h->pistes) {
      CHECK_MSG(p.points.size() >= 2 && p.points.size() == p.widths.size(),
                id + ": a piste has no line");
      if (p.points.size() < 2) continue;
      bool on = true;
      for (const mg::PlanPoint& q : p.points) on = on && on_map(q);
      CHECK_MSG(on, id + ": a piste's bend stands off the map");
      CHECK_MSG(mg::face_height(*face, p.points.front().x, p.points.front().z) + 1 >=
                    mg::face_height(*face, p.points.back().x, p.points.back().z),
                id + ": a piste climbs from its first bend to its last");
    }
    for (const mg::HintLake& l : h->lakes) {
      bool on = l.shore.size() >= 3;
      for (const mg::PlanPoint& q : l.shore) on = on && on_map(q);
      CHECK_MSG(on, id + ": a lake's shore stands off the map");
    }
    lakes += int(h->lakes.size());
    const auto woods = mg::face_woods(*face, *h);
    if (woods) {
      wooded++;
      CHECK_MSG(woods->line >= woods->lo && woods->line <= woods->hi,
                id + ": its tree line stands outside its relief");
    }
    got.push_back(id + " lifts=" + std::to_string(h->lifts.size()) + " pistes=" +
                  std::to_string(h->pistes.size()) + " lakes=" + std::to_string(h->lakes.size()) +
                  " line=" + (woods ? f0(woods->line) : std::string("-")) +
                  " hash=" + hash_of(data->data));
    const size_t at = got.size() - 1;
    if (!write && at < want.size()) {
      CHECK_MSG(got[at] == want[at], id + ": the hints moved:\n  got  " + got[at] + "\n  want " +
                                         want[at] + "\nIf the move is meant, rewrite " + kGolden +
                                         " with GE_GOLDEN_WRITE=1 and say why in the PR.");
    }
  }
  // Most real ski areas stand in woods or by them.
  CHECK_MSG(wooded * 2 > int(baked.size()), std::to_string(wooded) + " faces have woods");
  // Most mountains hold a lake somewhere on their face.
  CHECK_MSG(lakes > int(baked.size()), std::to_string(lakes) + " lakes on the faces");
  if (!write) CHECK(got.size() == want.size());
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# face, then its hints' lift, piste and lake counts, tree line (m) and the data's hash "
           "(mountain_real_hints_test)\n";
    for (const auto& r : got) out << r << "\n";
    std::printf("  wrote %zu rows to %s\n", got.size(), kGolden);
  }
}
