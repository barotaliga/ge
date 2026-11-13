// The sequel's companions (sequel_party.hpp states what they do).
//
// ge:summary The sequel's companions: the nav off the brushes, the fighters made, their packets
// ge:keywords sequel_party_add sequel_party_enter sequel_party_packets companion sequel escort
// ge:skill shooter bots
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/sequel_party.hpp"

#include "shooter/features.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/bot_arsenal.hpp"
#include "shooters_edge/bots/companion.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kBotHz = 1000.0 / bots::kPilotMsec;

bool build_nav(SequelParty& p, const sv::RulesCommon& s, const sv::MapSpec& map,
               std::string& error) {
  if (map.brushes == nullptr) {
    error = "Companions can't find their way on " + map.name + ".";
    return false;
  }
  auto nav = std::make_unique<bots::MapNav>();
  // The sequel's move, its air with no air acceleration (a single-player or co-op game's).
  nav->move = bots::sequel_nav_move(0);
  if (!bots::build_map_nav(s, map, "info_player_start", *nav, error)) {
    error = "Companions can't find their way on " + map.name + ".";
    return false;
  }
  p.nav = std::move(nav);
  p.entities = map.entities.empty() ? map.brushes->entity_text : map.entities;
  return true;
}

void make(SequelParty& p, const sv::RulesCommon& s, int slot) {
  SequelMate& m = *p.mates[static_cast<size_t>(slot)];
  m.fighter = bots::Fighter();
  m.sequence = 0;
  for (sh::MoveCmd& cmd : m.cmds) cmd = {};
  bots::Fighter& f = m.fighter;
  f.arsenal = &bots::sequel_arsenal();
  f.features = sh::features_of(m.skill);
  bots::fighter_init(f, p.nav->graph, s, slot, p.entities.c_str(), m.skill,
                     sh::traits_of(m.skill, sh::Temper::balanced), kBotHz,
                     p.seed + 31u * static_cast<uint32_t>(slot));
  f.bot.yaw = s.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
  f.bot.pilot.world = &p.nav->world;
  if (p.nav->swim.leading > 0) f.swim = &p.nav->swim;
  f.trap = &p.nav->trap;
  bots::escort_init(f, s, slot, 0);
}

}  // namespace

int sequel_party_size(const SequelParty& p) {
  int n = 0;
  for (const auto& m : p.mates) n += m ? 1 : 0;
  return n;
}

int sequel_party_add(SequelParty& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                     sh::AimSkill skill, uint32_t held, std::string& error) {
  const sv::RulesCommon& s = *rules;
  if (sequel_party_size(p) >= kSequelMatesMost) {
    error = "The party is full (" + std::to_string(kSequelMatesMost + 1) + " players).";
    return -1;
  }
  int slot = -1;
  for (int i = 1; i < sh::kMaxClients && i < s.sv.set.maxclients; i++)
    if (!p.mates[static_cast<size_t>(i)] && !s.clients[i].active && ((held >> i) & 1u) == 0) {
      slot = i;
      break;
    }
  if (slot < 0) {
    error = "The server is full.";
    return -1;
  }
  if (!p.nav && !build_nav(p, s, map, error)) return -1;
  auto m = std::make_unique<SequelMate>();
  m->skill = skill;
  m->name = "mate" + std::to_string(++p.named);
  rules.connect(slot, m->name, false, true);
  p.mates[static_cast<size_t>(slot)] = std::move(m);
  make(p, s, slot);
  return slot;
}

int sequel_party_last(const SequelParty& p) {
  for (int i = sh::kMaxClients - 1; i > 0; i--)
    if (p.mates[static_cast<size_t>(i)]) return i;
  return -1;
}

void sequel_party_forget(SequelParty& p, int slot) {
  if (slot > 0 && slot < sh::kMaxClients) p.mates[static_cast<size_t>(slot)].reset();
}

void sequel_party_enter(SequelParty& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                        bool reconnect, std::vector<int>& gone, std::string& error) {
  gone.clear();
  p.nav.reset();
  if (sequel_party_size(p) == 0) return;
  const sv::RulesCommon& s = *rules;
  if (!build_nav(p, s, map, error)) {
    for (int i = 1; i < sh::kMaxClients; i++)
      if (p.mates[static_cast<size_t>(i)]) {
        gone.push_back(i);
        p.mates[static_cast<size_t>(i)].reset();
      }
    return;
  }
  for (int i = 1; i < sh::kMaxClients; i++) {
    if (!p.mates[static_cast<size_t>(i)]) continue;
    if (reconnect) rules.connect(i, p.mates[static_cast<size_t>(i)]->name, false, true);
    if (s.clients[i].active) make(p, s, i);
  }
}

void sequel_party_packets(SequelParty& p, const sv::RulesRunner& rules, int msec,
                          std::vector<sh::Packet>& out) {
  const sv::RulesCommon& s = *rules;
  const sv::SequelServer* sq = sv::sequel_of(rules);
  for (int i = 1; i < sh::kMaxClients; i++) {
    if (!p.mates[static_cast<size_t>(i)] || !s.clients[i].active) continue;
    SequelMate& m = *p.mates[static_cast<size_t>(i)];
    m.cmds[0] = m.cmds[1];
    m.cmds[1] = m.cmds[2];
    const sh::Vec3f view = s.sv.edicts[static_cast<size_t>(i) + 1].v_angle;
    const bool forced = sq != nullptr && sq->players[i].forced;
    if (forced) m.fighter.bot.yaw = view.y;
    m.cmds[2] = bots::fighter_command(m.fighter, s, i, msec);
    if (forced) m.cmds[2].angles = view;
    sh::Packet pk;
    pk.client = i;
    pk.sequence = ++m.sequence;
    for (int k = 0; k < 3; k++) pk.cmds[k] = m.cmds[k];
    out.push_back(pk);
  }
}

}  // namespace shooters_edge::client
