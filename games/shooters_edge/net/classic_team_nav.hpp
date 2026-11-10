// A TEAM GAME'S MAP, SHARED BY ITS FIGHTERS: the nav graph off the map (bots/map_nav.hpp), the
// dives into the pockets only the water reaches, and the place names, with the one call that sets
// a fighter up on its slot over them. Our recorded team game (classic_team.hpp) and a team of bots
// on someone else's server (classic_bot_team.hpp) set their fighters up the same way.
//
// THE DIVES: made once, when the first fighter has read the map's items: for each item no route
// from the map's main part reaches, the pocket it stands in (the dry nodes reached from it and not
// from the main part), a swim field into it, and the way into the water it is entered by (from a
// node of the main part that reaches most of it again, nearest the pocket by water).
//
// THE PLACE NAMES: the ones given (a `.loc` file), else each key item's, weapon's and mega health's
// spot by its callout.
//
// ge:summary A team game's map shared by its fighters: nav, dives, place names, a fighter's setup
// ge:keywords team-nav dives pockets swim-field locs place-names fighter-setup team_fighter_setup
// ge:skill bots net shooter
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooter/locs.hpp"
#include "shooter/swim.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/goals.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/map_plan.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;
namespace bt = shooters_edge::bots;

struct TeamDive {
  std::vector<uint8_t> pocket;  // the pocket's dry nodes
  sh::SwimField field;
  int32_t node = -1, entry = -1;  // the node it is entered from, and the wet node it drops to
  float swim_s = 0;
};

struct TeamNav {
  bt::MapNav nav;
  sh::Locs given;  // the map's place names from a file (none: named off its items)
  sh::Locs locs;   // every fighter's, set with the first
  std::vector<std::unique_ptr<TeamDive>> dives;
  std::vector<int32_t> item_dive;  // each item's dive, -1 none
  bool dives_made = false;
  bt::MapPlan plan;  // the map's team plan (bots/map_plan.hpp), made with the first fighter
  std::vector<bt::Goal> goals;  // and every goal the map offers (bots/goals.hpp), made with it
  bool plan_made = false;
};

// Fighter `f` set up on player slot `slot` from what `seen` holds (our server's world, or a
// client's: classic_mirror.hpp): its items off the map's `entities`, its yaw, the map's place
// names, the dives and the swim field, and the map's team plan (made with the first).
void team_fighter_setup(TeamNav& tn, bt::Fighter& f, const server::RulesCommon& seen, int slot,
                        const char* entities, sh::AimSkill skill, const sh::Traits& traits,
                        double hz, uint32_t seed, const std::string& name);

}  // namespace shooters_edge::net
