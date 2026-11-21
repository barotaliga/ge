// THE DROP-DOWN CONSOLE: what a player sees and types when they press the key above Tab (the
// backquote, under the tilde: `bind ` toggleconsole` is every game's default). It slides down
// from the top of the screen over the game, holding everything that happened: the console's own
// output, the commands typed, the chat (to all, to the team, whispers), the server's messages and
// the game's events, each line kept with its kind and the time it came. Closed, the newest few
// lines show at the top of the screen for a few seconds (the notify lines).
//
// THE INPUT LINE. Typed text is edited in place (left, right, home, end, backspace, delete), Tab
// completes a command, alias or cvar name (twice: lists every match), Up and Down walk the
// history, Page Up and Page Down scroll the lines. Enter submits it:
//   - a line starting with `/` or `\` is a command (that character dropped): `/r_bloom 0`;
//   - a line starting with `//` is a note: shown in the scrollback, never run or said;
//   - any other line is a CHAT message (`say <line>`) while `con_chat` is 1 (a multiplayer game
//     sets it), a command while it is 0 (single player: there is nobody to chat to), with 2
//     a command when its first word names a command, alias or setting (any case), else chat, and
//     with 3 said to the team. A game with its own setting for it (`game_chat`) rules over it.
//
// CHAT goes out through the game's `Chat` sender (net/session.hpp's chat in a multiplayer game);
// with none set, a line is shown locally as said. The chat COMMANDS (`say`, `say_team`,
// `say_near`, `whisper`, the `messagemode` family) and the voice buttons are not the view's: a
// multiplayer game opts into them with console/talk.hpp's `add_talk`, so a single-player game
// has none.
//
// THE CHAT LINE (`open_chat`): the one-line prompt a `messagemode` opens over the game (the
// console stays up). While it is open the typed text and the edit keys are its, Enter says it
// to whom it was opened for and closes it, Escape closes it unsaid. `typing()` is true while
// either the console or the chat line has the keyboard.
//
// THE VIEW HOLDS NO PIXELS: it is the model a renderer draws (the 2D layer's text pass in the
// app, the CPU raster in the `console` lab), so it is tested and pictured headless. Its cvars:
// con_height (the share of the screen it covers), con_speed (screens a second it slides),
// con_lines (the scrollback kept), con_notify (0: no notify lines), con_notifylines,
// con_notifytime, con_clearnotify (1: opening or closing the console lets the notify lines so
// far go), con_chat, con_suggest (how
// many suggestions the list under the input line holds), and what the panel
// is drawn on: con_bgtop and con_bgbottom (a gradient's colours, "rrggbb"), con_bgalpha and
// con_bgimage (a picture over the gradient); and con_font, the charset its text is written in
// (docs/charsets.md: terminal, mono, ...; "" the 2D layer's own UI font); con_clock (1: the
// local time in the panel's lower right corner); con_history (the typed lines kept, 128).
//
// `clearnotify` lets the notify lines so far go. `remotemode [address [password]]` sends each line
// typed without a slash to the game's `rcon` command, the prompt `#` (the address and password set
// `rcon_address` and `rcon_password` when the game has them); again, it ends. con_scale sizes the
// text (0: as drawn). A game shows what it is loading on the console's foot (`set_foot`).
//
// `condump [name]` writes the scrollback's text to `<folder>/<name>.txt` (`condump` the name when
// none is given; the folder is the game's, `set_dump_folder`), a line a line.
//
// ge:summary The drop-down console: scrollback by kind, the input line, history, / commands, chat
// ge:keywords console view drop-down tilde backquote toggleconsole scrollback history input-line
// ge:keywords chat say say_team say_near proximity whisper message server event notify
// ge:keywords chat-line messagemode chat-prompt typing
// ge:keywords completion tab con_height con_chat con_speed con_lines slash-command loading full
// ge:keywords background gradient colour con_bgtop con_bgbottom con_bgalpha con_bgimage con_font
// ge:keywords autocomplete suggestion suggestions hint con_suggest condump con_clock game_chat
// ge:keywords con_history
// ge:keywords clearnotify remotemode rcon con_scale foot loading download-bar
// ge:skill console
// ge:test tests/console_view_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

// What a scrollback line is, which is how it is coloured.
enum class LineKind : uint8_t {
  output,   // the console's own printing (a cvar's value, a list, a problem)
  command,  // a command the player submitted, echoed
  chat,     // said to everyone
  team,     // said to the team
  whisper,  // said to one player, or by one to this one
  nearby,   // said to the players near the speaker
  server,   // the server's message (a player joined, the map changes)
  event,    // the game's (a gate missed, a record, a crash)
};

std::string_view line_kind_name(LineKind k);

struct ViewLine {
  LineKind kind = LineKind::output;
  std::string text;
  double time_s = 0;  // the view's clock when it came
  uint64_t n = 0;     // its number, counted from the view's first line
};

// Where a chat line goes.
enum class ChatTo : uint8_t { all, team, one, nearby };

// Sends a chat line: `to` is the nick for ChatTo::one. True when it went (a game with a network
// prints it when the server echoes it back); false prints the reason.
using ChatSender = std::function<bool(ChatTo to, std::string_view nick, std::string_view text)>;

// A colour, 0..255 a channel.
struct Rgb {
  uint8_t r = 0, g = 0, b = 0;
  bool operator==(const Rgb&) const = default;
};

// "rrggbb" or "#rrggbb" (case free) as a colour; false when it is not one.
bool parse_rgb(std::string_view text, Rgb& out);

// What the panel is drawn on: a vertical gradient from `top` to `bottom` (the same colour: a flat
// fill) at `alpha`, under `image` when one is named (a file the game's assets hold; the gradient
// shows where it is transparent and while it loads). From con_bgtop, con_bgbottom, con_bgalpha
// and con_bgimage; a colour cvar that is not a colour draws its default.
struct Background {
  Rgb top{16, 20, 28}, bottom{16, 20, 28};
  double alpha = 0.85;
  std::string image;
};

// One match for what is being typed, as the list under the input line shows it.
struct Suggestion {
  enum class Kind : uint8_t { cvar, command, alias };
  std::string name;
  Kind kind = Kind::command;
  std::string detail;  // a cvar's value, a command's help, an alias's body
  int uses = 0;        // how often it was submitted from this console
};

// The keys the input line answers, besides typed text.
enum class EditKey : uint8_t {
  enter,
  backspace,
  del,
  left,
  right,
  home,
  end,
  tab,
  up,
  down,
  page_up,
  page_down,
  escape
};

class ConsoleView {  // NOLINT(clang-analyzer-optin.performance.Padding): fields by subject
 public:
  // Registers the view's cvars and commands on `c` (toggleconsole, clear) and takes `c`'s printing
  // into the scrollback.
  explicit ConsoleView(Console& c);

  // SLIDING. `frame` moves the drop toward open or closed at con_speed and advances the clock.
  // Opening or closing it lets the notify lines so far go while `con_clearnotify` is 1.
  void toggle() { set_open(!open_); }
  void set_open(bool open);
  bool open() const { return open_; }
  void frame(double dt_s);
  // LOADING: the whole screen covered at once (a game loading, as the classic games start); let go,
  // it slides up at con_speed to where open() puts it.
  void set_full(bool full);
  bool full() const { return full_; }
  // How far down it is: 0 closed .. 1 at con_height.
  double drop() const { return drop_; }
  // The screen's share it covers now: drop() × con_height.
  double cover() const;
  double clock() const { return clock_; }

  // LINES.
  void add(LineKind kind, std::string_view text);
  const std::deque<ViewLine>& lines() const { return lines_; }
  void clear() { lines_.clear(); }
  // The `rows` lines a page shows, oldest first, ending `scroll` lines above the newest.
  std::vector<const ViewLine*> page(int rows) const;
  int scroll() const { return scroll_; }
  // Lets the notify lines so far go (a demo's jump), the scrollback keeping them.
  void clear_notify() { notify_from_ = added_; }
  // The newest lines younger than con_notifytime, at most con_notifylines: shown while closed.
  std::vector<const ViewLine*> notify() const;

  // TYPING: into the chat line while it is open, else into the console's input line.
  void type(std::string_view text);
  void key(EditKey k);
  const std::string& input() const { return input_; }
  size_t cursor() const { return cursor_; }
  const std::vector<std::string>& history() const { return history_; }

  // AUTOCOMPLETE. While the first word is typed (after a `/`, or bare when con_chat is 0), every
  // command, cvar and alias it starts is a suggestion, the most submitted first (then the
  // shortest, then by name), at most con_suggest of them. Up and Down move through them, Tab
  // takes the selected one (the top one when none is), Enter on a cvar fills it in for a value
  // and on a command runs it. `hint` is the rest of the name Tab would take, drawn after the
  // cursor.
  std::vector<Suggestion> suggestions() const;
  int selected() const { return selected_; }
  std::string hint() const;

  // What the panel is drawn on (the con_bg* cvars).
  Background background() const;
  // con_font: the charset the text is written in ("": the 2D layer's UI font).
  const std::string& font() const { return font_; }

  // A game's own setting for what a line typed without a slash is, in its order: 0 a command, 1
  // said to everyone, 2 said to the team; -1 (none): con_chat rules.
  int game_chat = -1;
  // What a line typed without a slash is now, in con_chat's values (3: said to the team).
  int chat_mode() const;
  // Where `condump` writes ("": it refuses, saying so).
  void set_dump_folder(std::string folder) { dump_folder_ = std::move(folder); }
  // The scrollback written to `<folder>/<name>.txt`; false with the reason in `why`.
  bool dump(std::string_view name, std::string& why) const;
  // con_clock: the local time drawn in the panel's corner.
  bool clock_shown() const { return clock_on_; }
  // con_scale: the text's size against the console's own (0, the default: 1).
  double text_scale() const { return scale_ > 0 ? scale_ : 1; }
  // THE FOOT: a line the game shows under the scrollback while it joins a game (the file coming
  // and its share, what is loading); `progress` 0..1 draws a bar before it, below 0 none; ""
  // takes it away.
  void set_foot(std::string text, double progress = -1) {
    foot_ = std::move(text);
    foot_progress_ = progress;
  }
  const std::string& foot() const { return foot_; }
  double foot_progress() const { return foot_progress_; }
  // REMOTE MODE (`remotemode`): a line typed without a slash runs as `rcon <line>`, a server's
  // console from this one; the prompt is `#`.
  bool remote() const { return remote_; }
  // What the input line begins with: "] ", or "# " in remote mode.
  std::string_view prompt() const { return remote_ ? "# " : "] "; }

  // The game's chat; none: a line is shown as said by `name`.
  void set_chat(ChatSender send) { chat_ = std::move(send); }
  // `text` said to `to` (`nick`: ChatTo::one's player) through the game's chat; a `"..."` round
  // the whole text is dropped, an empty text says nothing.
  void say(ChatTo to, std::string_view nick, std::string_view text);

  // THE CHAT LINE. Opened for `to` (a `messagemode`), empty; opening it again re-aims it.
  void open_chat(ChatTo to);
  void close_chat();
  bool chatting() const { return chatting_; }
  ChatTo chat_to() const { return chat_to_; }
  const std::string& chat_input() const { return chat_input_; }
  size_t chat_cursor() const { return chat_cursor_; }
  // The words before the typed text, as a player reads them ("Say:", "Team:", "Nearby:").
  std::string_view chat_prompt() const;
  // The console or the chat line has the keyboard.
  bool typing() const { return open_ || chatting_; }

 private:
  void submit();
  // Whether `line`'s first word names a command, alias or setting (con_chat 2).
  bool names_something(std::string_view line) const;
  bool accept();             // the selected or top suggestion into the input line; false: none
  std::string head() const;  // the first word being typed, or "" when no suggestion applies
  void chat_key(EditKey k);
  Console& c_;
  std::deque<ViewLine> lines_;
  std::vector<std::string> history_;
  std::string input_;
  size_t cursor_ = 0;
  int history_at_ = -1;  // -1: the line being typed; else an index into history_
  std::string typed_;    // the line being typed while the history is walked
  int scroll_ = 0;
  bool open_ = false;
  int selected_ = -1;  // the suggestion Up and Down moved to; -1: none
  int32_t suggest_ = 8;
  std::map<std::string, int> uses_;  // submissions by command name
  double drop_ = 0, clock_ = 0;
  bool full_ = false;
  double over_ = 0;  // the share of the screen held past the drop: 1 while full, then sliding up
  double height_ = 0.5, speed_ = 3;
  int32_t keep_ = 1024, notify_lines_ = 4;
  bool notify_on_ = true, clear_notify_ = true;  // con_notify, con_clearnotify
  uint64_t added_ = 0;                           // the lines added so far
  uint64_t notify_from_ = 0;                     // the first line the notify area may show
  double notify_time_ = 4;
  int chat_default_ = 0;   // con_chat
  bool clock_on_ = false;  // con_clock
  int history_max_ = 128;  // con_history: the typed lines kept, the oldest dropped first
  bool remote_ = false;    // remotemode
  double scale_ = 0;       // con_scale
  std::string foot_;
  double foot_progress_ = -1;
  std::string dump_folder_;
  std::string bg_top_ = "10141c", bg_bottom_ = "10141c", bg_image_;
  std::string font_ = "mono";
  double bg_alpha_ = 0.85;
  ChatSender chat_;
  bool chatting_ = false;
  ChatTo chat_to_ = ChatTo::all;
  std::string chat_input_;
  size_t chat_cursor_ = 0;
};

}  // namespace ge::console
