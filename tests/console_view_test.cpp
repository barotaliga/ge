// The drop-down console: the slide, the submit rule (/ commands, chat otherwise in a multiplayer
// game), chat to all, a team and one player, editing, history, completion, scrollback, notify.
//
// ge:summary Tests the drop-down console: slide, / commands, chat, editing, history, completion
// ge:keywords test console view submit chat say whisper history completion scroll notify
// ge:skill console testing
#include "check.hpp"
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

TEST(chat_goes_through_the_games_sender) {
  Console c;
  ConsoleView v(c);
  std::vector<std::string> sent;
  v.set_chat([&](ChatTo to, std::string_view nick, std::string_view text) {
    sent.push_back(std::to_string(static_cast<int>(to)) + "|" + std::string(nick) + "|" +
                   std::string(text));
    return true;
  });
  c.execute("say hi there; say_team \"go left\"; whisper bob see you; whisper bob");
  c.execute("say_prox over here");
  CHECK(sent.size() == 4);
  CHECK(sent[0] == "0||hi there" && sent[1] == "1||go left" && sent[2] == "2|bob|see you");
  CHECK(sent[3] == "3||over here");
  CHECK(c.problems().size() == 1);  // the whisper without a text
}

// A game's held key: `+voice_prox` talks to the players near you while the key is down.
TEST(a_held_key_is_a_button) {
  Console c;
  ConsoleView v(c);
  const Button& talk = c.add_button("voice_prox", "+voice_prox: talk to the players near you");
  c.execute("bind v +voice_prox");
  c.key("v", true);
  c.run();
  CHECK(talk.down());
  c.key("v", false);
  c.run();
  CHECK(!talk.down());
}

TEST(say_prox_shown_locally) {
  Console c;
  ConsoleView v(c);
  c.execute("name ada; say_prox over here");
  CHECK(!v.lines().empty() && v.lines().back().kind == LineKind::nearby);
  CHECK(v.lines().back().text == "(near) ada: over here");
}

TEST(editing_history_and_completion) {
  Console c;
  ConsoleView v(c);
  c.cvars().add({.name = "cl_interp", .fallback = "100"});
  c.cvars().add({.name = "cl_cmdrate", .fallback = "60"});
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
  v.type("cl_");
  v.key(EditKey::tab);
  CHECK(v.input() == "cl_");
  v.key(EditKey::tab);  // a second Tab lists the matches
  CHECK(v.lines().back().text.find("cl_cmdrate  cl_interp") != std::string::npos);
  v.key(EditKey::up);
  CHECK(v.input() == "/cl_interp ");
  v.key(EditKey::up);
  CHECK(v.input() == "echo one");
  v.key(EditKey::down);
  v.key(EditKey::down);
  CHECK(v.input() == "cl_");
}

TEST(scrollback_notify_and_triggers_on_game_lines) {
  Console c;
  ConsoleView v(c);
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
