// WHAT A TEAM MESSAGE MEANS, for any team shooter: its kind (a report, took, safe, lost, help,
// coming, a point at an item, an enemy, an enemy's power, have, get, over, a timer, attack, the
// team's plan, a pack, an acknowledgement, a trap), the item and place it is about, the speaker's
// status and a timer's seconds. Bots act on these meanings and players' lines are read into them;
// how a message is WORDED and how its places look (the words, the colour codes, the macros, the
// location file) is each game's, which writes a `Said` as its own text and reads its own lines back
// (Shooter's Edge's in games/shooters_edge/bots/talk.hpp).
//
// ge:summary What a team message means for any team shooter: its kind, item, place, status, plan
// ge:keywords team-talk team-message said say-kind strategy say-status callout meaning
// ge:skill shooter bots
// ge:test tests/shooters_edge_talk_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace ge::shooter {

enum class SayKind : uint8_t {
  none,  // nothing recognised
  report,
  took,
  safe,
  lost,
  help,
  coming,
  point,
  enemy,
  enemy_power,
  have,
  get,
  over,
  timer,
  attack,
  plan,  // the team's strategy (`Strategy`)
  pack,  // a backpack worth having lies there ("rl pack [mh]": an enemy's launcher, or one dropped)
  ack,   // "ok", "k", "np": a line taken in
  trap,  // "trap [X]": a floor trap's floor just slid away (floor_trap.hpp): off it, not into it
};
const char* say_kind_name(SayKind k);

// THE TEAM'S STRATEGY, said by its leader as a person types it and followed by every member who
// reads it: TAKE an item together (`lets take quad`, `all quad`, `rush ra`), CAMP one or two key
// areas (`camp ra and rl`, `hold ra rl`, `control ra`: the team split between them), or FREE
// (`free play`, `go free`: each his own way again).
enum class Strategy : uint8_t { none, take, camp, free };
const char* strategy_name(Strategy s);

// A player's status as a message carries it (-1: not said).
struct SayStatus {
  std::string armor_kind;  // "ra", "ya", "ga" ("" none)
  std::string weapon;      // the best weapon, "rl", "lg", ... ("" not said)
  int armor = -1, health = -1;
  bool quad = false;
};

struct Said {
  std::string item;  // the item it is about: "ra", "quad", "rl", "mh", ... ("" none)
  std::string loc;   // the place in its brackets ("" none)
  SayStatus status;
  int secs = -1;  // a timer's seconds
  SayKind kind = SayKind::none;
  Strategy strategy = Strategy::none;  // a plan's
  std::string item2;                   // a plan's second item ("" none)
  int variant = 0;                     // which of a plan's or an ack's wordings it is said in
};

}  // namespace ge::shooter
