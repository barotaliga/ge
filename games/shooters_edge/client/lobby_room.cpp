// The lobby on the listen server: lobby_room.hpp's header says what it does.
//
// ge:summary The lobby on the listen server: bots seated, warmup's limits held, the match restart
// ge:keywords lobby room addbot removebot fighter nav-graph room_enter room_packets
// ge:skill shooter bots
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/client/lobby_room.hpp"

#include <cmath>

#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kBotHz = 1000.0 / bots::kPilotMsec;
constexpr float kPlatRise = 96, kPlatSpeed = 64;  // the arena's plat in the baseline mod's text

// The entity drawn with model `name` (an inline brush model "*n"), or -1.
int entity_on(const sv::QcServer& s, const char* name) {
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || d.modelindex >= static_cast<int>(s.models.size())) continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == name) return e;
  }
  return -1;
}

// The bots' graph for `map`, built when it is not already.
bool ensure_graph(Room& r, const sv::QcServer& s, const sv::MapSpec& map, std::string& error) {
  if (r.graph && r.graph_of == map.name) return true;
  if (map.models.empty()) {
    error = "the map " + map.name + " has no world model to walk on; next: load a map first";
    return false;
  }
  auto w = std::make_unique<sh::MoveWorld>();
  sh::MoveWorld& world = *w;
  sh::map_world(map.models[0], world);
  std::vector<sh::NavMover> movers;
  if (map.name == "arena" && map.models.size() > 2) {
    const int door = entity_on(s, "*1"), plat = entity_on(s, "*2");
    if (door > 0 && plat > 0) {
      sh::NavMover d, p;
      d.ent = door;
      d.absmin = s.sv.edicts[static_cast<size_t>(door)].absmin;
      d.absmax = s.sv.edicts[static_cast<size_t>(door)].absmax;
      p.ent = plat;
      p.absmin = s.sv.edicts[static_cast<size_t>(plat)].absmin;
      p.absmax = s.sv.edicts[static_cast<size_t>(plat)].absmax;
      p.model = &map.models[2];
      p.travel = {0, 0, kPlatRise};
      p.ride_s = 1 + kPlatRise / kPlatSpeed;
      movers = {d, p};
    }
  }
  auto g = std::make_unique<sh::NavGraph>();
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, *g);
  if (g->nodes.empty()) {
    error = "no floor a bot can walk on " + map.name + "; next: play bots on another map";
    return false;
  }
  r.graph = std::move(g);
  r.world = std::move(w);
  r.graph_of = map.name;
  return true;
}

uint32_t bot_seed(const Room& r, int slot) { return r.seed + 31u * static_cast<uint32_t>(slot); }

void make_fighter(Room& r, const sv::QcServer& s, const sv::MapSpec& map, int slot) {
  RoomBot& b = *r.bots[static_cast<size_t>(slot)];
  b.fighter = bots::Fighter();
  b.sequence = 0;
  for (sh::MoveCmd& c : b.cmds) c = {};
  bots::fighter_init(b.fighter, *r.graph, s, slot, map.entities.c_str(), b.skill, kBotHz,
                     bot_seed(r, slot));
  b.fighter.bot.yaw = s.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
  b.fighter.bot.pilot.world = r.world.get();  // the hop looks ahead through the map
}

}  // namespace

hud::LobbyView lobby_view(const Room& r, double now, int me, bool roster) {
  hud::LobbyView v;
  const sv::Lobby& l = r.lobby;
  v.phase = l.phase == sv::LobbyPhase::warmup      ? hud::LobbyShown::warmup
            : l.phase == sv::LobbyPhase::countdown ? hud::LobbyShown::countdown
                                                   : hud::LobbyShown::match;
  v.seconds_left = sv::lobby_seconds_left(l, now);
  const double left = l.countdown_ends - now;
  v.fraction = left > 0 ? left - std::floor(left) : 0;
  v.roster = roster;
  const int leader = sv::lobby_leader(l);
  for (int i = 0; i < sv::kLobbySeats; i++) {
    const sv::LobbySeat& s = l.seats[static_cast<size_t>(i)];
    if (!s.used) continue;
    v.rows.push_back({s.name, static_cast<int>(s.team), s.ready, s.bot, i == leader, i == me});
  }
  return v;
}

bool skill_named(const std::string& word, sh::AimSkill& out) {
  for (const sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert})
    if (word == sh::aim_skill_name(k)) {
      out = k;
      return true;
    }
  return false;
}

int room_add_bot(Room& r, sv::QcServer& s, const sv::MapSpec& map, sh::AimSkill skill,
                 std::string& error) {
  int slot = -1;
  for (int i = 1; i < sv::kLobbySeats && i < sh::kMaxClients; i++)
    if (!r.lobby.seats[static_cast<size_t>(i)].used && !s.clients[i].active) {
      slot = i;
      break;
    }
  if (slot < 0) {
    error = "the server is full; next: `removebot` first";
    return -1;
  }
  if (!ensure_graph(r, s, map, error)) return -1;
  const std::string name = "bot" + std::to_string(++r.bots_named);
  r.bots[static_cast<size_t>(slot)] = std::make_unique<RoomBot>();
  r.bots[static_cast<size_t>(slot)]->skill = skill;
  r.bots[static_cast<size_t>(slot)]->name = name;
  sv::connect(s, slot, name, false, true);
  sv::lobby_join(r.lobby, slot, name + " (" + sh::aim_skill_name(skill) + ")", true, s.sv.time);
  sv::lobby_apply(s, r.lobby, slot);
  make_fighter(r, s, map, slot);
  return slot;
}

bool room_remove_bot(Room& r, sv::QcServer& s) {
  for (int i = sv::kLobbySeats - 1; i > 0; i--) {
    if (!r.bots[static_cast<size_t>(i)]) continue;
    sv::disconnect(s, i);
    r.bots[static_cast<size_t>(i)].reset();
    sv::lobby_leave(r.lobby, i, s.sv.time);
    return true;
  }
  return false;
}

void room_enter(Room& r, sv::QcServer& s, const sv::MapSpec& map) {
  bool any = false;
  for (const auto& b : r.bots) any = any || b != nullptr;
  std::string error;
  if (any && !ensure_graph(r, s, map, error)) {  // a map no bot can walk: the bots leave
    r.lobby.said.push_back("the bots leave: " + error);
    for (int i = 1; i < sv::kLobbySeats; i++)
      if (r.bots[static_cast<size_t>(i)]) {
        r.bots[static_cast<size_t>(i)].reset();
        sv::lobby_leave(r.lobby, i, s.sv.time);
      }
  }
  for (int i = 1; i < sv::kLobbySeats && i < sh::kMaxClients; i++)
    if (r.bots[static_cast<size_t>(i)])
      sv::connect(s, i,
                  "bot" + r.lobby.seats[static_cast<size_t>(i)].name.substr(
                              3, r.lobby.seats[static_cast<size_t>(i)].name.find(' ') - 3),
                  false, true);
  for (int i = 0; i < sv::kLobbySeats && i < sh::kMaxClients; i++)
    if (r.lobby.seats[static_cast<size_t>(i)].used && s.clients[i].active)
      sv::lobby_apply(s, r.lobby, i);
  for (int i = 1; i < sv::kLobbySeats && i < sh::kMaxClients; i++)
    if (r.bots[static_cast<size_t>(i)]) make_fighter(r, s, map, i);
  if (r.lobby.phase != sv::LobbyPhase::match) {
    sv::server_command(s, "fraglimit 0");
    sv::server_command(s, "timelimit 0");
  }
}

void room_packets(Room& r, const sv::QcServer& s, int msec, std::vector<sh::Packet>& out) {
  for (int i = 1; i < sv::kLobbySeats && i < sh::kMaxClients; i++) {
    if (!r.bots[static_cast<size_t>(i)] || !s.clients[i].active) continue;
    RoomBot& b = *r.bots[static_cast<size_t>(i)];
    b.cmds[0] = b.cmds[1];
    b.cmds[1] = b.cmds[2];
    b.cmds[2] = bots::fighter_command(b.fighter, s, i, msec);
    sh::Packet p;
    p.client = i;
    p.sequence = ++b.sequence;
    for (int k = 0; k < 3; k++) p.cmds[k] = b.cmds[k];
    out.push_back(p);
  }
}

}  // namespace shooters_edge::client
