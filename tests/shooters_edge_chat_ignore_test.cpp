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
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
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

TEST(a_kept_chat_line_sounds_is_filtered_stamped_and_marked) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
  const cl::ChatClock at{9, 5, 42};
  const auto frame = [&](std::vector<cl::DemoPrint> prints) {
    s.started.clear();
    cl::drop_ignored(g, s, prints, 0, nullptr, at);
    return prints;
  };
  // Every chat line plays the talk sound from your own place, heard everywhere; other prints none.
  std::vector<cl::DemoPrint> got = frame({{3, "bob: hi\n"}, {2, "bob was gibbed\n"}});
  CHECK(got.size() == 2 && s.started.size() == 1);
  CHECK(std::string(s.started[0].name) == "misc/talk.wav" && s.started[0].attenuation == 0);
  CHECK(s.started[0].entity == 1);
  // A player hears the game's own file for a speaker known; only the volume is his.
  c.execute("s_mm1_file misc/beep.wav; s_mm1_volume 0.5; s_otherchat_file misc/other.wav");
  c.run();
  frame({{3, "bob: hi\n"}, {3, "nobody: hi\n"}});
  CHECK(s.started.size() == 2 && std::string(s.started[0].name) == "misc/talk.wav");
  CHECK(s.started[0].volume == 0.5f);
  CHECK(std::string(s.started[1].name) == "misc/other.wav");  // no speaker: his file
  // Watching, every file is his.
  s.spectator[0] = true;
  frame({{3, "bob: hi\n"}});
  CHECK(s.started.size() == 1 && std::string(s.started[0].name) == "misc/beep.wav");
  s.spectator[0] = false;
  // 2: team lines only; 0 none (the old name still reaches it).
  c.execute("s_chat_custom 2");
  c.run();
  frame({{3, "bob: hi\n"}, {3, "(bob): go\n"}});
  CHECK(s.started.size() == 1);
  c.execute("cl_chatsound 0");
  c.run();
  frame({{3, "(bob): go\n"}});
  CHECK(s.started.empty());
  // msg_filter 2 hides team lines, 1 the rest.
  c.execute("msg_filter 2");
  c.run();
  got = frame({{3, "bob: hi\n"}, {3, "(bob): go\n"}});
  CHECK(got.size() == 1 && got[0].text == "bob: hi\n");
  c.execute("msg_filter 1");
  c.run();
  got = frame({{3, "bob: hi\n"}, {3, "eye: yo\n"}, {3, "(bob): go\n"}});
  CHECK(got.size() == 1 && got[0].text == "(bob): go\n");
  c.execute("msg_filter 0");
  // The time: hours and minutes; the seconds only while you watch or before the match.
  c.execute("con_timestamps 2");
  c.run();
  got = frame({{3, "bob: hi\n"}});
  CHECK(got[0].text == " 9:05 bob: hi\n");
  s.serverinfo = {{"teamplay", "2"}, {"status", "Standby"}};
  got = frame({{3, "bob: hi\n"}, {2, "the match begins\n"}});
  CHECK(got[0].text == " 9:05:42 bob: hi\n" && got[1].text == "the match begins\n");
  c.execute("con_timestamps 0");
  // A line that names you, not at its start, begins with the mark.
  c.execute("con_highlight 2; con_highlight_mark \">> \"");
  c.run();
  got = frame({{3, "bob: hi me\n"}, {3, "me: hi\n"}, {1, "bob rides me's rocket\n"}});
  CHECK(got[0].text == ">> bob: hi me\n" && got[1].text == "me: hi\n");
  CHECK(got[2].text == ">> bob rides me's rocket\n");
}
