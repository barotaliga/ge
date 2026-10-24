// The drop-down console: the slide, the submit rule (/ commands, chat otherwise in a multiplayer
// game), chat to all, a team and one player, editing, history, autocomplete, scrollback, notify.
//
// ge:summary Tests the drop-down console: slide, / commands, chat, editing, history, completion
// ge:keywords test console view submit chat say whisper history completion scroll notify
// ge:keywords autocomplete suggestion
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
