// The drop-down console: the slide, the submit rule (/ commands, chat otherwise in a multiplayer
// game), chat to all, a team and one player, editing, history, autocomplete, scrollback, notify.
//
// ge:summary Tests the drop-down console: slide, / commands, chat, editing, history, completion
// ge:keywords test console view submit chat say whisper history completion scroll notify
// ge:keywords autocomplete suggestion condump con_clock game_chat team
// ge:skill console testing
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "console/talk.hpp"
#include "console/view.hpp"

using namespace ge::console;

TEST(slides_at_its_speed_and_the_tilde_toggles_it) {
  Console c;
  ConsoleView v(c);
  c.key("`", true);
  c.run();
  CHECK(v.open());
  v.frame(0.1);  // 3 screens a second over half a screen: 0.6 of the way
  CHECK(v.drop() > 0.59 && v.drop() < 0.61);
  v.frame(1);
  CHECK(v.drop() == 1 && v.cover() == 0.5);
  v.key(EditKey::escape);
  v.frame(1);
  CHECK(!v.open() && v.drop() == 0);
}

TEST(escape_clears_the_text_first_and_the_tilde_keeps_it) {
  Console c;
  ConsoleView v(c);
  v.set_open(true);
  v.type("map e1m1");
  v.toggle();  // the console's own key: closed, the text kept
  CHECK(!v.open() && v.input() == "map e1m1");
  v.toggle();
  v.key(EditKey::escape);  // text typed: cleared, still open
  CHECK(v.open() && v.input().empty());
  v.key(EditKey::escape);  // nothing typed: closed
  CHECK(!v.open());
}

TEST(full_covers_the_screen_at_once_then_slides_up_at_its_speed) {
  Console c;
  ConsoleView v(c);
  v.set_full(true);
  CHECK(v.cover() == 1);  // at once, before a frame
  v.frame(1);
  CHECK(v.cover() == 1);  // held while loading
  v.set_full(false);
  v.frame(0.1);  // 3 screens a second: 0.3 of the screen up
  CHECK(v.cover() > 0.69 && v.cover() < 0.71);
  v.frame(1);
  CHECK(v.cover() == 0);
  v.set_full(true);  // let go while open: it stops at con_height
  v.set_open(true);
  v.set_full(false);
  for (int i = 0; i < 100; i++) v.frame(0.05);
  CHECK(v.cover() == 0.5);
}

TEST(slash_is_a_command_and_plain_text_is_chat_when_con_chat_is_on) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "r_bloom", .fallback = "0.04"});
  v.type("r_bloom 0");  // single player: a line without a / is a command too
  v.key(EditKey::enter);
  c.run();
  CHECK(c.cvars().find("r_bloom")->number == 0);
  c.execute("con_chat 1; name Ada");
  v.type("hello all");
  v.key(EditKey::enter);
  v.type("/r_bloom 0.5");
  v.key(EditKey::enter);
  c.run();
  CHECK(c.cvars().find("r_bloom")->number == 0.5);
  bool chat = false, command = false;
  for (const ViewLine& l : v.lines()) {
    if (l.kind == LineKind::chat && l.text == "Ada: hello all") chat = true;
    if (l.kind == LineKind::command && l.text == "] r_bloom 0.5") command = true;
  }
  CHECK(chat && command);
}

TEST(con_chat_2_runs_a_line_whose_first_word_is_known_and_says_the_rest) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "r_bloom", .fallback = "0.04"});
  c.execute("con_chat 2; name Ada; alias hop \"r_bloom 0.2\"");
  c.run();
  const auto enter = [&](const char* line) {
    v.type(line);
    v.key(EditKey::enter);
    c.run();
  };
  enter("R_BLOOM 0.3");  // a setting, any case: run
  CHECK(c.cvars().find("r_bloom")->number == 0.3);
  enter("hop");  // an alias: run
  CHECK(c.cvars().find("r_bloom")->number == 0.2);
  enter("gg all");         // nothing known: said
  enter("\\r_bloom 0.6");  // a backslash runs it too
  CHECK(c.cvars().find("r_bloom")->number == 0.6);
  enter("// r_bloom 0.9 later");  // a note: shown, not run
  CHECK(c.cvars().find("r_bloom")->number == 0.6);
  bool said = false, note = false;
  for (const ViewLine& l : v.lines()) {
    if (l.kind == LineKind::chat && l.text == "Ada: gg all") said = true;
    if (l.kind == LineKind::command && l.text == "// r_bloom 0.9 later") note = true;
  }
  CHECK(said && note);
}

// con_chat 3 says a plain line to the team; a game's own setting (game_chat, in its order: 2 the
// team) rules over con_chat while it is set.
TEST(con_chat_3_and_a_games_own_setting_say_to_the_team) {
  Console c;
  ConsoleView v(c);
  std::vector<std::pair<ChatTo, std::string>> sent;
  v.set_chat([&](ChatTo to, std::string_view, std::string_view text) {
    sent.emplace_back(to, std::string(text));
    return true;
  });
  const auto enter = [&](const char* line) {
    v.type(line);
    v.key(EditKey::enter);
    c.run();
  };
  c.execute("con_chat 3");
  c.run();
  enter("go quad");
  CHECK(sent.size() == 1 && sent[0].first == ChatTo::team && sent[0].second == "go quad");
  CHECK(v.suggestions().empty());
  c.execute("con_chat 0");
  c.run();
  v.game_chat = 2;  // the game's own: said to the team
  CHECK(v.chat_mode() == 3);
  enter("rl up");
  v.game_chat = 1;
  enter("gg");
  v.game_chat = 0;
  enter("con_chat 1");  // a command
  CHECK(sent.size() == 3 && sent[1].first == ChatTo::team && sent[2].first == ChatTo::all);
  CHECK(c.cvars().find("con_chat")->value == "1");
  v.game_chat = -1;  // con_chat rules again
  CHECK(v.chat_mode() == 1);
}

// condump writes the scrollback to a plain name in the game's folder, never elsewhere.
TEST(condump_writes_the_lines_to_a_text_file) {
  Console c;
  ConsoleView v(c);
  c.execute("condump");
  c.run();
  CHECK(v.lines().back().text == "The console can't be written to a file here.");
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_condump_test";
  std::filesystem::remove_all(dir);
  v.set_dump_folder(dir.string());
  c.print("first line");
  c.execute("condump ../away");
  c.run();
  CHECK(v.lines().back().text == "Give the file a plain name, without a folder.");
  c.execute("condump match1");
  c.run();
  std::ifstream in(dir / "match1.txt");
  std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(all.find("first line\n") != std::string::npos);
  CHECK(v.lines().back().text.starts_with("Dumped console text to "));
  CHECK(c.cvars().find("con_clock") != nullptr && !v.clock_shown());
  c.execute("con_clock 1");
  c.run();
  CHECK(v.clock_shown());
  std::filesystem::remove_all(dir);
}

// remotemode sends typed lines to the game's rcon, the prompt `#`, a slash still running a command
// here; clearnotify lets the notify lines go; con_scale and the foot are the view's to report.
TEST(remotemode_clearnotify_scale_and_the_foot) {
  Console c;
  ConsoleView v(c);
  c.execute("remotemode");
  c.run();
  CHECK(!v.remote() && v.lines().back().text == "There is no remote console in this game.");
  std::vector<std::string> sent;
  c.add_command("rcon", "", [&](Console&, const Args& a) { sent.push_back(a.rest(0)); });
  c.cvars().add({.name = "rcon_address", .fallback = "", .kind = CvarKind::text});
  c.execute("remotemode");  // no server named, none set: how to name one
  c.run();
  CHECK(!v.remote() && v.lines().back().text == "Usage: remotemode <address> <password>");
  c.execute("remotemode 10.0.0.2:27910 secret");
  c.run();
  CHECK(v.remote() && v.prompt() == "# ");
  CHECK(c.cvars().find("rcon_address")->value == "10.0.0.2:27910");
  const auto enter = [&](const char* line) {
    v.type(line);
    v.key(EditKey::enter);
    c.run();
  };
  enter("status");
  enter("/con_chat 1");  // a slash: run here
  CHECK(sent.size() == 1 && sent[0] == "status");
  CHECK(c.cvars().find("con_chat")->value == "1");
  enter("/remotemode");
  CHECK(!v.remote() && v.prompt() == "] ");
  c.print("a line");
  CHECK(!v.notify().empty());
  c.execute("clearnotify");
  c.run();
  CHECK(v.notify().empty());
  CHECK(v.text_scale() == 1);
  c.execute("con_scale 2");
  c.run();
  CHECK(v.text_scale() == 2);
  v.set_foot("maps/dm2.bsp 40% (90 KB)", 0.4);
  CHECK(v.foot() == "maps/dm2.bsp 40% (90 KB)" && v.foot_progress() == 0.4);
}

// A game's held key: `+duck` crouches while the key is down.
TEST(a_held_key_is_a_button) {
  Console c;
  ConsoleView v(c);
  const Button& talk = c.add_button("duck", "+duck: crouch while held");
  c.execute("bind v +duck");
  c.key("v", true);
  c.run();
  CHECK(talk.down());
  c.key("v", false);
  c.run();
  CHECK(!talk.down());
}

// The panel's background: a flat colour by default, a gradient and an image from cfg lines.
TEST(background_from_cvars) {
  Console c;
  ConsoleView v(c);
  Background b = v.background();
  CHECK(b.top == b.bottom && b.image.empty());
  c.execute("con_bgtop #203048; con_bgbottom 0a0c10; con_bgalpha 0.6; con_bgimage gfx/conback.png");
  b = v.background();
  CHECK(b.top == (Rgb{0x20, 0x30, 0x48}) && b.bottom == (Rgb{0x0a, 0x0c, 0x10}));
  CHECK(b.alpha == 0.6 && b.image == "gfx/conback.png");
  c.execute("con_bgbottom blue");  // not a colour: the default
  CHECK(v.background().bottom == (Rgb{0x10, 0x14, 0x1c}));
  Rgb x;
  CHECK(!parse_rgb("12345", x) && !parse_rgb("gg0000", x) && parse_rgb("#FFffFF", x));
}

TEST(editing_and_history) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "cl_interp", .fallback = "100"});
  v.type("ecjo");
  v.key(EditKey::left);
  v.key(EditKey::backspace);
  v.type("h");
  CHECK(v.input() == "echo" && v.cursor() == 3);
  v.key(EditKey::end);
  v.type(" one");
  v.key(EditKey::enter);
  v.type("/cl_i");
  v.key(EditKey::tab);
  CHECK(v.input() == "/cl_interp ");
  v.key(EditKey::enter);
  v.key(EditKey::up);  // an empty line: the arrows walk the history
  CHECK(v.input() == "/cl_interp ");
  v.key(EditKey::up);
  CHECK(v.input() == "echo one");
  v.key(EditKey::down);
  v.key(EditKey::down);
  CHECK(v.input().empty());
}

// con_history: only the newest typed lines are walked back through.
TEST(con_history_keeps_the_newest_lines) {
  Console c;
  ConsoleView v(c);
  c.execute("con_history 2");
  for (const char* line : {"echo a", "echo b", "echo c"}) {
    v.type(line);
    v.key(EditKey::enter);
  }
  CHECK(v.history().size() == 2 && v.history().front() == "echo b");
}

// The list under the input line: every name the typed word starts, the most submitted first.
TEST(suggestions_rank_by_use_then_length) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "sv_deathmatch", .fallback = "0"});
  c.cvars().add({.name = "sv_demotxt", .fallback = "1"});
  c.cvars().add({.name = "sv_delay", .fallback = "5"});
  v.type("/sv_de");
  std::vector<Suggestion> s = v.suggestions();
  CHECK(s.size() == 3 && s[0].name == "sv_delay" && s[1].name == "sv_demotxt");
  CHECK(s[0].kind == Suggestion::Kind::cvar && s[0].detail == "5");
  CHECK(v.hint() == "lay");
  v.key(EditKey::enter);  // nothing selected: the line runs as typed (and fails, unknown)
  for (int i = 0; i < 2; ++i) {
    v.type("/sv_deathmatch 1");
    v.key(EditKey::enter);
  }
  c.run();
  v.type("/sv_dea");
  CHECK(v.hint() == "thmatch");
  v.type("");
  v.key(EditKey::backspace);
  v.key(EditKey::backspace);  // "/sv_d": the most used one is still on top
  s = v.suggestions();
  CHECK(s.size() == 3 && s[0].name == "sv_deathmatch" && s[0].uses == 2);
  v.key(EditKey::tab);
  CHECK(v.input() == "/sv_deathmatch " && v.suggestions().empty());
  c.execute("con_suggest 2");
  v.key(EditKey::backspace);
  CHECK(v.suggestions().size() == 1);  // the whole name still matches itself
  v.type(" ");
  CHECK(v.suggestions().empty());  // a blank after the word: no list
  while (!v.input().empty()) v.key(EditKey::backspace);
  v.type("/sv_d");
  CHECK(v.suggestions().size() == 2);  // con_suggest caps the list
}

// Up and Down move through the list; Enter on a cvar waits for its value, on a command runs it.
TEST(arrows_select_and_enter_takes_the_selection) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "r_bloom", .fallback = "0.04"});
  c.cvars().add({.name = "r_taa", .fallback = "1"});
  c.execute("alias r_go \"r_bloom 0.3\"");
  v.type("/r_");
  CHECK(v.selected() == -1 && v.suggestions().size() == 3);
  v.key(EditKey::down);
  v.key(EditKey::down);
  CHECK(v.selected() == 1 && v.suggestions()[1].name == "r_taa");
  CHECK(v.hint() == "taa");
  v.key(EditKey::up);
  v.key(EditKey::up);
  CHECK(v.selected() == -1);
  v.key(EditKey::up);  // past the top wraps to the bottom
  CHECK(v.selected() == 2 && v.suggestions()[2].name == "r_bloom");
  v.key(EditKey::enter);
  CHECK(v.input() == "/r_bloom " && v.history().empty());  // waiting for the value
  v.type("0");
  v.key(EditKey::enter);
  c.run();
  CHECK(c.cvars().find("r_bloom")->number == 0);
  v.type("/r_g");
  v.key(EditKey::down);
  CHECK(v.suggestions()[0].kind == Suggestion::Kind::alias);
  v.key(EditKey::enter);  // an alias runs
  c.run();
  CHECK(c.cvars().find("r_bloom")->number == 0.3 && v.input().empty());
  c.execute("con_chat 1");
  v.type("r_");  // chat in a multiplayer game: no list without a /
  CHECK(v.suggestions().empty());
}

TEST(scrollback_notify_and_triggers_on_game_lines) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk);
  c.execute("con_lines 64; alias gg \"say gg\"; msg_trigger gg \"wins the race\"");
  for (int i = 0; i < 100; ++i) v.add(LineKind::output, "line " + std::to_string(i));
  CHECK(v.lines().size() == 64 && v.lines().front().text == "line 36");
  auto page = v.page(10);
  CHECK(page.size() == 10 && page.back()->text == "line 99");
  v.key(EditKey::page_up);
  CHECK(v.page(10).back()->text == "line 91");
  v.frame(10);
  v.add(LineKind::server, "Ada wins the race");
  c.run();
  const auto n = v.notify();
  CHECK(n.size() == 2 && n[0]->kind == LineKind::server && n[1]->text == "player: gg");
  v.frame(5);
  CHECK(v.notify().empty());
}

TEST(a_print_ending_in_its_newline_is_one_line_not_two) {
  Console c;
  ConsoleView v(c);
  v.add(LineKind::output, "You got the shells\n");
  v.add(LineKind::output, "You got the nails\n");
  CHECK(v.lines().size() == 2 && v.lines()[0].text == "You got the shells" &&
        v.lines()[1].text == "You got the nails");
  v.add(LineKind::output, "\n");  // a blank line asked for stays one
  v.add(LineKind::output, "a\n\nb\n");
  CHECK(v.lines().size() == 6 && v.lines()[2].text.empty() && v.lines()[4].text.empty() &&
        v.lines()[5].text == "b");
}

TEST(a_selection_copies_the_text_between_its_ends_either_way_round) {
  Console c;
  ConsoleView v(c);
  v.add(LineKind::output, "first line");
  v.add(LineKind::output, "second line");
  v.add(LineKind::output, "third line");
  const uint64_t n0 = v.lines()[0].n;
  CHECK(!v.has_selection());
  v.select_begin({n0 + 2, 5});  // dragged up from "third|"
  v.select_to({n0, 6});         // to "first |line"
  CHECK(v.has_selection());
  CHECK(v.selected_text() == "line\nsecond line\nthird");
  v.select_begin({n0 + 1, 3});  // a press that does not move selects nothing
  CHECK(!v.has_selection() && v.selected_text().empty());
  v.select_to({n0 + 1, 99});  // past a line's end: to its end
  CHECK(v.selected_text() == "ond line");
  v.select_clear();
  CHECK(!v.has_selection());
}

TEST(a_line_is_rewritten_in_place_while_it_is_kept) {
  Console c;
  ConsoleView v(c);
  c.print("Loading base1... 0.1s");
  const uint64_t n = v.newest();
  c.print("Added packfile baseq2/pak0.pak");
  CHECK(v.rewrite(n, "Loading base1... 2.4s"));
  CHECK(v.lines().size() == 2 && v.lines()[0].text == "Loading base1... 2.4s" &&
        v.lines()[1].text == "Added packfile baseq2/pak0.pak");
  CHECK(!v.rewrite(n + 5, "nothing there"));
  v.clear();
  CHECK(!v.rewrite(n, "scrolled out"));
}

TEST(con_notify_hides_the_notify_lines_and_toggling_lets_them_go) {
  Console c;
  ConsoleView v(c);
  v.add(LineKind::server, "one");
  v.add(LineKind::server, "two");
  CHECK(v.notify().size() == 2);
  c.execute("con_notify 0");
  c.run();
  CHECK(v.notify().empty());
  c.execute("con_notify 1");
  c.run();
  v.toggle();  // opened: the lines so far go from the notify area, not from the scrollback
  v.toggle();
  CHECK(v.notify().empty() && v.lines().size() >= 2);
  v.add(LineKind::server, "three");
  CHECK(v.notify().size() == 1 && v.notify()[0]->text == "three");
  c.execute("con_clearnotify 0");
  c.run();
  v.toggle();
  CHECK(v.notify().size() == 1);  // kept
  v.clear_notify();               // a demo's jump lets them go whatever con_clearnotify says
  CHECK(v.notify().empty());
  v.add(LineKind::server, "four");
  CHECK(v.notify().size() == 1 && v.notify()[0]->text == "four");
}

TEST(con_scroll_brings_the_newest_lines_back_on_a_new_line_or_a_key) {
  Console c;
  ConsoleView v(c);
  for (int i = 0; i < 40; ++i) v.add(LineKind::output, "line " + std::to_string(i));
  v.key(EditKey::page_up);
  v.add(LineKind::output, "new");
  CHECK(v.scroll() == 8);  // 0: a new line leaves the view where it was scrolled
  v.type("x");
  CHECK(v.scroll() == 8);
  c.execute("con_scroll 1");
  v.add(LineKind::output, "newer");
  CHECK(v.scroll() == 0 && v.page(1).back()->text == "newer");
  c.execute("con_scroll 2");
  v.key(EditKey::page_up);
  v.add(LineKind::output, "newest");
  CHECK(v.scroll() == 8);
  v.key(EditKey::page_up);  // the page keys scroll still
  CHECK(v.scroll() == 16);
  v.type("y");
  CHECK(v.scroll() == 0);
  v.key(EditKey::page_up);
  v.key(EditKey::left);
  CHECK(v.scroll() == 0);
}

// A game's stamps: each line added while they are on keeps the time apart from its text; condump
// writes it, the text and its search never hold it.
TEST(stamps_keep_the_time_beside_each_line) {
  Console c;
  ConsoleView v(c);
  v.add(LineKind::output, "before");
  CHECK(v.lines().back().stamp.empty());
  v.stamps = true;
  v.stamp_format = "<%Y>";
  v.add(LineKind::output, "two\nlines");
  CHECK(v.lines().back().text == "lines" && v.lines().back().stamp.size() == 6);
  CHECK(v.lines().back().stamp.front() == '<' && v.lines()[1].stamp == v.lines().back().stamp);
  v.stamp_format.clear();  // no form: no stamp
  v.add(LineKind::output, "bare");
  CHECK(v.lines().back().stamp.empty());
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_stamp_dump_test";
  std::filesystem::remove_all(dir);
  v.set_dump_folder(dir.string());
  c.execute("condump stamped");
  c.run();
  std::ifstream in(dir / "stamped.txt");
  std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(all.find(v.lines()[2].stamp + "lines\n") != std::string::npos);
  CHECK(all.find("before\n") != std::string::npos);
  std::filesystem::remove_all(dir);
}

// A game's own Tab: the word completed to what the matches share, the matches listed in its form,
// past the threshold asked about first.
TEST(a_games_tab_completes_the_shared_part_and_lists_the_matches) {
  Console c;
  ConsoleView v(c);
  for (const char* n : {"zfoo_a", "zfoo_bb", "zfoo_c"}) c.add_command(n, "", {});
  c.cvars().add({.name = "zfoo_var", .fallback = "1"});
  v.list_mode = 0;
  v.set_open(true);
  for (const char ch : std::string("zf")) v.type(std::string(1, ch));
  v.key(EditKey::tab);
  CHECK(v.input() == "zfoo_");
  const size_t n = v.lines().size();
  CHECK(v.lines()[n - 5].text == "]zfoo_" && v.lines()[n - 4].text == "zfoo_a");
  CHECK(v.lines().back().text == "zfoo_var");
  v.list_mode = 2;
  v.key(EditKey::tab);
  bool commands = false, variable = false;
  for (const ViewLine& l : v.lines()) {
    commands |= l.text == "3 possible commands:";
    variable |= l.text == "1 possible variable:";
  }
  CHECK(commands && variable);
  // Past the threshold the first Tab asks, the second lists.
  v.list_threshold = 2;
  v.list_mode = 1;
  v.key(EditKey::tab);
  CHECK(v.lines().back().text == "Press TAB again to display all 4 possibilities.");
  v.key(EditKey::tab);
  CHECK(v.lines().back().text.starts_with("zfoo_a"));
  // One match: the whole name and a space.
  v.type("b");
  v.key(EditKey::tab);
  CHECK(v.input() == "zfoo_bb ");
}
