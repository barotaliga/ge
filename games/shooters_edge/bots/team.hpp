// A TEAM OF FIGHTERS: what turns four fighters (fighter.hpp) into a side of a team game, as the
// team game is played: survival and equipment before frags, the strongest player holding the
// map's key area, the quad taken on its timer by the players nearest it, the armour and the health
// left for the teammate who needs them most, the weak falling back to their own, and no shot taken
// through a teammate.
//
// THE KEY AREAS come off the map's items (the map's text, as every fighter reads it): the area held
// is the best armour's (the 200 armour, else the 150, else the 100), and the quad's. Nothing is
// named per map: a map whose quad stands by its best armour, as the classic small team map's does,
// gets one area held and a quad taken from it.
//
// THE PLAN, every `kPlan_s` (each decision on `ai::Why` when a member has one), gives each living
// member a ROLE:
// - `hold`: one player, the strongest (health plus armour's protection, a launcher counted extra):
//   to the held area and stays on it, fighting from there, chasing no further than `kLeash`;
// - `rush`: the quad due within `kRushAhead_s` (or there) and no teammate carrying it: the two
//   members nearest it (the holder last) go and wait on it;
// - `carry`: a member with the quad hunts: to the enemy last seen by anyone on the team, else to
//   the held area;
// - `free`: the rest gather what they lack (each item claimed by one member, the one it is worth
//   most to) and come back to the held area when there is nothing worth having.
// A member's sightings are the team's (`enemy_at`: where and when an enemy was last seen by
// anyone), as players call them out.
//
// Deterministic: a pure function of the server as the last frame left it and the members' state.
//
// ge:summary A team of fighters: roles, the held area, the quad's timer, claims and callouts
// ge:keywords team teamplay roles hold rush carry free key-area quad-timer stacking claims
// ge:keywords callout 4on4 team-game fighter regroup
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kPlan_s = 0.5;       // how often the team's roles are planned
constexpr double kRushAhead_s = 12;   // the quad due within this: the rushers go to wait on it
constexpr float kLeash = 700;         // a holder fights no further than this from his area, units
constexpr float kHoldStrong = 110;    // a holder this strong goes to his area; weaker he heals
constexpr float kFireClearance = 40;  // a teammate this near the line of fire holds the shot, units

struct TeamStats {
  int64_t plans = 0, rushes = 0, quads = 0, held_shots = 0, claims = 0, callouts = 0;
  int64_t role_commands[4] = {};  // members' commands in each role
};

struct Team {
  std::string name;
  uint32_t friends = 0;           // the members' slots as bits
  std::vector<Fighter*> members;  // in slot order
  int32_t hold_item = -1;         // the item whose area is held (an index of the members' items)
  int32_t quad_item = -1;
  std::vector<int> claimed;  // per item: the slot going for it (-1: nobody)
  sh::Vec3f enemy_at;        // where an enemy was last seen by any member
  double enemy_seen = -1;    // and when (-1: never)
  double next_plan = 0;
  TeamStats stats;
};

// The team's key areas off its members' items (every member's list is the same map's).
void team_init(Team& t);
// The roles, when due, off the server `s` as the last frame left it.
void team_plan(Team& t, const server::QcServer& s);

}  // namespace shooters_edge::bots
