// THE TEAM'S WORDS: what a team message says, as a team game's players type them with their
// message keys, written and read the same way whether a bot or a person sent it. A message is one
// line of short words, colour codes and braces allowed (`{&c0b0safe&cfff}` reads `safe`), its
// place in square brackets (`[ra]`, the place names of locs.hpp), and the player's status as
// `ra:150 h:100 rl` (armour kind and value, health, the best weapon held, `quad` while powered).
//
// The kinds, as written (`say_text`) and as recognised (`parse_say`, which takes any order and
// the common spellings: `took`/`got`, `coming`/`going`/`omw`, `over`/`dead`/`done`, `mega`/`mh`):
// - REPORT `ra:150 h:100 rl [ra]` (the status alone)
// - TOOK `took ra [ra]`, SAFE `safe [ra] ra:200 h:100 rl`, LOST `lost [ra]`
// - HELP `help [mh] ga:0 h:30 sg`, COMING `coming ra [rl]` (going for the item)
// - POINT `ra at [ra]`: "that item is there: you take it"; ENEMY `enemy at [quad]`
// - ENEMY POWER `enemy quad [ra]`; HAVE `have quad [rl]`; GET `get quad`; OVER `quad over`
// - TIMER `quad in 15`, `ra in 10`, `quad delayed 15 secs` (seconds until it is back)
//
// Pure text: deterministic, no state.
//
// ge:summary The team's words: team messages written and read, for bots and players alike
// ge:keywords team-message say_team teamplay callout report took safe lost help coming point
// ge:keywords enemy powerup timer quad-in parse_say say_text status-report colour-codes
// ge:skill shooter bots
// ge:test tests/shooters_edge_talk_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace shooters_edge::bots {

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
};
const char* say_kind_name(SayKind k);

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
};

// The item words (in a message's own spelling), as the item kinds' callouts name them.
bool item_word(std::string_view w);

// The message's text.
std::string say_text(const Said& s);

// What a message says (a team line's text, without the speaker's name).
Said parse_say(std::string_view text);

// A team line as the server prints it to a teammate, "(name): text", split; false when the line
// is not one.
bool split_team_line(std::string_view line, std::string& name, std::string& text);

}  // namespace shooters_edge::bots
