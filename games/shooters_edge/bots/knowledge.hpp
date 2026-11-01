// WHAT A FIGHTER KNOWS OF THE MAP'S ITEMS, by how well it knows the map (`MapSense`), the way a
// player learns a map over matches:
// - an EXPERT knows where every item is from the start, and times the items that win games (the
//   best armours, the mega health, the rocket launcher, the quad and the shield) once he has seen
//   one taken: he knows to the second when it is back;
// - an AVERAGE player knows where every item is, but times only the small ones he sees taken: a
//   key item he saw go is back "some time later", so he checks it again rather than wait on it;
// - a NOVICE knows nothing at the start. He goes for what he sees, and recalls an item from
//   elsewhere on the map only once he has seen it `kLearnSightings` separate times, which for the
//   quad on a big map is late in the match. With nothing known he walks the map to learn it.
//
// What is believed comes from sight alone: an item's spot within `kItemSight`, in the view's cone
// and with a clear line (`sees_player`), shows whether the item is there. An item out of sight
// keeps the last belief; one believed gone is believed back at its time (timed) or after half as
// long again (a guess). Nothing reads the server's item state directly.
//
// Deterministic: a pure function of what the fighter was sent and its view.
//
// ge:summary What a fighter knows of the map's items: discovery by sight, recall, spawn timing
// ge:keywords knowledge map-sense novice average expert discovery learn recall item-timing
// ge:keywords respawn-timing sight sightings key-item callout-name explore
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr int32_t kLearnSightings = 3;  // a novice recalls an item seen this many separate times
constexpr double kSightingGap_s = 15;   // sightings this far apart count separately
constexpr float kItemSight = 1500;      // an item's spot is made out within this, units
constexpr float kGuessLate = 1.5f;      // an untimed item is guessed back after this × its time

// How well a player of aim `skill` knows the map: the expert's aim is the expert's map.
MapSense map_sense_of(sh::AimSkill skill);
const char* map_sense_name(MapSense s);

// The items a game turns on: the best armours, the mega health, the rocket launcher, the powers.
bool key_item(const ItemSpot& it);

// The item as a player calls it out: "RA", "quad", "RL", "mega" and so on.
const char* item_call(const ItemSpot& it);

// What the player starts the match knowing (call after the items are read).
void knowledge_init(Fighter& f);

struct ItemNews {
  int32_t taken = -1;    // an item it saw taken (or found gone) this think, -1 none
  bool by_me = false;    // and it took it itself
  int32_t learned = -1;  // an item a novice recalls from now on, -1 none
};

// One think's look at the items from `eye` (the player's entity `me`, its view `pitch`/`yaw`), at
// time `t`; returns the news a team calls out.
ItemNews knowledge_update(Fighter& f, const server::QcServer& s, int me, sh::Vec3f eye, double t);

// Whether item `i` may be a goal: known, or in sight now.
bool item_in_mind(const Fighter& f, int32_t i);

// How many items the player knows (recall), for the labs.
int32_t items_known(const Fighter& f);

// A node for a novice with nothing in mind to walk to (a far node of those it can reach, drawn
// off its own seed), or -1.
int32_t explore_node(Fighter& f, sh::Vec3f me);

}  // namespace shooters_edge::bots
