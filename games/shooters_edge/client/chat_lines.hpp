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
// - **Quake II's spellings** (`cl_chat_*`, in the classic layer for both games): `cl_chat_notify`
//   0 keeps chat lines out of the HUD's notify feed (the console still has them); `cl_chat_filter`
//   1 cuts a chat line at its first newline and drops its unprintable characters. Quake II's own
//   `cl_chat_sound` (0 none, 1 `misc/talk.wav`, 2 `misc/talk1.wav`, or a sound's name) rules the
//   sound while it is loaded, over `s_chat_custom` and the kinds' files.
// - **The chat HUD** (`scr_chathud`: 0 off, 1 the plain colour, 2 the alternate green): the last
//   `scr_chathud_lines` chat lines (4, at most 32) in a feed of their own at the screen's lower
//   left (the HUD's `chat` element, placed by its `hud_chat_*` settings), each fading over the
//   last second of `scr_chathud_time` (0: never); `clearchathud` empties it.
// - **Quake II's version reply**: there a chat line from another player that ENDS in `!version`
//   is answered with the version 1 to 2 s later, at most once in two minutes (`cl_queries` 0:
//   never).
// - **`followip [n]`**: the n-th last address a chat line named (1 the newest; 16 are kept)
//   joined.
// - **`trigger <command> <match>`**: the command run when a line that is not chat, or the centre
//   message, matches the wildcard (`*`, `?`); `trigger` alone lists them, `untrigger <command>
//   <match>` takes one away, `untrigger all` every one. Never while watching through a relay.
//
// ge:summary A chat line's sound, filter, time stamp and mark on a classic server joined
// ge:keywords chat-sound s_chat_custom s_mm1_file s_mm2_file s_spec_file s_otherchat_file
// ge:keywords s_mm1_volume msg_filter con_timestamps con_highlight con_highlight_mark talk-wav
// ge:keywords cl_chatsound chat_line_extras cl_chat_notify cl_chat_filter cl_chat_sound
// ge:keywords followip trigger untrigger cl_nofake tp_soundtrigger cl_queries f_version f_server
// ge:keywords version-query msg_trigger lastip sound-trigger fake-name chat_line_teamplay
// ge:keywords scr_chathud scr_chathud_lines scr_chathud_time clearchathud chat-hud
// ge:keywords scr_chathud_x scr_chathud_y add_q2_chat_place
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#pragma once

#include <array>
#include <string>
#include <vector>

#include "console/chat_ignore.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "hud/feed.hpp"
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
  int notify = 1;      // cl_chat_notify
  int strip = 0;       // cl_chat_filter
  // Quake II's own cl_chat_sound while it is loaded; "": s_chat_custom and the files rule.
  std::string named_sound;
  int hud = 0;                         // scr_chathud
  int hud_lines = 4;                   // scr_chathud_lines
  double hud_time = 0;                 // scr_chathud_time (0: never fades)
  double hud_x = 8, hud_y = -64;       // Quake II's scr_chathud_x and _y: its 2D pixels
  ge::hud::Feed feed{4, -1, 1, 0.15};  // the chat HUD's lines
  std::vector<std::string> seen;       // the addresses chat lines named, newest last (followip)
  struct Trigger {
    std::string command, match;
  };
  std::vector<Trigger> triggers;        // trigger
  bool relay = false;                   // watching through a relay: the triggers wait
  ge::console::Console* con = nullptr;  // where followip and the triggers run their commands
  // The team's extras (chat_teamplay): cl_nofake, tp_soundtrigger, cl_queries, msg_trigger's
  // switch (tp_msgtriggers, bound by the regex triggers), when each query was last answered.
  int nofake = 2;
  std::string sound_trigger = "~";
  bool queries = true;
  std::array<double, 3> answered_at{-1e9, -1e9, -1e9};
  // Quake II's version reply: a chat line ending in `!version` answered once in kQ2ReplyEveryS,
  // 1 to 2 s after it (`reply_due`, -1 none; said by `chat_reply_due`).
  double reply_due = -1;
  std::array<std::string, 8> sound_names;  // the sound words played, kept while they sound
  size_t sound_next = 0;
};

// Quake II's version reply: its wait after the question (the least and the most) and how seldom.
inline constexpr double kQ2ReplyLeastS = 1, kQ2ReplyMostS = 2, kQ2ReplyEveryS = 120;
// Says the version reply that is due at `now_s` (`ChatExtras::reply_due`), once.
void chat_reply_due(ChatExtras& x, double now_s, std::string_view version);

// Quake II's own `cl_chat_sound` on `x` (its layer: cvar_layers.cpp's add_q2).
void add_q2_chat_sound(ge::console::Console& con, ChatExtras& x);
// Quake II's own place for the chat lines on screen, in its layer: `scr_chathud_x` (8) and
// `scr_chathud_y` (-64), pixels of its 2D drawing (scr_scale screen px each), y up from the bottom
// when negative. Its bar's chat lines are drawn there (hud/layout.hpp `place_chat_px`); every
// other game places them by our `hud_chat_x` and `hud_chat_y`.
void add_q2_chat_place(ge::console::Console& con, ChatExtras& x);

// The sound a chat line plays under Quake II's `cl_chat_sound` `named` (nullptr: none).
const char* q2_chat_sound(const std::string& named);

// The addresses (`a.b.c.d` or `a.b.c.d:port`) a chat line names, in order.
std::vector<std::string> addresses_in(std::string_view text);

// A non-chat line (or the centre message) through the triggers: each matching one's command run.
void run_triggers(ChatExtras& x, std::string_view text);

// A line told the player (the console has it already) into the HUD's feeds: a chat line into
// the chat HUD while it is on and into `notify` unless cl_chat_notify is 0; another into `notify`.
// With `ns` (the console's con_notify, con_notifylines and con_notifytime: the HUD's feed is the
// window's one notify area), `notify` sized and timed by them, and none of it under con_notify 0.
void tell_feeds(ChatExtras& x, ge::hud::Feed& notify, std::string_view line, bool chat,
                ge::ui::Color ink, double at,
                const ge::console::ConsoleView::NotifySettings* ns = nullptr);

// The settings, bound to `x` (which must outlive `con`), in the classic layer; `cl_chatsound` and
// the `con_sound_*` spellings as synonyms.
void add_chat_extras(ge::console::Console& con, ChatExtras& x);

// The local time a stamp shows.
struct ChatClock {
  int hour = 0, minute = 0, second = 0;
};
ChatClock chat_clock_now();

// One kept line `p` read as `line`: false when `msg_filter` hides it; else cleaned
// (`cl_chat_filter`), stamped and marked in place, its addresses kept for followip, and its sound
// added to `s.started`.
bool chat_line_extras(ChatExtras& x, DemoState& s, const ge::console::ChatLine& line, DemoPrint& p,
                      const ChatClock& now);

// THE TEAM'S EXTRAS on a kept line `p` (every classic game), after chat_line_extras:
// - **`cl_nofake`** (2: the default): a received chat line's carriage returns, and its line feeds
//   but the last, become blanks, so a line cannot pass for another player's or start a second
//   line; 1 on every chat line, 2 on every one but your own team's, 0 none.
// - **`tp_soundtrigger`** (`~`; "" off): a team line from your own team that ends in one of its
//   characters and a sound's name (`quad up ~misc/gong`) plays that sound (`.wav` added when it
//   has no extension; under the game's `sound/`; a name with `..` none) in place of the chat
//   sound, and the word is taken off the line.
// - **The queries** (`cl_queries` 1, the default): a line to everyone (not a team line, not your
//   own, not while you watch) that is `f_version` or `!version`, or `f_server` (each optionally
//   followed by digits and blanks), is answered with `say`: the game's name and version, or the
//   server's address. Each kind at most once in 20 seconds.
// - **`msg_trigger`** (console/console.hpp): every kept line, a chat line's words after its
//   speaker, fires the triggers of its level (0 low .. 3 chat; `t` your team's chat) while
//   `tp_msgtriggers` is 1; a chat line that asks a query fires none.
// `version` is what `f_version` answers; `now_s` the wall clock.
void chat_line_teamplay(ChatExtras& x, DemoState& s, const ge::console::ChatLine& line,
                        DemoPrint& p, double now_s, bool triggers_on, std::string_view version);

}  // namespace shooters_edge::client
