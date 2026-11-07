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
#include "shooter/swim.hpp"

namespace sh = ge::shooter;

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
  sh::NavGraph g;
  Pool() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-512, -512, -320}, {200, 512, 0}),
        box(sh::kContentsSolid, {200, -512, -320}, {1024, 512, -256}),
        box(sh::kContentsWater, {200, -512, -256}, {1024, 512, -8})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.count = 1;
    g.lo = {-512, -512, -320};
    g.hi = {1024, 512, 256};
    sh::NavNode ledge;
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
  sh::SwimField f;
  const uint8_t exits[1] = {1};
  sh::build_swim(pool().world, pool().g, exits, sh::SwimSettings{}, f);
  CHECK(f.wet > 100);
  CHECK_MSG(f.leading == f.wet, "a wet cell of an open pool has no way out");
  CHECK(sh::swim_wet(f, {600, 0, -150}));
  CHECK(!sh::swim_wet(f, {0, 0, -150}));  // inside the ground
  CHECK(!sh::swim_wet(f, {0, 0, 100}));   // the air over it
}

TEST(the_way_out_ends_on_the_exit) {
  sh::SwimField f;
  const uint8_t exits[1] = {1};
  sh::build_swim(pool().world, pool().g, exits, sh::SwimSettings{}, f);
  sh::Vec3f p{1000, 480, -230}, to;
  bool out = false;
  for (int step = 0; step < 200 && !out; step++) {
    CHECK_MSG(sh::swim_next(f, pool().g, p, to), "lost the way at step " + std::to_string(step));
    out = to == pool().g.nodes[0].origin;
    p = to;
  }
  CHECK_MSG(out, "the way never reached the ledge");
}

TEST(no_exit_no_way) {
  sh::SwimField f;
  const uint8_t exits[1] = {0};
  sh::build_swim(pool().world, pool().g, exits, sh::SwimSettings{}, f);
  sh::Vec3f to;
  CHECK(f.wet > 100 && f.leading == 0);
  CHECK(!sh::swim_next(f, pool().g, {600, 0, -150}, to));
}

TEST(a_field_is_built_the_same_twice) {
  sh::SwimField a, b;
  const uint8_t exits[1] = {1};
  sh::build_swim(pool().world, pool().g, exits, sh::SwimSettings{}, a);
  sh::build_swim(pool().world, pool().g, exits, sh::SwimSettings{}, b);
  CHECK(a.next == b.next && a.exit_node == b.exit_node && a.steps == b.steps);
}

TEST(the_water_is_part_of_the_graph) {
  const Pool& p = pool();
  sh::NavGraph g = p.g;
  const std::vector<uint8_t> wet_floor(g.nodes.size(), 0);
  const sh::SwimNavCounts c = sh::nav_add_swim(p.world, g, wet_floor);
  CHECK_MSG(c.cells > 100, "the pool has " + std::to_string(c.cells) + " swim nodes");
  CHECK_MSG(c.exits == 1, "the ledge has " + std::to_string(c.exits) + " climbs out, not one");
  // From the pool's far deep corner to the ledge: swum, then climbed out.
  const int32_t deep = sh::nav_nearest(g, {900, 400, -150});
  CHECK(deep >= g.swim_first);
  ge::ai::Search search;
  const sh::NavRoute r = sh::nav_route(g, deep, 0, search);
  CHECK_MSG(!r.nodes.empty(), "no route out of the pool's far corner onto the ledge");
  for (size_t k = 0; k + 1 < r.nodes.size(); k++) {
    const sh::NavNode& n = g.nodes[static_cast<size_t>(r.nodes[k])];
    bool swim = false;
    for (int32_t q = 0; q < n.links; q++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(q)];
      swim = swim || (l.to == r.nodes[k + 1] && l.kind == sh::NavKind::swim);
    }
    CHECK_MSG(swim, "a link of the way out of the water is not a swim");
  }
  // And back in off the ledge.
  CHECK_MSG(!sh::nav_route(g, 0, deep, search).nodes.empty(), "no way off the ledge into the pool");
}
