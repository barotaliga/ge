// A CHAT LINE'S EXTRAS on a classic server joined, after the ignore lists (chat_ignore.hpp) kept
// it: its sound, the kinds hidden, a time before it and a mark when it names you.
//
// - **The kind** of a chat line (print level 3), from the ignore lists' reading of its start: a
//   line to everyone (`name: `) is the first kind, a team line (`(name): `) the second, a
//   spectator's line (a speaker watching, or `[SPEC] name: `) the spectators', and any other
//   chat-level line (no speaker known, a TV viewer's) the other kind.
// - **The sound** (`s_chat_custom`: 0 none, 1 every kind, 2 team lines only): each kind's file
//   (`s_mm1_file`, `s_mm2_file`, `s_spec_file`, `s_otherchat_file`, under the game's `sound/`,
//   `misc/talk.wav` each) at its volume (`s_mm1_volume` ... 0..1, 1 each; 0 plays nothing), heard
//   everywhere from your own place. A player at play hears `misc/talk.wav` for a speaker known
//   whatever the files say (only the volumes are his); watching, every file is his. Your own lines
//   sound too.
// - **Hidden** (`msg_filter`, a sum: 1 the first kind and the spectators', 2 team lines): gone
//   before they are shown.
// - **The time** (`con_timestamps`): 1 the local hour and minute before every chat line, the hour
//   padded to two places with a space (` 9:05 `); 2 the seconds too, but only while you watch or
//   the server's `status` is standby.
// - **Named** (`con_highlight`): a line holding your name at its third character or later (any
//   print level; at the start it is your own line) begins with `con_highlight_mark` with 2 or 3.
//   Our console draws every line in one colour, so 1's all-white line looks as before.
//
// ge:summary A chat line's sound, filter, time stamp and mark on a classic server joined
// ge:keywords chat-sound s_chat_custom s_mm1_file s_mm2_file s_spec_file s_otherchat_file
// ge:keywords s_mm1_volume msg_filter con_timestamps con_highlight con_highlight_mark talk-wav
// ge:keywords cl_chatsound chat_line_extras
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#pragma once

#include <array>
#include <string>

#include "console/chat_ignore.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

// The kinds of chat line, each with its sound.
inline constexpr int kChatFirst = 0, kChatTeam = 1, kChatSpec = 2, kChatOther = 3, kChatKinds = 4;

struct ChatExtras {
  int sound = 1;  // s_chat_custom
  std::array<std::string, kChatKinds> file{"misc/talk.wav", "misc/talk.wav", "misc/talk.wav",
                                           "misc/talk.wav"};
  std::array<float, kChatKinds> volume{1, 1, 1, 1};
  int filter = 0;      // msg_filter
  int timestamps = 0;  // con_timestamps
  int highlight = 0;   // con_highlight
  std::string mark;    // con_highlight_mark
};

// The settings, bound to `x` (which must outlive `con`), in the classic layer; `cl_chatsound` and
// the `con_sound_*` spellings as synonyms.
void add_chat_extras(ge::console::Console& con, ChatExtras& x);

// The local time a stamp shows.
struct ChatClock {
  int hour = 0, minute = 0, second = 0;
};
ChatClock chat_clock_now();

// One kept line `p` read as `line`: false when `msg_filter` hides it; else stamped and marked in
// place, and its sound added to `s.started`.
bool chat_line_extras(const ChatExtras& x, DemoState& s, const ge::console::ChatLine& line,
                      DemoPrint& p, const ChatClock& now);

}  // namespace shooters_edge::client
