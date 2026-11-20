// THE FIELDS TO A GOAL (plugins/shooter/nav_field.hpp): times and ways off a hand-made graph, a
// door only a button opens taken as an errand (the press, the way back through it), refused when
// the door shuts before the way reaches it, a door anyone opens crossed at its penalty, a rocket
// jump taken only by a jumper who takes it, and a field kept once built.
//
// ge:summary Tests: fields to a goal from every node, doors as button errands, the cache
// ge:keywords test nav-field errand button door-wait layers rocket-jump cache
// ge:skill shooter testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "shooter/nav_field.hpp"

namespace sh = ge::shooter;

namespace {

// A row of `n` nodes 64 apart, walked both ways at 1 s a link; `door` (an entity) on the link
// between nodes `at` and `at + 1` both ways, none when negative.
sh::NavGraph row(int n, int32_t door = -1, int at = -1) {
  sh::NavGraph g;
  for (int i = 0; i < n; i++) {
    sh::NavNode nd;
    nd.origin = {64.f * static_cast<float>(i), 0, 24};
    nd.first_link = static_cast<int32_t>(g.links.size());
    for (const int to : {i - 1, i + 1}) {
      if (to < 0 || to >= n) continue;
      sh::NavLink l;
      l.to = to;
      l.kind = sh::NavKind::walk;
      l.time_s = 1;
      if ((i == at && to == at + 1) || (i == at + 1 && to == at)) l.mover = door;
      g.links.push_back(l);
      nd.links++;
    }
    g.nodes.push_back(nd);
  }
  return g;
}

}  // namespace

TEST(a_field_holds_every_nodes_time_and_way_to_its_goal) {
  const sh::NavGraph g = row(5);
  sh::NavFields fs;
  sh::nav_fields_init(fs, g, {});
  const sh::NavJumper on_foot;  // a named jumper: the field outlives no temporary
  const sh::NavField& f = sh::nav_field(fs, 4, on_foot);
  for (int i = 0; i < 5; i++) CHECK(sh::nav_field_time(fs, f, i) == static_cast<float>(4 - i));
  sh::NavFieldWay w;
  CHECK(sh::nav_field_way(fs, f, 1, w));
  CHECK(w.nodes == (std::vector<int32_t>{1, 2, 3, 4}));
  CHECK(w.errand == -1);
  CHECK(w.time_s == 3);
}

TEST(a_door_only_a_button_opens_is_an_errand_and_one_anyone_opens_costs_its_penalty) {
  // The door (entity 7) between nodes 2 and 3; its button pressed from node 0.
  const sh::NavGraph g = row(5, 7, 2);
  sh::NavErrand e;
  e.button = 0;
  e.doors[0] = 7;
  e.press_s = 0.5f;
  e.open_s = 10;
  sh::NavFields fs;
  sh::nav_fields_init(fs, g, std::span<const sh::NavErrand>(&e, 1));
  const sh::NavJumper on_foot;  // a named jumper: the field outlives no temporary
  const sh::NavField& f = sh::nav_field(fs, 4, on_foot);
  // From node 1: back to the button (1 s), the press, then on through the door (4 s and the
  // door's penalty).
  CHECK(sh::nav_field_time(fs, f, 1) == 1 + 0.5f + 4 + 1);
  sh::NavFieldWay w;
  CHECK(sh::nav_field_way(fs, f, 1, w));
  CHECK(w.nodes == (std::vector<int32_t>{1, 0, 1, 2, 3, 4}));
  CHECK(w.errand == 0);
  CHECK(w.press_at == 1);
  // Past the door already: no errand.
  CHECK(sh::nav_field_time(fs, f, 3) == 1);
  // The door open (its layer): straight through.
  CHECK(sh::nav_field_time(fs, f, 1, 1) == 3 + 1);
  // The same door with no button: crossed by touch at its penalty.
  sh::NavFields open;
  sh::nav_fields_init(open, g, {});
  CHECK(sh::nav_field_time(open, sh::nav_field(open, 4, {}), 1) == 3 + 1);
}

TEST(a_door_that_shuts_before_the_way_reaches_it_is_no_way) {
  const sh::NavGraph g = row(5, 7, 2);
  sh::NavErrand e;
  e.button = 0;
  e.doors[0] = 7;
  e.open_s = 2;  // the run from the button to the door is 3 s and its penalty
  sh::NavFields fs;
  sh::nav_fields_init(fs, g, std::span<const sh::NavErrand>(&e, 1));
  const sh::NavJumper on_foot;  // a named jumper: the field outlives no temporary
  const sh::NavField& f = sh::nav_field(fs, 4, on_foot);
  CHECK(std::isinf(sh::nav_field_time(fs, f, 1)));
  sh::NavFieldWay w;
  CHECK(!sh::nav_field_way(fs, f, 1, w));
}

TEST(a_rocket_jump_is_taken_only_by_a_jumper_who_takes_it_and_fields_are_kept) {
  sh::NavGraph g = row(5);
  // A rocket jump from node 0 straight to node 4 at 1.5 s.
  sh::NavLink rj;
  rj.to = 4;
  rj.kind = sh::NavKind::rocket;
  rj.time_s = 1.5f;
  rj.level = 1;
  g.links.insert(g.links.begin() + g.nodes[0].first_link + g.nodes[0].links, rj);
  g.nodes[0].links++;
  for (size_t i = 1; i < g.nodes.size(); i++) g.nodes[i].first_link++;
  sh::NavFields fs;
  sh::nav_fields_init(fs, g, {});
  CHECK(sh::nav_field_time(fs, sh::nav_field(fs, 4, {}), 0) == 4);
  sh::NavJumper rocket;
  rocket.rockets = true;
  CHECK(sh::nav_field_time(fs, sh::nav_field(fs, 4, rocket), 0) == 1.5f);
  rocket.level = 0;  // an amateur takes none above his level
  CHECK(sh::nav_field_time(fs, sh::nav_field(fs, 4, rocket), 0) == 4);
  CHECK(fs.built == 3);
  sh::nav_field(fs, 4, {});
  CHECK(fs.built == 3);  // asked again: kept
}

TEST(a_shot_button_is_shot_from_any_node_that_sees_it) {
  // The door between nodes 3 and 4; its button at node 0, and shot from node 2 too.
  const sh::NavGraph g = row(6, 7, 3);
  sh::NavErrand e;
  e.button = 0;
  e.also = {2};
  e.doors[0] = 7;
  e.press_s = 0.8f;
  e.open_s = 3;
  sh::NavFields fs;
  sh::nav_fields_init(fs, g, std::span<const sh::NavErrand>(&e, 1));
  const sh::NavJumper on_foot;  // a named jumper: the field outlives no temporary
  const sh::NavField& f = sh::nav_field(fs, 5, on_foot);
  // From node 2: the shot where it stands, then 3 links and the door's penalty.
  CHECK(sh::nav_field_time(fs, f, 2) == 0.8f + 3 + 1);
  sh::NavFieldWay w;
  CHECK(sh::nav_field_way(fs, f, 1, w));
  CHECK(w.errand == 0);
  CHECK(w.nodes[w.press_at] == 2);  // on the way, not back to the button's own node
}
