// engine/detail/surfaces.hpp and the floor clutter (props.hpp `clutter_rules`): a texture's class
// by its name's words and its colour; over the test yard, grass on the grassy floors and none on
// the metal deck or the timber, almost none of it under the roof; no stones on metal; clods only
// on earth; every prop on a floor its rule's classes allow; the clutter's shapes standing on
// y = 0 inside a unit; the same seed the same clutter; and the yard's clutter keeps its bits in
// tests/golden/detail_clutter.txt.
//
// ge:summary Test: surface classes by name and colour, floor clutter by class and open sky; golden
// ge:keywords test detail clutter surface-class texture-name colour grass stone outdoor indoor
// ge:keywords golden yard
// ge:skill detail testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "detail/prop_shapes.hpp"
#include "detail/props.hpp"
#include "detail/solid.hpp"
#include "detail/surfaces.hpp"
#include "detail/test_hall.hpp"
#include "detail_support.hpp"

namespace {

namespace gd = ge::detail;
using C = gd::SurfaceClass;

struct Yard {
  gd::TestYard yard = gd::test_yard();
  gd::TriMesh mesh;
  gd::Receivers rec;
  std::vector<C> classes;
  Yard() {
    for (const gd::Solid& s : yard.solids) gd::triangulate(s, gd::polygons_of(s), mesh);
    rec = gd::bin_receivers(mesh);
    for (const gd::YardTexture& t : yard.textures)
      classes.push_back(gd::classify_surface(t.name, t.rgb));
  }
};

const Yard& yard() {
  static const Yard y;
  return y;
}

std::vector<gd::Prop> scatter(uint32_t seed, gd::ScatterReport* report = nullptr) {
  gd::ScatterEnv env;
  env.damp = gd::yard_damp;
  env.ctx = &yard().yard;
  env.class_of_tag = yard().classes;
  const auto rules = gd::clutter_rules();
  return gd::scatter_props(yard().mesh, yard().rec, rules, seed, env, report);
}

ge::test::BitsHash hash_of(const std::vector<gd::Prop>& props) {
  ge::test::BitsHash h;
  for (const gd::Prop& p : props) {
    h.add(p.at);
    h.add(p.up);
    h.add(p.yaw_rad);
    h.add(p.size_m);
    h.add(static_cast<double>(p.variant) + 16 * static_cast<double>(p.kind));
  }
  return h;
}

// Props of `kind` standing on `tag`.
int count(const std::vector<gd::Prop>& props, gd::PropKind kind, int tag) {
  int n = 0;
  for (const gd::Prop& p : props) n += p.kind == kind && yard().mesh.tag[p.tri] == tag ? 1 : 0;
  return n;
}

}  // namespace

TEST(a_textures_name_and_colour_give_its_class) {
  CHECK(gd::classify_name("grass_lawn") == C::grass);
  CHECK(gd::classify_name("GRASS1") == C::grass);
  CHECK(gd::classify_name("mossy_rock") == C::stone);
  CHECK(gd::classify_name("metal_floor") == C::metal);
  CHECK(gd::classify_name("wood_plank") == C::wood);
  CHECK(gd::classify_name("ground_02") == C::dirt);
  CHECK(gd::classify_name("sky4") == C::sky);
  CHECK(gd::classify_name("lava_rock") == C::liquid);
  CHECK(gd::classify_name("wall_09") == C::unknown);
  const double green[3] = {0.2, 0.34, 0.12}, grey[3] = {0.4, 0.4, 0.41};
  CHECK(gd::classify_surface("ground_02", green) == C::grass);
  CHECK(gd::classify_surface("wall_09", green) == C::grass);
  CHECK(gd::classify_surface("wall_09", grey) == C::unknown);
  CHECK(gd::classify_surface("metal_green", green) == C::metal);  // no grass grows on metal
  CHECK(gd::classify_surface("rock", nullptr) == C::stone);
}

TEST(grass_grows_on_grass_under_the_sky) {
  const auto props = scatter(38);
  const int lawn = count(props, gd::PropKind::grass, 0);
  const int green = count(props, gd::PropKind::grass, 2);
  const int path = count(props, gd::PropKind::grass, 1);
  CHECK_MSG(lawn > 1000 && green > 500,
            "lawn " + std::to_string(lawn) + " green " + std::to_string(green));
  CHECK_MSG(path * 10 < lawn, "path " + std::to_string(path));  // tufts thin over bare earth
  for (int tag : {3, 4, 5, 6, 7}) CHECK(count(props, gd::PropKind::grass, tag) == 0);
  for (int tag : {5, 6}) {
    CHECK(count(props, gd::PropKind::twig, tag) == 0 &&
          count(props, gd::PropKind::branch, tag) == 0);
    CHECK(count(props, gd::PropKind::pebble, tag) == 0 &&
          count(props, gd::PropKind::rock, tag) == 0);
  }
  CHECK(count(props, gd::PropKind::shell, 5) > 0);
  CHECK(count(props, gd::PropKind::pebble, 3) > 50);  // stones on stone
  for (int tag : {0, 2, 3, 4, 5, 6, 7}) {
    CHECK(count(props, gd::PropKind::dirt, tag) == 0);
  }
}

TEST(clutter_stands_on_the_classes_its_rule_allows) {
  const auto rules = gd::clutter_rules();
  const auto props = scatter(7);
  CHECK(!props.empty());
  for (const gd::Prop& p : props) {
    const C cls = yard().classes[yard().mesh.tag[p.tri]];
    CHECK(rules[p.rule].classes & gd::class_bit(cls));
    CHECK(gd::normal_of(yard().mesh, p.tri).y > 0.8);
  }
}

TEST(the_same_seed_scatters_the_same_clutter) {
  const auto a = scatter(38), b = scatter(38), c = scatter(39);
  CHECK(a.size() == b.size() && hash_of(a).h == hash_of(b).h);
  CHECK(hash_of(a).h != hash_of(c).h);
}

TEST(the_clutters_shapes_stand_on_the_ground) {
  for (int k = static_cast<int>(gd::PropKind::pebble); k < gd::kPropKinds; ++k) {
    for (uint32_t var = 0; var < 4; ++var) {
      const gd::PropMesh m = gd::prop_mesh(static_cast<gd::PropKind>(k), var);
      CHECK(!m.pos.empty() && m.pos.size() % 3 == 0 && m.nrm.size() == m.pos.size());
      double lo = 1e9, hi = -1e9;
      for (const gd::Vec3& v : m.pos) {
        lo = std::fmin(lo, v.y);
        hi = std::fmax(hi, v.y);
        CHECK_MSG(std::fabs(v.x) <= 0.8 && std::fabs(v.z) <= 0.8,
                  "kind " + std::to_string(k) + " reaches " + std::to_string(v.x) + ", " +
                      std::to_string(v.z));
      }
      CHECK_MSG(std::fabs(lo) < 1e-9 && hi > 0.005, "kind " + std::to_string(k) + " spans " +
                                                        std::to_string(lo) + " to " +
                                                        std::to_string(hi));
      for (const gd::Vec3& n : m.nrm) CHECK(std::fabs(gd::length(n) - 1) < 1e-9);
    }
  }
}

TEST(the_yards_clutter_keeps_its_bits) {
  std::vector<std::vector<uint64_t>> got;
  for (uint32_t seed : {38u, 7u}) {
    gd::ScatterReport rep;
    const auto props = scatter(seed, &rep);
    std::vector<uint64_t> row{seed};
    for (int k = 0; k < gd::kPropKinds; ++k) row.push_back(rep.placed[k]);
    row.push_back(hash_of(props).h);
    got.push_back(row);
  }
  ge::test::BitsHash shapes;
  for (int k = static_cast<int>(gd::PropKind::pebble); k < gd::kPropKinds; ++k) {
    for (const gd::Vec3& v : gd::prop_mesh(static_cast<gd::PropKind>(k), 1).pos) shapes.add(v);
  }
  got.push_back({shapes.h});
  ge::test::check_golden(
      "tests/golden/detail_clutter.txt", got,
      "per seed: the props placed by kind, the clutter's hash; then the "
      "clutter's shapes' hash (detail_clutter_test)",
      "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
      "detail-clutter lab's pictures before and after in the PR (skill: detail)");
}
