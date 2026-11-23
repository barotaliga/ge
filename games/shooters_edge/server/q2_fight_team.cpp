// Quake II runner's fight: who plays on whose team (by skin or model), the field of view a player
// asks for, the body left where a player died, and the scores' lines.
//
// ge:summary Quake II runner's fight: teams by skin or model, the fov, bodies left, the scores
// ge:keywords q2 fight teams skin-teams model-teams fov bodies scores
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

// The part of `skin` ("model/skin") that names a team: the skin after the slash, or the model;
// a skin with no slash is its own team whole.
std::string team_part(const std::string& skin, bool by_model) {
  const size_t slash = skin.find('/');
  if (slash == std::string::npos) return skin;
  return by_model ? skin.substr(0, slash) : skin.substr(slash + 1);
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

const std::string& info_ref(const Q2Server& s, int slot, const char* key) {
  static const std::string kNone;
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == key) return v;
  return kNone;
}

std::string info(const Q2Server& s, int slot, const char* key) { return info_ref(s, slot, key); }

const std::string& name_of(const Q2Server& s, int slot) {
  static const std::string kNobody = "someone";
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return kNobody;
}

}  // namespace

bool fight_teammates(const Q2Server& s, int a, int b) {
  if (a < 0 || b < 0 || a >= sh::kMaxClients || b >= sh::kMaxClients) return false;
  if (s.cvars.find("coop")->on()) return true;
  return deathmatch(s) && fight_skin_mates(s, a, b);
}

bool fight_skin_mates(const Q2Server& s, int a, int b) {
  if (a < 0 || b < 0 || a >= sh::kMaxClients || b >= sh::kMaxClients) return false;
  if (!(fight_anyflag(s, 64) || fight_anyflag(s, 128))) return false;
  const bool by_model = fight_anyflag(s, 128);
  const std::string ta = team_part(info(s, a, "skin"), by_model);
  return !ta.empty() && ta == team_part(info(s, b, "skin"), by_model);
}

float fight_fov(const Q2Server& s, int slot) {
  if ((fight_dmflag(s, 32768)) || s.intermission_at > 0) return 90;
  const std::string text = info(s, slot, "fov");
  if (text.empty()) return 90;
  const float fov = std::strtof(text.c_str(), nullptr);
  return fov < 1 ? 1 : fov > 160 ? 160 : fov;
}

void fight_bury(Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  if (p.alive || p.gibbed || p.died_at <= 0) return;
  Q2Body& b = s.bodies[s.next_body];
  s.next_body = (s.next_body + 1) % kQ2Bodies;
  b.live = true;
  b.origin = unpack(p.move.origin);
  b.yaw = p.view_angles.y;
  b.health = p.health;
  b.frame = p.pose.frame;
}

std::vector<std::string> fight_scores(const Q2Server& s) {
  std::vector<int> order;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && !s.clients[i].spectator) order.push_back(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return s.players[a].frags > s.players[b].frags; });
  std::vector<std::string> lines;
  for (const int i : order) {
    std::string n = std::to_string(static_cast<int>(s.players[i].frags));
    while (n.size() < 4) n.insert(n.begin(), ' ');
    lines.push_back(n + "  " + name_of(s, i));
  }
  return lines;
}

}  // namespace shooters_edge::server
