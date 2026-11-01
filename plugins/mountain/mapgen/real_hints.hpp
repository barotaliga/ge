// R25 — A REAL FACE'S HINTS: where the real ski area on a real face has its lifts and its
// pistes, coarsely, how wooded each height of the face is, and where its lakes lie.
//
// Baked offline off the open map data by the `real-hints` lab into one generated source a face
// (real_hints/, listed in real_hints/index.cpp) on the same crop as the face's heights, so a hint
// stands on the map where the real lift or piste stands on the face. Nothing is named: a lift is
// its kind and its two ends (the bottom the lower on the face), a piste its grade and its bends
// down its line (top first) with its width at each, the woods the face's tree line and its cover
// in `kWoodsBands` bands of its height, a lake its shore (the lakes are not hints to the
// generator, which builds no water: a game lays them after it, mountain/tarn/mapped.hpp).
//
// They are HINTS, not a plan: the generator leans its stations, its runs and its woods toward
// them where its own rules allow, and builds what they cannot fit as it would on any massif.
//
// THE FORMAT (packed.hpp's varints, then base64): the lifts' count, then each lift's kind and its
// bottom's and top's x and z in `kHintGrain` m steps; the pistes' count, then each piste's grade
// (`HintGrade`), its bends' count and each bend's step in x and z from the one before (the first
// from the map's corner) in `kHintGrain` m and its width in `kHintWidthStep` m (0 where the map
// gives none); then 1 and the woods' nine bytes (the tree line's share of the face's relief, then
// every band's cover, bottom first, 0..255 for 0..1), or 0 for a face whose map draws no forest;
// then, only where the map has water on the face, the lakes' count and each lake's shore: its
// points' count and each point's step in x and z from the one before (the first from the map's
// corner) in `kHintGrain` m. A face with no lakes ends at its woods, so the hints baked before the
// lakes were read back the same.
//
// The map data: © OpenStreetMap contributors, under the Open Database Licence (ODbL 1.0); the
// baked hints are a derived database under the same licence.
//
// ge:summary R25 a real face's hints: the real lifts, pistes and woods by height, decoded
// ge:keywords real-hints hint-lift hint-piste woods-by-height tree-line openstreetmap r25 hint-lake
// ge:skill mountain
// ge:test tests/mountain_real_hints_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The grain a lift's ends and a piste's bends are kept in, m, and a piste's width.
inline constexpr double kHintGrain = 2;
inline constexpr double kHintWidthStep = 2;
// The bands of a face's height its woods are kept in.
inline constexpr int kWoodsBands = 8;

enum class HintLiftKind : uint8_t { chair, gondola, drag };

// A real piste's grade as its map signs it: R23's four colours, or a ski route (off-piste).
enum class HintGrade : uint8_t { green, blue, red, black, route };

struct HintLift {
  HintLiftKind kind;
  PlanPoint bottom;
  PlanPoint top;
};

struct HintPiste {
  HintGrade grade;
  std::vector<PlanPoint> points;  // its bends, top first
  std::vector<double> widths;     // its width at each, m; 0 where the map gives none
};

// A face's woods as kept: the tree line's share of the face's relief, and each band's share
// wooded, bottom first, every one 0..1.
struct HintWoods {
  double line;
  std::array<double, kWoodsBands> bands;
};

// A real lake (or reservoir, or pond) on the face: its shore once round, unclosed, on the map.
struct HintLake {
  std::vector<PlanPoint> shore;
};

struct RealHints {
  std::vector<HintLift> lifts;
  std::vector<HintPiste> pistes;
  std::optional<HintWoods> woods;  // none where the face's map draws no forest
  std::vector<HintLake> lakes;
};

// One face's hints as baked (real_hints/): its id and its packed data (base64).
struct HintData {
  const char* id;
  const char* data;
};

// Every face's hints baked, in the crops' order (real_hints/index.cpp, generated).
std::span<const HintData* const> baked_hints();

// The face `id`'s hints decoded, or nothing for an id no hints are baked for.
std::optional<RealHints> real_hints(std::string_view id);

// The packed data decoded, or nothing when it is torn or runs on past its end. `encode_hints` is
// its inverse over hints already on the grain.
std::optional<RealHints> decode_hints(std::string_view data);
std::string encode_hints(const RealHints& hints);

// A face's WOODS BY HEIGHT in its own real metres: its lowest and highest sample, the tree line,
// and the bands' cover splitting that span evenly, bottom first.
struct FaceWoods {
  double lo;
  double hi;
  double line;
  std::array<double, kWoodsBands> bands;
};

// The hints' woods laid on the face's heights, or nothing where its map draws no forest.
std::optional<FaceWoods> face_woods(const RealFace& face, const RealHints& hints);

// The share of the face wooded at real height `h`, m: its bands read smoothly between their
// middles, the end bands held.
double face_cover_at(const FaceWoods& woods, double h);

}  // namespace ge::mountain::mapgen
