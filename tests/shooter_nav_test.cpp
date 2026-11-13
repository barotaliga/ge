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
#include "shooter/brush_hull.hpp"
#include "shooter/nav.hpp"
#include "shooter/nav_script.hpp"
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

TEST(stairs_too_steep_to_walk_and_too_short_to_run_up_are_hopped) {
  // A floor, then three steps 30 high and 32 deep (no tread to run up on), then a landing: each
  // step is a standing jump against its riser, landing on the tread short of its node and walked.
  const auto box = [](sh::Vec3f lo, sh::Vec3f hi) {
    sh::Brush b;
    b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
                {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
    return b;
  };
  const std::vector<sh::Brush> brushes = {
      box({-256, -128, -64}, {0, 128, 0}), box({0, -128, -64}, {32, 128, 30}),
      box({32, -128, -64}, {64, 128, 60}), box({64, -128, -64}, {96, 128, 90}),
      box({96, -128, -64}, {256, 128, 90})};
  const sh::HullData point = sh::build_point_hull(brushes);
  const sh::HullData clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
  sh::MoveWorld world;
  world.solids[0].point_hull = point.hull();
  world.solids[0].clip_hull = clip.hull();
  world.solids[0].mins = {-256, -128, -64};
  world.solids[0].maxs = {256, 128, 256};
  world.count = 1;
  sh::NavGraph g;
  sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
  const int32_t from = sh::nav_nearest(g, {-112, 0, 24});
  const int32_t to = sh::nav_nearest(g, {176, 0, 114});
  CHECK(from >= 0 && to >= 0);
  ge::ai::Search search;
  const sh::NavRoute r = sh::nav_route(g, from, to, search);
  CHECK_MSG(!r.nodes.empty(),
            "no way up stairs a person hops up: read the standing jump in nav.cpp");
  int standing = 0;
  for (size_t k = 0; k + 1 < r.nodes.size(); k++)
    for (const sh::NavLink& l : links_of(g, r.nodes[k]))
      if (l.to == r.nodes[k + 1] && l.kind == sh::NavKind::jump && l.speed == 0) standing++;
  CHECK_MSG(standing >= 1, "the way up took " + std::to_string(standing) +
                               " standing jumps: the treads leave no room to run up");
}

// Stairs with treads 16 deep (half the grid), as e1m2's yellow armour has: a floor, steps of 28
// and 32, a landing at 88. The grid's columns miss the first tread.
struct NarrowStairs {
  sh::HullData point, clip;
  sh::MoveWorld world;
  NarrowStairs() {
    const auto box = [](sh::Vec3f lo, sh::Vec3f hi) {
      sh::Brush b;
      b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
                  {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
      return b;
    };
    const std::vector<sh::Brush> brushes = {
        box({-256, -128, -64}, {0, 128, 0}), box({0, -128, -64}, {16, 128, 28}),
        box({16, -128, -64}, {32, 128, 60}), box({32, -128, -64}, {256, 128, 88})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-256, -128, -64};
    world.solids[0].maxs = {256, 128, 256};
    world.count = 1;
  }
};

TEST(a_tread_between_two_columns_gets_a_node_only_when_asked) {
  const NarrowStairs st;
  sh::NavGraph g;
  sh::build_nav(st.world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
  const auto on_tread = [](const sh::NavGraph& gr) {
    int n = 0;
    for (const sh::NavNode& nd : gr.nodes) n += nd.origin.z > 51 && nd.origin.z < 53 ? 1 : 0;
    return n;
  };
  CHECK_MSG(on_tread(g) == 0,
            "the grid found the first tread by itself: the fixture no longer "
            "has a tread narrower than a column; read NarrowStairs");
  sh::NavSettings with;
  with.treads = true;
  sh::build_nav(st.world, {}, sh::MoveSettings{}, with, g);
  CHECK_MSG(on_tread(g) > 0 && g.counts.treads > 0,
            "treads on, still no node on the 16-deep tread at feet 28: read the TREADS pass in "
            "nav.cpp's build_nav");
}

TEST(a_scripted_way_hops_up_narrow_stairs_in_one_flow) {
  const NarrowStairs st;
  sh::NavScript sc;
  sc.steps = {{{-64, 0, 24}, false},
              {{8, 0, 52}, true},
              {{24, 0, 84}, true},
              {{48, 0, 112}, true},
              {{96, 0, 112}, false}};
  const sh::NavProof pr =
      sh::prove_script(st.world, sh::MoveSettings{}, {-160, 0, 24}, sc, sh::NavSettings{});
  CHECK_MSG(pr.reached, "the script up the stairs did not hold (ended at " +
                            std::to_string(pr.end.x) + " " + std::to_string(pr.end.z) + " after " +
                            std::to_string(pr.time_s) +
                            " s): read script_keys in shooter/nav_script.cpp");
  // A run of 160 from standing, then three hops of about half a second each (each lands against
  // the next riser and drops onto its tread): about 2.2 s, where a standing jump costs a second.
  CHECK_MSG(pr.time_s < 2.5f, "the script took " + std::to_string(pr.time_s) +
                                  " s: a run and three hops should take under 2.5");
  // The same keys from the same place, step by step, end where the proof did: one rule for both.
  size_t i = 0;
  const sh::ScriptKeys k0 = sh::script_keys(sc, i, {-160, 0, 24}, {}, true, 8);
  CHECK(!k0.done && i == 0 && !k0.jump && k0.forward == 1);
  // Carried onto the entry from the side at a hop's speed: braked there, not hopped on.
  i = 0;
  const sh::ScriptKeys k1 = sh::script_keys(sc, i, {-64, 0, 24}, {0, 380, 0}, true, 8);
  CHECK_MSG(i == 0 && k1.forward == 0 && !k1.jump,
            "a player at the entry moving 380 across the script was let on: read lined_up in "
            "shooter/nav_script.cpp");
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

TEST(a_shut_link_with_no_mover_is_kept_off_the_route_a_cut_one_is_not) {
  // A bridge the nav holds as a solid carries no mover: its links shut while it is in are kept off
  // only as `shut` (`bridgeseen`); `cut` is read on links through a mover and gap tricks.
  const Course& c = course();
  const sh::NavGraph& g = c.graph;
  ge::ai::Search search;
  const int32_t from = sh::nav_nearest(g, c.map.platform), to = sh::nav_nearest(g, c.map.east_ledge);
  const sh::NavRoute r = sh::nav_route(g, from, to, search);
  CHECK_MSG(r.nodes.size() > 2, "no route from the west platform to the east ledge");
  int32_t a = -1, b = -1;
  for (size_t i = 0; i + 1 < r.nodes.size() && a < 0; i++)
    for (const sh::NavLink& l : links_of(g, r.nodes[i]))
      if (l.to == r.nodes[i + 1] && l.mover < 0 && !l.gap) a = r.nodes[i], b = l.to;
  CHECK_MSG(a >= 0, "the route has no plain link to cut");
  const sh::NavExtraLink cut[1] = {{a, b, sh::NavKind::walk, 0}};
  auto uses = [&](const sh::NavRoute& x) {
    for (size_t i = 0; i + 1 < x.nodes.size(); i++)
      if (x.nodes[i] == a && x.nodes[i + 1] == b) return true;
    return false;
  };
  CHECK_MSG(uses(sh::nav_route(g, from, to, search, 1, {}, cut)),
            "a cut plain link is kept off the route: the pilot's failed jumps now cut (they halved "
            "dm2's jumps): read nav_route's cut filter");
  CHECK_MSG(!uses(sh::nav_route(g, from, to, search, 1, {}, {}, {}, cut)),
            "a shut plain link is still taken: nav_route's shut filter is wrong");
}

TEST(the_arena_doorway_is_tagged_with_its_door) {
  se::Arena arena;
  se::build_arena(arena);
  sh::MoveWorld world;
  sh::map_world(arena.world.model(), world);
  const int kDoorEnt = 40;
  sh::NavMover door;
  door.ent = kDoorEnt;
  door.absmin = arena.door.mins;
  door.absmax = arena.door.maxs;
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

namespace {

// A floor (its top at z 0) and a block 160 high beside it, too high for any jump (the apex is 46)
// and in reach of a rocket jump's push (about 300 up): the block's top is a pocket a drop leaves.
struct Block {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  Block() {
    const auto box = [](sh::Vec3f lo, sh::Vec3f hi) {
      sh::Brush b;
      b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
                  {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
      return b;
    };
    const std::vector<sh::Brush> brushes = {box({-384, -128, -64}, {384, 128, 0}),
                                            box({96, -128, 0}, {384, 128, 160})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-384, -128, -64};
    world.solids[0].maxs = {384, 128, 512};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
  }
};

}  // namespace

TEST(a_rocket_jump_climbs_what_no_jump_does) {
  const Block b;
  const sh::Vec3f from{-64, 0, 24}, to{160, 0, 184};
  const sh::NavProof jump =
      sh::prove_link(b.world, sh::MoveSettings{}, from, to, sh::NavKind::jump, 320, b.g.set);
  CHECK_MSG(!jump.reached, "a jump climbed 160 units: the apex is 46");
  float health = 0;
  const sh::NavProof rj =
      sh::prove_rocket_jump(b.world, sh::MoveSettings{}, from, to, 320, b.g.set, {}, &health);
  CHECK_MSG(rj.end.z > 160 + 20,
            "the rocket jump did not land on the block: ended at z " + std::to_string(rj.end.z));
  CHECK_MSG(health > 30 && health < 60, "the blast took " + std::to_string(health) +
                                            " of the jumper's health: half of 120 less "
                                            "half the distance");
}

TEST(a_pocket_only_a_rocket_jump_reaches_is_bridged_by_one) {
  Block b;
  const int32_t floor = sh::nav_nearest(b.g, {-256, 0, 24});
  const int32_t top = sh::nav_nearest(b.g, {320, 0, 184});
  CHECK(floor >= 0 && top >= 0);
  if (floor < 0 || top < 0) return;
  ge::ai::Search search;
  sh::nav_reach(b.g, floor, search);
  CHECK_MSG(!search.reached(top), "the block's top is reached before the bridge: no pocket");
  std::vector<uint8_t> main(b.g.nodes.size());
  for (size_t n = 0; n < main.size(); n++) main[n] = search.reached(static_cast<int32_t>(n));
  const sh::NavBridgeCounts c = sh::nav_bridge_pockets(b.world, sh::MoveSettings{}, b.g, main);
  CHECK_MSG(c.by_rocket >= 1, "no rocket jump bridged the block's top");
  CHECK_MSG(c.by_flip >= 1,
            "the block's top was bridged by the shot at the feet, not the flip "
            "(tried first: the skilled jump)");
  CHECK_MSG(sh::nav_route(b.g, floor, top, search).nodes.empty(),
            "a route without rocket jumps reached the block's top");
  const sh::NavRoute r = sh::nav_route(b.g, floor, top, search, 1, {}, {}, {.rockets = true});
  CHECK_MSG(!r.nodes.empty(), "no route with rocket jumps reaches the block's top");
  int rockets = 0;
  for (size_t k = 0; k + 1 < r.nodes.size(); k++)
    for (const sh::NavLink& l : links_of(b.g, r.nodes[k]))
      rockets += l.to == r.nodes[k + 1] && l.kind == sh::NavKind::rocket;
  CHECK_MSG(rockets == 1, "the way up took " + std::to_string(rockets) + " rocket jumps, not one");
  sh::nav_reach(b.g, top, search);
  CHECK_MSG(search.reached(floor), "no way back down off the block's top");
}

TEST(the_flip_flies_flatter_farther_and_quicker_than_the_shot_at_the_feet) {
  const auto box = [](sh::Vec3f lo, sh::Vec3f hi) {
    sh::Brush b;
    b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
                {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
    return b;
  };
  const std::vector<sh::Brush> brushes = {box({-512, -256, -64}, {2048, 256, 0})};
  const sh::HullData point = sh::build_point_hull(brushes);
  const sh::HullData clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
  sh::MoveWorld world;
  world.solids[0].point_hull = point.hull();
  world.solids[0].clip_hull = clip.hull();
  world.solids[0].mins = {-512, -256, -64};
  world.solids[0].maxs = {2048, 256, 1024};
  world.count = 1;
  sh::NavSettings set;
  set.limit_s = 4;
  sh::RocketJumpSpec flip;
  flip.back = true;
  const sh::NavProof down =
      sh::prove_rocket_jump(world, sh::MoveSettings{}, {0, 0, 24}, {1900, 0, 24}, 320, set, {});
  const sh::NavProof back =
      sh::prove_rocket_jump(world, sh::MoveSettings{}, {0, 0, 24}, {1900, 0, 24}, 320, set, flip);
  const float down_v = down.end.x / down.time_s, back_v = back.end.x / back.time_s;
  CHECK_MSG(back_v > down_v * 1.3f, "the flip crossed " + std::to_string(back_v) +
                                        " units/s against the shot at the feet's " +
                                        std::to_string(down_v) + ": its push is not forward");
  CHECK_MSG(back.hi.z < down.hi.z - 80, "the flip's apex " + std::to_string(back.hi.z) +
                                            " is not well under the shot at the feet's " +
                                            std::to_string(down.hi.z));
  CHECK_MSG(back.end.x > down.end.x, "the flip landed at x " + std::to_string(back.end.x) +
                                         ", short of the shot at the feet's " +
                                         std::to_string(down.end.x));
}
