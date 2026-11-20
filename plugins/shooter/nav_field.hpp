// THE WAY TO AN ITEM FROM EVERYWHERE: a FIELD per goal node, worked out once and kept (a reverse
// search from the goal over the nav's links), holding for every node the time to the goal and the
// next step, so a bot reads its way off the table instead of searching on the fly, and every bot of
// a kind reads the same way from the same place.
//
// DOORS AS ERRANDS: a door only a button opens (`NavErrand`: the button's node and the doors it
// fires) is shut in the field's first LAYER; each button has a layer of its own where its doors
// stand open. Pressing (or shooting) the button at its node steps from the first layer into its own
// at `press_s`, kept only when the way on from the button crosses one of its doors within
// `open_s` (the door shuts again after its wait). So a field says, from anywhere, whether the way
// to the goal starts with a button and which: dm2's north red armour is the window gate's shot
// button, then through the window. A door anything opens by touch is crossed as `nav_route`
// crosses one (its links cost `mover_penalty_s` more).
//
// A field is built for one jumper (`NavJumper`: rocket and grenade jumps, the gap tricks, the
// level): the fields of a map are built on the first ask for a goal and jumper and kept
// (`NavFields`); a gap trick is taken by a jumper who builds its speed on the way
// (`gap_build_s`), wherever it starts on the way. What changes in a match (a door found shut, a
// bridge still in, a jump missed twice) is not in a field: the caller checks the way read off it.
//
// ge:summary Fields to a goal from every node: time and next step, doors only a button opens as errands
// ge:keywords nav-field field route-table reverse-search item-field errand door button layers
// ge:keywords press shoot-button door-wait precomputed-routes way-from-everywhere
// ge:skill shooter bots
// ge:test tests/shooter_nav_field_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ai/path.hpp"
#include "shooter/nav.hpp"

namespace ge::shooter {

// A button and the doors it opens.
struct NavErrand {
  int32_t button = -1;        // the node it is pressed (or shot) from
  std::vector<int32_t> also;  // more nodes it is pressed or shot from (a shot button: every one
                              // that sees it near enough)
  static constexpr int kDoors = 4;
  int32_t doors[kDoors] = {-1, -1, -1, -1};  // the door entities it fires (the movers' `ent`)
  float press_s = 0.5f;                      // the press or the shot, s
  float open_s = 0;  // how long its doors stay open once fired, s (<= 0: for good)
};

// The field to `goal` for `jumper`: per layer (0: every errand's door shut; 1 + k: errand k's
// open), per node, the time to the goal and the next state.
struct NavField {
  int32_t goal = -1;
  NavJumper jumper;
  int32_t layers = 1;
  std::vector<float> time_s;  // [layer * nodes + node]: to the goal, s; infinity: no way
  std::vector<int32_t> next;  // [layer * nodes + node]: the next state; -1 at the goal or none
};

struct NavFields {
  const NavGraph* g = nullptr;
  std::vector<NavErrand> errands;
  float mover_penalty_s = 1;
  // The links into each node: [rev_first[n], rev_first[n + 1]) of rev_from (the link's start) and
  // rev_link (its index in `g->links`).
  std::vector<int32_t> rev_first, rev_from, rev_link;
  // The errands pressed at each node: [press_first[n], press_first[n + 1]) of press_errand.
  std::vector<int32_t> press_first, press_errand;
  std::vector<NavField> fields;  // every field built so far
  std::vector<float> to_cross;   // a build's scratch: a state's time to its layer's door
  ai::Search search;
  int64_t built = 0;  // fields built (each a search over every layer)
};

// Ready for `g` and its errands (each button's doors among `g`'s movers); no field built yet.
void nav_fields_init(NavFields& fs, const NavGraph& g, std::span<const NavErrand> errands);

// The field to node `goal` for `jumper`, built on the first ask and kept.
const NavField& nav_field(NavFields& fs, int32_t goal, const NavJumper& jumper);

// The time from node `from` in `layer` (0: every errand's door shut), s; infinity: no way.
float nav_field_time(const NavFields& fs, const NavField& f, int32_t from, int32_t layer = 0);

// The way read off a field from `from` in `layer`: the nodes in order to the goal (a node once
// where a button is pressed), the first errand pressed on it and the place in `nodes` it is pressed
// at (`press_at`); `time_s` the field's time from `from`. False with no way.
struct NavFieldWay {
  std::vector<int32_t> nodes;
  int32_t errand = -1;
  size_t press_at = 0;
  float time_s = 0;
};
bool nav_field_way(const NavFields& fs, const NavField& f, int32_t from, NavFieldWay& out,
                   int32_t layer = 0);

}  // namespace ge::shooter
