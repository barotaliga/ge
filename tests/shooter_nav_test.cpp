// NAVIGATION PROVEN BY THE MOVE: the nav graph on the nav course (games/shooters_edge/sim/
// nav_course.hpp) has a node on every floor, reaches the crate only by a jump and across the pit
// only by a jump on the move, leaves the pit only up its steps, never reaches the ledge past the
// steep ramp; a route's links prove again when replayed; the test arena's doorway is tagged with
// the door; and the graph's bits are held by tests/golden/shooter_nav.txt, which this test writes
// itself under GE_GOLDEN_WRITE=1 (a deliberate change to the move or the graph rewrites it, and
// the PR says why).
//
// ge:summary Tests: the nav graph's nodes, link kinds, reach, routes, mover tags and bits
// ge:keywords test nav nav-graph prove_link build_nav nav_route nav_nearest nav_reach golden
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/sim/arena.hpp"
#include "shooters_edge/sim/nav_course.hpp"

namespace sh = ge::shooter;
namespace se = shooters_edge::sim;

namespace {

constexpr const char* kGolden = "tests/golden/shooter_nav.txt";

struct Course {
  se::NavCourse map;
  sh::MoveWorld world;
  sh::NavGraph graph;
  Course() {
    se::build_nav_course(map);
    sh::map_world(map.world.model(), world);
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, graph);
  }
};

const Course& course() {
  static const Course c;
  return c;
}

const sh::NavNode& node(const sh::NavGraph& g, int32_t n) {
  return g.nodes[static_cast<size_t>(n)];
}

std::span<const sh::NavLink> links_of(const sh::NavGraph& g, int32_t n) {
  const sh::NavNode& a = node(g, n);
  return {g.links.data() + a.first_link, static_cast<size_t>(a.links)};
}

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

}  // namespace

TEST(every_floor_of_the_course_has_a_node) {
  const Course& c = course();
  const sh::Vec3f spots[] = {c.map.platform,   c.map.crate_top,  c.map.pit_floor,
                             c.map.east_floor, c.map.east_ledge, c.map.steep_top};
  for (const sh::Vec3f& p : spots) {
    const int32_t n = sh::nav_nearest(c.graph, p);
    CHECK_MSG(n >= 0,
              "no node near a standing spot of the course: read column_floors in "
              "plugins/shooter/nav.cpp");
    if (n < 0) continue;
    const float dz = node(c.graph, n).origin.z - p.z;
    CHECK_MSG(dz >= 0 && dz <= 0.05f, "a node stands off its floor by " + std::to_string(dz) +
                                          " (expected the 1/32 the trace leaves)");
  }
}

TEST(the_crate_is_reached_only_by_a_jump) {
  const Course& c = course();
  const sh::NavGraph& g = c.graph;
  int into = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const bool from_floor = g.nodes[a].origin.z < 30;
    for (const sh::NavLink& l : links_of(g, static_cast<int32_t>(a))) {
      if (!from_floor || node(g, l.to).origin.z < 59) continue;
      if (node(g, l.to).origin.z > 61) continue;  // the crate's top is at z 60
      into++;
      CHECK_MSG(l.kind == sh::NavKind::jump,
                std::string("a floor node reaches the crate by ") + sh::nav_kind_name(l.kind));
    }
  }
  CHECK_MSG(into > 0, "nothing jumps onto the 36-unit crate: a jump's apex is 45.6 units");
}

TEST(the_pit_is_crossed_by_a_jump_and_left_up_its_steps) {
  const Course& c = course();
  const sh::NavGraph& g = c.graph;
  int across = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& na = g.nodes[a];
    for (const sh::NavLink& l : links_of(g, static_cast<int32_t>(a))) {
      const sh::NavNode& nb = node(g, l.to);
      // The columns on the rims stand at x 0 and 96, their boxes over the edge.
      const bool west_to_east = na.origin.x <= 0 && nb.origin.x >= 96;
      const bool east_to_west = na.origin.x >= 96 && nb.origin.x <= 0;
      const bool floors = na.origin.z > 0 && nb.origin.z > 0;
      if ((west_to_east || east_to_west) && floors) {
        across++;
        CHECK_MSG(l.kind == sh::NavKind::jump, std::string("the pit is crossed by a ") +
                                                   sh::nav_kind_name(l.kind) +
                                                   ": only a jump on the move clears 96 units");
      }
      // Out of the pit (z -72) onto a floor: only off its top steps.
      if (na.origin.z < -40 && nb.origin.z > 0)
        CHECK_MSG(na.origin.y > 96, "a link climbs out of the pit away from its steps");
    }
  }
  CHECK_MSG(across > 0, "no jump crosses the 96-unit pit");
  sh::NavRoute r;
  ge::ai::Search search;
  r = sh::nav_route(g, sh::nav_nearest(g, c.map.pit_floor), sh::nav_nearest(g, c.map.east_floor),
                    search);
  CHECK_MSG(!r.nodes.empty(), "no way out of the pit to the east floor");
}

TEST(the_ledge_past_the_steep_ramp_is_out_of_reach) {
  const Course& c = course();
  const sh::NavGraph& g = c.graph;
  const int32_t top = sh::nav_nearest(g, c.map.steep_top);
  const int32_t start = sh::nav_nearest(g, c.map.platform);
  CHECK(top >= 0 && start >= 0);
  if (top < 0 || start < 0) return;
  // A drop off it is allowed; no link climbs onto it.
  for (size_t a = 0; a < g.nodes.size(); a++) {
    if (g.nodes[a].origin.z > 150) continue;
    for (const sh::NavLink& l : links_of(g, static_cast<int32_t>(a)))
      CHECK_MSG(node(g, l.to).origin.z < 150,
                "a link climbs onto the ledge past the 51 degree ramp, which slides a player off");
  }
  ge::ai::Search search;
  CHECK(sh::nav_route(g, start, top, search).nodes.empty());
  const int32_t reached = sh::nav_reach(g, start, search);
  CHECK(!search.reached(top));
  CHECK_MSG(reached > static_cast<int32_t>(g.nodes.size()) * 9 / 10,
            "less than nine tenths of the course is reached from the west platform");
}

TEST(a_route_proves_again_link_by_link) {
  const Course& c = course();
  const sh::NavGraph& g = c.graph;
  ge::ai::Search search;
  const sh::NavRoute r = sh::nav_route(g, sh::nav_nearest(g, c.map.platform),
                                       sh::nav_nearest(g, c.map.east_ledge), search);
  CHECK_MSG(r.nodes.size() > 2, "no route from the west platform to the east ledge");
  bool ran = false;
  for (size_t i = 0; i + 1 < r.nodes.size(); i++) {
    for (const sh::NavLink& l : links_of(g, r.nodes[i])) {
      if (l.to != r.nodes[i + 1]) continue;
      const sh::NavProof p = sh::prove_link(c.world, sh::MoveSettings{}, node(g, r.nodes[i]).origin,
                                            node(g, l.to).origin, l.kind, l.speed, g.set);
      CHECK_MSG(p.reached && p.time_s == l.time_s,
                "a route's link does not prove again to the same time: the proof reads state "
                "from outside its arguments (load the determinism skill)");
      if (l.kind == sh::NavKind::jump) ran = true;
      break;
    }
  }
  CHECK_MSG(ran, "the quickest way east does not jump the pit");
}

TEST(the_arena_doorway_is_tagged_with_its_door) {
  se::Arena arena;
  se::build_arena(arena);
  sh::MoveWorld world;
  sh::map_world(arena.world.model(), world);
  const int kDoorEnt = 40;
  sh::NavMover door{kDoorEnt, arena.door.mins, arena.door.maxs};
  sh::NavGraph g;
  sh::build_nav(world, std::span<const sh::NavMover>(&door, 1), sh::MoveSettings{},
                sh::NavSettings{}, g);
  ge::ai::Search search;
  const sh::NavRoute r = sh::nav_route(g, sh::nav_nearest(g, arena.spawn[0]),
                                       sh::nav_nearest(g, {400, 0, 24}), search);
  CHECK_MSG(!r.nodes.empty(), "no way from the west room to the east room through the doorway");
  int tagged = 0;
  for (size_t i = 0; i + 1 < r.nodes.size(); i++)
    for (const sh::NavLink& l : links_of(g, r.nodes[i]))
      if (l.to == r.nodes[i + 1] && l.mover == kDoorEnt) tagged++;
  CHECK_MSG(tagged > 0, "the route through the doorway crosses no link tagged with the door");
}

TEST(the_graph_matches_its_own_bits) {
  const Course& c = course();
  sh::NavGraph again;
  sh::build_nav(c.world, {}, sh::MoveSettings{}, sh::NavSettings{}, again);
  const uint64_t h = sh::nav_hash(c.graph);
  CHECK_MSG(h == sh::nav_hash(again),
            "two builds of the nav course differ: the build reads "
            "state from outside its arguments (load determinism)");
  const unsigned long long row[4] = {c.graph.nodes.size(), c.graph.links.size(),
                                     static_cast<unsigned long long>(c.graph.counts.proofs), h};
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/nav.hpp on the nav course: nodes, links, proofs, nav_hash "
           "(tests/shooter_nav_test.cpp)\n";
    char line[96];
    std::snprintf(line, sizeof line, "%llx %llx %llx %016llx\n", row[0], row[1], row[2], row[3]);
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 4,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() != 1 || rows[0].size() != 4) return;
  for (int i = 0; i < 4; i++)
    CHECK_MSG(rows[0][static_cast<size_t>(i)] == row[i],
              "the nav course's graph moved off its golden bits: if the move or the graph changed "
              "on purpose, rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
}
