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
// - ATTACK `attack ya [gl]` (or `push`): "we take X back together: gather on me here"
// - PLAN `lets take quad`, `camp ra and rl`, `free play` (the team's strategy, below); ACK `ok`
// - PACK `rl pack [mh]` (a backpack with a launcher in it lies there: come and get it)
//
// Pure text: deterministic, no state.
//
// ge:summary The team's words in this game: team messages written and read, bots and players alike
// ge:keywords team-message say_team teamplay callout report took safe lost help coming point
// ge:keywords enemy powerup timer quad-in parse_say say_text status-report colour-codes attack push
// ge:skill shooter bots
// ge:test tests/shooters_edge_talk_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "shooter/team_talk.hpp"

namespace shooters_edge::bots {

using ge::shooter::Said;
using ge::shooter::say_kind_name;
using ge::shooter::SayKind;
using ge::shooter::SayStatus;
using ge::shooter::Strategy;
using ge::shooter::strategy_name;

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
