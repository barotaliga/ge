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
// the pool up to 40 under the ground's top.
struct Pool {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  Pool() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-512, -512, -320}, {200, 512, 0}),
        box(sh::kContentsSolid, {200, -512, -320}, {1024, 512, -256}),
        box(sh::kContentsWater, {200, -512, -256}, {1024, 512, -40})};
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
