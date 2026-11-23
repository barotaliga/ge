// The console: dispatch, the buffer and wait, aliases, exec from a file, problems with their
// file and line, a server's stuffed text, buttons, completion and suggestions; cfg_save under a
// meta-cvar.
//
// ge:summary Tests the console: dispatch, wait, aliases, exec, problems, stuffing, buttons
// ge:keywords test console dispatch buffer wait alias exec problem stuff button complete suggest
// ge:skill console testing
#include <map>
#include <string>

#include "check.hpp"
#include "console/cfg.hpp"
#include "console/console.hpp"

using namespace ge::console;

namespace {

// A console reading its files from `files`.
void use_files(Console& c, std::map<std::string, std::string>& files) {
  c.set_files(
      [&files](std::string_view name, std::string& text) {
        const auto it = files.find(std::string(name));
        if (it == files.end()) return false;
        text = it->second;
        return true;
      },
      [&files](std::string_view name, std::string_view text) {
        files[std::string(name)] = std::string(text);
        return true;
      });
}

}  // namespace

TEST(a_cvar_name_prints_or_sets) {
  Console c;
  c.cvars().add({.name = "r_gamma", .fallback = "1"});
  c.execute("r_gamma; r_gamma 1.2; r_gamma");
  CHECK(c.output().size() == 2);
  CHECK(c.output()[0] == "\"r_gamma\" is \"1\" (default)");
  CHECK(c.output()[1] == "\"r_gamma\" is \"1.2\" (default \"1\")");
}

TEST(wait_splits_frames) {
  Console c;
  c.append("echo a; wait; echo b");
  CHECK(c.run() == 2);
  CHECK(c.output().size() == 1 && c.pending());
  c.run();
  CHECK(c.output().size() == 2 && c.output()[1] == "b" && !c.pending());
}

TEST(sleep_holds_the_buffer_until_the_hosts_time_passes) {
  Console plain;  // a player's console: no recipe commands at all
  plain.execute("sleep 1");
  CHECK(!plain.problems().empty() && plain.commands().count("sleep") == 0);
  Console c;
  add_recipe_commands(c);
  double now = 10.0;
  c.set_clock([&now] { return now; });
  c.append("echo a; sleep 1.5; echo b");
  CHECK(c.run() == 2);
  CHECK(c.sleeping());
  now = 11.0;
  CHECK(c.run() == 0);  // half a second still to go
  c.past_sleep([&c] {
    c.append("echo startup");
    c.run();
  });  // a host's config, ahead of it
  CHECK(c.output().back() == "startup" && c.sleeping());
  c.execute("echo now");  // the host's own line runs past the sleep; the held one waits on
  CHECK(c.output().size() == 3 && c.output()[2] == "now" && c.sleeping() && c.pending());
  now = 11.5;
  c.run();
  CHECK(!c.sleeping() && c.output().size() == 4 && c.output()[3] == "b" && !c.pending());
  // No clock handed in: one frame, like wait.
  Console w;
  add_recipe_commands(w);
  w.append("echo a; sleep 3; echo b");
  w.run();
  CHECK(w.output().size() == 1 && w.pending());
  w.run();
  CHECK(w.output().size() == 2);
  // A word that is no number is refused with the usage.
  Console bad;
  add_recipe_commands(bad);
  bad.execute("sleep soon");
  CHECK(!bad.problems().empty());
  bad.clear_problems();
  bad.execute("sleep inf");  // never a sleep that never ends
  CHECK(!bad.problems().empty());
}

TEST(aliases_with_arguments_run_before_what_follows) {
  Console c;
  c.execute("alias hi \"echo hello $1; echo $*\"; hi there \"you two\"; echo after");
  CHECK(c.output().size() == 3);
  CHECK(c.output()[0] == "hello there");
  CHECK(c.output()[1] == "there you two");
  CHECK(c.output()[2] == "after");
}

TEST(expansion_happens_when_an_alias_runs_not_when_defined) {
  Console c;
  c.cvars().add({.name = "x", .fallback = "1"});
  c.execute("alias show \"echo x is $x\"; set x 2; show");
  CHECK(c.output().back() == "x is 2");
}

TEST(runaway_aliases_stop_with_a_problem) {
  Console c;
  c.execute("alias loop loop; loop; echo still here");
  CHECK(!c.problems().empty());
  CHECK(c.problems()[0].what.find("'loop' calls itself") != std::string::npos);
  CHECK(c.output().back() == "still here");
  // A loop that waits is a loop on purpose: it runs on, one pass a frame.
  Console w;
  w.cvars().add({.name = "n", .fallback = "0"});
  w.append("alias tick \"inc n; wait; tick\"; tick");
  for (int f = 0; f < 200; ++f) w.run();
  CHECK(w.problems().empty());
  CHECK(w.cvars().find("n")->number == 200);
}

TEST(exec_reads_files_and_problems_name_file_and_line) {
  Console c;
  std::map<std::string, std::string> files{
      {"a.cfg", "echo one\nexec b\necho three\n"},
      {"b.cfg", "echo two\n\nr_bloon 1\n"},
  };
  use_files(c, files);
  c.cvars().add({.name = "r_bloom", .fallback = "1"});
  c.execute("exec a; exec missing");
  CHECK(c.output()[0] == "one" && c.output()[1] == "two");
  CHECK(c.problems().size() == 2);
  CHECK(c.problems()[0].where.file == "b.cfg" && c.problems()[0].where.line == 3);
  CHECK(c.problems()[0].next == "did you mean 'r_bloom'?");
  CHECK(c.output()[2] == "b.cfg:3: unknown command 'r_bloon' (did you mean 'r_bloom'?)");
  CHECK(c.output()[3] == "three");
  CHECK(c.problems()[1].what == "couldn't exec missing.cfg: not found");
}

TEST(a_server_stuffs_only_what_is_allowed) {
  Console c(false);
  c.cvars().add({.name = "sensitivity", .fallback = "3", .flags = kArchive});
  c.cvars().add({.name = "sv_fps", .fallback = "77", .flags = kServer | kServerInfo});
  c.add_command("quit", "quit", [](Console& con, const Args&) { con.print("quitting"); });
  c.append("echo hello; quit; sensitivity 9; set sv_fps 60", Source::server);
  c.run();
  CHECK(c.output()[0] == "hello");
  CHECK(c.cvars().find("sensitivity")->value == "3");
  CHECK(c.cvars().find("sv_fps")->value == "60");
  CHECK(c.problems().size() == 2);
  // A client's own console cannot set the server's cvar.
  c.execute("sv_fps 10");
  CHECK(c.cvars().find("sv_fps")->value == "60");
}

TEST(buttons_follow_keys) {
  Console c;
  Button& jump = c.add_button("jump", "jump while held");
  c.execute("bind space +jump; bind ctrl+space \"echo mod\"; bind mouse1 +jump");
  c.key("space", true);
  c.key("MOUSE1", true);
  c.run();
  CHECK(jump.down() && jump.held == 2 && jump.pressed);
  c.key("space", false);
  c.run();
  CHECK(jump.down());
  c.key("mouse1", false);
  c.run();
  CHECK(!jump.down());
  c.key("ctrl+space", true);
  c.run();
  CHECK(c.output().back() == "mod");
  c.key("ctrl+mouse1", true);  // no ctrl bind: the key's own runs
  c.run();
  CHECK(jump.down());
}

TEST(command_line_plus_commands) {
  Console c;
  c.cvars().add({.name = "r_x", .fallback = "0"});
  CHECK(c.append_command_line({"+set", "r_x", "2", "+echo", "a b"}) == 2);
  c.run();
  CHECK(c.cvars().find("r_x")->number == 2);
  CHECK(c.output().back() == "a b");
}

// The first command on the command line needs no `+`; a `+` starts each one after it.
TEST(command_line_first_command_needs_no_plus) {
  Console c;
  c.cvars().add({.name = "r_x", .fallback = "0"});
  CHECK(c.append_command_line({"set", "r_x", "3", "+echo", "done"}) == 2);
  c.run();
  CHECK(c.cvars().find("r_x")->number == 3);
  CHECK(c.output().back() == "done");
}

// `+` and `-` mark a button's press and release; a plain command typed with a `+` runs as itself,
// and its `-` (a key bound to `+coopgame` let go) runs nothing.
TEST(a_plus_on_a_plain_command_is_dropped) {
  Console c;
  std::vector<std::string> got;
  c.add_command("coopgame", "", [&got](Console&, const Args& a) { got.push_back(a.raw); });
  Button& jump = c.add_button("jump", "");
  c.execute("+coopgame 2 expert");
  CHECK(got.size() == 1 && got[0] == "coopgame 2 expert");
  c.execute("-coopgame");
  CHECK(got.size() == 1 && c.problems().empty());
  c.execute("+jump");
  CHECK(jump.down());
  c.execute("-jump");
  CHECK(!jump.down());
  c.execute("+nothing");
  CHECK(!c.problems().empty() && c.problems().back().what == "unknown command '+nothing'");
}

TEST(completion_and_suggestions) {
  Console c;
  c.cvars().add({.name = "cl_interp", .fallback = "100"});
  c.execute("alias cl_go \"echo go\"");
  const auto m = c.complete("CL_");
  CHECK(m.size() == 2 && m[0] == "cl_go" && m[1] == "cl_interp");
  CHECK(c.suggest("cl_intrp") == "cl_interp");
  CHECK(c.suggest("zzzzzz").empty());
  CHECK(describe(c, "cl_interp").find("(default \"100\"") != std::string::npos);
}

TEST(macros_expand_ahead_of_cvars) {
  Console c;
  int health = 75;
  c.add_macro("health", [&health] { return std::to_string(health); });
  c.execute("echo hp $health");
  health = 40;
  c.execute("echo hp $health");
  CHECK(c.output()[0] == "hp 75" && c.output()[1] == "hp 40");
}

TEST(a_command_taken_away_is_gone) {
  Console c;
  c.add_command("NewGame", "a new game", [](Console& con, const Args&) { con.print("new"); });
  CHECK(c.has_command("newgame"));
  CHECK(c.remove_command("NEWGAME"));
  CHECK(!c.has_command("newgame"));
  CHECK(!c.remove_command("newgame"));
}

TEST(a_host_takes_the_lines_no_command_names) {
  Console c;
  std::vector<std::string> taken;
  c.set_unknown([&](const Args& a) {
    if (a[0] == "nothing") return false;
    taken.push_back(a.raw);
    return true;
  });
  c.execute("bot1 skill expert\nskill novice\nnothing here\n");
  CHECK(taken.size() == 2);
  CHECK(taken.size() == 2 && taken[0] == "bot1 skill expert" && taken[1] == "skill novice");
  CHECK(!c.problems().empty() && c.problems().back().what == "unknown command 'nothing'");
}

TEST(cfg_save_keeps_the_players_value_under_a_meta) {
  Console c;
  c.cvars().add({.name = "cl_fov", .fallback = "90", .flags = kArchive, .min = 10, .max = 140});
  c.execute("cl_fov 100; meta cl_comp 1 \"cl_fov 120\"; cl_comp 1");
  CHECK(c.cvars().find("cl_fov")->value == "100");  // the player's own stands
  c.execute("cl_comp 0; reset cl_fov");
  c.cvars().find("cl_fov")->explicit_set = false;  // as a fresh start would have it
  c.execute("cl_comp 1");
  CHECK(c.cvars().find("cl_fov")->value == "120");
  CHECK_MSG(c.save_text().find("seta cl_fov \"90\"\n") != std::string::npos, c.save_text());
}

TEST(a_tap_sees_every_line_printed_and_the_sink_still_gets_it) {
  ge::console::Console c{false};
  std::string seen;
  c.set_tap([&seen](std::string_view line) { seen.append(line).push_back('|'); });
  c.print("one");
  c.print("two");
  CHECK(seen == "one|two|");
  CHECK(c.output().size() == 2);
  c.set_tap(nullptr);
  c.print("three");
  CHECK(seen == "one|two|");
}
