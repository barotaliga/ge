// WHO HEARS WHOM, FOR EVERY SERVER OF THE CLASSIC GAMES: the first game's endpoint
// (classic_talk.hpp) and the sequel's net server (sequel_net_server.hpp) keep the same voice
// state for each connection and route each voice message and nearby chat line by the same rules,
// so a player's voice works alike in both games.
//
// A CONNECTION'S TALK (`TalkState`): the target its voice goes to, whether it hears voice at all
// and the talkers it ignores. The words a client sends to change it (`talk_word`):
//   voicetarg all|team|near|<slot>   the target (an unknown word sets team, so a word a later
//                                    client knows never breaks a server; a slot past the last is
//                                    ignored)
//   unmuteall, muteall               voice heard or not (a connection hears none until unmuteall)
//   vignore <slot> [0|1]             one talker ignored or heard again (no number: toggled)
//
// THE ROUTE (`talk_route`): everyone; the talker's team (a spectator's team is the spectators;
// what a team is, is the server's: the `team` key, or the sequel's teams by skin); the players
// within earshot (`earshot`, map units) whose place is in the talker's hearing set (no wall sound
// cannot pass); or one slot. A spectator reaches only spectators while the rules' `sv_spectalk`
// is 0.
//
// THE VOICE MESSAGE both protocols carry: down (to a listener) the talker's slot, the generation
// byte (the codec in its high nibble, 2 for Opus; the talker's stream generation in its low one),
// the first frame's sequence byte, a little-endian 16-bit length and the frames (each a 16-bit
// length and its bytes); up (from a talker) the same less the slot, which the server stamps from
// the connection. At most `kMostVoiceBytes` of frames.
//
// ge:summary Voice targets, mutes and routes shared by every classic server; the voice message
// ge:keywords voice-route voicetarg unmuteall muteall vignore talk-state talk_route talk_word
// ge:keywords earshot hearing-set spectalk voice-message voice-body say_near nearby
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_talk_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "net/wire.hpp"
#include "progs/text.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

inline constexpr size_t kMostVoiceBytes = 1024;  // a voice message's frames, bytes

// Who a talker's voice goes to (`voicetarg`): everyone, his team, the players near him or one.
enum class VoiceTo : uint8_t { all, team, near, one };

struct TalkState {
  VoiceTo to = VoiceTo::team;
  int one = -1;          // the slot for VoiceTo::one
  bool heard = false;    // `unmuteall` sent (or a local seat, which listens from the start)
  uint32_t ignored = 0;  // a bit a talker it ignores (`vignore`)
};

// What the server knows of a talker and a listener for the route.
struct TalkPair {
  bool talker_spectator = false, listener_spectator = false;
  bool same_team = false;  // by the server's own teams
};

// The rules' `sv_spectalk` (on when the rules have none).
bool talk_spectalk(const sv::RulesCommon* game);
// Whether `listener` stands within `earshot` of `talker` with nothing sound cannot pass between
// (their players' entities, by slot); false with no rules or `earshot` 0.
bool talk_near(const sv::RulesCommon* game, int earshot, int talker, int listener);
// Whether `listener` hears `talker` speaking to `to` (`one`: the slot for VoiceTo::one), by the
// target's rule alone (the mutes are `talk_mutes`).
bool talk_route(const sv::RulesCommon* game, int earshot, int talker, int listener, VoiceTo to,
                int one, const TalkPair& p);
// Whether a listener in state `t` refuses `talker`'s voice (not unmuted, or ignoring him).
inline bool talk_mutes(const TalkState& t, int talker) {
  return !t.heard || (talker >= 0 && talker < 32 && ((t.ignored >> talker) & 1u) != 0);
}
// A client's talk word (voicetarg, unmuteall, muteall, vignore) applied to `t`: true when it was
// one. `slots`: the slots there are.
bool talk_word(TalkState& t, const ge::progs::Words& w, int slots);
// The text a `say_near` carries, its surrounding quotes dropped.
std::string talk_said(const ge::progs::Words& w);
// The line a nearby chat prints: "<name> (nearby): <text>\n".
std::string talk_near_line(const std::string& name, const std::string& text);

// A voice message's body down to a listener, appended to `out`.
void write_voice_down(std::vector<uint8_t>& out, int talker, int gen, int seq,
                      std::span<const uint8_t> frames);
// A voice message's body up from a talker; false when it runs past the end or past
// `kMostVoiceBytes`.
bool read_voice_up(ge::net::Reader& r, int& gen, int& seq, std::vector<uint8_t>& frames);

}  // namespace shooters_edge::net
