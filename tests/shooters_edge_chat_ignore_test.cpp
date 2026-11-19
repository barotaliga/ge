// CHAT IGNORE ON A CLASSIC SERVER JOINED: the printed lines read as chat (everyone's, team,
// spectators', a TV viewer's) by the longest name that fits, and the lists and weapon lines drop
// them from a frame's prints; a new level clears the players' flags.
//
// ge:summary Tests: the classic chat lines read for the ignore lists
// ge:keywords test chat-ignore classic-chat drop_ignored chat_line_of
// ge:skill net testing
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/chat_ignore.hpp"

namespace cl = shooters_edge::client;
namespace con = ge::console;

namespace {

cl::DemoState room() {
  cl::DemoState s;
  s.names[0] = "me";
  s.names[1] = "bob";
  s.names[2] = "bobby";
  s.names[3] = "eye";
  s.spectator[3] = true;
  s.teams[0] = s.teams[1] = "red";
  s.teams[2] = "blue";
  s.player = 0;
  s.serverinfo = {{"teamplay", "2"}, {"status", "Countdown"}};
  return s;
}

}  // namespace

TEST(classic_lines_read_as_chat) {
  const cl::DemoState s = room();
  const auto line = [&](const char* text) { return cl::chat_line_of(s, {3, text}); };
  CHECK(line("bobby: hi\n").place == 2);  // the longest name that fits
  CHECK(line("bob: hi\n").place == 1 && line("bob: hi\n").kind == con::ChatKind::everyone);
  CHECK(line("(bob): go\n").kind == con::ChatKind::team);
  CHECK(line("(bobby): go\n").kind == con::ChatKind::none);  // another team's
  CHECK(line("eye: hello\n").kind == con::ChatKind::spectator);
  CHECK(line("#12:viewer: hi\n").kind == con::ChatKind::tv);
  CHECK(cl::chat_line_of(s, {2, "bob: hi\n"}).kind == con::ChatKind::none);  // not chat
  CHECK(line("[SPEC] eye: x\n").kind == con::ChatKind::none);                // only while you watch
}

TEST(the_lists_drop_a_frames_lines) {
  con::Console c;
  cl::ClassicIgnore g;
  cl::DemoState s = room();
  cl::add_classic_ignore(c, g, s);
  c.execute("ignore bob");
  c.execute("ignore_no_weapon 1");
  c.run();
  std::vector<cl::DemoPrint> prints{{3, "bob: hi\n"},
                                    {3, "bobby: hi\n"},
                                    {2, "no weapon.\n"},
                                    {3, "(bob): go\n"},
                                    {1, "bob died\n"}};
  cl::drop_ignored(g, s, prints, 0);
  CHECK(prints.size() == 3);
  CHECK(prints[0].text == "bobby: hi\n" && prints[1].text == "(bob): go\n");
  s.levels = 1;  // a new level: the flags go
  std::vector<cl::DemoPrint> again{{3, "bob: hi\n"}};
  cl::drop_ignored(g, s, again, 1);
  CHECK(again.size() == 1);
}
