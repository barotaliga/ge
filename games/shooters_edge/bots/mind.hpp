// A FIGHTER'S OWN MIND IN A TEAM GAME: everything one player believes of his teammates and of the
// game's plan, which he learns only from what he sees and from the team's messages (talk.hpp), as
// a player at a keyboard does. No two fighters share anything: each one reads the team lines his
// own client is sent (a person's as well as a bot's), keeps his own picture of where his mates are
// and what they have, and decides his own role from it.
//
// READING by how well he plays (his map sense, knowledge.hpp): an EXPERT reads every line; an
// AVERAGE player misses about half of the lines said once but reads a line said again within
// `kRepeatWindow_s`, and misses more while fighting; a NOVICE is busy staying alive, reads one
// line in six and a line only once it has been said three times, and none while fighting. What a
// line says, once read:
// - a place (`[ra]`, or an item word) is where its speaker is, or where the enemy is;
// - a status (`ra:150 h:100 rl`) is the speaker's strength, `quaded` or `have quad` his quad;
// - `took X`, `X in N`: X's timer (exact as said; `took` counted from now by the item's own time);
// - `coming X`: the speaker goes for X, so this player does not; `safe [X]`: the speaker holds X;
//   `lost [X]`: X's holder is gone and the enemy is there; `help [X]`: a call to answer;
// - `X at [Y]`, a POINT: "you take X", taken by the player who believes he is the nearest of his
//   team to it; `get quad`: the quad is due.
//
// THE ROLE each player plans every `kPlan_s` from his own beliefs (team.hpp names the roles):
// carry with the quad; rush when he believes the quad due within `kRushAhead_s` and himself one of
// the two of his team nearest it; hold the key area he claimed (at the start the nearest of the
// rocket launcher, the best armour and the quad that no teammate said he is coming for, called
// with `coming X`, then held with `safe [X]`); free otherwise. A player gives an item up to a
// teammate in sight near it when he has no use for it (the armour no better than his own, a weapon
// he holds, rockets he is full on) or when a teammate has the quad (the best armour, the rocket
// launcher, the lightning gun), and points at it with his status. The quad's carrier sweeps the
// key areas. The calls that matter most (the quad's and the shield's) are said again more often
// when he believes no teammate is near them.
//
// Deterministic: a pure function of what the fighter saw, heard and drew from its own stream.
//
// ge:summary A fighter's own mind: team lines read by skill, its role chosen alone, its calls
// ge:keywords mind team-message read-by-skill beliefs mates claims timers role rush hold carry
// ge:keywords point give-up opening-rush safe report spam quad-call separate-brains
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/locs.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/bots/talk.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::bots {

constexpr double kRepeatWindow_s = 12;  // a line said again within this reads as a repeat
constexpr double kMateFresh_s = 15;     // a mate's place or status this old is still believed
constexpr double kReport_s[3] = {40, 20, 12};  // a report this often: novice, average, expert

// What a player believes of one teammate.
struct Mate {
  std::string name;
  ge::shooter::Vec3f at;     // where he was last seen or said to be
  double at_t = -1e9;        // when
  double strength_t = -1e9;  // when his status was last read
  double quad_t = -1e9;      // when he was last believed to have the quad
  double going_t = -1e9;     // when he said where he is going
  double holds_t = -1e9;     // when he last said his area is safe
  float strength = 100;      // health plus armour's protection, a launcher counted extra
  int32_t going = -1;        // the item he said he is coming for
  int32_t holds = -1;        // the key item whose area he holds
  int slot = -1;             // his player slot, once seen
  bool quad = false;
};

// A line it was sent, kept a while to know a repeat.
struct HeardLine {
  std::string text;
  double t = 0;
  int count = 1;
};

struct Mind {
  std::vector<Mate> mates;
  std::vector<HeardLine> heard;
  ge::shooter::Vec3f enemy_at;  // where the enemy was last called out (heard or seen)
  ge::shooter::Vec3f help_at;   // where a mate last called for help
  double enemy_t = -1e9;
  double help_until = -1e9;
  double told_t = -1e9;        // when it was pointed at `told`
  double next_plan = 0;        // its own planning clock
  double reported_at = -1e9;   // its last status report
  double safe_at = -1e9;       // its last `safe`
  double quad_call_at = -1e9;  // its last call of the quad
  double pent_call_at = -1e9;
  double timer_call_at = -1e9;
  double gave_at = -1e9;         // its last point at an item it gave up
  double help_said_at = -1e9;    // its last call for help
  int64_t read = 0, missed = 0;  // team lines read and missed
  int32_t told = -1;             // an item a mate pointed it at ("you take it")
  int32_t claim = -1;            // the key item whose area it claimed (-1: none)
  int32_t gave = -1;             // the item it gave up last
  bool claimed_said = false;
  bool opened = false;  // its first plan alive is behind it
};

struct Fighter;
struct Stats;

// A team line this player's client was sent, from the player named `from`: read or missed by its
// skill, and what it says taken into its beliefs when read.
void mind_hear(Fighter& f, const std::string& from, const std::string& text, double t);

// The role and the calls, every `kPlan_s`, off its own beliefs (`me` is its entity).
void mind_plan(Fighter& f, const server::QcServer& s, int me, const Stats& st, double t);

// A team message from this player, its place filled in from where it stands (`at`).
bool mind_say(Fighter& f, double t, Said said, ge::shooter::Vec3f at);

// The name of the place `p` is in ("" with no place names).
std::string place_of(const Fighter& f, ge::shooter::Vec3f p);

// Whether a teammate is believed to be going for item `i` now.
bool mate_going(const Fighter& f, int32_t i, double t);

// Whether item `i` is left to a teammate (no use to this player, or the quad's carrier's due).
bool gives_up(const Fighter& f, const server::QcServer& s, int me, int32_t i, const Stats& st,
              double t);

// This player's status as a message carries it.
SayStatus status_of(const Stats& st);

}  // namespace shooters_edge::bots
