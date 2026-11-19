// engine/server/lrcon: prefixes added, refused twice, listed, taken away; a line judged by them;
// a game's own spellings of the three commands.
//
// ge:summary Test: lrcon's allowed lines and its administrator's commands
// ge:keywords test lrcon lrcon_allows lrcon_command
// ge:skill net testing
#include "check.hpp"
#include "server/lrcon.hpp"

using namespace ge::server;

TEST(a_helper_runs_only_the_lines_an_administrator_allowed) {
  LimitedRcon l{"helper", {}};
  std::string out;
  CHECK(!lrcon_allows(l, "map q2dm1"));
  CHECK(lrcon_command(l, "lrcon_add map", out) && out == "Helpers may now run \"map\".\n");
  CHECK(lrcon_command(l, "lrcon_add map", out) && out == "\"map\" is already allowed.\n");
  CHECK(lrcon_command(l, "lrcon_add kick ", out));
  CHECK(lrcon_allows(l, "map q2dm1") && lrcon_allows(l, "kick 3") && !lrcon_allows(l, "quit"));
  CHECK(lrcon_command(l, "lrcon_list", out) && out == "map\nkick \n");
  CHECK(lrcon_command(l, "lrcon_del map", out) && !lrcon_allows(l, "map q2dm1"));
  CHECK(!lrcon_command(l, "status", out));
  const char* const words[3] = {"addlrconcmd", "dellrconcmd", "listlrconcmds"};
  CHECK(lrcon_command(l, "addlrconcmd gamemap", out, &words) && lrcon_allows(l, "gamemap x"));
}
