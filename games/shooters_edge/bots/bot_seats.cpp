// The bots in a game's seats: bot_seats.hpp's header says what they do.
//
// ge:summary The bots in a game's seats: fighters seated, their nav per map, their packets a frame
// ge:keywords bot-seats sv_bots bot_seats_add bot_seats_enter bot_seats_packets
// ge:skill bots shooter
// ge:test tests/shooters_edge_q2_bots_server_test.cpp
#include "shooters_edge/bots/bot_seats.hpp"

#include <chrono>
#include <future>

#include "shooterbots/features.hpp"
#include "shooterbots/traits.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::bots {

// A map's nav as built, before it is the seats'.
struct NavBuilt {
  std::unique_ptr<NavGraph> graph;  // a map of clip hulls'
  std::unique_ptr<sh::MoveWorld> world;
  std::unique_ptr<MapNav> nav;  // a map of brushes'
  std::string entities;
};

// What a nav reads of the match, copied as it stands for a thread of its own: `BotView` takes it as
// it takes the rules (members of the same names).
struct MatchCopy {
  sh::Server sv;
  sh::RulesWorld world;
  std::vector<std::string> models, sounds;
  std::string map_name;
};

namespace {

constexpr double kBotHz = 1000.0 / kPilotMsec;
constexpr float kPlatRise = 96, kPlatSpeed = 64;  // the arena's plat in the baseline mod's text

// The entity drawn with model `name` (an inline brush model "*n"), or -1.
int entity_on(const BotView& s, const char* name) {
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || d.modelindex >= static_cast<int>(s.models.size())) continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == name) return e;
  }
  return -1;
}

// A map of clip hulls' nav: its world model's, the arena's door and plat its movers.
bool hulls_nav(NavBuilt& b, const BotView& s, const sv::MapSpec& map, std::string& error) {
  if (map.models.empty()) {
    error = "the map " + map.name + " has no world for bots to walk on";
    return false;
  }
  auto w = std::make_unique<sh::MoveWorld>();
  sh::MoveWorld& world = *w;
  map_world(map.models[0], world);
  std::vector<NavMover> movers;
  if (map.name == "arena" && map.models.size() > 2) {
    const int door = entity_on(s, "*1"), plat = entity_on(s, "*2");
    if (door > 0 && plat > 0) {
      NavMover d, p;
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
  auto g = std::make_unique<NavGraph>();
  build_nav(world, movers, sh::MoveSettings{}, NavSettings{}, *g);
  if (g->nodes.empty()) {
    error = "Bots can't play on " + map.name + ": there is no floor they can walk on.";
    return false;
  }
  b.graph = std::move(g);
  b.world = std::move(w);
  b.entities = map.entities;
  return true;
}

// A map of brushes' nav, from its deathmatch spots, on `move` (Quake II's when its rules play;
// nullptr: Quake's).
bool brushes_nav(NavBuilt& b, const BotView& s, const sv::MapSpec& map,
                 const sh::MoveSettings* move, std::string& error) {
  auto nav = std::make_unique<MapNav>();
  if (move != nullptr) nav->move = *move;
  if (!build_map_nav(s, map, quake_maps(), "info_player_deathmatch", *nav, error)) {
    error = "Bots can't play on " + map.name + ": there is no floor they can walk on.";
    return false;
  }
  b.nav = std::move(nav);
  b.entities = map.entities.empty() ? map.brushes->entity_text : map.entities;
  return true;
}

// Either kind, as the map is.
bool any_nav(NavBuilt& b, const BotView& s, const sv::MapSpec& map, const sh::MoveSettings* move,
             std::string& error) {
  return map.brushes != nullptr ? brushes_nav(b, s, map, move, error) : hulls_nav(b, s, map, error);
}

}  // namespace

// A nav building on another thread: the map's copy and the match's, and what it built.
struct NavJob {
  std::string name;                   // the map's
  const void* from = nullptr;         // the loaded map it was asked for
  std::shared_ptr<sv::MapSpec> spec;  // its copy, which the nav traces
  MatchCopy match;
  bool q2_move = false;
  sh::MoveSettings move;
  NavBuilt built;
  std::string error;
  bool ok = false;
  std::future<void> done;
};

namespace {

void forget_nav(BotSeats& b) {
  b.graph.reset();
  b.world.reset();
  b.nav.reset();
  b.nav_spec.reset();
  b.nav_of.clear();
  b.nav_from = nullptr;
}

void take_nav(BotSeats& b, NavBuilt& built, const std::string& name, const void* from) {
  b.graph = std::move(built.graph);
  b.world = std::move(built.world);
  b.nav = std::move(built.nav);
  b.entities = std::move(built.entities);
  b.nav_of = name;
  b.nav_from = from;
}

// The bots' nav for `map`: built at once (true when the map has a floor), or, with `background`,
// asked of a thread of its own (true; `bot_seats_land` takes it, or empties the seats when it finds
// no floor). Already built, or building, for the same loaded map: nothing more.
bool ensure_nav(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map,
                std::string& error) {
  // The same map's nav kept (a restart): the same name from the same loaded map, since the nav
  // traces the map it was built from.
  const void* from = map.brushes != nullptr ? static_cast<const void*>(map.brushes.get())
                                            : static_cast<const void*>(map.models.data());
  if ((b.graph || b.nav) && b.nav_of == map.name && b.nav_from == from) return true;
  if (b.job && b.job->name == map.name && b.job->from == from) return true;
  b.job.reset();  // another map's, still building: waited out and dropped
  forget_nav(b);
  const sv::Q2Server* q = sv::q2_of(rules);
  b.arsenal = q != nullptr ? &q2_arsenal() : &first_arsenal();
  sh::MoveSettings move;
  if (q != nullptr) move = q2_nav_move(sv::q2_air_accelerate(*q));
  if (!b.background) {
    NavBuilt built;
    if (!any_nav(built, *rules, map, q != nullptr ? &move : nullptr, error)) return false;
    take_nav(b, built, map.name, from);
    return true;
  }
  auto job = std::make_shared<NavJob>();
  job->name = map.name;
  job->from = from;
  job->spec = std::make_shared<sv::MapSpec>(map);
  const sv::RulesCommon& s = *rules;
  job->match = {s.sv, s.world, s.models, s.sounds, s.map_name};
  job->q2_move = q != nullptr;
  job->move = move;
  NavJob* j = job.get();
  job->done = std::async(std::launch::async, [j] {
    j->ok = any_nav(j->built, j->match, *j->spec, j->q2_move ? &j->move : nullptr, j->error);
  });
  b.job = std::move(job);
  return true;
}

void make_fighter(BotSeats& b, const sv::RulesCommon& s, int slot) {
  SeatBot& m = *b.bots[static_cast<size_t>(slot)];
  m.fighter = Fighter();
  m.made = b.graph || b.nav;
  if (!m.made) return;  // its nav still building: made when it lands
  m.sequence = 0;
  for (sh::MoveCmd& c : m.cmds) c = {};
  Fighter& f = m.fighter;
  f.features = features_of(m.skill);
  const NavGraph& graph = b.nav ? b.nav->graph : *b.graph;
  fighter_init(f, graph, s, slot, b.entities.c_str(), m.skill, traits_of(m.skill, Temper::balanced),
               kBotHz, b.seed + 31u * static_cast<uint32_t>(slot), *b.arsenal);
  f.bot.yaw = s.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
  if (b.nav) {
    f.bot.pilot.world = &b.nav->world;
    if (b.nav->swim.leading > 0) f.swim = &b.nav->swim;
    f.trap = &b.nav->trap;
  } else {
    f.bot.pilot.world = b.world.get();  // the hop looks ahead through the map
  }
}

}  // namespace

int bot_seats_count(const BotSeats& b) {
  int n = 0;
  for (const auto& m : b.bots) n += m ? 1 : 0;
  return n;
}

int bot_seats_add(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map, AimSkill skill,
                  uint32_t taken, std::string& error) {
  const sv::RulesCommon& s = *rules;
  int slot = -1;
  for (int i = 1; i < sh::kMaxClients && i < s.sv.set.maxclients; i++)
    if (!b.bots[static_cast<size_t>(i)] && !s.clients[i].active &&
        (((b.held | taken) >> i) & 1u) == 0) {
      slot = i;
      break;
    }
  if (slot < 0) {
    error = "The server is full.";
    return -1;
  }
  if (!ensure_nav(b, rules, map, error)) return -1;
  auto m = std::make_unique<SeatBot>();
  m->skill = skill;
  m->name = "bot" + std::to_string(++b.named);
  rules.connect(slot, m->name, false, true);
  b.bots[static_cast<size_t>(slot)] = std::move(m);
  make_fighter(b, s, slot);
  return slot;
}

int bot_seats_last(const BotSeats& b) {
  for (int i = sh::kMaxClients - 1; i > 0; i--)
    if (b.bots[static_cast<size_t>(i)]) return i;
  return -1;
}

void bot_seats_forget(BotSeats& b, int slot) {
  if (slot > 0 && slot < sh::kMaxClients) b.bots[static_cast<size_t>(slot)].reset();
}

bool bot_seats_nav(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map,
                   std::vector<int>& gone, std::string& error) {
  gone.clear();
  if (bot_seats_count(b) == 0 || ensure_nav(b, rules, map, error)) return true;
  for (int i = 1; i < sh::kMaxClients; i++)
    if (b.bots[static_cast<size_t>(i)]) {
      gone.push_back(i);
      b.bots[static_cast<size_t>(i)].reset();
    }
  return false;
}

void bot_seats_connect(BotSeats& b, const sv::RulesRunner& rules) {
  for (int i = 1; i < sh::kMaxClients; i++)
    if (b.bots[static_cast<size_t>(i)])
      rules.connect(i, b.bots[static_cast<size_t>(i)]->name, false, true);
}

void bot_seats_remake(BotSeats& b, const sv::RulesRunner& rules) {
  const sv::RulesCommon& s = *rules;
  for (int i = 1; i < sh::kMaxClients; i++)
    if (b.bots[static_cast<size_t>(i)] && s.clients[i].active) make_fighter(b, s, i);
}

void bot_seats_enter(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map,
                     bool reconnect, std::vector<int>& gone, std::string& error) {
  if (!bot_seats_nav(b, rules, map, gone, error)) return;
  if (reconnect) bot_seats_connect(b, rules);
  bot_seats_remake(b, rules);
}

bool bot_seats_nav_ready(const BotSeats& b) { return !b.job && (b.graph || b.nav); }

bool bot_seats_land(BotSeats& b, const sv::RulesRunner& rules) {
  if (!b.job || b.job->done.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return false;
  const std::shared_ptr<NavJob> job = std::move(b.job);
  job->done.get();
  if (job->ok) {
    take_nav(b, job->built, job->name, job->from);
    b.nav_spec = job->spec;
    bot_seats_remake(b, rules);
    return true;
  }
  b.gone_why = job->error;
  for (int i = 1; i < sh::kMaxClients; i++)
    if (b.bots[static_cast<size_t>(i)]) {
      b.gone.push_back(i);
      b.bots[static_cast<size_t>(i)].reset();
    }
  return true;
}

void bot_seats_packets(BotSeats& b, const sv::RulesRunner& rules, int msec,
                       std::vector<sh::Packet>& out) {
  bot_seats_land(b, rules);
  const sv::RulesCommon& s = *rules;
  const sv::Q2Server* q = sv::q2_of(rules);
  for (int i = 1; i < sh::kMaxClients; i++) {
    if (!b.bots[static_cast<size_t>(i)] || !s.clients[i].active) continue;
    SeatBot& m = *b.bots[static_cast<size_t>(i)];
    m.cmds[0] = m.cmds[1];
    m.cmds[1] = m.cmds[2];
    const sh::Vec3f view = s.sv.edicts[static_cast<size_t>(i) + 1].v_angle;
    if (!m.made) {  // its nav still building: it stands where it is
      m.cmds[2] = {};
      m.cmds[2].msec = static_cast<uint8_t>(msec);
      m.cmds[2].angles = view;
      if (q != nullptr && !q->opt.clients_take_view) {  // the rules' delta taken off, as below
        const int16_t* delta = q->players[i].move.delta_angles;
        constexpr float kDegrees = 360.0f / 65536;
        m.cmds[2].angles.x -= static_cast<float>(delta[0]) * kDegrees;
        m.cmds[2].angles.y -= static_cast<float>(delta[1]) * kDegrees;
        m.cmds[2].angles.z -= static_cast<float>(delta[2]) * kDegrees;
      }
    } else if (q == nullptr || q->opt.clients_take_view) {
      // A view the rules turned (a spawn spot's, a teleport's) sent back whole: the turn spent.
      const bool turned = q != nullptr && q->players[i].forced;
      if (turned) m.fighter.bot.yaw = view.y;
      m.cmds[2] = fighter_command(m.fighter, s, i, msec);
      if (turned) m.cmds[2].angles = view;
    } else {
      // Quake II's own model: the view is the command's plus the delta, kept; a new delta is a
      // turn.
      const int16_t* delta = q->players[i].move.delta_angles;
      if (delta[0] != m.delta_seen[0] || delta[1] != m.delta_seen[1] ||
          delta[2] != m.delta_seen[2]) {
        m.fighter.bot.yaw = view.y;
        for (int k = 0; k < 3; k++) m.delta_seen[k] = delta[k];
      }
      m.cmds[2] = fighter_command(m.fighter, s, i, msec);
      constexpr float kDegrees = 360.0f / 65536;
      m.cmds[2].angles.x -= static_cast<float>(delta[0]) * kDegrees;
      m.cmds[2].angles.y -= static_cast<float>(delta[1]) * kDegrees;
      m.cmds[2].angles.z -= static_cast<float>(delta[2]) * kDegrees;
    }
    sh::Packet p;
    p.client = i;
    p.sequence = ++m.sequence;
    for (int k = 0; k < 3; k++) p.cmds[k] = m.cmds[k];
    out.push_back(p);
  }
}

}  // namespace shooters_edge::bots
