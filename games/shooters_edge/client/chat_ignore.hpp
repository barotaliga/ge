// CHAT IGNORE ON A CLASSIC SERVER JOINED: the engine's ignore lists (console/chat_ignore.hpp) and
// the shooter's weapon lines (shooter/weapon_lines.hpp) fed the classic games' printed lines
// (porting/shooters-edge/chat-ignore.md holds the facts).
//
// - The players: each place's name, team and spectator flag from the server's user info, the
//   local player's place, `teamplay` and the match running (the server's `status` not `standby`).
// - A chat line (print level 3) by its start: `name: ` chat to everyone (a spectator's when the
//   speaker watches), `(name): ` team chat (when the speaker is you or, in a team game, on your
//   team), `[SPEC] name: ` spectators' own chat (while you watch), `#<digits>:` a TV viewer's. The
//   longest name that fits wins, so a name that begins another is not confused with it.
// - A team line with a tag (`filter`, console/chat_ignore.hpp): hidden, or shown without its tag.
// - A server line (print level 2): the weapon lines.
// - Then the regex triggers (`re_trigger`), on the lines kept and the centre message; then each
//   line's extras (chat_lines.hpp: its sound, `msg_filter`, the time and the mark).
// - A new level clears the players' flags and the flood's memory.
//
// ge:summary The classic games' chat lines read for the engine's ignore lists on a server joined
// ge:keywords chat-ignore classic-chat ignore drop_ignored chat_line_of ignore_qtv
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/chat_ignore.hpp"
#include "console/console.hpp"
#include "console/re_triggers.hpp"
#include "shooter/weapon_lines.hpp"
#include "shooters_edge/client/chat_lines.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

struct QwIgnore {
  ge::console::ChatIgnore lists;
  ge::shooter::WeaponLines lines;
  int levels = -1;  // the level the lists are for
  // The player's regex triggers on the lines kept (console/re_triggers.hpp), `tp_msgtriggers`
  // turning them off, and the console their aliases run in.
  ge::console::ReTriggers triggers;
  ge::console::Console* con = nullptr;
  ChatExtras chat;  // each kept line's sound, filter, stamp and mark (chat_lines.hpp)
};

// The engine's ignore commands and settings and the shooter's weapon lines over `g`, the players
// read from `s` (both must outlive `con`), each in its layer.
void add_qw_ignore(ge::console::Console& con, QwIgnore& g, const DemoState& s);
// The players as the engine's lists read them.
ge::console::ChatRoster roster_of(const DemoState& s);
// A printed line read as a chat line (kind none: not a player's chat).
ge::console::ChatLine chat_line_of(const DemoState& s, const DemoPrint& p);
// `prints` without the lines the lists, the team tags and the weapon lines drop, at `now_s` (wall
// clock); then each line kept, and the centre message `centre` (when given), through the regex
// triggers (by print level: 0 low, 1 medium, 2 high, 3 chat), a `remove` taking it away.
// Each line kept then has its extras (chat_lines.hpp): its sound into `s.started`, `msg_filter`,
// the stamp and the mark (at `clock`, the local time).
void drop_ignored(QwIgnore& g, DemoState& s, std::vector<DemoPrint>& prints, double now_s,
                  std::string* centre = nullptr, const ChatClock& clock = chat_clock_now());

}  // namespace shooters_edge::client
