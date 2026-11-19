// engine/server/cmdfilter: commands added with each action, matched by their first word in any
// case, listed, removed by name, number and all; refusals for a bad action or a missing text; a
// game's own spellings; the log line. engine/server/bans: the blackhole list's words and match.
//
// ge:summary Test: the command filters a server holds its clients to, and its blackhole list
// ge:keywords test filtercmd cmdfilter_match cmdfilter_command blackhole blackholed
// ge:skill net testing
#include "check.hpp"
#include "net/address.hpp"
#include "server/bans.hpp"
#include "server/cmdfilter.hpp"

using namespace ge::server;

TEST(a_filtered_command_is_matched_by_its_first_word_and_does_what_its_entry_says) {
  CmdFilters f;
  std::string out;
  CHECK(cmdfilter_match(f, "god") == nullptr);
  CHECK(cmdfilter_command(f, "filtercmd_add god", out) && out == "added 1 god ignore\n");
  CHECK(cmdfilter_command(f, "filtercmd_add Give print \"Not here, friend.\"", out));
  CHECK(cmdfilter_command(f, "filtercmd_add noclip stuff \"echo no\"", out));
  CHECK(cmdfilter_command(f, "filtercmd_add notarget kick cheat words", out));
  const CmdFilter* g = cmdfilter_match(f, "GIVE all");
  CHECK(g != nullptr && g->action == FilterDo::print && g->text == "Not here, friend.");
  CHECK(cmdfilter_match(f, "  god") != nullptr && cmdfilter_match(f, "godmode") == nullptr);
  CHECK(cmdfilter_match(f, "notarget")->text == "cheat words");
  CHECK(cmdfilter_command(f, "filtercmd_list", out));
  CHECK(out ==
        "1 god ignore\n2 give print \"Not here, friend.\"\n3 noclip stuff \"echo no\"\n"
        "4 notarget kick \"cheat words\"\n4 commands filtered\n");
  CHECK(cmdfilter_command(f, "filtercmd_add god log", out) && out == "added 4 god log\n");
  CHECK(cmdfilter_command(f, "filtercmd_add x shout", out));
  CHECK(out == "filtercmd_add: shout is not an action; give ignore, log, print, stuff or kick\n");
  CHECK(cmdfilter_command(f, "filtercmd_add x print", out));
  CHECK(out == "filtercmd_add: print needs the text to show\n");
  CHECK(cmdfilter_command(f, "filtercmd_remove 1", out) && out == "removed give\n");
  CHECK(cmdfilter_command(f, "filtercmd_remove god", out) && out == "removed god\n");
  CHECK(cmdfilter_command(f, "filtercmd_remove 9", out));
  CHECK(out == "filtercmd_remove: there is no entry 9; filtercmd_list numbers them\n");
  CHECK(cmdfilter_command(f, "filtercmd_remove all", out) && f.entries.empty());
  CHECK(!cmdfilter_command(f, "kick 3", out));
  static const char* const kWords[3] = {"addfiltercmd", "delfiltercmd", "listfiltercmds"};
  CHECK(cmdfilter_command(f, "addfiltercmd god", out, &kWords) && f.entries.size() == 1);
  CHECK(cmdfilter_command(f, "listfiltercmds", out, &kWords) && out.find("god") == 2);
  CHECK(cmdfilter_log_line("Ranger", "10.0.0.2:27901", "god 1") ==
        "Ranger[10.0.0.2:27901]: issued banned command: god");
}

TEST(a_blackholed_address_is_matched_by_its_pattern_until_its_time) {
  BanList holes;
  std::string out;
  ge::net::Address a, b;
  CHECK(ge::net::parse_address("10.1.2.3:27910", a) && ge::net::parse_address("10.2.0.1:1", b));
  CHECK(!blackholed(holes, a, 100));
  CHECK(blackhole_command(holes, "blackhole_addip 10.1 +60", 100, out));
  CHECK(blackholed(holes, a, 100) && !blackholed(holes, b, 100) && !blackholed(holes, a, 160));
  CHECK(blackhole_command(holes, "blackhole_listip", 100, out) && out.find("10.1") == 0);
  CHECK(!blackhole_command(holes, "addip 10.2", 100, out) &&
        !ban_command(holes, "blackhole_addip 1", 100, out));
  CHECK(blackhole_command(holes, "blackhole_removeip 10.1", 100, out) && holes.entries.empty());
}
