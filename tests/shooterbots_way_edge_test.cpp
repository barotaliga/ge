// The pit's lip (plugins/shooterbots/way_hands.cpp, `way_edge`): a wish over the edge of a walkway
// whose only landing is a floor no node stands on (space's shaft under its walkway, the killing
// floor at its foot: no node there) is turned back; his own route down that way is let be, and a
// wish along the walkway is left alone.
//
// ge:summary Tests the pit's lip: a step off onto a floor with no node braked, a route down let be
// ge:keywords test way_edge pit-lip ledge fall landing nav_nearest space
// ge:skill shooterbots bots testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/rules_world.hpp"
#include "shooter_loop_bench.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/way_through.hpp"

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace lb = ge::test::loop;

namespace {

// The least a BotView reads, over the bench's server.
struct View {
  explicit View(const sh::Server& server) : sv(server) {}
  const sh::Server& sv;
  sh::RulesWorld world;
  std::vector<std::string> models{""}, sounds{""};
  std::string map_name = "test";
};

// A walkway (x -512..0, its top at z 0) over a shaft whose floor is 900 down; the nav knows only
// the walkway (the floor's nodes gone, as a killing floor's are).
struct Shaft {
  lb::Bench bench;
  sh::HullData point, clip;
  sh::MoveWorld world;
  sb::MapNav nav;
  std::unique_ptr<sb::Fighter> f = std::make_unique<sb::Fighter>();
  Shaft() {
    const std::vector<sh::Brush> walkway = {lb::box({-512, -64, -64}, {0, 64, 0})};
    std::vector<sh::Brush> all = walkway;
    all.push_back(lb::box({-512, -512, -960}, {512, 512, -900}));
    bench.start(all);
    point = sh::build_point_hull(walkway);
    clip = sh::build_box_hull(walkway, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-512, -64, -64};
    world.solids[0].maxs = {0, 64, 256};  // room over the walkway for the columns
    world.count = 1;
    sb::build_nav(world, {}, sh::MoveSettings{}, sb::NavSettings{}, nav.graph);
    nav.trap.assign(nav.graph.nodes.size(), 0);
    sh::Edict& me = bench.ent(1);
    me.free = false;
    me.health = 100;
    me.flags = sh::kFlagOnGround;
    me.waterlevel = 0;
    me.origin = {-20, 0, 24};
  }
  // The keys after the guard for a wish `forward` along yaw `yaw`; `turned` whether it turned them.
  sh::MoveCmd keys(float yaw, int16_t forward, bool& turned) {
    View v(bench.sv);
    sh::MoveCmd cmd{};
    cmd.angles = {0, yaw, 0};
    cmd.forward = forward;
    turned = sb::way_edge(nav, *f, sb::BotView(v), 1, cmd);
    return cmd;
  }
};

}  // namespace

TEST(a_step_off_a_walkway_onto_a_floor_no_node_stands_on_is_turned_back) {
  Shaft t;
  CHECK_MSG(!t.nav.graph.nodes.empty(), "the walkway has no nodes: read build_nav in nav.cpp");
  bool turned = false;
  const sh::MoveCmd off = t.keys(0, 400, turned);  // +x: over the edge 20 ahead
  CHECK_MSG(turned && off.forward < 0,
            "a step off the walkway onto a floor with no node was let be: a landing with no node "
            "near it is no floor with a way back (way_edge in way_hands.cpp)");
  const sh::MoveCmd along = t.keys(180, 400, turned);  // -x: along the walkway
  CHECK(!turned && along.forward == 400);
}

TEST(a_step_off_a_walkway_on_his_own_route_down_is_let_be) {
  Shaft t;
  // His route: from the walkway's node to one far below it (a drop of his own).
  const int32_t from = sb::nav_nearest(t.nav.graph, {-20, 0, 24});
  CHECK(from >= 0);
  sb::NavNode low;
  low.origin = {200, 0, -500};
  t.nav.graph.nodes.push_back(low);
  t.nav.trap.push_back(0);
  sb::Pilot& p = t.f->bot.pilot.pilot;
  p.route = {from, static_cast<int32_t>(t.nav.graph.nodes.size() - 1)};
  p.k = 0;
  bool turned = true;
  const sh::MoveCmd off = t.keys(0, 400, turned);
  CHECK_MSG(!turned && off.forward == 400,
            "his own route down off the walkway was braked at its edge (way_edge in way_hands.cpp)");
}
