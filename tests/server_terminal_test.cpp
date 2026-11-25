// The dedicated terminal's line editor (engine/server/terminal.hpp): letters typed in place, the
// arrows and the edit keys by their escape sequences, Enter giving the line, the history walked
// up and down with the line being typed kept, Tab completing a command word and listing the
// matches by com_completion_mode, the threshold's question, and the history file's last lines.
//
// ge:summary Tests the terminal line editor: keys, history, Tab, the history file
// ge:keywords test terminal line-editor sys_history tab com_completion_mode
// ge:skill net console testing
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "server/terminal.hpp"

namespace gs = ge::server;
namespace cn = ge::console;

namespace {

struct Typed {
  std::string out;
  std::vector<std::string> done;
};

Typed type(gs::TermEdit& t, const cn::Console& con, const std::string& keys) {
  Typed r;
  for (const char c : keys) gs::term_feed(t, con, c, r.out, r.done);
  return r;
}

void add_words(cn::Console& con) {
  for (const char* w : {"map", "maplist", "mapname", "status", "say"})
    con.add_command(w, std::string(w) + ": a word", [](cn::Console&, const cn::Args&) {});
}

}  // namespace

TEST(keys_edit_the_line_in_place_and_enter_gives_it) {
  cn::Console con(true);
  gs::TermEdit t;
  Typed r = type(t, con, "stats");
  CHECK(t.line == "stats" && r.done.empty());
  CHECK(r.out.ends_with("]stats"));
  type(t, con, "\x1b[D\x1b[D\x1b[D\x7f");  // left three, backspace: "sats"
  CHECK(t.line == "sats" && t.cursor == 1);
  type(t, con, "t\x1b[H");  // insert, then home
  CHECK(t.line == "stats" && t.cursor == 0);
  type(t, con, "\x1b[3~");  // delete
  CHECK(t.line == "tats");
  type(t, con, "\x05\x15");  // end, then cut to the start
  CHECK(t.line.empty());
  r = type(t, con, "map q2dm1\r");
  CHECK(r.done.size() == 1 && r.done[0] == "map q2dm1" && t.line.empty());
  CHECK(gs::term_redraw(t) == "\r\x1b[K]");
  t.line = "abc";
  t.cursor = 1;
  CHECK(gs::term_redraw(t) == "\r\x1b[K]abc\x1b[2D");
}

TEST(up_and_down_walk_the_history_and_keep_the_line_being_typed) {
  cn::Console con(true);
  gs::TermEdit t;
  type(t, con, "one\rtwo\r\r");  // the empty line is not kept
  CHECK(t.history.size() == 2);
  type(t, con, "thr");
  type(t, con, "\x1b[A");
  CHECK(t.line == "two");
  type(t, con, "\x1b[A\x1b[A");
  CHECK(t.line == "one");
  type(t, con, "\x1b[B");
  CHECK(t.line == "two");
  type(t, con, "\x1b[B");
  CHECK(t.line == "thr");
}

TEST(tab_completes_the_word_and_lists_by_mode) {
  cn::Console con(true);
  add_words(con);
  gs::TermEdit t;
  Typed r = type(t, con, "sta\t");
  CHECK(t.line == "status ");
  t = {};
  t.mode = 0;
  r = type(t, con, "ma\t");
  CHECK(t.line == "map");
  CHECK_MSG(r.out.find("]map\r\nmap\r\nmaplist\r\nmapname\r\n") != std::string::npos, r.out);
  t = {};
  t.mode = 2;
  r = type(t, con, "map\t");
  CHECK_MSG(
      r.out.find("3 possible commands:\r\nmap      maplist  mapname\r\n") != std::string::npos,
      r.out);
  t = {};
  t.threshold = 2;
  r = type(t, con, "ma\t");
  CHECK(r.out.find("Press TAB again to display all 3 possibilities.") != std::string::npos);
  r = type(t, con, "\t");
  CHECK(r.out.find("maplist") != std::string::npos);
}

TEST(the_history_file_keeps_the_last_lines) {
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "ge_server_terminal_syshistory";
  cn::Console con(true);
  gs::TermEdit t;
  type(t, con, "a\rb\rc\r");
  CHECK(gs::term_history_save(t, file.string(), 2));
  gs::TermEdit back;
  gs::term_history_load(back, file.string(), 128);
  CHECK(back.history == std::vector<std::string>({"b", "c"}));
  gs::TermEdit none;
  gs::term_history_load(none, file.string(), 0);
  CHECK(none.history.empty());
  std::filesystem::remove(file);
}
