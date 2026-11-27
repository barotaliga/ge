// The crushers the nav avoids (plugins/shooterbots/map_nav.hpp `mark_crusher_harm`): a train that
// crushes and comes to rest for good pins whoever stands under it there, so the floor under that
// stop harms while its top stays the way on; one that only passes harms nothing. On e1m4 the
// crusher, shot down by two buttons, stays on its pit's floor and held a bot that cannot die
// there the rest of the map; e1m3's ceiling comes down and goes back up.
//
// ge:summary Tests for the crushers and killing triggers the nav avoids: the floor under a
// ge:summary crusher's last stop, a trigger_hurt pit
// ge:keywords test map_nav mark_crusher_harm crusher train dmg end_stop harm e1m4 e1m3
// ge:keywords mark_hurt_harm trigger_hurt rboss push_carries_into trigger_push cut_hurt_links
// ge:keywords pit-lip func_rotating wheel
// ge:skill shooterbots testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "shooterbots/map_nav.hpp"

namespace {

namespace sb = ge::shooterbots;

// Node 0 on the pit's floor (origin 24 over it at 864), node 1 on the crusher's top where it
// rests (909), node 2 on the floor beside the pit.
sb::NavGraph pit() {
  sb::NavGraph g;
  g.nodes.resize(3);
  g.nodes[0].origin = {-250, 1600, 888};
  g.nodes[1].origin = {-250, 1600, 934};
  g.nodes[2].origin = {-100, 1600, 888};
  return g;
}

// A slab 28 high over the pit at 1317, its track down to 881 and back; `stays`: it stays at the
// bottom for good.
sb::NavMover crusher(bool stays) {
  sb::NavMover mv;
  mv.absmin = {-359, 1529, 1317};
  mv.absmax = {-153, 1743, 1345};
  mv.rest = {0, 0, 0};
  mv.stops = {{0, 0, 0}, {0, 0, -436}};
  mv.stop_wait_s = {0, 0};
  mv.dmg = 90;
  mv.end_stop = stays ? 1 : -1;
  return mv;
}

}  // namespace

TEST(the_floor_under_a_crusher_that_stays_down_harms_and_its_top_does_not) {
  const sb::NavGraph g = pit();
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_crusher_harm(g, {crusher(true)}, harm) == 1);
  CHECK(harm[0] == 1);
  CHECK(harm[1] == 0);
  CHECK(harm[2] == 0);
}

TEST(a_crusher_that_moves_on_or_does_no_harm_marks_nothing) {
  const sb::NavGraph g = pit();
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_crusher_harm(g, {crusher(false)}, harm) == 0);
  sb::NavMover soft = crusher(true);
  soft.dmg = 0;
  CHECK(sb::mark_crusher_harm(g, {soft}, harm) == 0);
  CHECK(harm[0] == 0 && harm[1] == 0 && harm[2] == 0);
}

// rboss's pit under its walkway (`trigger_hurt` *14: 10000 through any protection, 1040..1200 by
// -1536..-1408, 528..688, at a corridor's end): a node in it or at its edge harms, one well
// back or over it does not.
TEST(a_floor_node_in_or_at_the_edge_of_a_killing_trigger_harms_and_one_well_back_does_not) {
  sb::NavGraph g;
  g.nodes.resize(4);
  g.nodes[0].origin = {1149, -1437, 557};  // where the bot died
  g.nodes[1].origin = {1149, -1340, 557};  // the corridor well back from it
  g.nodes[2].origin = {1149, -1437, 820};  // well over its top
  g.nodes[3].origin = {1149, -1380, 557};  // at its edge, the floor sloping in
  const std::vector<sb::HurtBox> pits{{{1040, -1536, 528}, {1200, -1408, 688}}};
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_hurt_harm(g, pits, harm) == 2);
  CHECK(harm[0] == 1);
  CHECK(harm[1] == 0);
  CHECK(harm[2] == 0);
  CHECK(harm[3] == 1);
  CHECK(sb::mark_hurt_harm(g, pits, harm) == 0);  // marked once
}

// rboss's currents: a `trigger_push` south (270) from -704 to -1408 ends at the pit's box, so it
// carries whoever steps in to his death; pushed north, or up, or with the pit off its line, it
// carries him into nothing.
// rboss's strip at the bottom of its pit (448..464): a node on the lip beside it (552, 9 units
// off its side) harms; one on a bridge high over it, or well back from the lip, does not.
TEST(a_node_on_a_killing_pits_lip_harms_and_one_on_a_bridge_over_it_does_not) {
  sb::NavGraph g;
  g.nodes.resize(3);
  g.nodes[0].origin = {-23, 880, 552};   // the lip
  g.nodes[1].origin = {-200, 880, 600};  // a bridge over the pit
  g.nodes[2].origin = {40, 880, 552};    // well back from the lip
  const std::vector<sb::HurtBox> strip{{{-368, 808, 448}, {-32, 984, 464}}};
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_hurt_harm(g, strip, harm) == 1);
  CHECK(harm[0] == 1 && harm[1] == 0 && harm[2] == 0);
}

// rboss's grinder (`func_rotating` *27 turning from the start, 1840..2032 by 816..976, 368..528)
// over its pit's strip: a node standing on its top harms; one beside it, even close, does not.
TEST(a_node_on_a_turning_wheel_harms_and_one_beside_it_does_not) {
  sb::NavGraph g;
  g.nodes.resize(2);
  g.nodes[0].origin = {1928, 896, 552};   // on its top
  g.nodes[1].origin = {1928, 1000, 552};  // beside it
  const std::vector<sb::HurtBox> wheel{{{1840, 816, 368}, {2032, 976, 528}, false}};
  std::vector<uint8_t> harm(g.nodes.size(), 0);
  CHECK(sb::mark_hurt_harm(g, wheel, harm) == 1);
  CHECK(harm[0] == 1 && harm[1] == 0);
}

TEST(a_push_that_runs_into_a_killing_box_kills_as_it_does) {
  const sb::HurtBox pit{{1040, -1536, 528}, {1200, -1408, 688}};
  const sb::HurtBox push{{1040, -1408, 528}, {1200, -704, 688}};
  CHECK(sb::push_carries_into(push, 270, pit));
  CHECK(!sb::push_carries_into(push, 90, pit));
  CHECK(!sb::push_carries_into(push, -1, pit));
  CHECK(!sb::push_carries_into(push, 45, pit));
  const sb::HurtBox aside{{1400, -1536, 528}, {1500, -1408, 688}};
  CHECK(!sb::push_carries_into(push, 270, aside));
}

// rboss's electric strip in a pit's floor (`trigger_hurt` *29, -368..-32 by 808..984, 448..464):
// the walk across it between floor nodes on either side is cut; the walk beside it, and a jump
// over it, stay.
TEST(a_walk_across_a_killing_strip_is_cut_and_one_beside_it_stays) {
  sb::NavGraph g;
  g.nodes.resize(4);
  g.nodes[0].origin = {-200, 720, 472};
  g.nodes[1].origin = {-200, 1072, 472};
  g.nodes[2].origin = {-500, 720, 472};
  g.nodes[3].origin = {-200, 1072, 560};
  sb::NavLink across{}, beside{}, over{};
  across.to = 1;
  across.kind = sb::NavKind::walk;
  beside.to = 2;
  beside.kind = sb::NavKind::walk;
  over.to = 3;  // its line at its top, 560: over the strip
  over.kind = sb::NavKind::jump;
  g.links = {across, beside, over};
  g.nodes[0].first_link = 0;
  g.nodes[0].links = 3;
  const std::vector<sb::HurtBox> strip{{{-368, 808, 448}, {-32, 984, 464}}};
  CHECK(sb::cut_hurt_links(g, strip) == 1);
  CHECK(g.nodes[0].links == 2);
  CHECK(g.links[0].to == 2);
  CHECK(g.links[1].to == 3);
  CHECK(sb::cut_hurt_links(g, {}) == 0);
  // The pit's floor round the strip, 60 off it at its own height, harms too; a floor at the lip's
  // height as far off does not.
  sb::NavGraph ring;
  ring.nodes.resize(2);
  ring.nodes[0].origin = {-200, 748, 504};
  ring.nodes[1].origin = {-200, 748, 616};
  std::vector<uint8_t> harm(2, 0);
  CHECK(sb::mark_hurt_harm(ring, strip, harm) == 1);
  CHECK(harm[0] == 1 && harm[1] == 0);
  // A drop from the lip over the strip to the pit's floor past it falls through the strip.
  sb::NavGraph pit;
  pit.nodes.resize(2);
  pit.nodes[0].origin = {-200, 700, 616};
  pit.nodes[1].origin = {-200, 1072, 472};
  sb::NavLink drop{};
  drop.to = 1;
  drop.kind = sb::NavKind::drop;
  pit.links = {drop};
  pit.nodes[0].links = 1;
  CHECK(sb::cut_hurt_links(pit, strip) == 1);
  // A drop off the lip to the pit's floor short of the strip lands in it: the fall carries on.
  pit.nodes[0].origin = {-200, 690, 624};
  pit.nodes[1].origin = {-200, 760, 472};
  pit.links = {drop};
  pit.nodes[0].links = 1;
  CHECK(sb::cut_hurt_links(pit, strip) == 1);
  // Away from it, it does not.
  pit.nodes[1].origin = {-200, 620, 472};
  pit.links = {drop};
  pit.nodes[0].links = 1;
  CHECK(sb::cut_hurt_links(pit, strip) == 0);
}
