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
// - A server line (print level 2): the weapon lines.
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
#include "shooter/weapon_lines.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

struct ClassicIgnore {
  ge::console::ChatIgnore lists;
  ge::shooter::WeaponLines lines;
  int levels = -1;  // the level the lists are for
};

// The engine's ignore commands and settings and the shooter's weapon lines over `g`, the players
// read from `s` (both must outlive `con`), each in its layer.
void add_classic_ignore(ge::console::Console& con, ClassicIgnore& g, const DemoState& s);
// The players as the engine's lists read them.
ge::console::ChatRoster roster_of(const DemoState& s);
// A printed line read as a chat line (kind none: not a player's chat).
ge::console::ChatLine chat_line_of(const DemoState& s, const DemoPrint& p);
// `prints` without the lines the lists and the weapon lines drop, at `now_s` (wall clock).
void drop_ignored(ClassicIgnore& g, const DemoState& s, std::vector<DemoPrint>& prints,
                  double now_s);

}  // namespace shooters_edge::client
