// The classic endpoint's talk (classic_talk.hpp states the rules).
//
// ge:summary Voice targets, voice mutes, say_near by earshot and walls on the classic endpoint
// ge:keywords voicetarg unmuteall muteall vignore say_near talk_hears talk_near earshot
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_talk_test.cpp
#include "shooters_edge/net/classic_talk.hpp"

#include <cstdlib>
#include <cstring>

#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/server/chat_rules.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

bool talk_near(const ClassicServer& s, int talker, int listener) {
  return talk_near(s.game, s.cfg.earshot, talker, listener);
}

bool talk_hears(const ClassicServer& s, int talker, int listener, VoiceTo to, int one) {
  const ClassicClient& c = s.clients[talker];
  const ClassicClient& d = s.clients[listener];
  TalkPair p;
  p.talker_spectator = c.spectator;
  p.listener_spectator = d.spectator;
  // The same `team` key (with no teams, every player's empty key: everyone).
  p.same_team = info_get(d.userinfo, "team") == info_get(c.userinfo, "team");
  return talk_route(s.game, s.cfg.earshot, talker, listener, to, one, p);
}

bool classic_talk_command(ClassicServer& s, int slot, const pg::Words& w) {
  ClassicClient& c = s.clients[slot];
  const std::string cmd = pg::word(w, 0);
  if (talk_word(c.talk, w, sh::kMaxClients)) return true;
  if (cmd == "say_near") {
    const std::string text = talk_said(w);
    if (text.empty() || c.state != ConnState::spawned) return true;
    if (s.cfg.earshot <= 0) {
      client_print(s, slot, 2, "This server has no nearby chat.\n");
      return true;
    }
    const std::string line_out = talk_near_line(c.name, text);
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
  const std::string text = talk_said(w);
  if (text.empty()) return;
  int tracks[sh::kMaxClients];
  for (int j = 0; j < sh::kMaxClients; j++)
    tracks[j] = s.clients[j].spectator ? s.clients[j].track : -1;
  const sv::ChatLine out = sv::chat_route(*s.game, slot, team, text, tracks);
  for (int j = 0; j < sh::kMaxClients; j++)
    if ((out.to >> j & 1u) && s.clients[j].state == ConnState::spawned)
      client_print(s, j, 3, out.text);
}

}  // namespace shooters_edge::net
