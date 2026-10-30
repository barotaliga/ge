// plugins/mountain/app/night_lamps.hpp: the masts stand every 50 m down the piste off its left
// edge, tall enough for its width, aimed across it and downhill and tipped down at least 15
// degrees; the street lamps stand along every street but the car park's aisles, off the
// carriageway, facing it; the lamps come on with the dark and light a recreational night piste
// to its class; every lit building spills its windows' light before its front; a lamp's shadow
// key is its base and its index; the posts' meshes stand on their foot with the glass lit in the
// second set.
//
// ge:summary Tests for the mountain's night lamps: the masts, the street lamps, their light
// ge:keywords test night-lamps piste_lamps street_lamps lamps_lit lamp_lights window_lights
// ge:keywords floodlight street-lamp lux
// ge:skill mountain testing
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "mountain/app/building_look.hpp"
#include "mountain/app/night_lamps.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/village/streets.hpp"
#include "render/lights.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace ma = ge::mountain::app;
namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace r = ge::render;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

double wrap(double a) {
  while (a > kPi) a -= 2 * kPi;
  while (a < -kPi) a += 2 * kPi;
  return a;
}

}  // namespace

TEST(masts_stand_down_the_piste_off_its_edge_aimed_across_and_downhill) {
  const mg::Level level = ge::skier::synthetic_slope();
  const std::vector<ma::Lamp> masts = ma::piste_lamps(level);
  CHECK(!masts.empty());
  CHECK(std::fabs(static_cast<double>(masts.size()) - level.track.length / ma::kMastEvery) < 2);
  for (size_t k = 0; k < masts.size(); ++k) {
    const ma::Lamp& m = masts[k];
    CHECK(m.kind == ma::LampKind::flood);
    // Off the piste: as far from its nearest centreline point as its half-width and the 3 m.
    double best = 1e18;
    const mg::TrackPoint* near = nullptr;
    for (const mg::TrackPoint& p : level.track.points) {
      const double d = std::hypot(p.x - m.x, p.z - m.z);
      if (d < best) {
        best = d;
        near = &p;
      }
    }
    CHECK(near != nullptr);
    if (near == nullptr) return;
    CHECK(best > near->width * 0.5 + ma::kMastOut * 0.5);
    CHECK(best < near->width * 0.5 + ma::kMastOut * 2);
    // On its left, aimed to its right and downhill of square, tipped down 15 degrees or more.
    const double rx = std::cos(near->heading), rz = -std::sin(near->heading);
    CHECK((m.x - near->x) * rx + (m.z - near->z) * rz < 0);
    const double off = wrap(m.heading - near->heading);
    CHECK(off > 0 && off < 90 * kDeg);
    CHECK(m.tilt >= ma::kMastMinTilt * kDeg - 1e-12);
    CHECK(m.height == (near->width > ma::kMastBroad ? ma::kMastHigh : ma::kMastLow));
    CHECK(std::fabs(m.y - level.ground_at(m.x, m.z)) < 1e-9);
    if (k > 0) {
      const double apart = std::hypot(m.x - masts[k - 1].x, m.z - masts[k - 1].z);
      CHECK(apart < 7 * m.height + 1);
    }
  }
}

TEST(the_lamps_come_on_with_the_dark_and_light_the_piste_to_its_class) {
  CHECK(ma::lamps_lit(10 * kDeg) == 0);
  CHECK(ma::lamps_lit(ma::kLightsOnElevation) == 1);
  CHECK(ma::lamps_lit(-20 * kDeg) == 1);
  const double half = ma::lamps_lit(ma::kLightsOnElevation + 1.5 * kDeg);
  CHECK(half > 0.2 && half < 0.8);

  const mg::Level level = ge::skier::synthetic_slope();
  const std::vector<ma::Lamp> masts = ma::piste_lamps(level);
  std::vector<r::LocalLight> lights;
  ma::lamp_lights(masts, 0, true, ma::kFloodKeys, lights);
  CHECK(lights.empty());
  ma::lamp_lights(masts, 1, true, ma::kFloodKeys, lights);
  CHECK(lights.size() == 2 * masts.size());
  for (size_t k = 0; k < lights.size(); ++k) {
    CHECK(lights[k].shadow == ma::kFloodKeys + k);  // a mast's near flood, then its throw
    CHECK(lights[k].outer_deg > lights[k].inner_deg);
  }
  // The light on the snow of the piste between the first and the last mast: a mean of 20 lx or
  // more and nowhere under 4 lx (a recreational night piste's class), on the face up.
  double sum = 0, least = 1e18;
  int n = 0;
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s < ma::kMastFirst + ma::kMastEvery || p.s > level.track.length - ma::kMastEvery) {
      continue;
    }
    for (const double across : {-0.35, 0.0, 0.35}) {
      const double at[3] = {p.x + std::cos(p.heading) * across * p.width,
                            level.ground_at(p.x, p.z) + 0.05,
                            p.z - std::sin(p.heading) * across * p.width};
      // A face up: each light's share by the cosine it falls at.
      double e = 0;
      for (const r::LocalLight& l : lights) {
        const double d[3] = {l.position[0] - at[0], l.position[1] - at[1], l.position[2] - at[2]};
        const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        e += r::lamp_lux_at(std::span(&l, 1), at, 0) * std::max(d[1] / len, 0.0);
      }
      sum += e;
      least = std::min(least, e);
      ++n;
    }
  }
  CHECK(n > 0);
  CHECK_MSG(sum / n >= 20, "mean " + std::to_string(sum / n) + " lx");
  CHECK_MSG(least >= 4, "least " + std::to_string(least) + " lx");
}

TEST(street_lamps_stand_along_the_streets_off_the_carriageway_facing_it) {
  mg::GenerateOptions opts;
  opts.version = 10;
  const mg::Level level = mg::generate_level(38, opts);
  const std::optional<vg::Streets> plan = vg::plan_streets(level);
  CHECK(plan.has_value());
  if (!plan) return;
  const std::vector<ma::Lamp> lamps = ma::street_lamps(level, *plan);
  double street_m = 0;
  for (const vg::Street& st : plan->streets) {
    if (st.kind != vg::StreetKind::aisle) street_m += st.length;
  }
  CHECK(!lamps.empty());
  CHECK(static_cast<double>(lamps.size()) > street_m / ma::kRoadEvery - 2);
  CHECK(static_cast<double>(lamps.size()) < street_m / ma::kStreetEvery + 2);
  for (const ma::Lamp& l : lamps) {
    CHECK(l.kind == ma::LampKind::street);
    CHECK(l.height == ma::kPostHeight);
    // Off every carriageway and aisle (a lane's width from each centreline point), shining down.
    for (const vg::Street& st : plan->streets) {
      for (const vg::StreetPoint& p : st.points) {
        CHECK_MSG(std::hypot(p.x - l.x, p.z - l.z) > st.section.lane * 0.99,
                  std::string(vg::street_kind_name(st.kind)) + " at " + std::to_string(p.s) +
                      " m: a lamp " + std::to_string(std::hypot(p.x - l.x, p.z - l.z)) + " m off");
      }
    }
    float dir[3];
    ma::lamp_direction(l, dir);
    CHECK(dir[1] < -0.9f);
  }
}

TEST(lit_buildings_spill_their_windows_light_before_their_front) {
  vg::Building b{};
  b.id = 7;
  b.x = 10;
  b.z = 20;
  b.y = 100;
  b.heading = 0.5 * kPi;  // facing +x
  b.width = 12;
  b.depth = 8;
  const vg::Building both[2] = {b, b};
  const uint8_t lit[2] = {1, 0};
  std::vector<r::LocalLight> out;
  ma::window_lights(both, lit, out);
  CHECK(out.size() == 1);
  CHECK(out[0].position[0] > b.x + b.depth * 0.5);
  CHECK(std::fabs(out[0].position[2] - b.z) < 1e-9);
  CHECK(out[0].shadow == 0);
  CHECK(out[0].outer_deg == 0);
}

TEST(the_posts_stand_on_their_foot_with_their_glass_lit_in_the_second_set) {
  const std::vector<r::MeshSource> meshes = ma::lamp_post_meshes();
  CHECK(meshes.size() == 3);
  const double tops[3] = {ma::kMastLow, ma::kMastHigh, ma::kPostHeight};
  for (size_t k = 0; k < meshes.size(); ++k) {
    const r::MeshData& m = meshes[k].lods.front().mesh;
    float lo = 1e9f, hi = -1e9f;
    for (size_t v = 1; v < m.positions.size(); v += 3) {
      lo = std::min(lo, m.positions[v]);
      hi = std::max(hi, m.positions[v]);
    }
    CHECK(std::fabs(lo) < 1e-6f);
    CHECK(std::fabs(hi - tops[k]) < 0.5);
    CHECK(std::count(m.parts.begin(), m.parts.end(), 1) > 0);
  }
  const std::vector<r::Surface> s = ma::lamp_post_surfaces();
  CHECK(s.size() == 4);
  CHECK(s[1].emissive_nits == 0);
  CHECK(s[3].emissive_nits > 0);
  ma::Lamp mast;
  mast.height = ma::kMastHigh;
  CHECK(ma::lamp_instance(mast, false).mesh == 1);
  CHECK(ma::lamp_instance(mast, true).material == 2);
}
