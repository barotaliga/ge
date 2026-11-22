// THE DEDICATED SERVER'S TERMINAL as an editor: when the server's input is a terminal it has in
// the foreground (and `TERM` is set and not `dumb`), the keys are read one at a time and the line
// is edited in place after the prompt `]`: Left, Right, Home and End (and Ctrl-A, Ctrl-E) move,
// Backspace and Delete take a letter, Ctrl-U and Ctrl-K cut to the start or the end, Up and Down
// walk the typed lines, Enter runs the line. Tab completes the command word as the player's
// console does with Quake II's settings (console/tab_list.hpp): `com_completion_mode` (0 one a
// line, 1 columns, 2 columns by kind) and `com_completion_treshold` (50: past it, the first Tab
// asks). A line the server prints while one is being typed is put above it and the line drawn
// again.
//
// THE HISTORY: the last 128 lines typed are kept; `sys_history` (128) of them are saved when the
// server ends to `.syshistory` in the base game folder and read back when it starts (0 or less:
// neither). Empty lines are not kept.
//
// The editor is a model (`TermEdit`, `term_feed`) that writes what the terminal shows into a
// string, so it is tested without a terminal; main.cpp puts the terminal in raw mode around it.
//
// ge:summary The dedicated terminal's line editor: keys, history, Tab, the history file
// ge:keywords terminal line-editor sys_history syshistory history tab com_completion_mode
// ge:keywords com_completion_treshold raw-mode prompt
// ge:skill net console
// ge:test tests/server_terminal_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "console/console.hpp"

namespace ge::server {

constexpr int kHistoryKept = 128;

struct TermEdit {
  std::string line;
  size_t cursor = 0;
  std::vector<std::string> history;  // oldest first
  int at = -1;                       // -1: the line being typed; else an index into history
  std::string typed;                 // the line being typed while the history is walked
  std::string escape;                // an escape sequence's bytes so far
  std::string asked;                 // the line the last Tab asked about
  int mode = 1;                      // com_completion_mode
  int threshold = 50;                // com_completion_treshold
};

// One byte typed: what the terminal should show appended to `out`, a finished line to `done`.
void term_feed(TermEdit& t, const console::Console& con, char c, std::string& out,
               std::vector<std::string>& done);
// The prompt and the line drawn again, the cursor in its place.
std::string term_redraw(const TermEdit& t);
// The line taken away, so a printed line can take its place ("\r" and clear to the end).
std::string term_hide();

// The history file: its last `keep` lines read into `t.history` (0 or less: nothing read), and
// the last `keep` of `t.history` written (0 or less: nothing written). False when it cannot be
// written.
void term_history_load(TermEdit& t, const std::string& path, int keep);
bool term_history_save(const TermEdit& t, const std::string& path, int keep);

}  // namespace ge::server
