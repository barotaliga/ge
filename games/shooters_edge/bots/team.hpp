// A TEAM OF FIGHTERS: four fighters (fighter.hpp) on one side of a team game, each with a mind of
// his own (mind.hpp). The team is a ROSTER and nothing more: who is on the side (as a scoreboard
// shows it: each member's slot and name), so a player knows who not to shoot through and whose
// lines are his team's. Nothing any member believes is shared: what one knows reaches another only
// as a team message (`team_say`, a `say_team` his client sends), read or missed by the other's
// skill.
//
// THE ROLES each member takes, by his own plan every `kPlan_s` (mind.hpp):
// - `hold`: on the key area he claimed (the best armour, the launcher or the quad), fighting from
//   there and chasing no further than `kLeash`;
// - `rush`: the quad believed due within `kRushAhead_s`, and he one of the two believed nearest;
// - `carry`: he has the quad: the key areas swept, the enemy hunted;
// - `free`: the rest: what he lacks, a call for help answered, a point followed.
//
// The rest of the struct is the record a lab draws (the lines said, the counts, the key areas);
// no fighter reads it.
//
// ge:summary A team of fighters: the roster, the roles' names, the lines said and the lab's record
// ge:keywords team teamplay roster roles hold rush carry free key-area 4on4 team-game team_say
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
  std::vector<TeamLine> log;      // every message said, in order (for the labs)
  std::vector<Fighter*> members;  // the roster, in slot order (slots and names only are read)
  TeamStats stats;
  uint32_t friends = 0;    // the members' slots as bits
  int32_t hold_item = -1;  // the key areas for the labs' map: the best armour's
  int32_t quad_item = -1;  // and the quad's
};

// Member `f` says `text` to its team (a `say_team` its client sends), unless it spoke within
// `kSayGap_s` or it was sent the same words within `kRepeat_s`; true when said. Nothing on no team.
bool team_say(Fighter& f, double t, const std::string& text);

// The roster's bits and the labs' key areas off the members' items (one map's list).
void team_init(Team& t);
// The lab's count of the commands spent in each role (the roles are each member's own).
void team_count(Team& t);

}  // namespace shooters_edge::bots
