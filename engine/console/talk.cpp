// Talk's commands and buttons (talk.hpp states them).
//
// ge:summary Registers talk's chat commands, chat line and voice buttons; picks the voice route
// ge:keywords talk add_talk talk_voice chat_to_word say say_team say_near messagemode voice
// ge:skill console net
// ge:test tests/console_talk_test.cpp
#include "console/talk.hpp"

namespace ge::console {

namespace {

bool held(const Button* b) { return b != nullptr && b->down(); }

void bind_free(Console& c, const char* key, const char* command) {
  if (c.binds().get(BindMode::game, key).empty()) c.binds().set(BindMode::game, key, command);
}

}  // namespace

void add_talk(Console& c, ConsoleView& view, Talk& talk, const TalkSpec& spec) {
  talk.spec = spec;
  ConsoleView* v = &view;
  if (spec.text) {
    c.add_command(
        "say", "say <text>: a chat line to everyone",
        [v](Console&, const Args& a) { v->say(ChatTo::all, "", a.rest(0)); }, kStuffable);
    c.add_command("messagemode", "messagemode: type a chat line to everyone",
                  [v](Console&, const Args&) { v->open_chat(ChatTo::all); });
    if (spec.bind_keys) bind_free(c, "t", "messagemode");
    if (spec.team) {
      c.add_command(
          "say_team", "say_team <text>: a chat line to your team",
          [v](Console&, const Args& a) { v->say(ChatTo::team, "", a.rest(0)); }, kStuffable);
      c.add_command("messagemode2", "messagemode2: type a chat line to your team",
                    [v](Console&, const Args&) { v->open_chat(ChatTo::team); });
      if (spec.bind_keys) bind_free(c, "y", "messagemode2");
    }
    if (spec.nearby) {
      c.add_command(
          "say_near", "say_near <text>: a chat line to the players near you",
          [v](Console&, const Args& a) { v->say(ChatTo::nearby, "", a.rest(0)); }, kStuffable);
      c.add_command("messagemode3", "messagemode3: type a chat line to the players near you",
                    [v](Console&, const Args&) { v->open_chat(ChatTo::nearby); });
      if (spec.bind_keys) bind_free(c, "u", "messagemode3");
    }
    if (spec.whisper) {
      c.add_command("whisper", "whisper <nick> <text>: a chat line to one player",
                    [v](Console& con, const Args& a) {
                      if (a.size() < 3) {
                        con.error("whisper needs a nick and a text",
                                  "usage: whisper <nick> <text>");
                        return;
                      }
                      v->say(ChatTo::one, a[1], a.rest(1));
                    });
    }
  }
  if (spec.voice) {
    talk.voice_all = &c.add_button("voice", "+voice: talk to everyone while held");
    if (spec.bind_keys) bind_free(c, "v", "+voice");
    if (spec.team) {
      talk.voice_team = &c.add_button("voice_team", "+voice_team: talk to your team while held");
      if (spec.bind_keys) bind_free(c, "b", "+voice_team");
    }
    if (spec.nearby) {
      talk.voice_near =
          &c.add_button("voice_near", "+voice_near: talk to the players near you while held");
      if (spec.bind_keys) bind_free(c, "n", "+voice_near");
    }
  }
}

TalkVoice talk_voice(const Talk& talk) {
  if (held(talk.voice_near)) return {true, ChatTo::nearby};
  if (held(talk.voice_team)) return {true, ChatTo::team};
  if (held(talk.voice_all)) return {true, ChatTo::all};
  return {};
}

std::string_view chat_to_word(ChatTo to) {
  switch (to) {
    case ChatTo::team:
      return "team";
    case ChatTo::nearby:
      return "near";
    case ChatTo::one:
      return "one";
    case ChatTo::all:
      break;
  }
  return "all";
}

bool chat_to_of(std::string_view word, ChatTo& out) {
  for (ChatTo to : {ChatTo::all, ChatTo::team, ChatTo::nearby, ChatTo::one}) {
    if (word == chat_to_word(to)) {
      out = to;
      return true;
    }
  }
  return false;
}

}  // namespace ge::console
