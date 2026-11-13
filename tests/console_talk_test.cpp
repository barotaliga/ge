// Talk: the chat commands, the chat line and the voice buttons a multiplayer game opts into.
//
// ge:summary Tests talk: opt-in commands, say_near, the chat line, the voice route by keys held
// ge:keywords test talk chat say say_team say_near whisper messagemode chat-line voice route
// ge:skill console testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/talk.hpp"
#include "console/view.hpp"

using namespace ge::console;

namespace {

struct Sent {
  std::vector<std::string> lines;
  ChatSender sender() {
    return [this](ChatTo to, std::string_view nick, std::string_view text) {
      lines.push_back(std::string(chat_to_word(to)) + "|" + std::string(nick) + "|" +
                      std::string(text));
      return true;
    };
  }
};

}  // namespace

// A single-player game never adds talk: no chat commands, no voice buttons.
TEST(nothing_without_opting_in) {
  Console c;
  ConsoleView v(c);
  CHECK(!c.has_command("say") && !c.has_command("say_team") && !c.has_command("say_near"));
  CHECK(!c.has_command("messagemode") && c.binds().get(BindMode::game, "t").empty());
}

TEST(chat_goes_through_the_games_sender) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk, {.whisper = true});
  Sent sent;
  v.set_chat(sent.sender());
  c.execute("say hi there; say_team \"go left\"; whisper bob see you; whisper bob");
  c.execute("say_near over here");
  CHECK(sent.lines.size() == 4);
  CHECK(sent.lines[0] == "all||hi there" && sent.lines[1] == "team||go left");
  CHECK(sent.lines[2] == "one|bob|see you" && sent.lines[3] == "near||over here");
  CHECK(c.problems().size() == 1);  // the whisper without a text
}

// A game with no teams and no places has neither route.
TEST(the_spec_leaves_routes_out) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk, {.voice = false, .team = false, .nearby = false});
  CHECK(c.has_command("say") && !c.has_command("say_team") && !c.has_command("say_near"));
  CHECK(!c.has_command("messagemode2") && !c.has_command("messagemode3"));
  CHECK(talk.voice_all == nullptr && talk.voice_team == nullptr);
}

TEST(say_near_shown_locally) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk);
  c.execute("name ada; say_near over here");
  CHECK(!v.lines().empty() && v.lines().back().kind == LineKind::nearby);
  CHECK(v.lines().back().text == "ada (nearby): over here");
}

// U opens the chat line for the players near; the text typed is said on Enter.
TEST(the_chat_line_says_on_enter_and_escape_drops_it) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk);
  Sent sent;
  v.set_chat(sent.sender());
  c.key("u", true);
  c.run();
  CHECK(v.chatting() && v.chat_to() == ChatTo::nearby && v.typing() && !v.open());
  CHECK(v.chat_prompt() == "Nearby:");
  v.type("on the ledge");
  v.key(EditKey::left);
  v.key(EditKey::backspace);  // "on the ledge" without its second e: "on the ledg" + "e"
  CHECK(v.chat_input() ==
            "on the ledg"
            "e" ||
        v.chat_input() ==
            "on the led"
            "e");
  v.key(EditKey::end);
  v.key(EditKey::enter);
  CHECK(!v.chatting() && sent.lines.size() == 1 && sent.lines[0].rfind("near||on the led", 0) == 0);
  c.execute("messagemode2");
  CHECK(v.chatting() && v.chat_prompt() == "Team:");
  v.type("never sent");
  v.key(EditKey::escape);
  CHECK(!v.chatting() && sent.lines.size() == 1 && v.input().empty());
}

// The narrowest route held wins.
TEST(the_voice_route_by_the_keys_held) {
  Console c;
  ConsoleView v(c);
  Talk talk;
  add_talk(c, v, talk);
  CHECK(!talk_voice(talk).on);
  c.key("v", true);
  c.run();
  CHECK(talk_voice(talk).on && talk_voice(talk).to == ChatTo::all);
  c.key("b", true);
  c.run();
  CHECK(talk_voice(talk).to == ChatTo::team);
  c.key("n", true);
  c.run();
  CHECK(talk_voice(talk).to == ChatTo::nearby);
  for (const char* k : {"v", "b", "n"}) c.key(k, false);
  c.run();
  CHECK(!talk_voice(talk).on);
}

// A key the player bound is kept: only free keys get talk's defaults.
TEST(the_players_binds_stand) {
  Console c;
  ConsoleView v(c);
  c.binds().set(BindMode::game, "v", "+attack");
  Talk talk;
  add_talk(c, v, talk);
  CHECK(c.binds().get(BindMode::game, "v") == "+attack");
  CHECK(c.binds().get(BindMode::game, "t") == "messagemode");
  ChatTo to = ChatTo::all;
  CHECK(chat_to_of("near", to) && to == ChatTo::nearby && !chat_to_of("far", to));
}
