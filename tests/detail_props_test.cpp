// engine/detail/props.hpp and prop_shapes.hpp: the same rules and seed scatter the same props; a
// different seed scatters others; rubble gathers at walls' feet, moss in the damp; no two props
// of a rule stand closer than its spacing; every prop stands on a surface its rule allows; the
// shapes are closed outward-wound solids standing on y = 0; and the hall's props keep their bits in
// tests/golden/detail_props.txt.
//
// ge:summary Test: seeded props by rules: determinism, walls' feet, damp, spacing, shapes; golden
// ge:keywords test detail props scatter rubble moss debris spacing damp shapes golden
// ge:skill detail testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "detail/prop_shapes.hpp"
#include "detail/props.hpp"
#include "detail/solid.hpp"
#include "detail/test_hall.hpp"
#include "detail_support.hpp"

namespace {

namespace gd = ge::detail;

// The kinds the default rules place; the floor clutter's are tests/detail_clutter_test.cpp's.
constexpr int kHallKinds = 3;

struct Hall {
  gd::TestHall hall = gd::test_hall();
  gd::TriMesh mesh;
  gd::Receivers rec;
  Hall() {
    for (const gd::Solid& s : hall.solids) gd::triangulate(s, gd::polygons_of(s), mesh);
    rec = gd::bin_receivers(mesh);
  }
};

const Hall& hall() {
  static const Hall h;
  return h;
}

std::vector<gd::Prop> scatter(uint32_t seed, gd::ScatterReport* report = nullptr) {
  const auto rules = gd::default_rules();
  return gd::scatter_props(hall().mesh, hall().rec, rules, seed, gd::hall_damp, &hall().hall,
                           report);
}

ge::test::BitsHash hash_of(const std::vector<gd::Prop>& props) {
  ge::test::BitsHash h;
  for (const gd::Prop& p : props) {
    h.add(p.at);
    h.add(p.up);
    h.add(p.yaw_rad);
    h.add(p.size_m);
    h.add(static_cast<double>(p.variant));
  }
  return h;
}

}  // namespace

TEST(the_same_seed_scatters_the_same_props) {
  const auto a = scatter(38), b = scatter(38), c = scatter(39);
  CHECK(!a.empty() && a.size() == b.size() && hash_of(a).h == hash_of(b).h);
  CHECK(hash_of(a).h != hash_of(c).h);
}

TEST(rubble_gathers_at_walls_feet_and_moss_in_the_damp) {
  gd::ScatterReport rep;
  const auto props = scatter(38, &rep);
  int rubble_near = 0, rubble_open = 0, moss_damp = 0, moss_dry = 0;
  for (const gd::Prop& p : props) {
    // The open middle of the hall's floor, away from every wall, pillar, step and the basin.
    const bool open = p.at.x > -1 && p.at.x < 2 && p.at.z > 1.5 && p.at.z < 4.5;
    const bool near_wall = p.at.z > 9.6 || p.at.x < -5.6 || p.at.x > 5.6;
    if (p.kind == gd::PropKind::rubble) {
      rubble_near += near_wall ? 1 : 0;
      rubble_open += open ? 1 : 0;
    }
    if (p.kind == gd::PropKind::moss) {
      (gd::hall_damp(&hall().hall, p.at) > 0.3 ? moss_damp : moss_dry) += 1;
    }
  }
  CHECK_MSG(rubble_near > 10 * (rubble_open + 1),
            "rubble near " + std::to_string(rubble_near) + " open " + std::to_string(rubble_open));
  CHECK_MSG(moss_damp > 3 * moss_dry,
            "moss damp " + std::to_string(moss_damp) + " dry " + std::to_string(moss_dry));
  for (int k = 0; k < kHallKinds; ++k) {
    CHECK(rep.placed[k] > 0 && rep.placed[k] <= rep.tried[k]);
  }
}

TEST(props_keep_their_spacing_and_their_surfaces) {
  const auto rules = gd::default_rules();
  const auto props = scatter(7);
  for (size_t i = 0; i < props.size(); ++i) {
    const gd::Prop& p = props[i];
    const gd::Vec3 n = gd::normal_of(hall().mesh, p.tri);
    CHECK(gd::dot(n, p.up) > 0.999999);
    if (rules[p.rule].place == gd::PropPlace::floor) CHECK(n.y > 0.8);
    if (rules[p.rule].place == gd::PropPlace::wall) CHECK(std::fabs(n.y) < 0.36);
  }
  // Spacing is measured between the points sampled on the surface, before the sink.
  for (size_t i = 0; i < props.size(); ++i) {
    for (size_t j = i + 1; j < props.size(); ++j) {
      if (props[i].rule != props[j].rule) continue;
      const gd::PropRule& r = rules[props[i].rule];
      const gd::Vec3 a = gd::mad(props[i].at, props[i].up, props[i].size_m * r.sink_share);
      const gd::Vec3 b = gd::mad(props[j].at, props[j].up, props[j].size_m * r.sink_share);
      CHECK(gd::length(gd::sub(a, b)) >= r.spacing_m - 1e-9);
    }
  }
}

TEST(the_shapes_are_closed_and_stand_on_the_ground) {
  for (int k = 0; k < kHallKinds; ++k) {
    for (uint32_t var = 0; var < 4; ++var) {
      const gd::PropMesh m = gd::prop_mesh(static_cast<gd::PropKind>(k), var);
      CHECK(!m.pos.empty() && m.pos.size() % 3 == 0 && m.nrm.size() == m.pos.size());
      double lo = 1e9, hi = -1e9;
      gd::Vec3 c{0, 0, 0};
      for (const gd::Vec3& v : m.pos) {
        lo = std::fmin(lo, v.y);
        hi = std::fmax(hi, v.y);
        c = gd::add(c, v);
        CHECK(std::fabs(v.x) <= 0.8 && std::fabs(v.z) <= 0.8);
      }
      CHECK(std::fabs(lo) < 1e-12 && hi > 0.02);
      c = gd::mul(c, 1.0 / static_cast<double>(m.pos.size()));
      // Outward: the faces' signed volume about the centre is positive, and its area faces out.
      double volume = 0;
      for (size_t i = 0; i < m.pos.size(); i += 3) {
        const gd::Vec3 a = gd::sub(m.pos[i], c), b = gd::sub(m.pos[i + 1], c),
                       d = gd::sub(m.pos[i + 2], c);
        volume += gd::dot(a, gd::cross(b, d)) / 6;
      }
      CHECK_MSG(volume > 0, "kind " + std::to_string(k) + " variant " + std::to_string(var) +
                                " volume " + std::to_string(volume));
    }
  }
}

TEST(the_halls_props_keep_their_bits) {
  std::vector<std::vector<uint64_t>> got;
  for (uint32_t seed : {38u, 7u}) {
    gd::ScatterReport rep;
    const auto props = scatter(seed, &rep);
    got.push_back({seed, rep.placed[0], rep.placed[1], rep.placed[2], hash_of(props).h});
  }
  ge::test::BitsHash shapes;
  for (int k = 0; k < kHallKinds; ++k) {
    for (const gd::Vec3& v : gd::prop_mesh(static_cast<gd::PropKind>(k), 1).pos) shapes.add(v);
  }
  got.push_back({shapes.h});
  ge::test::check_golden("tests/golden/detail_props.txt", got,
                         "per seed: rubble debris moss placed, the props' hash; then the shapes' "
                         "hash (detail_props_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
                         "detail-props lab's pictures before and after in the PR (skill: detail)");
}
