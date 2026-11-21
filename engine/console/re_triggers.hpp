// REGULAR-EXPRESSION TRIGGERS: a player's alias run when a line from the game matches a pattern,
// for any game whose console shows its server's lines (a frag counter, a sound on a word).
//
// THE COMMANDS (`add_re_triggers`):
//   re_trigger                      every trigger: `name : "pattern" : times matched`
//   re_trigger <name>               one, with its options; a pattern alone lists the names it finds
//   re_trigger <name> <pattern>     made, or its pattern replaced (its options and place kept); a
//                                   pattern the console cannot read is said and the trigger goes
//   re_trigger_options <name> <option> ...   final|notfinal, remove|noremove, mask <bits>,
//                                   interval <s>, enable|disable, noaction|action, nolog|log
//   re_trigger_delete|_enable|_disable <name or pattern> ...
//   re_trigger_match <name> <text>  the trigger tried on the text (even disabled, or too soon)
// A pattern is ECMAScript, case counting, found anywhere in the line, the line's newline included.
//
// THE LINES (`re_trigger_line`), by kind, a trigger's `mask` the kinds it sees (default 31, the
// server's): 1 the server's low prints (pickups), 2 medium (deaths), 4 high, 8 chat, 16 centre
// messages (32, the `echo` command's output, is kept for a game that feeds it). The newest trigger
// is tried first. A match counts, fills `re_trigger_match_0` (the whole match) to
// `re_trigger_match_9` with the groups (later ones keep their values) and runs the alias of the
// trigger's name at once (`noaction`: none); `remove` hides the line, `nolog` keeps it out of the
// log, `final` stops the triggers after it, and `interval` skips the trigger until that many
// seconds have passed since it last fired. `ReTriggers::on` false turns them all off (a game binds
// it: Shooter's Edge's `tp_msgtriggers`).
//
// ge:summary Regular-expression triggers: a player's alias run on a game line matching a pattern
// ge:keywords re_trigger re_trigger_options re_trigger_delete re_trigger_enable re_trigger_disable
// ge:keywords re_trigger_match re_trigger_match_0 regex trigger message-trigger mask final remove
// ge:skill console
// ge:test tests/console_re_triggers_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

// The kinds of line, a trigger's mask bits.
inline constexpr int kReLow = 1, kReMedium = 2, kReHigh = 4, kReChat = 8, kReCentre = 16,
                     kReEcho = 32;
inline constexpr int kReMaskDefault = 31;
inline constexpr int kReMatches = 10;

struct ReTrigger {
  std::string name, pattern;
  std::shared_ptr<const std::regex> re;
  bool final = false, remove = false, enabled = true, action = true, log = true;
  int mask = kReMaskDefault;
  double interval = 0;  // s between firings, 0 none
  double last = -1e300;
  int matched = 0;
};

struct ReTriggers {
  std::vector<ReTrigger> list;  // the newest first
  bool on = true;
  double now = 0;  // s, the clock `interval` reads, set by the game each frame
};

// What a line's triggers asked.
struct ReLine {
  bool remove = false, nolog = false;
};

// The commands and `re_trigger_match_0..9` (read-only) over `t` (it must outlive `con`).
void add_re_triggers(Console& con, ReTriggers& t);
// `line` (its newline, if any, at the end) of kind `kind` (one mask bit) at `now_s` through the
// triggers: each that matches counts, fills the match settings and runs its alias.
ReLine re_trigger_line(Console& con, ReTriggers& t, std::string_view line, int kind, double now_s);

}  // namespace ge::console
