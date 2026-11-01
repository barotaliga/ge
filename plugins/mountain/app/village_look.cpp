// The village as drawn: the streets' mesh, the vehicles' meshes, their colours (village_look.hpp).
//
// ge:summary The streets' strips off the ground mask, the vehicles' boxes and wheels, the paints
// ge:keywords village street-mesh vehicle-mesh wheels glasshouse bumpers paints livery
// ge:skill mountain render
// ge:test tests/mountain_village_look_test.cpp
#include "mountain/app/village_look.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/num.hpp"

namespace ge::mountain::app {

namespace {

namespace vg = village;

// The paints, linear rgb: white, silver, black, red, blue, green, yellow, slate; the bus's livery.
constexpr std::array<std::array<float, 3>, kPaints> kPaintRgb{{
    {0.78f, 0.79f, 0.80f},
    {0.42f, 0.44f, 0.47f},
    {0.025f, 0.025f, 0.03f},
    {0.48f, 0.035f, 0.03f},
    {0.035f, 0.10f, 0.36f},
    {0.04f, 0.13f, 0.07f},
    {0.62f, 0.42f, 0.04f},
    {0.15f, 0.16f, 0.18f},
    {0.04f, 0.22f, 0.50f},
}};
constexpr std::array<float, 3> kGlassRgb{0.015f, 0.022f, 0.03f};
constexpr std::array<float, 3> kTyreRgb{0.018f, 0.018f, 0.02f};
constexpr std::array<float, 3> kTrimRgb{0.035f, 0.035f, 0.04f};
constexpr std::array<float, 3> kLampRgb{0.85f, 0.82f, 0.70f};
// A ploughed street packed under the traffic, grit in it; a sidewalk the blower left.
constexpr std::array<float, 3> kCarriageRgb{0.30f, 0.29f, 0.28f};
constexpr std::array<float, 3> kWalkRgb{0.58f, 0.59f, 0.61f};
// Hoar frost's white, and how far a full frost takes each part toward it (the glass rimed over,
// a tyre's flank hardly; the streets' packed snow already pale).
constexpr std::array<float, 3> kFrostRgb{0.80f, 0.84f, 0.90f};
constexpr std::array<float, kVillageParts> kFrostReach{0.7f, 0.95f, 0.3f, 0.6f, 0.8f, 0.5f, 0.4f};
// A full glaze: the carriageway this much darker, its roughness and its reflectance.
constexpr float kGlazeDark = 0.15f, kGlazeRough = 0.04f, kGlazeSpec = 0.5f;

void box(render::MeshData& m, float cx, float cy, float cz, float hx, float hy, float hz,
         uint8_t part) {
  const float c[3] = {cx, cy, cz};
  const float h[3] = {hx, hy, hz};
  render::add_box(m, c, h, part);
}

// A box from y0 to y1 and z0 to z1, `hx` either side of x 0.
void slab(render::MeshData& m, float hx, float y0, float y1, float z0, float z1, uint8_t part) {
  box(m, 0, (y0 + y1) / 2, (z0 + z1) / 2, hx, (y1 - y0) / 2, (z1 - z0) / 2, part);
}

// A wheel of radius `r` and half its width `hw` at (x, r, z): two boxes crossed, an octagon from
// the side.
void wheel(render::MeshData& m, float x, float z, float r, float hw, uint8_t part) {
  box(m, x, r, z, hw, r * 0.72f, r, part);
  box(m, x, r, z, hw, r, r * 0.72f, part);
}

void car(render::MeshData& m, const vg::VehicleSize& v, float rear, float front) {
  const float hl = static_cast<float>(v.length) / 2;
  const float hw = static_cast<float>(v.width) / 2;
  const auto h = static_cast<float>(v.height);
  const auto r = static_cast<float>(v.wheel);
  const float sill = r * 0.55f;
  const float belt = h * 0.6f;
  // The body to the belt line, the glasshouse over it from the tailgate to the windscreen's foot
  // over the front wheels, and the roof on it.
  slab(m, hw, sill, belt, -hl, hl, kPaint);
  const float glass_back = -hl + 0.2f;
  const float glass_front = front + r * 0.4f;
  slab(m, hw * 0.88f, belt, h - 0.07f, glass_back, glass_front, kGlass);
  slab(m, hw * 0.86f, h - 0.07f, h, glass_back + 0.1f, glass_front - 0.25f, kPaint);
  // The bumpers, the sills under the doors, the mirrors and the lamps.
  slab(m, hw + 0.02f, sill - 0.05f, sill + 0.28f, hl - 0.06f, hl + 0.03f, kTrim);
  slab(m, hw + 0.02f, sill - 0.05f, sill + 0.28f, -hl - 0.03f, -hl + 0.06f, kTrim);
  box(m, 0, sill, 0, hw + 0.01f, 0.06f, hl - r * 2.2f, kTrim);
  for (const float side : {-1.0f, 1.0f}) {
    box(m, side * (hw + 0.08f), belt + 0.05f, glass_front - 0.15f, 0.08f, 0.06f, 0.05f, kTrim);
    box(m, side * (hw - 0.22f), belt - 0.12f, hl + 0.005f, 0.16f, 0.06f, 0.02f, kLamp);
  }
  const float tx = static_cast<float>(v.track) / 2;
  for (const float side : {-1.0f, 1.0f}) {
    wheel(m, side * tx, rear, r, 0.11f, kTyre);
    wheel(m, side * tx, front, r, 0.11f, kTyre);
  }
}

void van(render::MeshData& m, const vg::VehicleSize& v, float rear, float front) {
  const float hl = static_cast<float>(v.length) / 2;
  const float hw = static_cast<float>(v.width) / 2;
  const auto h = static_cast<float>(v.height);
  const auto r = static_cast<float>(v.wheel);
  const float sill = r * 0.6f;
  // The box to the roof; the cab's glass a band round its front.
  const float bonnet = hl - 0.55f;
  slab(m, hw, sill, h * 0.52f, -hl, hl, kPaint);
  slab(m, hw, h * 0.52f, h, -hl, bonnet, kPaint);
  slab(m, hw * 0.94f, h * 0.52f, h * 0.86f, bonnet - 0.9f, bonnet + 0.12f, kGlass);
  slab(m, hw + 0.02f, sill - 0.05f, sill + 0.3f, hl - 0.06f, hl + 0.03f, kTrim);
  slab(m, hw + 0.02f, sill - 0.05f, sill + 0.3f, -hl - 0.03f, -hl + 0.06f, kTrim);
  for (const float side : {-1.0f, 1.0f}) {
    box(m, side * (hw + 0.1f), h * 0.6f, bonnet - 0.3f, 0.1f, 0.12f, 0.05f, kTrim);
    box(m, side * (hw - 0.25f), h * 0.42f, hl + 0.005f, 0.18f, 0.07f, 0.02f, kLamp);
  }
  const float tx = static_cast<float>(v.track) / 2;
  for (const float side : {-1.0f, 1.0f}) {
    wheel(m, side * tx, rear, r, 0.12f, kTyre);
    wheel(m, side * tx, front, r, 0.12f, kTyre);
  }
}

void bus(render::MeshData& m, const vg::VehicleSize& v, float rear, float front) {
  const float hl = static_cast<float>(v.length) / 2;
  const float hw = static_cast<float>(v.width) / 2;
  const auto h = static_cast<float>(v.height);
  const auto r = static_cast<float>(v.wheel);
  const float floor = 0.32f;
  slab(m, hw, floor, h, -hl, hl, kPaint);
  // The band of windows down both sides and the windscreen, standing a little proud of the body.
  slab(m, hw + 0.015f, h * 0.42f, h * 0.84f, -hl + 0.5f, hl - 0.15f, kGlass);
  slab(m, hw * 0.92f, h * 0.3f, h * 0.86f, hl - 0.15f, hl + 0.015f, kGlass);
  slab(m, hw + 0.02f, floor - 0.05f, floor + 0.3f, hl - 0.04f, hl + 0.05f, kTrim);
  slab(m, hw + 0.02f, floor - 0.05f, floor + 0.3f, -hl - 0.05f, -hl + 0.04f, kTrim);
  for (const float side : {-1.0f, 1.0f}) {
    box(m, side * (hw - 0.3f), floor + 0.45f, hl + 0.02f, 0.2f, 0.08f, 0.02f, kLamp);
    box(m, side * (hw + 0.15f), h * 0.8f, hl - 0.1f, 0.12f, 0.2f, 0.04f, kTrim);
  }
  const float tx = static_cast<float>(v.track) / 2;
  for (const float side : {-1.0f, 1.0f}) {
    wheel(m, side * tx, rear, r, 0.16f, kTyre);
    wheel(m, side * tx, front, r, 0.16f, kTyre);
  }
}

void bike(render::MeshData& m, const vg::VehicleSize& v, float rear, float front) {
  const auto r = static_cast<float>(v.wheel);
  wheel(m, 0, rear, r, 0.04f, kTyre);
  wheel(m, 0, front, r, 0.04f, kTyre);
  // The frame from the rear hub to the bars, the bars across.
  const float mid = (rear + front) / 2;
  box(m, 0, r + 0.12f, mid, 0.025f, 0.03f, (front - rear) / 2 - 0.05f, kTyre);
  box(m, 0, r + 0.22f, front - 0.1f, 0.025f, 0.22f, 0.03f, kTyre);
  box(m, 0, r + 0.45f, front - 0.12f, static_cast<float>(v.width) / 2, 0.02f, 0.02f, kTrim);
  // The rider: his legs to the pedals, his back leant to the bars, his head in a helmet.
  box(m, 0, r + 0.25f, rear + 0.35f, 0.14f, 0.3f, 0.08f, kTrim);
  box(m, 0, r + 0.75f, mid - 0.05f, 0.19f, 0.25f, 0.2f, kPaint);
  for (const float side : {-1.0f, 1.0f}) {
    box(m, side * 0.22f, r + 0.6f, front - 0.3f, 0.04f, 0.04f, 0.2f, kPaint);
  }
  const float c[3] = {0, static_cast<float>(v.height) - 0.12f, mid + 0.12f};
  const float rad[3] = {0.12f, 0.13f, 0.14f};
  render::add_ellipsoid(m, c, rad, 3, 6, kTrim);
}

}  // namespace

render::MeshData vehicle_mesh(vg::VehicleKind kind, int lod) {
  const vg::VehicleSize v = vg::vehicle_size(kind);
  const auto mid = static_cast<float>(vg::axle_to_middle(kind));
  const float rear = -mid;
  const float front = rear + static_cast<float>(v.wheelbase);
  render::MeshData m;
  const float hl = static_cast<float>(v.length) / 2;
  const float hw = static_cast<float>(v.width) / 2;
  const auto h = static_cast<float>(v.height);
  if (lod > 0) {
    // FAR: the body as a box, a car's glasshouse a smaller one over it.
    if (kind == vg::VehicleKind::bike) {
      slab(m, 0.2f, 0, h, rear, front, kPaint);
    } else if (kind == vg::VehicleKind::bus || kind == vg::VehicleKind::van) {
      slab(m, hw, 0.15f, h, -hl, hl, kPaint);
    } else {
      slab(m, hw, 0.15f, h * 0.6f, -hl, hl, kPaint);
      slab(m, hw * 0.88f, h * 0.6f, h, -hl + 0.3f, front, kGlass);
    }
    return m;
  }
  switch (kind) {
    case vg::VehicleKind::hatch:
    case vg::VehicleKind::estate:
    case vg::VehicleKind::suv:
      car(m, v, rear, front);
      break;
    case vg::VehicleKind::van:
      van(m, v, rear, front);
      break;
    case vg::VehicleKind::bus:
      bus(m, v, rear, front);
      break;
    case vg::VehicleKind::bike:
      bike(m, v, rear, front);
      break;
  }
  return m;
}

std::vector<render::MeshSource> vehicle_meshes() {
  std::vector<render::MeshSource> out;
  out.reserve(kVehicleKinds);
  for (int k = 0; k < kVehicleKinds; ++k) {
    const auto kind = static_cast<vg::VehicleKind>(k);
    render::MeshSource s;
    s.lods.push_back({vehicle_mesh(kind, 0), kVehicleLodPixels});
    s.lods.push_back({vehicle_mesh(kind, 1), 0.0f});
    out.push_back(std::move(s));
  }
  return out;
}

render::MeshData street_mesh(const mapgen::Level& level, const vg::GroundMask& mask) {
  render::MeshData m;
  const double cell = vg::GroundMask::kCell;
  const auto run_most = static_cast<int32_t>(std::lround(kStreetRun / cell));
  const auto part_of = [](uint8_t g) -> int {
    if (g == static_cast<uint8_t>(vg::Ground::road)) return kCarriage;
    if (g == static_cast<uint8_t>(vg::Ground::street)) return kWalk;
    return -1;
  };
  // Heights at the strip's corners, the snow's own a hand's breadth under it; x and z are the
  // mask's own, from its corner (the instance stands there).
  const auto at = [&](double lx, double lz, float out[3]) {
    out[0] = static_cast<float>(lx);
    out[1] = static_cast<float>(level.ground_at(mask.x0 + lx, mask.z0 + lz) + kStreetLift);
    out[2] = static_cast<float>(lz);
  };
  for (int32_t j = 0; j < mask.rows; ++j) {
    const size_t row = static_cast<size_t>(j) * static_cast<size_t>(mask.cols);
    int32_t i = 0;
    while (i < mask.cols) {
      const int part = part_of(mask.data[row + static_cast<size_t>(i)]);
      if (part < 0) {
        ++i;
        continue;
      }
      int32_t n = 1;
      while (i + n < mask.cols && n < run_most &&
             part_of(mask.data[row + static_cast<size_t>(i + n)]) == part) {
        ++n;
      }
      const double x0 = i * cell;
      const double x1 = (i + n) * cell;
      const double z0 = j * cell;
      const double z1 = (j + 1) * cell;
      float a[3], b[3], c[3], d[3];
      at(x0, z0, a);
      at(x0, z1, b);
      at(x1, z0, c);
      at(x1, z1, d);
      // Wound so each face's normal is up.
      render::add_triangle(m, a, b, c, static_cast<uint8_t>(part));
      render::add_triangle(m, c, b, d, static_cast<uint8_t>(part));
      i += n;
    }
  }
  return m;
}

std::vector<float> village_materials() {
  std::vector<float> out;
  out.reserve(size_t{kPaints + 1} * kVillageParts * 3);
  for (int set = 0; set <= kPaints; ++set) {
    // Set 0 is the streets' (its paint unused); set k + 1 wears paint k.
    const std::array<float, 3>& paint = kPaintRgb[static_cast<size_t>(set == 0 ? 0 : set - 1)];
    const std::array<const std::array<float, 3>*, kVillageParts> parts{
        &paint, &kGlassRgb, &kTyreRgb, &kTrimRgb, &kLampRgb, &kCarriageRgb, &kWalkRgb};
    for (const auto* rgb : parts) out.insert(out.end(), rgb->begin(), rgb->end());
  }
  return out;
}

std::vector<render::Surface> village_surfaces(const VillageCold& cold) {
  const std::vector<float> rgb = village_materials();
  std::vector<render::Surface> out;
  out.reserve(rgb.size() / 3);
  const auto frost = static_cast<float>(std::clamp(cold.frost, 0.0, 1.0));
  const auto glaze = static_cast<float>(std::clamp(cold.glaze, 0.0, 1.0));
  for (size_t i = 0; i + 2 < rgb.size(); i += 3) {
    const size_t part = (i / 3) % kVillageParts;
    render::Surface s = render::matte_surface(&rgb[i]);
    if (part == kCarriage && glaze > 0) {
      for (float& c : s.albedo) c *= 1 - (1 - kGlazeDark) * glaze;
      s.rough = 1 - (1 - kGlazeRough) * glaze;
      s.spec = kGlazeSpec * glaze;
    }
    // The frost lies over the glaze: hoar grows on black ice too.
    const float w = frost * kFrostReach[part];
    if (w > 0) {
      for (size_t c = 0; c < 3; ++c) s.albedo[c] += (kFrostRgb[c] - s.albedo[c]) * w;
      s.rough += (1 - s.rough) * w;
    }
    out.push_back(s);
  }
  return out;
}

VillageCold cold_steps(const VillageCold& cold) {
  const auto step = [](double v) {
    return core::num::round(std::clamp(v, 0.0, 1.0) / kColdStep) * kColdStep;
  };
  return {step(cold.frost), step(cold.glaze)};
}

uint32_t vehicle_material(int32_t id, vg::VehicleKind kind) {
  uint32_t paint = kPaints - 1;
  if (kind != vg::VehicleKind::bus) {
    auto h = static_cast<uint32_t>(id) * 0x9e3779b1u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    paint = h % (kPaints - 1);
  }
  return (paint + 1) * kVillageParts;
}

render::Instance vehicle_instance(const vg::VehiclePose& pose, int32_t id) {
  render::Instance in;
  in.mesh = static_cast<uint32_t>(pose.kind);
  in.material = vehicle_material(id, pose.kind);
  if (!pose.shown) {
    in.scale[0] = in.scale[1] = in.scale[2] = 0;
    return in;
  }
  in.position[0] = pose.x;
  in.position[1] = pose.y;
  in.position[2] = pose.z;
  const double half = pose.heading * 0.5;
  in.rotation[1] = static_cast<float>(std::sin(half));
  in.rotation[3] = static_cast<float>(std::cos(half));
  return in;
}

render::Instance parked_instance(const vg::Parked& car) {
  vg::VehiclePose pose;
  pose.shown = true;
  pose.kind = car.kind;
  pose.x = car.x;
  pose.y = car.y;
  pose.z = car.z;
  pose.heading = car.heading;
  // Dealt apart from the moving vehicles' ids.
  return vehicle_instance(pose, 1000 + car.id);
}

}  // namespace ge::mountain::app
