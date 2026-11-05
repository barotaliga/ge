// The co-op party on the listen server (coop_party.hpp states what it does).
//
// ge:summary The co-op party: companions seated, their graph per map, their packets each frame
// ge:keywords party_add party_remove party_enter party_packets companion coop nav-graph
// ge:skill shooter bots
// ge:test tests/shooters_edge_campaign_view_test.cpp
#include "shooters_edge/client/coop_party.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kHz = 72;  // the campaign host's frames a second

bool ensure_graph(Party& p, const sv::Campaign& c, const sv::MapSpec& map, std::string& error) {
  if (p.graph && p.graph_of == map.name && p.graph_at == c.history.size()) return true;
  if (map.models.empty()) {
    error = "the map " + map.name + " has no world model to walk on; next: load a map first";
    return false;
  }
  auto w = std::make_unique<sh::MoveWorld>();
  sh::map_world(map.models[0], *w);
  auto g = std::make_unique<sh::NavGraph>();
  sh::build_nav(*w, {}, sh::MoveSettings{}, sh::NavSettings{}, *g);
  if (g->nodes.empty()) {
    error = "no floor a companion can walk on " + map.name + "; next: play coop on another map";
    return false;
  }
  p.graph = std::move(g);
  p.world = std::move(w);
  p.graph_of = map.name;
  p.graph_at = c.history.size();
  return true;
}

void make(Party& p, const sv::RulesCommon& s, const sv::MapSpec& map, int slot) {
  PartyMate& m = *p.mates[static_cast<size_t>(slot)];
  m.fighter = bots::Fighter();
  m.sequence = 0;
  for (sh::MoveCmd& cmd : m.cmds) cmd = {};
  bots::fighter_init(m.fighter, *p.graph, s, slot, map.entities.c_str(), m.skill,
                     sh::traits_of(m.skill, sh::Temper::balanced), kHz,
                     p.seed + 31u * static_cast<uint32_t>(slot));
  m.fighter.bot.yaw = s.world.entities[static_cast<size_t>(slot) + 1].view_angles.y;
  m.fighter.bot.pilot.world = p.world.get();
  bots::escort_init(m.fighter, s, slot, 0);
}

}  // namespace

int party_size(const Party& p) {
  int n = 0;
  for (const auto& m : p.mates) n += m ? 1 : 0;
  return n;
}

int party_add(Party& p, sv::Campaign& c, const sv::MapSpec& map, sh::AimSkill skill,
              std::string& error) {
  if (!c.server) {
    error = "no map is running; next: start a coop game (`coopgame`)";
    return -1;
  }
  if (party_size(p) >= kPartyMost) {
    error = "the party is full (" + std::to_string(kPartyMost) + "); next: `removebot` first";
    return -1;
  }
  const sv::RulesCommon& s = *c.server;
  int slot = -1;
  for (int i = 1; i < sh::kMaxClients && i < s.sv.set.maxclients; i++)
    if (!c.players[i].active && !s.clients[i].active) {
      slot = i;
      break;
    }
  if (slot < 0) {
    error = "the server is full; next: `removebot` first";
    return -1;
  }
  if (!ensure_graph(p, c, map, error)) return -1;
  auto m = std::make_unique<PartyMate>();
  m->skill = skill;
  m->name = "mate" + std::to_string(++p.named);
  const std::string name = m->name;
  p.mates[static_cast<size_t>(slot)] = std::move(m);
  sv::campaign_join(c, slot, name);
  make(p, *c.server, map, slot);
  return slot;
}

bool party_remove(Party& p, sv::Campaign& c) {
  for (int i = sh::kMaxClients - 1; i > 0; i--) {
    if (!p.mates[static_cast<size_t>(i)]) continue;
    if (c.server) c.server.drop(i);
    c.players[i].active = false;
    p.mates[static_cast<size_t>(i)].reset();
    return true;
  }
  return false;
}

void party_enter(Party& p, sv::Campaign& c, const sv::MapSpec& map, std::string& error) {
  if (party_size(p) == 0 || !c.server) return;
  if (!ensure_graph(p, c, map, error)) {
    while (party_remove(p, c)) {
    }
    error = "the companions leave: " + error;
    return;
  }
  for (int i = 1; i < sh::kMaxClients; i++)
    if (p.mates[static_cast<size_t>(i)] && c.server->clients[i].active) make(p, *c.server, map, i);
}

void party_packets(Party& p, const sv::RulesCommon& s, int msec, std::vector<sh::Packet>& out) {
  for (int i = 1; i < sh::kMaxClients; i++) {
    if (!p.mates[static_cast<size_t>(i)] || !s.clients[i].active) continue;
    PartyMate& m = *p.mates[static_cast<size_t>(i)];
    m.cmds[0] = m.cmds[1];
    m.cmds[1] = m.cmds[2];
    m.cmds[2] = bots::fighter_command(m.fighter, s, i, msec);
    sh::Packet pk;
    pk.client = i;
    pk.sequence = ++m.sequence;
    for (int k = 0; k < 3; k++) pk.cmds[k] = m.cmds[k];
    out.push_back(pk);
  }
}

}  // namespace shooters_edge::client
