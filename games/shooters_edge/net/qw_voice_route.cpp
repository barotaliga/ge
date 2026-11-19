// Who hears whom on every QuakeWorld server (qw_voice_route.hpp states the rules).
//
// ge:summary Voice targets, mutes and routes shared by every QuakeWorld server; the voice message
// ge:keywords voicetarg unmuteall muteall vignore talk_route talk_near earshot voice-message
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_talk_test.cpp
#include "shooters_edge/net/qw_voice_route.hpp"

#include <cstdlib>
#include <cstring>

namespace shooters_edge::net {

namespace pg = ge::progs;
namespace gn = ge::net;

bool talk_spectalk(const sv::RulesCommon* game) {
  const ge::console::Cvar* v = game != nullptr ? game->cvars.find("sv_spectalk") : nullptr;
  return v == nullptr || v->on();
}

bool talk_near(const sv::RulesCommon* game, int earshot, int talker, int listener) {
  if (earshot <= 0 || game == nullptr) return false;
  if (talker == listener) return true;
  const std::vector<sv::WorldEntity>& ents = game->world.entities;
  const size_t a = static_cast<size_t>(talker) + 1, b = static_cast<size_t>(listener) + 1;
  if (a >= ents.size() || b >= ents.size() || ents[a].free || ents[b].free) return false;
  const sh::Vec3f p = ents[a].origin, q = ents[b].origin;
  const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
  const auto reach = static_cast<float>(earshot);
  if (dx * dx + dy * dy + dz * dz > reach * reach) return false;
  // A wall sound does not pass: the listener's place outside the talker's hearing set.
  return game->vis == nullptr || game->vis(game->vis_ctx, p, q, false);
}

bool talk_route(const sv::RulesCommon* game, int earshot, int talker, int listener, VoiceTo to,
                int one, const TalkPair& p) {
  if (p.talker_spectator && !p.listener_spectator && !talk_spectalk(game)) return false;
  switch (to) {
    case VoiceTo::all:
      return true;
    case VoiceTo::team:
      // A spectator's team is the spectators; a player's the server's own teams.
      if (p.talker_spectator) return p.listener_spectator;
      return !p.listener_spectator && p.same_team;
    case VoiceTo::near:
      return talk_near(game, earshot, talker, listener);
    case VoiceTo::one:
      return listener == one;
  }
  return false;
}

bool talk_word(TalkState& t, const pg::Words& w, int slots) {
  const std::string cmd = pg::word(w, 0);
  if (cmd == "voicetarg") {
    const char* word = pg::word(w, 1);
    if (word[0] >= '0' && word[0] <= '9') {
      const int n = std::atoi(word);
      if (n >= 0 && n < slots) {
        t.to = VoiceTo::one;
        t.one = n;
      }
    } else if (std::strcmp(word, "all") == 0) {
      t.to = VoiceTo::all;
    } else if (std::strcmp(word, "near") == 0) {
      t.to = VoiceTo::near;
    } else {
      t.to = VoiceTo::team;  // `team`, and any word this server does not know
    }
    return true;
  }
  if (cmd == "unmuteall" || cmd == "muteall") {
    t.heard = cmd == "unmuteall";
    return true;
  }
  if (cmd == "vignore") {
    const int n = std::atoi(pg::word(w, 1));
    if (w.count < 2 || n < 0 || n >= slots || n >= 32) return true;
    const uint32_t bit = 1u << n;
    const bool mute = w.count >= 3 ? std::atoi(pg::word(w, 2)) != 0 : (t.ignored & bit) == 0;
    t.ignored = mute ? t.ignored | bit : t.ignored & ~bit;
    return true;
  }
  return false;
}

std::string talk_said(const pg::Words& w) {
  std::string t = w.args;
  if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
  return t;
}

std::string talk_near_line(const std::string& name, const std::string& text) {
  return name + " (nearby): " + text + "\n";
}

void write_voice_down(std::vector<uint8_t>& out, int talker, int gen, int seq,
                      std::span<const uint8_t> frames) {
  out.push_back(static_cast<uint8_t>(talker));
  out.push_back(static_cast<uint8_t>(gen));
  out.push_back(static_cast<uint8_t>(seq));
  out.push_back(static_cast<uint8_t>(frames.size() & 255));
  out.push_back(static_cast<uint8_t>(frames.size() >> 8));
  out.insert(out.end(), frames.begin(), frames.end());
}

bool read_voice_up(gn::Reader& r, int& gen, int& seq, std::vector<uint8_t>& frames) {
  gen = gn::read_u8(r);
  seq = gn::read_u8(r);
  const size_t n = gn::read_u16(r);
  const uint8_t* p = gn::read_view(r, n);
  if (r.bad || p == nullptr || n > kMostVoiceBytes) return false;
  frames.assign(p, p + n);
  return true;
}

}  // namespace shooters_edge::net
