// The mountain's lamps after dark: where the masts and posts stand, their lights, their meshes.
//
// ge:summary The piste's masts, the street lamps and the windows' spill, placed, lit and drawn
// ge:keywords night lamps piste_lamps street_lamps lamps_lit lamp_lights window_lights
// ge:keywords lamp_post_meshes lamp_instance
// ge:skill mountain render
// ge:test tests/mountain_night_lamps_test.cpp
#include "mountain/app/night_lamps.hpp"

#include <algorithm>
#include <cmath>

#include "mountain/app/building_look.hpp"
#include "mountain/village/street_rules.hpp"

namespace ge::mountain::app {

namespace {

namespace vg = village;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The lamps' light, linear rgb of a luminance near 1: the floods' neutral white (4000 K), the
// street lamps' warm white (3000 K), a lamp-lit room's.
constexpr float kFloodRgb[3] = {1.0f, 0.90f, 0.78f};
constexpr float kStreetRgb[3] = {1.0f, 0.78f, 0.52f};
constexpr float kRoomRgb[3] = {1.0f, 0.70f, 0.42f};
// The cones off the axis, degrees: whole inside, none past.
constexpr float kNearInner = 40, kNearOuter = 70;
constexpr float kThrowInner = 20, kThrowOuter = 45;
constexpr float kStreetInner = 40, kStreetOuter = 72;
// A street lamp's head is tipped this far under level toward the carriageway, rad.
constexpr double kStreetTilt = 75 * kDeg;
// How far forward of its post a lamp's head stands, m: a mast's flood, a street lamp's arm.
constexpr double kFloodAhead = 0.4;
constexpr double kFloodApart = 0.4;  // each of a mast's two floods off its middle
constexpr double kArm = 0.9;
// The posts' surfaces: galvanised steel, the lamp's glass (lit: its LED's glow, cd/m^2).
constexpr float kSteelRgb[3] = {0.16f, 0.17f, 0.18f};
constexpr float kGlassRgb[3] = {0.95f, 0.92f, 0.85f};
constexpr float kGlassNits = 4000;

// The track's point at arc `s` (the first at or past it).
const mapgen::TrackPoint* track_at(const mapgen::Level& level, double s) {
  const std::vector<mapgen::TrackPoint>& t = level.track.points;
  const auto it = std::lower_bound(t.begin(), t.end(), s,
                                   [](const mapgen::TrackPoint& p, double v) { return p.s < v; });
  return it == t.end() ? nullptr : &*it;
}

// How far out from a street's centreline its lamps stand on `side`, m: the sidewalk's middle,
// or past the windrow where it has none.
double post_out(const vg::Section& sec, int side) {
  const vg::StreetSide& s = sec.sides[static_cast<size_t>(side)];
  return sec.lane + s.park + s.bank + (s.walk > 0 ? s.walk * 0.5 : 0.5);
}

// Whether (x, z) is on any street's carriageway or the car park's aisles (a junction's corner,
// the mouth of a cross street): no post stands there.
bool on_carriageway(const vg::Streets& plan, double x, double z) {
  for (const vg::Street& st : plan.streets) {
    const double reach = st.section.lane + 0.5;
    for (const vg::StreetPoint& p : st.points) {
      if ((p.x - x) * (p.x - x) + (p.z - z) * (p.z - z) < reach * reach) return true;
    }
  }
  return false;
}

}  // namespace

std::vector<Lamp> piste_lamps(const mapgen::Level& level) {
  std::vector<Lamp> out;
  if (level.track.points.size() < 2) return out;
  const double length = level.track.length;
  for (int k = 0;; ++k) {
    const double s = kMastFirst + k * kMastEvery;
    if (s >= length - kMastFirst * 0.5) break;
    const mapgen::TrackPoint* p = track_at(level, s);
    if (p == nullptr) break;
    // On the piste's left, aimed across it to the right and downhill.
    const double out_m = p->width * 0.5 + kMastOut;
    const double rx = std::cos(p->heading), rz = -std::sin(p->heading);
    Lamp m;
    m.kind = LampKind::flood;
    m.x = p->x - rx * out_m;
    m.z = p->z - rz * out_m;
    m.y = level.ground_at(m.x, m.z);
    m.height = p->width > kMastBroad ? kMastHigh : kMastLow;
    m.heading = p->heading + (90 - kNearDownhill) * kDeg;
    m.tilt = std::max(kMastMinTilt * kDeg, std::atan2(m.height, kMastOut + kNearAt * p->width));
    m.throw_heading = p->heading + (90 - kThrowDownhill) * kDeg;
    m.throw_tilt = std::max(kMastMinTilt * kDeg, std::atan2(m.height, kThrowAt));
    const double broad = std::max(p->width / kMastBroad, 1.0);
    m.bright = broad * broad;
    out.push_back(m);
  }
  return out;
}

std::vector<Lamp> street_lamps(const mapgen::Level& level, const vg::Streets& plan) {
  std::vector<Lamp> out;
  int alternate = 0;
  for (const vg::Street& st : plan.streets) {
    if (st.kind == vg::StreetKind::aisle || st.points.size() < 2) continue;
    const double every = st.kind == vg::StreetKind::road ? kRoadEvery : kStreetEvery;
    for (int k = 0;; ++k) {
      const double s = every * (k + 0.5);
      if (s >= st.length) break;
      // The sidewalk's side (side 0, the street's left, carries the one sidewalk a back street
      // has); the main street's both sides in turn.
      const int side = st.kind == vg::StreetKind::main ? (alternate++ & 1) : 0;
      const double out_m = post_out(st.section, side);
      const vg::StreetPoint p = vg::street_at(st, s);
      const mapgen::PlanPoint foot = vg::beside_point(p, side == 0 ? -out_m : out_m);
      if (on_carriageway(plan, foot.x, foot.z)) continue;
      Lamp l;
      l.kind = LampKind::street;
      l.x = foot.x;
      l.z = foot.z;
      l.y = level.ground_at(l.x, l.z);
      l.height = kPostHeight;
      // Facing the carriageway: right of the heading from the left side, left from the right.
      l.heading = p.heading + (side == 0 ? 0.5 : -0.5) * kPi;
      l.tilt = kStreetTilt;
      out.push_back(l);
    }
  }
  return out;
}

double lamps_lit(double sun_elevation) {
  const double top = kLightsOnElevation + 3 * kDeg;
  const double t = std::clamp((top - sun_elevation) / (top - kLightsOnElevation), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

void lamp_direction(const Lamp& lamp, float out[3]) {
  const double c = std::cos(lamp.tilt);
  out[0] = static_cast<float>(std::sin(lamp.heading) * c);
  out[1] = static_cast<float>(-std::sin(lamp.tilt));
  out[2] = static_cast<float>(std::cos(lamp.heading) * c);
}

void lamp_lights(std::span<const Lamp> lamps, double lit, bool shadows, uint32_t key_base,
                 std::vector<render::LocalLight>& out) {
  if (lit <= 0) return;
  for (size_t k = 0; k < lamps.size(); ++k) {
    const Lamp& lamp = lamps[k];
    render::LocalLight l;
    if (lamp.kind == LampKind::street) {
      l.position[0] = lamp.x + std::sin(lamp.heading) * kArm;
      l.position[1] = lamp.y + lamp.height;
      l.position[2] = lamp.z + std::cos(lamp.heading) * kArm;
      lamp_direction(lamp, l.direction);
      std::copy(kStreetRgb, kStreetRgb + 3, l.colour);
      l.candela = static_cast<float>(kStreetCandela * lit);
      l.range_m = kStreetRange;
      l.radius_m = 0.15f;
      l.inner_deg = kStreetInner;
      l.outer_deg = kStreetOuter;
      l.shadow = shadows ? key_base + static_cast<uint32_t>(k) : 0;
      out.push_back(l);
      continue;
    }
    // A mast's two floods, side by side on its crossbar.
    for (int f = 0; f < 2; ++f) {
      Lamp head = lamp;
      if (f == 1) {
        head.heading = lamp.throw_heading;
        head.tilt = lamp.throw_tilt;
      }
      const double side = f == 0 ? -kFloodApart : kFloodApart;
      const double rx = std::cos(lamp.heading), rz = -std::sin(lamp.heading);
      l.position[0] = lamp.x + std::sin(lamp.heading) * kFloodAhead + rx * side;
      l.position[1] = lamp.y + lamp.height;
      l.position[2] = lamp.z + std::cos(lamp.heading) * kFloodAhead + rz * side;
      lamp_direction(head, l.direction);
      std::copy(kFloodRgb, kFloodRgb + 3, l.colour);
      l.candela = static_cast<float>((f == 0 ? kNearCandela : kThrowCandela) * lamp.bright * lit);
      l.range_m = kFloodRange;
      l.radius_m = 0.3f;
      l.inner_deg = f == 0 ? kNearInner : kThrowInner;
      l.outer_deg = f == 0 ? kNearOuter : kThrowOuter;
      l.shadow = shadows ? key_base + 2 * static_cast<uint32_t>(k) + static_cast<uint32_t>(f) : 0;
      out.push_back(l);
    }
  }
}

void window_lights(std::span<const vg::Building> buildings, std::span<const uint8_t> lit,
                   std::vector<render::LocalLight>& out) {
  const size_t n = std::min(buildings.size(), lit.size());
  for (size_t k = 0; k < n; ++k) {
    if (lit[k] == 0) continue;
    const vg::Building& b = buildings[k];
    const double out_m = b.depth * 0.5 + 1.0;
    render::LocalLight l;
    l.position[0] = b.x + std::sin(b.heading) * out_m;
    l.position[1] = b.y + 1.5;
    l.position[2] = b.z + std::cos(b.heading) * out_m;
    std::copy(kRoomRgb, kRoomRgb + 3, l.colour);
    l.candela = std::clamp(kWindowCandela * static_cast<float>(b.width / 6), 15.0f, 120.0f);
    l.range_m = kWindowRange;
    l.radius_m = 1.5f;  // a front of windows, not a bulb
    out.push_back(l);
  }
}

std::vector<render::MeshSource> lamp_post_meshes() {
  std::vector<render::MeshSource> out;
  // The masts: a tapered pole, the flood's box on a bracket at its top, its glass to the front.
  for (const double h : {kMastLow, kMastHigh}) {
    render::MeshData m;
    const auto top = static_cast<float>(h);
    render::add_cylinder(m, 0, top + 0.3f, 0.13f, 0.08f, 8, 0);
    const float bracket[3] = {0, top, static_cast<float>(kFloodAhead) * 0.5f};
    const float bracket_half[3] = {0.05f, 0.05f, static_cast<float>(kFloodAhead) * 0.5f};
    render::add_box(m, bracket, bracket_half, 0);
    const float bar[3] = {0, top, static_cast<float>(kFloodAhead) - 0.12f};
    const float bar_half[3] = {static_cast<float>(kFloodApart) + 0.1f, 0.04f, 0.04f};
    render::add_box(m, bar, bar_half, 0);
    for (const double side : {-kFloodApart, kFloodApart}) {
      const auto x = static_cast<float>(side);
      const float head[3] = {x, top, static_cast<float>(kFloodAhead)};
      const float head_half[3] = {0.22f, 0.16f, 0.08f};
      render::add_box(m, head, head_half, 0);
      const float glass[3] = {x, top, static_cast<float>(kFloodAhead) + 0.085f};
      const float glass_half[3] = {0.19f, 0.13f, 0.01f};
      render::add_box(m, glass, glass_half, 1);
    }
    render::MeshSource s;
    s.lods.push_back({std::move(m), 0.0f});
    out.push_back(std::move(s));
  }
  // The street lamp: a post, an arm over the kerb, the lamp's head with its glass underneath.
  render::MeshData m;
  const auto top = static_cast<float>(kPostHeight);
  render::add_cylinder(m, 0, top + 0.1f, 0.08f, 0.05f, 8, 0);
  const float arm[3] = {0, top, static_cast<float>(kArm) * 0.5f};
  const float arm_half[3] = {0.03f, 0.03f, static_cast<float>(kArm) * 0.5f};
  render::add_box(m, arm, arm_half, 0);
  const float head[3] = {0, top - 0.04f, static_cast<float>(kArm)};
  const float head_half[3] = {0.16f, 0.06f, 0.28f};
  render::add_box(m, head, head_half, 0);
  const float glass[3] = {0, top - 0.105f, static_cast<float>(kArm)};
  const float glass_half[3] = {0.13f, 0.008f, 0.24f};
  render::add_box(m, glass, glass_half, 1);
  render::MeshSource s;
  s.lods.push_back({std::move(m), 0.0f});
  out.push_back(std::move(s));
  return out;
}

std::vector<render::Surface> lamp_post_surfaces() {
  std::vector<render::Surface> out;
  for (const bool lit : {false, true}) {
    out.push_back(render::metal_surface(kSteelRgb, 0.55f));
    render::Surface glass = render::matte_surface(kGlassRgb);
    glass.rough = 0.2f;
    glass.spec = 0.5f;
    if (lit) glass.emissive_nits = kGlassNits;
    out.push_back(glass);
  }
  return out;
}

render::Instance lamp_instance(const Lamp& lamp, bool lit) {
  render::Instance in;
  in.mesh = lamp.kind == LampKind::street ? 2u : (lamp.height > kMastLow ? 1u : 0u);
  in.material = lit ? 2u : 0u;
  in.position[0] = lamp.x;
  in.position[1] = lamp.y;
  in.position[2] = lamp.z;
  const double half = lamp.heading * 0.5;
  in.rotation[1] = static_cast<float>(std::sin(half));
  in.rotation[3] = static_cast<float>(std::cos(half));
  return in;
}

}  // namespace ge::mountain::app
