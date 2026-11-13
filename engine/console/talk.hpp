// TALK: text chat and voice chat as a multiplayer game turns them on. A game opts in with
// `add_talk` and a `TalkSpec` naming what it has (a single-player game never calls it, so its
// console has no chat at all); the commands then reach the console view's chat (view.hpp's
// `ChatSender`, which the game points at its network) and the voice buttons are read by the game
// each frame (`talk_voice`).
//
// THE TEXT COMMANDS, each only when the spec has its route:
//   say <text>                   everyone
//   say_team <text>              the speaker's team                       (spec.team)
//   say_near <text>              the players near the speaker             (spec.nearby)
//   whisper <nick> <text>        one player                               (spec.whisper)
//   messagemode                  the chat line opened for everyone        (bound to T)
//   messagemode2                 ... for the team                         (Y; spec.team)
//   messagemode3                 ... for the players near                 (U; spec.nearby)
// THE VOICE BUTTONS (spec.voice), push to talk while held:
//   +voice                       everyone the server lets hear the speaker  (V)
//   +voice_team                  the team                                   (B; spec.team)
//   +voice_near                  the players near                           (N; spec.nearby)
// The keys are bound only where nothing is bound yet, so a player's own binds stand. HOW NEAR is
// the game's (its server's earshot); WHO is on a team is the game's too.
//
// THE ROUTE while talking (`talk_voice`): with several voice keys held, the narrowest route wins
// (near, then team, then everyone), so holding a second key never makes a word reach further than
// the player meant.
//
// The words a server reads for a route (`chat_to_word`): all, team, near, one.
//
// ge:summary Talk: the chat and voice commands and buttons a multiplayer game opts into
// ge:keywords talk chat voice-chat opt-in say say_team say_near whisper messagemode messagemode2
// ge:keywords messagemode3 push-to-talk +voice +voice_team +voice_near nearby proximity route
// ge:keywords talk_voice add_talk talkspec multiplayer chat-line
// ge:skill console net
// ge:test tests/console_talk_test.cpp
#pragma once

#include <string_view>

#include "console/console.hpp"
#include "console/view.hpp"

namespace ge::console {

// What a game's talk has: every field false leaves that part out.
struct TalkSpec {
  bool text = true;       // say and the chat line
  bool voice = true;      // the push-to-talk buttons
  bool team = true;       // a team route (the game has teams)
  bool nearby = true;     // a near route (the game has places a player stands in)
  bool whisper = false;   // a route to one player by name
  bool bind_keys = true;  // T, Y, U, V, B, N bound where nothing is
};

// The talk a game keeps: the spec it was added with and the buttons it reads.
struct Talk {
  TalkSpec spec;
  const Button* voice_all = nullptr;   // +voice
  const Button* voice_team = nullptr;  // +voice_team
  const Button* voice_near = nullptr;  // +voice_near
};

// Who the player is talking to this frame.
struct TalkVoice {
  bool on = false;  // a voice key is held
  ChatTo to = ChatTo::all;
};

// The commands and buttons of `spec` registered on `c`, chat through `view`.
void add_talk(Console& c, ConsoleView& view, Talk& talk, const TalkSpec& spec = {});
// The voice keys held now, the narrowest route first.
TalkVoice talk_voice(const Talk& talk);
// The word a server reads for a route: "all", "team", "near", "one".
std::string_view chat_to_word(ChatTo to);
// The route a word names (chat_to_word's words); false for another word.
bool chat_to_of(std::string_view word, ChatTo& out);

}  // namespace ge::console
