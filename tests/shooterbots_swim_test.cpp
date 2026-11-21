// THE WAY OUT OF THE WATER: a pool beside a ledge whose top is a node the field leads to. Every
// wet cell of the pool has a way out; following `swim_next` from the pool's far deep corner ends
// on the ledge's node; a point in the solid ground or in the dry air
// above the ledge is in no wet cell; and the field is the same twice.
//
// ge:summary Tests: the swim field over a pool: every wet cell leads out, the way ends at the exit
// ge:keywords test swim water pool build_swim swim_next swim_wet flow-field exit
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooterbots/swim.hpp"

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// The ground (x < 200, its top at z 0), the pool's floor (x >= 200, at z -256) and the water in
// the pool up to 8 under the ground's top: low enough a swimmer's water jump takes him over the
// edge (an exit is proven by the move; 40 under, no one climbs out).
struct Pool {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sb::NavGraph g;
  explicit Pool(float lip = 8) {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-512, -512, -320}, {200, 512, 0}),
        box(sh::kContentsSolid, {200, -512, -320}, {1024, 512, -256}),
        box(sh::kContentsWater, {200, -512, -256}, {1024, 512, -lip})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.count = 1;
    g.lo = {-512, -512, -320};
    g.hi = {1024, 512, 256};
    sb::NavNode ledge;
    ledge.origin = {170, 0, 24};  // standing on the ground at its edge
    g.nodes.push_back(ledge);
  }
};

const Pool& pool() {
  static const Pool p;
  return p;
}

}  // namespace

TEST(every_wet_cell_of_a_pool_has_a_way_out) {
  sb::SwimField f;
  const uint8_t exits[1] = {1};
  sb::build_swim(pool().world, pool().g, exits, sb::SwimSettings{}, f);
  CHECK(f.wet > 100);
  CHECK_MSG(f.leading == f.wet, "a wet cell of an open pool has no way out");
  CHECK(sb::swim_wet(f, {600, 0, -150}));
  CHECK(!sb::swim_wet(f, {0, 0, -150}));  // inside the ground
  CHECK(!sb::swim_wet(f, {0, 0, 100}));   // the air over it
}

TEST(the_way_out_ends_on_the_exit) {
  sb::SwimField f;
  const uint8_t exits[1] = {1};
  sb::build_swim(pool().world, pool().g, exits, sb::SwimSettings{}, f);
  sh::Vec3f p{1000, 480, -230}, to;
  bool out = false;
  for (int step = 0; step < 200 && !out; step++) {
    CHECK_MSG(sb::swim_next(f, pool().g, p, to), "lost the way at step " + std::to_string(step));
    out = to == pool().g.nodes[0].origin;
    p = to;
  }
  CHECK_MSG(out, "the way never reached the ledge");
}

TEST(no_exit_no_way) {
  sb::SwimField f;
  const uint8_t exits[1] = {0};
  sb::build_swim(pool().world, pool().g, exits, sb::SwimSettings{}, f);
  sh::Vec3f to;
  CHECK(f.wet > 100 && f.leading == 0);
  CHECK(!sb::swim_next(f, pool().g, {600, 0, -150}, to));
}

TEST(a_field_is_built_the_same_twice) {
  sb::SwimField a, b;
  const uint8_t exits[1] = {1};
  sb::build_swim(pool().world, pool().g, exits, sb::SwimSettings{}, a);
  sb::build_swim(pool().world, pool().g, exits, sb::SwimSettings{}, b);
  CHECK(a.next == b.next && a.exit_node == b.exit_node && a.steps == b.steps);
}

TEST(the_water_is_part_of_the_graph) {
  const Pool& p = pool();
  sb::NavGraph g = p.g;
  const std::vector<uint8_t> wet_floor(g.nodes.size(), 0);
  const sb::SwimNavCounts c = sb::nav_add_swim(p.world, g, wet_floor);
  CHECK_MSG(c.cells > 100, "the pool has " + std::to_string(c.cells) + " swim nodes");
  CHECK_MSG(c.exits == 1, "the ledge has " + std::to_string(c.exits) + " climbs out, not one");
  // From the pool's far deep corner to the ledge: swum, then climbed out.
  const int32_t deep = sb::nav_nearest(g, {900, 400, -150});
  CHECK(deep >= g.swim_first);
  ge::ai::Search search;
  const sb::NavRoute r = sb::nav_route(g, deep, 0, search);
  CHECK_MSG(!r.nodes.empty(), "no route out of the pool's far corner onto the ledge");
  for (size_t k = 0; k + 1 < r.nodes.size(); k++) {
    const sb::NavNode& n = g.nodes[static_cast<size_t>(r.nodes[k])];
    bool swim = false;
    for (int32_t q = 0; q < n.links; q++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(q)];
      swim = swim || (l.to == r.nodes[k + 1] && l.kind == sb::NavKind::swim);
    }
    CHECK_MSG(swim, "a link of the way out of the water is not a swim");
  }
  // And back in off the ledge.
  CHECK_MSG(!sb::nav_route(g, 0, deep, search).nodes.empty(), "no way off the ledge into the pool");
}

// A bank 20 over the water: Quake's water jump (24 ahead, solid 8 up, open 32 up) climbs it,
// Quake II's (30 ahead, solid 4 up, open 20 up, 350 up) cannot, so a swim under Quake II's move
// proves no climb out there and plans its way out elsewhere (base1's river: he jumped at such a
// bank for minutes).
// A bank 20 over the water: Quake's water jump (24 ahead, solid 8 up, open 32 up) climbs it,
// Quake II's (30 ahead, solid 4 up, open 20 up, 350 up) cannot, so a swim under Quake II's move
// proves no climb out there and plans its way out elsewhere (base1's river: he jumped at such a
// bank for minutes).
TEST(a_bank_too_high_for_the_games_water_jump_is_no_way_out) {
  const Pool high(20);
  sb::NavGraph quake = high.g, quake2 = high.g;
  const std::vector<uint8_t> wet_floor(high.g.nodes.size(), 0);
  sb::SwimNavSettings q2;
  q2.move.swimscale = 0.5;
  q2.move.waterjump = 350;
  q2.move.waterjump_reach = 30;
  q2.move.waterjump_lip = 4;
  q2.move.waterjump_open = 20;
  const sb::SwimNavCounts a = sb::nav_add_swim(high.world, quake, wet_floor);
  const sb::SwimNavCounts b = sb::nav_add_swim(high.world, quake2, wet_floor, q2);
  CHECK_MSG(a.exits == 1,
            "Quake's move climbs " + std::to_string(a.exits) + " times out, not once");
  CHECK_MSG(b.exits == 0,
            "Quake II's move climbs " + std::to_string(b.exits) + " times out, not never");
  // The same lip a little lower is one Quake II's water jump does clear.
  const Pool low(12);
  sb::NavGraph g = low.g;
  CHECK_MSG(sb::nav_add_swim(low.world, g, wet_floor, q2).exits == 1,
            "Quake II's move should climb a bank 12 over the water");
}

// A graph over a sham box (a map that gives its world 99999 every way) gets no swim grid rather
// than one past memory.
TEST(a_sham_world_box_gets_no_swim_grid) {
  sb::NavGraph g = pool().g;
  g.lo = {-99999, -99999, -99999};
  g.hi = {99999, 99999, 99999};
  const std::vector<uint8_t> wet_floor(g.nodes.size(), 0);
  const sb::SwimNavCounts c = sb::nav_add_swim(pool().world, g, wet_floor);
  CHECK(c.cells == 0);
}
