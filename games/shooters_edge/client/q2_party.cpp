// Quake II's companions (q2_party.hpp states what they do).
//
// ge:summary Quake II's companions: the nav off the brushes, the fighters made, their packets
// ge:keywords q2_party_add q2_party_enter q2_party_packets companion q2 escort
// ge:skill shooter bots
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/q2_party.hpp"

#include <array>

#include "shooterbots/bot_arsenal.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/companion.hpp"
#include "shooterbots/features.hpp"
#include "shooterbots/traits.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::client {

namespace {

constexpr double kBotHz = 1000.0 / bots::kPilotMsec;

bool build_nav(Q2Party& p, const sv::RulesCommon& s, const sv::MapSpec& map, std::string& error) {
  if (map.brushes == nullptr) {
    error = "Companions can't find their way on " + map.name + ".";
    return false;
  }
  auto nav = std::make_unique<bots::MapNav>();
  // Quake II's move, its air with no air acceleration (a single-player or co-op game's).
  nav->move = bots::q2_nav_move(0);
  if (!bots::build_map_nav(s, map, bots::quake_maps(), "info_player_start", *nav, error,
                           bots::campaign_nav_settings())) {
    error = "Companions can't find their way on " + map.name + ".";
    return false;
  }
  p.nav = std::move(nav);
  p.entities = map.entities.empty() ? map.brushes->entity_text : map.entities;
  return true;
}

void make(Q2Party& p, const sv::RulesCommon& s, int slot) {
  Q2Mate& m = *p.mates[static_cast<size_t>(slot)];
  m.fighter = bots::Fighter();
  m.sequence = 0;
  for (sh::MoveCmd& cmd : m.cmds) cmd = {};
  bots::Fighter& f = m.fighter;
  f.features = sb::features_of(m.skill);
  bots::fighter_init(f, p.nav->graph, s, slot, p.entities.c_str(), m.skill,
                     sb::traits_of(m.skill, sb::Temper::balanced), kBotHz,
                     p.seed + 31u * static_cast<uint32_t>(slot), bots::q2_arsenal());
  sb::exits_init(f, p.nav->graph, s, p.entities.c_str(), p.map);  // the exits' boxes
  f.bot.yaw = s.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
  f.bot.pilot.world = &p.nav->world;
  if (p.nav->swim.leading > 0) f.swim = &p.nav->swim;
  f.trap = &p.nav->trap;
  if (m.work && p.map != nullptr)
    sb::party_work_init(*m.work, f, *p.nav, s, *p.map);
  else
    bots::escort_init(f, s, slot, 0);
}

}  // namespace

int q2_party_size(const Q2Party& p) {
  int n = 0;
  for (const auto& m : p.mates) n += m ? 1 : 0;
  return n;
}

int q2_party_add(Q2Party& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                 sb::AimSkill skill, uint32_t held, std::string& error, bool helper) {
  const sv::RulesCommon& s = *rules;
  if (q2_party_size(p) >= kQ2MatesMost) {
    error = "The party is full (" + std::to_string(kQ2MatesMost + 1) + " players).";
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
  p.map = &map;
  auto m = std::make_unique<Q2Mate>();
  m->skill = skill;
  if (helper) m->work = std::make_unique<sb::PartyWork>();
  m->name = "mate" + std::to_string(++p.named);
  rules.connect(slot, m->name, false, true);
  p.mates[static_cast<size_t>(slot)] = std::move(m);
  make(p, s, slot);
  return slot;
}

int q2_party_last(const Q2Party& p) {
  for (int i = sh::kMaxClients - 1; i > 0; i--)
    if (p.mates[static_cast<size_t>(i)]) return i;
  return -1;
}

void q2_party_forget(Q2Party& p, int slot) {
  if (slot > 0 && slot < sh::kMaxClients) p.mates[static_cast<size_t>(slot)].reset();
}

void q2_party_enter(Q2Party& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                    bool reconnect, std::vector<int>& gone, std::string& error) {
  gone.clear();
  p.nav.reset();
  p.map = &map;
  p.round.share = sb::Party();  // the new map's steps
  if (q2_party_size(p) == 0) return;
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

void q2_party_packets(Q2Party& p, const sv::RulesRunner& rules, int msec,
                      std::vector<sh::Packet>& out) {
  const sv::RulesCommon& s = *rules;
  const sv::Q2Server* sq = sv::q2_of(rules);
  // The helpers' split of the map, round the people (every player not a companion).
  std::array<bool, sh::kMaxClients> bot{};
  std::array<sb::WayThrough*, sh::kMaxClients> helpers{};
  for (int i = 0; i < sh::kMaxClients; i++)
    if (const auto& m = p.mates[static_cast<size_t>(i)]; m) {
      bot[static_cast<size_t>(i)] = true;
      helpers[static_cast<size_t>(i)] = m->work ? &m->work->way : nullptr;
    }
  sb::party_round(p.round, p.nav.get(), s, bot, helpers);
  for (int i = 1; i < sh::kMaxClients; i++) {
    if (!p.mates[static_cast<size_t>(i)] || !s.clients[i].active) continue;
    Q2Mate& m = *p.mates[static_cast<size_t>(i)];
    if (m.work)
      sb::party_work_plan(*m.work, m.fighter, *p.nav, s, i + 1, s.sv.time, p.round.people);
    else  // an escort leaves what heals to the people too (party.hpp)
      sb::party_leave_items(m.fighter, s, i + 1, p.round.people);
    m.cmds[0] = m.cmds[1];
    m.cmds[1] = m.cmds[2];
    const sh::Vec3f view = s.sv.edicts[static_cast<size_t>(i) + 1].v_angle;
    const bool forced = sq != nullptr && sq->players[i].forced;
    if (forced) m.fighter.bot.yaw = view.y;
    m.cmds[2] = bots::fighter_command(m.fighter, s, i, msec);
    if (m.work)
      sb::party_work_command(*m.work, m.fighter, *p.nav, s, i + 1, s.sv.time, p.round.people,
                             m.cmds[2]);
    // A turned view is sent back as it is, whatever turned him this step: the rules let go of the
    // turn only then (else he walks half round from where he means at a spot facing back).
    if (forced) m.cmds[2].angles = view;
    sh::Packet pk;
    pk.client = i;
    pk.sequence = ++m.sequence;
    for (int k = 0; k < 3; k++) pk.cmds[k] = m.cmds[k];
    out.push_back(pk);
  }
}

}  // namespace shooters_edge::client
