// The definitions behind real_hints.hpp.
//
// ge:summary R25 a real face's hints decoded and encoded, the woods laid on its heights
// ge:keywords real-hints decode-hints encode-hints face-woods face-cover-at
// ge:skill mountain
// ge:test tests/mountain_real_hints_test.cpp
#include "mountain/mapgen/real_hints.hpp"

#include <algorithm>
#include <cmath>

#include "core/num.hpp"
#include "mountain/mapgen/packed.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;

constexpr int kLiftKinds = 3;
constexpr int kGrades = 5;

int64_t on_grain(double v, double grain) { return int64_t(num::round(v / grain)); }

}  // namespace

std::optional<RealHints> real_hints(std::string_view id) {
  for (const HintData* h : baked_hints()) {
    if (id == h->id) return decode_hints(h->data);
  }
  return std::nullopt;
}

std::optional<RealHints> decode_hints(std::string_view data) {
  const auto bytes = packed::from_base64(data);
  if (!bytes) return std::nullopt;
  packed::Reader in{*bytes};
  RealHints h;
  const int64_t lifts = in.get();
  if (!in.ok || lifts < 0 || lifts > 10000) return std::nullopt;
  for (int64_t i = 0; i < lifts; i++) {
    const int64_t kind = in.get();
    if (kind < 0 || kind >= kLiftKinds) return std::nullopt;
    HintLift l{HintLiftKind(kind), {}, {}};
    l.bottom.x = double(in.get()) * kHintGrain;
    l.bottom.z = double(in.get()) * kHintGrain;
    l.top.x = double(in.get()) * kHintGrain;
    l.top.z = double(in.get()) * kHintGrain;
    h.lifts.push_back(l);
  }
  const int64_t pistes = in.get();
  if (!in.ok || pistes < 0 || pistes > 10000) return std::nullopt;
  for (int64_t i = 0; i < pistes; i++) {
    const int64_t grade = in.get();
    const int64_t n = in.get();
    if (grade < 0 || grade >= kGrades || n < 0 || n > 100000) return std::nullopt;
    HintPiste p{HintGrade(grade), {}, {}};
    int64_t x = 0;
    int64_t z = 0;
    for (int64_t k = 0; k < n; k++) {
      x += in.get();
      z += in.get();
      p.points.push_back({double(x) * kHintGrain, double(z) * kHintGrain});
      p.widths.push_back(double(in.get()) * kHintWidthStep);
    }
    h.pistes.push_back(std::move(p));
  }
  const int64_t wooded = in.get();
  if (wooded == 1) {
    HintWoods w{};
    w.line = double(in.get()) / 255;
    for (double& b : w.bands) b = double(in.get()) / 255;
    h.woods = w;
  } else if (wooded != 0) {
    return std::nullopt;
  }
  if (!in.ok || !in.done()) return std::nullopt;
  return h;
}

std::string encode_hints(const RealHints& h) {
  packed::Writer out;
  out.put(int64_t(h.lifts.size()));
  for (const HintLift& l : h.lifts) {
    out.put(int64_t(l.kind));
    for (const double v : {l.bottom.x, l.bottom.z, l.top.x, l.top.z})
      out.put(on_grain(v, kHintGrain));
  }
  out.put(int64_t(h.pistes.size()));
  for (const HintPiste& p : h.pistes) {
    out.put(int64_t(p.grade));
    out.put(int64_t(p.points.size()));
    int64_t px = 0;
    int64_t pz = 0;
    for (size_t k = 0; k < p.points.size(); k++) {
      const int64_t x = on_grain(p.points[k].x, kHintGrain);
      const int64_t z = on_grain(p.points[k].z, kHintGrain);
      out.put(x - px);
      out.put(z - pz);
      out.put(on_grain(k < p.widths.size() ? p.widths[k] : 0, kHintWidthStep));
      px = x;
      pz = z;
    }
  }
  if (h.woods) {
    out.put(1);
    const auto byte = [](double v) {
      return std::clamp(on_grain(v * 255, 1), int64_t(0), int64_t(255));
    };
    out.put(byte(h.woods->line));
    for (const double b : h.woods->bands) out.put(byte(b));
  } else {
    out.put(0);
  }
  return packed::to_base64(out.bytes);
}

std::optional<FaceWoods> face_woods(const RealFace& face, const RealHints& hints) {
  if (!hints.woods || face.heights.empty()) return std::nullopt;
  const auto [lo, hi] = std::minmax_element(face.heights.begin(), face.heights.end());
  FaceWoods w{double(*lo), double(*hi), 0, hints.woods->bands};
  w.line = w.lo + hints.woods->line * (w.hi - w.lo);
  return w;
}

double face_cover_at(const FaceWoods& woods, double h) {
  constexpr int n = kWoodsBands;
  const double t = (h - woods.lo) / std::max(1.0, woods.hi - woods.lo) * n - 0.5;
  const int i = std::clamp(int(std::floor(t)), 0, n - 1);
  const int j = std::min(n - 1, i + 1);
  const double f = std::clamp(t - i, 0.0, 1.0);
  return woods.bands[size_t(i)] + (woods.bands[size_t(j)] - woods.bands[size_t(i)]) * f;
}

}  // namespace ge::mountain::mapgen
