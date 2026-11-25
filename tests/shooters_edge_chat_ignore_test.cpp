// CHAT IGNORE ON A CLASSIC SERVER JOINED: the printed lines read as chat (everyone's, team,
// spectators', a TV viewer's) by the longest name that fits, and the lists and weapon lines drop
// them from a frame's prints; a new level clears the players' flags.
//
// ge:summary Tests: the classic chat lines read for the ignore lists
// ge:keywords test chat-ignore classic-chat drop_ignored chat_line_of
// ge:skill net testing
#include <string>
#include <vector>

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

// Quake II's chat settings: the line cleaned, the addresses kept for followip, the message
// triggers on lines that are not chat (never through a relay), its one chat sound, and the feeds:
// cl_chat_notify and the chat lines on screen.
TEST(quake2s_chat_settings_clean_follow_trigger_and_feed) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
  std::vector<std::string> ran;
  c.add_command("connect", "", [&](con::Console&, const con::Args& a) { ran.push_back(a.raw); });
  c.add_command("say_team", "", [&](con::Console&, const con::Args& a) { ran.push_back(a.raw); });
  const auto frame = [&](std::vector<cl::DemoPrint> prints, std::string centre = {}) {
    s.started.clear();
    cl::drop_ignored(g, s, prints, 0, &centre);
    c.run();
    return prints;
  };
  c.execute("cl_chat_filter 1");
  c.run();
  std::vector<cl::DemoPrint> got = frame({{3, "bob: hi\x01 there\nmore\n"}, {2, "a\x01b\n"}});
  CHECK(got[0].text == "bob: hi there\n" && got[1].text == "a\x01b\n");  // chat alone
  // followip: the newest address first, the n-th last by number.
  frame({{3, "bob: play at 10.0.0.1:27910 or 192.168.1.20\n"}, {3, "bob: 300.1.1.1 no\n"}});
  CHECK(cl::addresses_in("x 1.2.3.4:5 1.2.3 1.2.3.4.5").size() == 1);
  c.execute("followip; followip 2; followip 3");
  c.run();
  CHECK(ran.size() == 2 && ran[0] == "connect 192.168.1.20" && ran[1] == "connect 10.0.0.1:27910");
  ran.clear();
  // trigger: on a server line or the centre message that matches, not on chat; untrigger.
  c.execute("trigger \"say_team quad up\" \"*quad*\"");
  c.run();
  CHECK(g.chat.triggers.size() == 1);
  frame({{3, "bob: quad\n"}, {2, "The quad is back\n"}}, "the quad is yours");
  CHECK(ran.size() == 2 && ran[0] == "say_team quad up");
  ran.clear();
  g.chat.relay = true;  // through a relay: never
  frame({{2, "The quad is back\n"}});
  CHECK(ran.empty());
  g.chat.relay = false;
  c.execute("untrigger \"say_team quad up\" \"*quad*\"");
  c.run();
  CHECK(g.chat.triggers.empty());
  // Quake II's own cl_chat_sound rules while it is set: by number or by name, 0 none.
  g.chat.named_sound = "2";
  frame({{3, "bob: hi\n"}});
  CHECK(s.started.size() == 1 && std::string(s.started[0].name) == "misc/talk1.wav");
  g.chat.named_sound = "misc/beep.wav";
  frame({{3, "bob: hi\n"}});
  CHECK(std::string(s.started[0].name) == "misc/beep.wav");
  g.chat.named_sound = "0";
  frame({{3, "bob: hi\n"}});
  CHECK(s.started.empty());
  // The feeds: chat in the notify lines unless cl_chat_notify is 0; on screen with scr_chathud.
  ge::hud::Feed notify{4, 4.0, 0.8, 0.15};
  const ge::ui::Color ink{1, 1, 1, 1};
  cl::tell_feeds(g.chat, notify, "bob: hi", true, ink, 0);
  CHECK(notify.lines().size() == 1 && g.chat.feed.lines().empty());
  c.execute("cl_chat_notify 0; scr_chathud 2; scr_chathud_lines 2; scr_chathud_time 3");
  c.run();
  for (const char* line : {"bob: a", "bob: b", "bob: c"})
    cl::tell_feeds(g.chat, notify, line, true, ink, 0);
  cl::tell_feeds(g.chat, notify, "bob died", false, ink, 0);
  CHECK(notify.lines().size() == 2 && notify.lines().back().text == "bob died");
  CHECK(g.chat.feed.lines().size() == 2 && g.chat.feed.lines()[0].text == "bob: b");
  CHECK(g.chat.feed.lines()[0].color.r < 1);  // the alternate colour
  CHECK(g.chat.feed.strength(0, 2) == 1 && g.chat.feed.strength(0, 2.5) < 1);  // the last second
  c.execute("clearchathud");
  c.run();
  CHECK(g.chat.feed.lines().empty());
}

// The team's extras on a kept line: cl_nofake, tp_soundtrigger, the queries and msg_trigger.
TEST(the_teams_extras_on_a_kept_line) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
  g.con = &c;
  g.chat.con = &c;
  c.execute("alias got \"echo got: $triggermatch\"; msg_trigger got \"quad\" -l 3");
  c.run();
  std::vector<cl::DemoPrint> prints{{3, "bob: fake\rme: x\n"},
                                    {3, "(bob): quad up ~misc/gong\n"},
                                    {3, "bobby: f_version\n"},
                                    {3, "bobby: quad?\n"}};
  c.clear_output();
  cl::drop_ignored(g, s, prints, 100);
  CHECK(prints.size() == 4);
  CHECK(prints[0].text == "bob: fake me: x\n");  // cl_nofake 2: another's line shown plain
  CHECK(prints[1].text == "(bob): quad up \n");  // the sound word taken off
  bool gong = false;
  for (const cl::DemoSound& d : s.started)
    gong = gong || (d.name != nullptr && std::string(d.name) == "misc/gong.wav");
  CHECK(gong);
  c.run();
  std::string got;
  for (const std::string& l : c.output()) got += l + "|";
  // Both chat lines fired (the team's too); the aliases run after, reading the last line.
  CHECK_MSG(got.find("got: quad?\n|got: quad?\n|") != std::string::npos, "fired twice: " + got);
  CHECK(c.expand_text("$triggermatch").starts_with("quad"));
  std::vector<cl::DemoPrint> again{{3, "bobby: f_version\n"}};
  cl::drop_ignored(g, s, again, 105);  // answered once in 20 seconds
  s.levels = 7;                        // a new map: $triggermatch goes
  std::vector<cl::DemoPrint> none;
  cl::drop_ignored(g, s, none, 106);
  CHECK(c.expand_text("$triggermatch").empty());
}

// f_fakeshaft: answered with the beam's smoothing as cl_fakeshaft has it.
TEST(f_fakeshaft_is_answered_with_the_beams_smoothing) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
  g.con = &c;
  g.chat.con = &c;
  c.cvars().add({.name = "cl_fakeshaft", .fallback = "1"});
  std::vector<std::string> said;
  c.add_command("say", "", [&said](con::Console&, const con::Args& a) { said.push_back(a[1]); });
  c.execute("cl_fakeshaft 0.25");
  std::vector<cl::DemoPrint> prints{{3, "bobby: f_fakeshaft\n"}};
  cl::drop_ignored(g, s, prints, 100);
  c.run();
  CHECK_MSG(said.size() == 1 && said[0] == "fakeshaft 25.0%",
            said.empty() ? std::string("nothing said") : said[0]);
}

// Quake II's version reply: a line ENDING in !version, answered 1 to 2 s on, once in two minutes.
TEST(quake_ii_answers_a_line_ending_in_version_after_a_second) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  s.format = cl::DemoFormat::q2;
  cl::add_qw_ignore(c, g, s);
  g.con = &c;
  g.chat.con = &c;
  std::vector<std::string> said;
  c.add_command("say", "",
                [&said](con::Console&, const con::Args& a) { said.push_back(a.rest(0)); });
  std::vector<cl::DemoPrint> asked{{3, "bobby: which client? !version\n"}};
  cl::drop_ignored(g, s, asked, 100);
  c.run();
  CHECK(said.empty());  // not at once
  std::vector<cl::DemoPrint> none;
  cl::drop_ignored(g, s, none, 100.9);
  c.run();
  CHECK(said.empty());
  cl::drop_ignored(g, s, none, 102.01);
  c.run();
  CHECK(said.size() == 1 && said[0].find("Shooter's Edge") != std::string::npos);
  std::vector<cl::DemoPrint> again{{3, "bobby: !version\n"}};
  cl::drop_ignored(g, s, again, 150);
  cl::drop_ignored(g, s, none, 153);
  c.run();
  CHECK(said.size() == 1);  // once in two minutes
  std::vector<cl::DemoPrint> start{{3, "bobby: !version please\n"}};
  cl::drop_ignored(g, s, start, 300);
  cl::drop_ignored(g, s, none, 303);
  c.run();
  CHECK(said.size() == 1);  // only a line that ends in it
}

// The window's own game: its lines go through the regex triggers and msg_trigger by their level.
TEST(the_windows_own_game_lines_fire_the_triggers) {
  con::Console c;
  cl::QwIgnore g;
  cl::DemoState s = room();
  cl::add_qw_ignore(c, g, s);
  g.con = &c;
  g.chat.con = &c;
  c.execute(
      "alias got \"echo got: $triggermatch\"; msg_trigger got \"quad\" -l 3;"
      "alias hide \"\"; re_trigger hide \"secret\"; re_trigger_options hide remove");
  c.run();
  c.clear_output();
  std::vector<std::string> shown;
  const std::vector<std::string> lines{"bob: quad up\n", "a secret area\n", "the quad is here\n",
                                       "You got the shells\n"};
  const size_t next =
      cl::game_lines(g, lines, {3, 2, 2}, 0, 50, [&](const std::string& l) { shown.push_back(l); });
  CHECK(next == lines.size());
  CHECK_MSG(shown.size() == 3 && shown[1] == "the quad is here\n", "the secret line taken away");
  c.run();
  std::string got;
  for (const std::string& l : c.output()) got += l + "|";
  // Only the chat line fires the chat level's trigger; the high print of quad does not.
  CHECK_MSG(got == "got: bob: quad up\n|", "fired: " + got);
  CHECK(cl::game_lines(g, lines, {}, next, 51, [&](const std::string&) { CHECK(false); }) ==
        lines.size());
}

// A program's line printed in pieces (a name, the rest; a team's name in its own letters) is shown
// whole, once; a piece with no line end left at the end of the look is shown as it is.
TEST(the_windows_own_game_lines_are_whole_when_printed_in_pieces) {
  cl::QwIgnore g;
  std::vector<std::string> shown;
  const std::vector<std::string> lines{
      "mate3",  " entered the game\n", "You have been assigned to ",
      "RED",    " team.\nA\nB",        "bob: hi\n",
      "no room"};
  const size_t next = cl::game_lines(g, lines, {2, 2, 2, 2, 2, 3, 2}, 0, 50,
                                     [&](const std::string& l) { shown.push_back(l); });
  CHECK(next == lines.size());
  const std::vector<std::string> want{"mate3 entered the game\n",
                                      "You have been assigned to RED team.\n",
                                      "A\n",
                                      "B",
                                      "bob: hi\n",
                                      "no room"};
  CHECK_MSG(shown == want, std::to_string(shown.size()) + " lines");
}
