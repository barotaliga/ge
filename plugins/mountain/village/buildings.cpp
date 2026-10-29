// The village's buildings stood on lots along its streets (buildings.hpp).
//
// ge:summary place_buildings: lots along the streets' sides, the fit, the trees felled
// ge:keywords village buildings place-buildings lots frontage walk fit felled obb
// ge:skill mountain
// ge:test tests/mountain_buildings_test.cpp
#include "mountain/village/buildings.hpp"

#include <cmath>
#include <cstring>
#include <optional>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::village {

namespace {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;
namespace lm = ge::core::libm;
namespace B = buildings;
using ge::core::hypot;
using mg::PlanPoint;

constexpr double kPi = 3.14159265358979323846;

// A line of street pieces end to end, each point knowing its piece and the reach of both its
// sides (to the back of the sidewalk), its arc from the line's start.
struct Line {
  std::vector<StreetPoint> points;
  std::vector<int32_t> street;
  std::vector<std::array<double, 2>> reach;
  double length = 0;
};

Line line_of(const Streets& plan, std::span<const int32_t> pieces) {
  Line line;
  for (size_t i = 0; i < pieces.size(); i++) {
    if (pieces[i] < 0) continue;
    const Street& st = plan.streets[static_cast<size_t>(pieces[i])];
    if (st.points.size() < 2) continue;
    // Each piece turned to run on from the last's end (the first, toward the next's start).
    bool flip = false;
    if (!line.points.empty()) {
      const StreetPoint& end = line.points.back();
      const StreetPoint& a = st.points.front();
      const StreetPoint& b = st.points.back();
      flip = hypot(b.x - end.x, b.z - end.z) < hypot(a.x - end.x, a.z - end.z);
    } else if (i + 1 < pieces.size() && pieces[i + 1] >= 0) {
      const Street& next = plan.streets[static_cast<size_t>(pieces[i + 1])];
      if (!next.points.empty()) {
        const StreetPoint& a = st.points.front();
        const StreetPoint& b = st.points.back();
        const StreetPoint& n0 = next.points.front();
        const StreetPoint& n1 = next.points.back();
        const double to_a = num::min(hypot(a.x - n0.x, a.z - n0.z), hypot(a.x - n1.x, a.z - n1.z));
        const double to_b = num::min(hypot(b.x - n0.x, b.z - n0.z), hypot(b.x - n1.x, b.z - n1.z));
        flip = to_a < to_b;
      }
    }
    const size_t n = st.points.size();
    for (size_t k = 0; k < n; k++) {
      StreetPoint p = st.points[flip ? n - 1 - k : k];
      std::array<double, 2> reach{side_reach(st.section, 0), side_reach(st.section, 1)};
      if (flip) {
        p.heading = p.heading + kPi;
        std::swap(reach[0], reach[1]);
      }
      if (!line.points.empty()) {
        const StreetPoint& last = line.points.back();
        const double d = hypot(p.x - last.x, p.z - last.z);
        if (d < 1e-6) continue;
        p.s = last.s + d;
      } else {
        p.s = 0;
      }
      line.points.push_back(p);
      line.street.push_back(pieces[i]);
      line.reach.push_back(reach);
    }
  }
  if (!line.points.empty()) line.length = line.points.back().s;
  return line;
}

// The line's point at arc `s` (clamped), its piece and its reach.
struct LineAt {
  StreetPoint p;
  int32_t street;
  std::array<double, 2> reach;
};

LineAt line_at(const Line& line, double s) {
  const std::vector<StreetPoint>& pts = line.points;
  s = num::max(0.0, num::min(line.length, s));
  size_t lo = 0;
  size_t hi = pts.size() - 1;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    if (pts[mid].s <= s) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const StreetPoint& a = pts[lo];
  const StreetPoint& b = pts[hi];
  const double span = b.s - a.s;
  const double t = span > 1e-9 ? (s - a.s) / span : 0;
  LineAt at;
  at.p = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, s,
          lm::atan2(b.x - a.x, b.z - a.z)};
  at.street = line.street[lo];
  at.reach = line.reach[lo];
  return at;
}

// A footprint's frame: its middle, the way it faces and the right of that.
struct Frame {
  double x, z;
  double fx, fz;  // forward
  double rx, rz;  // right: (cos h, -sin h)
};

Frame frame_of(double x, double z, double heading) {
  const double fx = lm::sin(heading);
  const double fz = lm::cos(heading);
  return {x, z, fx, fz, fz, -fx};
}

// Whether two footprints come within `gap` m of each other (the separating axes of two
// rectangles).
bool overlap(const Building& a, const Building& b, double gap) {
  const Frame fa = frame_of(a.x, a.z, a.heading);
  const Frame fb = frame_of(b.x, b.z, b.heading);
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const double ha[2] = {a.width / 2 + gap / 2, a.depth / 2 + gap / 2};
  const double hb[2] = {b.width / 2 + gap / 2, b.depth / 2 + gap / 2};
  const double axes[4][2] = {{fa.rx, fa.rz}, {fa.fx, fa.fz}, {fb.rx, fb.rz}, {fb.fx, fb.fz}};
  for (const auto& ax : axes) {
    const double ra = ha[0] * std::fabs(fa.rx * ax[0] + fa.rz * ax[1]) +
                      ha[1] * std::fabs(fa.fx * ax[0] + fa.fz * ax[1]);
    const double rb = hb[0] * std::fabs(fb.rx * ax[0] + fb.rz * ax[1]) +
                      hb[1] * std::fabs(fb.fx * ax[0] + fb.fz * ax[1]);
    if (std::fabs(dx * ax[0] + dz * ax[1]) > ra + rb) return false;
  }
  return true;
}

// What fills a walk once its own kinds are stood.
enum class Then : uint8_t { stop, hotels, town };

// ONE WALK along a side of a line: which side (0 left, 1 right), whether the buildings face the
// street (else they turn their backs on it), how far back they stand, where it starts and which
// way it goes along the line, the kinds it stands first and what it goes on with.
struct Walk {
  const Line* line;
  int32_t side;
  bool face_street;
  double setback;
  double from;
  int32_t dir;
  std::vector<BuildingKind> first;
  Then then;
  int32_t salt;
};

struct Placer {
  const mg::Level& level;
  const GroundMask& mask;
  const Clear& clear;
  uint32_t seed;
  std::vector<Building> out;
  int32_t hotels = 0;
  int32_t shops = 0;
  bool once[kBuildingKinds] = {};

  // The building of `kind` whose front's middle is at arc `s` of the walk's side, if it fits.
  std::optional<Building> try_at(const Walk& w, BuildingKind kind, double s) const {
    const BuildingSize size = building_size(kind);
    const LineAt at = line_at(*w.line, s);
    const double sign = w.side == 1 ? 1 : -1;
    const double lat = sign * (at.reach[static_cast<size_t>(w.side)] + w.setback + size.depth / 2);
    const PlanPoint c = beside_point(at.p, lat);
    // Facing the street: its front toward the line, the way of minus its side's right.
    const double rx = lm::cos(at.p.heading);
    const double rz = -lm::sin(at.p.heading);
    const double fx = (w.face_street ? -sign : sign) * rx;
    const double fz = (w.face_street ? -sign : sign) * rz;
    Building b{static_cast<int32_t>(out.size()),
               kind,
               c.x,
               0,
               c.z,
               lm::atan2(fx, fz),
               size.width,
               size.depth,
               size.eaves,
               size.ridge,
               0,
               at.street};
    const Frame f = frame_of(b.x, b.z, b.heading);
    double hi = -1e300;
    double lo = 1e300;
    const double hw = b.width / 2 + B::kPad;
    const double hd = b.depth / 2 + B::kPad;
    const auto nu = static_cast<int32_t>(std::ceil(2 * hw / B::kSample));
    const auto nv = static_cast<int32_t>(std::ceil(2 * hd / B::kSample));
    for (int32_t i = 0; i <= nu; i++) {
      for (int32_t j = 0; j <= nv; j++) {
        const double u = -hw + 2 * hw * i / nu;
        const double v = -hd + 2 * hd * j / nv;
        const double x = b.x + u * f.rx + v * f.fx;
        const double z = b.z + u * f.rz + v * f.fz;
        if (ground_at(mask, x, z) >= Ground::street) return std::nullopt;
        if (!clear(x, z, streets::kClearHub)) return std::nullopt;
      }
    }
    // The fall is the walls' own, every metre inside them.
    const auto mu = static_cast<int32_t>(std::ceil(b.width));
    const auto mv = static_cast<int32_t>(std::ceil(b.depth));
    for (int32_t i = 0; i <= mu; i++) {
      for (int32_t j = 0; j <= mv; j++) {
        const double u = b.width * (static_cast<double>(i) / mu - 0.5);
        const double v = b.depth * (static_cast<double>(j) / mv - 0.5);
        const double y = level.ground_at(b.x + u * f.rx + v * f.fx, b.z + u * f.rz + v * f.fz);
        hi = num::max(hi, y);
        lo = num::min(lo, y);
      }
    }
    if (hi - lo > B::kFall) return std::nullopt;
    for (const Building& o : out) {
      if (hypot(o.x - b.x, o.z - b.z) >
          (o.width + o.depth + b.width + b.depth) / 2 + B::kGapLeast) {
        continue;
      }
      if (overlap(o, b, B::kGapLeast)) return std::nullopt;
    }
    b.y = hi;
    b.low = lo;
    return b;
  }

  // The kind the walk stands next, or none when it is done.
  std::optional<BuildingKind> next(const Walk& w, size_t& i, int32_t k) const {
    while (i < w.first.size()) {
      const BuildingKind kind = w.first[i];
      const bool single = kind != BuildingKind::hotel && kind != BuildingKind::shop &&
                          kind != BuildingKind::house && kind != BuildingKind::flats;
      if (single && once[static_cast<size_t>(kind)]) {
        i++;
        continue;
      }
      if (kind == BuildingKind::hotel && hotels >= B::kHotels) {
        i++;
        continue;
      }
      if (kind == BuildingKind::shop && shops >= B::kShops) {
        i++;
        continue;
      }
      return kind;
    }
    switch (w.then) {
      case Then::stop:
        return std::nullopt;
      case Then::hotels:
        if (hotels >= B::kHotels) return std::nullopt;
        return BuildingKind::hotel;
      case Then::town:
        return pick(seed, "village-lots", k, w.salt) < B::kFlats ? BuildingKind::flats
                                                                 : BuildingKind::house;
    }
    return std::nullopt;
  }

  void walk(const Walk& w) {
    if (w.line->points.size() < 2) return;
    size_t i = 0;
    int32_t k = 0;
    double s = w.from;
    std::optional<BuildingKind> kind = next(w, i, k);
    double searched = 0;
    while (kind && static_cast<int32_t>(out.size()) < B::kMost) {
      const double width = building_size(*kind).width;
      const double mid = s + w.dir * width / 2;
      if (mid - width / 2 < 0 || mid + width / 2 > w.line->length) return;
      const std::optional<Building> b = try_at(w, *kind, mid);
      if (b) {
        out.push_back(*b);
        if (*kind == BuildingKind::hotel) hotels++;
        if (*kind == BuildingKind::shop) shops++;
        once[static_cast<size_t>(*kind)] = true;
        if (i < w.first.size() && w.first[i] == *kind) i++;
        const double gap =
            B::kGapLeast + (B::kGapMost - B::kGapLeast) * pick(seed, "village-lots", k, w.salt + 1);
        s += w.dir * (width + gap);
        if (pick(seed, "village-lots", k, w.salt + 2) < B::kGarden) {
          s += w.dir * B::kGardenWidth;
        }
        k++;
        searched = 0;
        kind = next(w, i, k);
        continue;
      }
      s += w.dir * B::kNudge;
      searched += B::kNudge;
      if (searched > B::kSearch) {
        // Given up here: on with the next kind from where this one was first tried.
        s -= w.dir * searched;
        searched = 0;
        if (i < w.first.size()) {
          i++;
        } else if (w.then != Then::town) {
          return;
        } else {
          s += w.dir * B::kSearch;
        }
        kind = next(w, i, k);
      }
    }
  }
};

// The side of a line toward the hub (away from the valley): 1 right, 0 left.
int32_t hub_side(const Line& line, int32_t valley) {
  const LineAt mid = line_at(line, line.length / 2);
  const PlanPoint q = beside_point(mid.p, 1);
  return (q.z - mid.p.z) * valley < 0 ? 1 : 0;
}

// The arc of `line` nearest (x, z).
double arc_near(const Line& line, double x, double z) {
  double best = 1e300;
  double s = 0;
  for (const StreetPoint& p : line.points) {
    const double d = hypot(p.x - x, p.z - z);
    if (d < best) {
      best = d;
      s = p.s;
    }
  }
  return s;
}

}  // namespace

std::vector<Building> place_buildings(const mapgen::Level& level, const Streets& plan,
                                      const GroundMask& mask, const Clear& clear) {
  using K = BuildingKind;
  if (plan.main.empty()) return {};
  Placer placer{level, mask, clear, num::to_uint32(level.seed), {}};
  const Line main = line_of(plan, plan.main);
  const Line back = line_of(plan, plan.back);
  if (main.points.size() < 2) return {};
  // Where the square fronts the main street, and its half along it.
  double square_s = main.length / 2;
  double square_half = 0;
  for (const VillageArea& a : plan.areas) {
    if (a.kind != AreaKind::square) continue;
    square_s = arc_near(main, a.x, a.z);
    square_half = a.half;
  }
  const int32_t hub = hub_side(main, plan.valley);
  const int32_t town = 1 - hub;
  const double out = square_half + B::kGapLeast;
  // The front row from the square, each way.
  placer.walk({&main,
               hub,
               false,
               B::kFrontRowSetback,
               square_s + out,
               1,
               {K::lodge, K::rental, K::school},
               Then::hotels,
               10});
  placer.walk({&main,
               hub,
               false,
               B::kFrontRowSetback,
               square_s - out,
               -1,
               {K::ticket, K::first_aid},
               Then::hotels,
               20});
  // Across the main street: the church opposite the square, then shops, then the town.
  const double church = building_size(K::church).width;
  placer.walk({&main,
               town,
               true,
               B::kSetback,
               square_s - church / 2,
               1,
               {K::church, K::shop, K::shop},
               Then::town,
               30});
  placer.walk({&main,
               town,
               true,
               B::kSetback,
               square_s - church / 2 - B::kGapLeast,
               -1,
               {K::shop, K::shop},
               Then::town,
               40});
  // The roads out: the garage and the pump house past the village.
  for (int32_t e = 0; e < 2; e++) {
    const int32_t road = plan.end_road[static_cast<size_t>(e)];
    if (road < 0) continue;
    const int32_t one[1] = {road};
    const Line r = line_of(plan, one);
    placer.walk({&r, 1, true, B::kSetback, 0, 1, {K::garage, K::pump_house}, Then::stop, 50 + e});
  }
  // The back street, both sides: houses and flats.
  for (int32_t side = 0; side < 2; side++) {
    placer.walk({&back, side, true, B::kSetback, 0, 1, {}, Then::town, 60 + side});
  }
  return std::move(placer.out);
}

bool in_building(const Building& b, double x, double z, double pad) {
  const Frame f = frame_of(b.x, b.z, b.heading);
  const double dx = x - b.x;
  const double dz = z - b.z;
  return std::fabs(dx * f.rx + dz * f.rz) <= b.width / 2 + pad &&
         std::fabs(dx * f.fx + dz * f.fz) <= b.depth / 2 + pad;
}

void fell_for_buildings(const mapgen::Level& level, std::span<const Building> buildings,
                        std::vector<uint8_t>& felled) {
  felled.resize(level.trees.size(), 0);
  for (size_t i = 0; i < level.trees.size(); i++) {
    if (felled[i] != 0) continue;
    const mapgen::TreeDef& t = level.trees[i];
    for (const Building& b : buildings) {
      if (in_building(b, t.x, t.z, lots::kFell)) {
        felled[i] = 1;
        break;
      }
    }
  }
}

uint64_t buildings_hash(std::span<const Building> buildings, uint64_t h) {
  const auto mix = [&h](double v) {
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  for (const Building& b : buildings) {
    mix(static_cast<double>(b.kind));
    mix(b.x);
    mix(b.y);
    mix(b.z);
    mix(b.heading);
    mix(b.low);
    mix(b.street);
  }
  return h;
}

}  // namespace ge::mountain::village
