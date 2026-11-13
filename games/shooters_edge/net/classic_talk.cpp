// The classic endpoint's talk (classic_talk.hpp states the rules).
//
// ge:summary Voice targets, voice mutes, say_near by earshot and walls on the classic endpoint
// ge:keywords voicetarg unmuteall muteall vignore say_near talk_hears talk_near earshot
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_talk_test.cpp
#include "shooters_edge/net/classic_talk.hpp"

#include <cstdlib>
#include <cstring>

#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/server/chat_rules.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

namespace {

bool spectalk(const ClassicServer& s) {
  const ge::console::Cvar* v = s.game != nullptr ? s.game->cvars.find("sv_spectalk") : nullptr;
  return v == nullptr || v->on();
}

// The text after the command word, its surrounding quotes dropped.
std::string said_text(const pg::Words& w) {
  std::string t = w.args;
  if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
  return t;
}

}  // namespace

bool talk_near(const ClassicServer& s, int talker, int listener) {
  if (s.cfg.earshot <= 0 || s.game == nullptr) return false;
  if (talker == listener) return true;
  const std::vector<sv::WorldEntity>& ents = s.game->world.entities;
  const size_t a = static_cast<size_t>(talker) + 1, b = static_cast<size_t>(listener) + 1;
  if (a >= ents.size() || b >= ents.size() || ents[a].free || ents[b].free) return false;
  const sh::Vec3f p = ents[a].origin, q = ents[b].origin;
  const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
  const auto reach = static_cast<float>(s.cfg.earshot);
  if (dx * dx + dy * dy + dz * dz > reach * reach) return false;
  // A wall sound does not pass: the listener's place outside the talker's hearing set.
  return s.game->vis == nullptr || s.game->vis(s.game->vis_ctx, p, q, false);
}

bool talk_hears(const ClassicServer& s, int talker, int listener, VoiceTo to, int one) {
  const ClassicClient& c = s.clients[talker];
  const ClassicClient& d = s.clients[listener];
  if (c.spectator && !spectalk(s) && !d.spectator) return false;
  switch (to) {
    case VoiceTo::all:
      return true;
    case VoiceTo::team:
      // A spectator's team is the spectators; a player's the same `team` key (with no teams,
      // every player's empty key: everyone).
      if (c.spectator) return d.spectator;
      return !d.spectator && info_get(d.userinfo, "team") == info_get(c.userinfo, "team");
    case VoiceTo::near:
      return talk_near(s, talker, listener);
    case VoiceTo::one:
      return listener == one;
  }
  return false;
}

bool classic_talk_command(ClassicServer& s, int slot, const pg::Words& w) {
  ClassicClient& c = s.clients[slot];
  const std::string cmd = pg::word(w, 0);
  if (cmd == "voicetarg") {
    const char* word = pg::word(w, 1);
    if (word[0] >= '0' && word[0] <= '9') {
      const int n = std::atoi(word);
      if (n >= 0 && n < sh::kMaxClients) {
        c.voice_to = VoiceTo::one;
        c.voice_one = n;
      }
    } else if (std::strcmp(word, "all") == 0) {
      c.voice_to = VoiceTo::all;
    } else if (std::strcmp(word, "near") == 0) {
      c.voice_to = VoiceTo::near;
    } else {
      c.voice_to = VoiceTo::team;  // `team`, and any word this server does not know
    }
    return true;
  }
  if (cmd == "unmuteall" || cmd == "muteall") {
    c.voice_heard = cmd == "unmuteall";
    return true;
  }
  if (cmd == "vignore") {
    const int n = std::atoi(pg::word(w, 1));
    if (w.count < 2 || n < 0 || n >= sh::kMaxClients) return true;
    const uint32_t bit = 1u << n;
    const bool mute = w.count >= 3 ? std::atoi(pg::word(w, 2)) != 0 : (c.voice_ignored & bit) == 0;
    c.voice_ignored = mute ? c.voice_ignored | bit : c.voice_ignored & ~bit;
    return true;
  }
  if (cmd == "say_near") {
    const std::string text = said_text(w);
    if (text.empty() || c.state != ConnState::spawned) return true;
    if (s.cfg.earshot <= 0) {
      client_print(s, slot, 2, "This server has no nearby chat.\n");
      return true;
    }
    const std::string line_out = c.name + " (nearby): " + text + "\n";
    for (int j = 0; j < sh::kMaxClients; j++) {
      if (s.clients[j].state != ConnState::spawned) continue;
      if (talk_hears(s, slot, j, VoiceTo::near, -1)) client_print(s, j, 3, line_out);
    }
    return true;
  }
  return false;
}

void classic_server_say(ClassicServer& s, int slot, bool team, const std::string& line) {
  if (s.game == nullptr) return;
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  const std::string text = said_text(w);
  if (text.empty()) return;
  const sv::ChatLine out = sv::chat_route(*s.game, slot, team, text);
  for (int j = 0; j < sh::kMaxClients; j++)
    if ((out.to >> j & 1u) && s.clients[j].state == ConnState::spawned)
      client_print(s, j, 3, out.text);
}

}  // namespace shooters_edge::net
