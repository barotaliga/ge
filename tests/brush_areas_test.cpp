// Version 38 areas: a map's areas flooded over its open portals, the leaf's area read from the
// file, and the answers outside the data.
//
// ge:summary Test: areas flood over open portals only; a shut portal splits; area 0 is alone
// ge:keywords test brush area area-portal flood_areas areas_connected area_at v38 second_view_leaf
// ge:keywords water-surface second-pvs-point
// ge:skill brush
#include <string>
#include <vector>

#include "brush/areas.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

// Areas 1, 2 and 3 in a row: portal 1 between 1 and 2, portal 2 between 2 and 3, each listed from
// both sides (as a compiler writes them), and the unused area 0.
br::Map row_of_three() {
  br::Map m;
  m.area_portals = {{0, 0}, {1, 2}, {1, 1}, {2, 3}, {2, 2}};
  m.areas = {{0, 0}, {1, 1}, {2, 2}, {1, 4}};
  return m;
}

TEST(areas_join_only_through_open_portals) {
  const br::Map m = row_of_three();
  std::vector<int32_t> g;
  std::vector<uint8_t> open(3, 0);
  br::flood_areas(m, open, &g);
  CHECK(g.size() == 4);
  CHECK(!br::areas_connected(g, 1, 2) && !br::areas_connected(g, 2, 3));
  CHECK(br::areas_connected(g, 2, 2));
  open[1] = 1;  // the first door opens
  br::flood_areas(m, open, &g);
  CHECK(br::areas_connected(g, 1, 2) && !br::areas_connected(g, 1, 3));
  open[2] = 1;  // and the second
  br::flood_areas(m, open, &g);
  CHECK(br::areas_connected(g, 1, 3));
  open[1] = 0;  // the first shuts again: 1 alone, 2 and 3 joined
  br::flood_areas(m, open, &g);
  CHECK(!br::areas_connected(g, 1, 3) && br::areas_connected(g, 2, 3));
  // Groups in order of each group's lowest area; area 0 is never flooded and joins nothing else.
  CHECK(g[0] == 0 && g[1] == 1 && g[2] == 2 && g[3] == 2);
  CHECK(!br::areas_connected(g, 0, 1) && br::areas_connected(g, 0, 0));
  // Out of range: joined to everything (a map without area data is one space).
  CHECK(br::areas_connected(g, 9, 1) && br::areas_connected({}, 1, 2));
}

TEST(a_v38_map_reads_its_leaves_areas) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v38, &bytes, &error), error);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &error), error);
  CHECK(m.areas.size() == 2 && m.area_portals.empty());
  CHECK(m.leaves[0].area == 0);  // the solid outside
  for (size_t i = 1; i < m.leaves.size(); i++) CHECK(m.leaves[i].area == 1);
  // Another family has none: every point in area 0, everything joined.
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error), error);
  br::Map q1;
  CHECK_MSG(br::read_bsp(bytes, &q1, &error), error);
  const float p[3] = {0, 0, 0};
  CHECK(q1.areas.empty() && br::area_at(q1, p) == 0);
}

// The potentially visible set with the frame's areas: a leaf in an area whose bit is unset (or in
// solid) marks none of its surfaces, with or without visibility; no bits, every leaf as before.
TEST(the_visible_set_leaves_out_unshown_areas) {
  br::Map m;
  m.surfaces.resize(3);
  m.marks = {0, 1, 2};
  br::Leaf solid, a1, a2;
  solid.solid = true;
  solid.area = 1;
  solid.first_mark = 0;
  solid.marks = 1;
  a1.area = 1;
  a1.first_mark = 1;
  a1.marks = 1;
  a2.area = 2;
  a2.first_mark = 2;
  a2.marks = 1;
  m.leaves = {solid, a1, a2};
  std::vector<uint8_t> seen;
  CHECK(br::potentially_visible(m, 1, &seen) == 3);  // no visibility, no areas: everything
  const uint8_t only_1[1] = {0x02}, both[1] = {0x06};
  CHECK(br::potentially_visible(m, 1, &seen, only_1) == 1 && seen[1] == 1 && seen[2] == 0);
  CHECK(br::potentially_visible(m, 1, &seen, both) == 2 && seen[0] == 0);  // solid draws none
  const uint8_t none[1] = {0};
  CHECK(br::potentially_visible(m, 1, &seen, none) == 0);
}

// The second view point (38): an eye just over a water surface also sees what the water's cluster
// sees, from 16 units below; in the water, from 16 units above; another family, the eye alone.
TEST(the_view_sees_from_a_second_point_across_water) {
  br::Map m;
  m.family = br::Family::v38;
  m.planes = {br::Plane{{0, 0, 1}, 0, 2}};  // the water's surface at z 0
  m.nodes = {br::Node{0, {-2, -3}}};        // above: leaf 1, below: leaf 2
  m.models.resize(1);
  m.surfaces.resize(3);
  m.marks = {0, 1, 2};
  br::Leaf solid, air, water;
  solid.solid = true;
  air.cluster = 0;
  air.first_mark = 1;
  air.marks = 1;
  water.cluster = 1;
  water.contents = 32;  // water
  water.first_mark = 2;
  water.marks = 1;
  m.leaves = {solid, air, water};
  m.clusters = 2;
  m.row_bytes = 1;
  m.pvs = {0x01, 0x02};  // each cluster sees itself alone
  const float near_over[3] = {0, 0, 8}, high[3] = {0, 0, 40}, near_under[3] = {0, 0, -8};
  CHECK(br::second_view_leaf(m, near_over, 1) == 2);
  CHECK(br::second_view_leaf(m, high, 1) == ~0u);  // 16 below is still the air
  CHECK(br::second_view_leaf(m, near_under, 2) == 1);
  std::vector<uint8_t> seen;
  CHECK(br::potentially_visible(m, 1, &seen) == 1 && seen[2] == 0);
  CHECK(br::potentially_visible(m, 1, &seen, {}, br::second_view_leaf(m, near_over, 1)) == 2 &&
        seen[1] == 1 && seen[2] == 1);
  m.family = br::Family::v29;
  CHECK(br::second_view_leaf(m, near_over, 1) == ~0u);
}

}  // namespace
