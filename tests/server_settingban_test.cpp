// engine/server/settingban: every match form; the query line; the entries that act, in order,
// stopping at a kick; the words added, refused, listed and removed by number, name and all; a
// game's own spellings; the log line.
//
// ge:summary Test: setting bans and user info bans: matches, hits, the administrator's words
// ge:keywords test cvarban userinfoban setting_ban_matches setting_ban_hits setting_ban_command
// ge:skill net testing
#include "check.hpp"
#include "server/settingban.hpp"

using namespace ge::server;

TEST(a_setting_ban_matches_by_each_form) {
  CHECK(setting_ban_matches("1", "1") && !setting_ban_matches("1", "10"));
  CHECK(setting_ban_matches("Gl_Modulate", "gl_modulate"));
  CHECK(setting_ban_matches("*", "x") && !setting_ban_matches("*", ""));
  CHECK(setting_ban_matches("!*", "") && !setting_ban_matches("!1", "1"));
  CHECK(setting_ban_matches("=0", "") && setting_ban_matches("=2", "2.0"));
  CHECK(setting_ban_matches(">3", "3.5") && !setting_ban_matches(">3", "3"));
  CHECK(setting_ban_matches("<1", "0.5") && !setting_ban_matches("<1", "1"));
  CHECK(setting_ban_matches("~Bot", "aimbot 2") && !setting_ban_matches("~bot", "human"));
  CHECK(setting_ban_matches("#*", "*") && setting_ban_matches("#!x", "!X"));
  CHECK(!setting_ban_matches("!~bot", "robot"));
}

TEST(entries_act_in_order_until_a_kick) {
  SettingBans b;
  std::string out;
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add gl_modulate >3", out));
  CHECK(out == "Added cvarban 1: gl_modulate >3 log\n");
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add GL_MODULATE >4 print \"Too bright.\"", out));
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add gl_modulate >5 kick bright", out));
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add gl_modulate * stuff \"echo never\"", out));
  CHECK(setting_ban_hits(b, "gl_modulate", "2").size() == 1);  // only `*`
  const auto hits = setting_ban_hits(b, "Gl_Modulate", "6");
  CHECK(hits.size() == 3 && hits[1]->text == "Too bright." && hits[2]->action == FilterDo::kick);
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add gl_modulate >3", out));
  CHECK(out == "cvarban_add: gl_modulate >3 log is listed already\n");
  CHECK(setting_ban_command(b, "cvarban", "cvarban_add x 1 shout", out));
  CHECK(out == "cvarban_add: shout is not an action; give log, print, stuff or kick\n");
  CHECK(setting_ban_command(b, "cvarban", "cvarban_list", out));
  CHECK(out.rfind("id setting ", 0) == 0 && out.find("2  GL_MODULATE ") != std::string::npos);
  CHECK(setting_ban_command(b, "cvarban", "cvarban_remove 4", out));
  CHECK(out == "Removed cvarban 4 (gl_modulate).\n");
  CHECK(setting_ban_command(b, "cvarban", "cvarban_remove nope", out));
  CHECK(out == "No cvarban names nope.\n");
  CHECK(setting_ban_command(b, "cvarban", "cvarban_remove gl_modulate", out));
  CHECK(out == "Removed 3 cvarbans.\n" && b.entries.empty());
  CHECK(!setting_ban_command(b, "cvarban", "cvarban_check", out));
  CHECK(!setting_ban_command(b, "userinfoban", "cvarban_list", out));
  static const char* const kWords[3] = {"adduserinfoban", "deluserinfoban", "listuserinfobans"};
  CHECK(setting_ban_command(b, "userinfoban", "adduserinfoban skin ~cheat kick", out, &kWords));
  CHECK(setting_ban_command(b, "userinfoban", "deluserinfoban all", out, &kWords));
  CHECK(out == "Removed 1 userinfoban.\n");
  CHECK(setting_ban_query("gl_modulate") ==
        "cmd \x7f"
        "c gl_modulate $gl_modulate\n");
  CHECK(setting_ban_log_line("eve", "10.0.0.1:27901", "gl_modulate", "6") ==
        "eve[10.0.0.1:27901]: matched cvarban: \"gl_modulate\" is \"6\"");
}
