// THE ENDPOINT'S TALK: who hears a player's chat line and voice on the classic server, whatever
// rules it serves (the first game's program, a compiled module, the sequel's rules), so both
// games get the same chat and voice (the client's side is engine/console/talk.hpp).
//
// THE VOICE TARGET (`voicetarg <word>`, a client's string command, kept for the connection):
//   all      everyone
//   team     the talker's team (the same `team` user info key); everyone with no teams. The
//            target every connection starts with, and the one an unknown word sets, so a word a
//            later client knows never breaks this server
//   near     the players within earshot (`ServerConfig::earshot`, sv_earshot, map units) whose
//            hearing set holds the talker's place (no wall between them that sound cannot pass)
//   <slot>   one player by slot, counted from 0; a slot past the last is ignored
// A spectator's voice reaches only spectators while `sv_spectalk` is 0. A listener hears voice
// only after his client sends `unmuteall` (and none after `muteall`); `vignore <slot> [0|1]`
// stops one talker reaching him (no number: toggled). The window's own player (a local seat)
// listens from the start.
//
// SAY NEAR (`say_near <text>`): the chat line read by the players the near voice reaches, the
// talker included, as "<name> (nearby): <text>" at the chat level; held to the flood rule as
// `say` is. A `say` or `say_team` the rules took nothing of (the sequel's rules have no chat)
// is routed by the server's own chat (server/chat_rules.hpp).
//
// ge:summary The classic endpoint's talk: voice targets, mutes, say_near by earshot and walls
// ge:keywords voice-target voicetarg unmuteall muteall vignore say_near nearby proximity earshot
// ge:keywords sv_earshot hearing-set voice-near near-chat talk_hears classic_talk_command
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_talk_test.cpp
#pragma once

#include <string>

#include "progs/text.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

// Whether `listener` hears `talker` speaking to `to` (`one`: the slot for VoiceTo::one), by the
// target's rule alone (the mutes and the voice bit are the relay's).
bool talk_hears(const ClassicServer& s, int talker, int listener, VoiceTo to, int one);
// Whether `listener` stands within earshot of `talker` with nothing sound cannot pass between.
bool talk_near(const ClassicServer& s, int talker, int listener);
// A client's talk command (voicetarg, unmuteall, muteall, vignore, say_near): true when it was one.
bool classic_talk_command(ClassicServer& s, int slot, const ge::progs::Words& w);
// A `say` or `say_team` the rules took nothing of, routed by the server's own chat.
void classic_server_say(ClassicServer& s, int slot, bool team, const std::string& line);

}  // namespace shooters_edge::net
