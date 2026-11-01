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
// THE MESSAGES are the team's voice and its wires: a member's sighting is the team's only once he
// calls it out ("enemy at RA"), a call for help ("need help") brings a strong free member, the one
// who timed the quad calls the rush ("quad in 10, get quad!"), the carrier calls "have quad!" and
// "lost quad", and a key item taken is called ("took RA", or by an expert who saw it go,
// "RA in 20"). Each is sent as a `say_team` by the member's client, so the recording carries it.
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
constexpr double kSayGap_s = 2.5;     // a member says no more than one message this often
constexpr double kRepeat_s = 8;       // the team says the same words again only after this

struct TeamStats {
  int64_t plans = 0, rushes = 0, quads = 0, held_shots = 0, claims = 0, callouts = 0;
  int64_t messages = 0;           // team messages said
  int64_t role_commands[4] = {};  // members' commands in each role
};

// One team message: when, by whom (its slot), the words.
struct TeamLine {
  double t = 0;
  int slot = -1;
  std::string text;
};

struct Team {
  // Ordered by size, largest first, so the struct packs tight.
  std::string name;
  std::string last_text;          // the last words said (at `last_text_at`)
  std::vector<TeamLine> log;      // every message said, in order (for the labs)
  std::vector<Fighter*> members;  // in slot order
  std::vector<int> claimed;       // per item: the slot going for it (-1: nobody)
  TeamStats stats;
  double last_text_at = -1e9;
  double enemy_seen = -1;  // when an enemy was last called out (-1: never)
  double help_until = -1;  // until when a call for help holds
  double next_plan = 0;
  Fighter* last_carrier = nullptr;  // who had the quad at the last plan
  sh::Vec3f enemy_at;               // where the enemy last called out was
  sh::Vec3f help_at;                // where a member last called for help
  uint32_t friends = 0;             // the members' slots as bits
  int32_t hold_item = -1;           // the item whose area is held (an index of the members' items)
  int32_t quad_item = -1;
  bool rushing = false;  // a rush was on at the last plan
};

// Member `f` says `text` to its team (a `say_team` its client sends), unless it spoke within
// `kSayGap_s` or the team said the same within `kRepeat_s`; true when said. Nothing on no team.
bool team_say(Fighter& f, double t, const std::string& text);

// The team's key areas off its members' items (every member's list is the same map's).
void team_init(Team& t);
// The roles, when due, off the server `s` as the last frame left it.
void team_plan(Team& t, const server::QcServer& s);

}  // namespace shooters_edge::bots
