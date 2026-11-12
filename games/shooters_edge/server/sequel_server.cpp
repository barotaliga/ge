// The sequel's rules as a runner (sequel_server.hpp states the rules): the map's start, the
// players' slots, their commands through the sequel's move, the 10 Hz frame, `kill` and the
// respawn, the console's settings, and the world the host reads.
//
// ge:summary The sequel's runner: the map started, players spawned and moved, the frame, the world
// ge:keywords sequel sequel-server sequel-runner spawn-point respawn kill deathmatch frame-100ms
// ge:keywords sequel_runner runner-calls fill-world
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_server_test.cpp
#include "shooters_edge/server/sequel_server.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "progs/text.hpp"
#include "shooters_edge/server/rjump_rules.hpp"
#include "shooters_edge/server/sequel_campaign.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monsters.hpp"
#include "shooters_edge/server/sequel_targets.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;
namespace con = ge::console;

constexpr double kFrameSeconds = 0.1;
constexpr double kKillAfter = 5;      // `kill` is refused this long after a spawn, s
constexpr double kRespawnAfter = 1;   // the dead may respawn this long after dying, s
constexpr uint8_t kRespawnHold = 14;  // the deathmatch respawn's teleport hold, in 8 ms
constexpr float kSpawnHealth = 100;
constexpr double kIntermissionHold = 5;  // the intermission's least time before a button leaves, s

SequelServer& sq(void* rules) { return *static_cast<SequelServer*>(rules); }

int16_t to_short_angle(float degrees) {
  return static_cast<int16_t>(num::to_int32(double(degrees) * (65536.0 / 360.0)));
}

sh::Vec3f parse_vec(const std::string& s) {
  sh::Vec3f v;
  if (std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z) != 3) return {};
  return v;
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void register_cvars(SequelServer& s) {
  s.cvars.add({.name = "deathmatch",
               .fallback = "1",
               .flags = con::kServer,
               .help = "the deathmatch rules: spawn points, respawns by attack"});
  s.cvars.add({.name = "coop",
               .fallback = "0",
               .flags = con::kServer,
               .help = "co-op: everyone on one team"});
  s.cvars.add({.name = "sv_gravity",
               .fallback = "800",
               .flags = con::kServer,
               .help = "the pull down on players, units/s^2"});
  s.cvars.add(
      {.name = "skill",
       .fallback = "1",
       .flags = con::kServer,
       .help = "0 easy .. 3 hard: which entities a map spawns, how many powerups are held"});
  s.cvars.add({.name = "dmflags",
               .fallback = "0",
               .flags = con::kServer,
               .help = "deathmatch's rule bits (8192: infinite ammo)"});
  s.cvars.add({.name = "fraglimit",
               .fallback = "0",
               .flags = con::kServer,
               .help = "the frags that end the level; 0 none"});
  s.cvars.add({.name = "timelimit",
               .fallback = "0",
               .flags = con::kServer,
               .help = "the minutes that end the level; 0 none"});
  s.cvars.add({.name = "sv_maplist",
               .fallback = "",
               .flags = con::kServer,
               .help = "the maps a level's end goes through in turn (spaces or commas)"});
  s.cvars.add({.name = "k_midair",
               .fallback = "0",
               .flags = con::kServer,
               .help = "1: midair in deathmatch, rockets that count only on a player in the air"});
  s.cvars.add({.name = "k_midair_minheight",
               .fallback = "1",
               .flags = con::kServer,
               .help = "midair: the least height a hit counts at, 0..4 (64, 128, 256, 512, 1024)"});
  s.cvars.add({.name = "sv_airaccelerate",
               .fallback = "0",
               .flags = con::kServer,
               .help = "the air move's acceleration in deathmatch; 0 the walking rule"});
  add_rjump_settings(s.cvars);  // sv_rjump, sv_rrjump (rjump_rules.hpp)
  const std::string share =
      ge::core::num::to_string(num::round(double(s.opt.speedjump) * 1000) / 1000);
  s.cvars.add({.name = "pm_speedjump",
               .fallback = share,
               .flags = con::kServer,
               .help = "0..1: a client with cl_speedjump 1 strafes for its player at this share of "
                       "the best strafe's gain (0 off)",
               .min = 0,
               .max = 1});
  s.sv.set.move.speedjump = s.opt.speedjump;
}

// The slots the loop's pool keeps (the endpoint and the listen read them by slot).
sh::ClientSlot& pool_slot(SequelServer& s, int slot) {
  if (s.sv.clients.size() < size_t(sh::kMaxClients)) s.sv.clients.resize(sh::kMaxClients);
  return s.sv.clients[size_t(slot)];
}

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }

// The squared distance from `p` to the nearest living player other than `skip`; huge for none.
double nearest_player(const SequelServer& s, sh::Vec3f p, int skip) {
  double best = 1e30;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const SequelPlayer& o = s.players[i];
    if (i == skip || !s.clients[i].active || !o.alive) continue;
    const sh::Vec3f d = unpack(o.move.origin) - p;
    const double sq = double(d.x) * d.x + double(d.y) * d.y + double(d.z) * d.z;
    if (sq < best) best = sq;
  }
  return best;
}

// The spot a player spawns at: in deathmatch a random `info_player_deathmatch` that is not one of
// the two nearest to a living player (any of them with two or fewer); else, or with none, the
// first `info_player_start`; else the map's origin.
const SequelEntity* spawn_spot(SequelServer& s, int slot) {
  if (deathmatch(s)) {
    std::vector<const SequelEntity*> spots;
    for (const SequelEntity& e : s.entities)
      if (e.classname == "info_player_deathmatch") spots.push_back(&e);
    if (!spots.empty() && fight_dmflag(s, 512)) {  // spawn farthest from every living player
      const SequelEntity* best = spots[0];
      double far = -1;
      for (const SequelEntity* e : spots) {
        const double d = nearest_player(s, e->origin, slot);
        if (d > far) {
          far = d;
          best = e;
        }
      }
      return best;
    }
    if (!spots.empty()) {
      const SequelEntity* near1 = nullptr;
      const SequelEntity* near2 = nullptr;
      if (spots.size() > 2) {
        double d1 = 1e31, d2 = 1e31;
        for (const SequelEntity* e : spots) {
          const double d = nearest_player(s, e->origin, slot);
          if (d < d1) {
            d2 = d1;
            near2 = near1;
            d1 = d;
            near1 = e;
          } else if (d < d2) {
            d2 = d;
            near2 = e;
          }
        }
      }
      std::vector<const SequelEntity*> allowed;
      for (const SequelEntity* e : spots)
        if (e != near1 && e != near2) allowed.push_back(e);
      return allowed[ge::core::pick_index(s.rng, allowed.size())];
    }
  }
  return campaign_spot(s, slot);
}

void spawn_player(SequelServer& s, int slot, bool respawn) {
  SequelPlayer& p = s.players[slot];
  if (respawn) fight_bury(s, slot);
  const SequelEntity* spot = spawn_spot(s, slot);
  const sh::Vec3f at = spot != nullptr ? spot->origin : sh::Vec3f{};
  const float yaw = spot != nullptr ? spot->angles.y : 0;
  sh::SequelState st;
  st.type = s.clients[slot].spectator ? sh::SequelType::spectator : sh::SequelType::normal;
  st.gravity = static_cast<int16_t>(s.cvars.find("sv_gravity")->integer());
  // 9 above the spot, then 1 higher; packed to the eighth, truncated.
  const float pos[3] = {at.x, at.y, at.z + 9 + 1};
  for (int i = 0; i < 3; i++) st.origin[i] = static_cast<int16_t>(num::to_int32(pos[i] * 8));
  // The delta angles turn the player's own view to the spot's yaw.
  const int16_t want[3] = {0, to_short_angle(yaw), 0};
  for (int i = 0; i < 3; i++) st.delta_angles[i] = int16_t(want[i] - p.cmd_angles[i]);
  p.forced = true;
  for (int i = 0; i < 3; i++) p.forced_angles[i] = want[i];
  if (respawn && deathmatch(s)) {
    st.flags |= sh::kSqTimeTeleport;
    st.time = kRespawnHold;
  }
  p.move = st;
  p.changed = true;
  p.alive = !s.clients[slot].spectator;
  p.health = p.alive ? kSpawnHealth : 0;
  p.spawned_at = s.level_time;
  p.view_angles = {0, yaw, 0};
  p.body = sq_new_body();
  p.body.health = static_cast<int>(p.health);
  p.mega = -1;
  p.taken.clear();
  p.mins = {-16, -16, -24};
  p.maxs = {16, 16, 32};
  p.view_height = 22;
  p.water_level = 0;
  p.on_ground = false;
  p.ground = -1;
  fight_spawn(s, slot);
  campaign_body(s, slot, respawn);
  fight_midair_kit(s, slot);
}

int skill(const SequelServer& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}

// Whether item row `item` is left out by `dmflags`: health (1), powerups (2), armour (2048), and
// with infinite ammo (8192) the ammo and the BFG.
bool dm_left_out(const SequelServer& s, int item) {
  if (item < 0 || !deathmatch(s)) return false;
  if (fight_midair(s)) return true;  // nothing lies on the map in midair
  const SqItem& it = kSqItems[item];
  const std::string_view name = it.classname;
  return (fight_dmflag(s, 1) && it.kind == SqKind::health) ||
         (fight_dmflag(s, 2) && it.kind == SqKind::powerup) ||
         (fight_dmflag(s, 2048) &&
          (it.kind == SqKind::armour || it.kind == SqKind::power_armour)) ||
         (fight_dmflag(s, 8192) && (it.kind == SqKind::ammo || name == "weapon_bfg"));
}

// Whether the map's entity is left out in this mode by its spawn flags.
bool left_out(const SequelServer& s, int flags) {
  if (deathmatch(s)) return (flags & 0x800) != 0;
  const int k = skill(s);
  return (k == 0 && (flags & 0x100) != 0) || (k == 1 && (flags & 0x200) != 0) ||
         (k >= 2 && (flags & 0x400) != 0);
}

constexpr float kItemHalf = 15;  // an item's box, ±15 each way

// Each item dropped up to 128 units onto the floor; one that starts in a solid is removed.
void drop_items(SequelServer& s) {
  const float mins[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float maxs[3] = {kItemHalf, kItemHalf, kItemHalf};
  for (SequelEntity& e : s.entities) {
    if (e.item < 0 || e.gone) continue;
    const float from[3] = {e.origin.x, e.origin.y, e.origin.z};
    const float to[3] = {e.origin.x, e.origin.y, e.origin.z - 128};
    const ge::brush::Trace t = s.tracer->trace(from, to, mins, maxs, sh::kSqSolid | sh::kSqWindow);
    if (t.start_solid) {
      e.gone = true;
      continue;
    }
    e.origin = {t.end[0], t.end[1], t.end[2]};
  }
}

// The items a living player's box touches, taken by their rule.
void touch_items(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    if (e.item < 0 || e.hidden || e.gone) continue;
    if (lo.x > e.origin.x + kItemHalf || hi.x < e.origin.x - kItemHalf ||
        lo.y > e.origin.y + kItemHalf || hi.y < e.origin.y - kItemHalf ||
        lo.z > e.origin.z + kItemHalf || hi.z < e.origin.z - kItemHalf)
      continue;
    const SqItem& it = kSqItems[e.item];
    // A weapon that stays (`dmflags` 4) is refused to one who holds it, and stays when taken.
    const bool stays = it.kind == SqKind::weapon && fight_dmflag(s, 4);
    if (stays && p.body.counts[e.item] > 0) continue;
    p.body.health = static_cast<int>(p.health);
    if (!sq_pick_up(e.item, p.body, deathmatch(s), skill(s), s.level_time)) continue;
    p.health = static_cast<float>(p.body.health);
    if (it.kind == SqKind::weapon && fight_dmflag(s, 8192)) {  // infinite ammo: a full stock
      const int k = static_cast<int>(it.ammo);
      p.body.ammo[k] = std::max(p.body.ammo[k], std::min(1000, p.body.max_ammo[k]));
    }
    p.taken.push_back(e.item);
    fight_took(s, slot, e.item);
    if (stays) continue;
    if (std::string_view(it.classname) == "item_health_mega") {
      e.hidden = true;  // shows again 20 s after its taker is back to his maximum
      e.respawn_at = 0;
      p.mega = static_cast<int>(i);
      p.drained_at = s.level_time + 4;  // the first point goes 5 s after
    } else if (deathmatch(s) && it.respawn_s > 0) {
      e.hidden = true;
      e.respawn_at = s.level_time + it.respawn_s;
    } else {
      e.gone = true;
    }
  }
  fight_touch_drops(s, slot);
}

// The frame's items: those whose time came show again; a mega counts its taker down.
void item_frame(SequelServer& s) {
  for (SequelEntity& e : s.entities)
    if (e.hidden && e.respawn_at > 0 && s.level_time + 0.001 >= e.respawn_at) {
      e.hidden = false;
      e.respawn_at = 0;
    }
  for (SequelPlayer& p : s.players) {
    if (p.mega < 0) continue;
    SequelEntity& mega = s.entities[size_t(p.mega)];
    if (p.alive && p.health > float(p.body.max_health)) {
      if (s.level_time + 0.001 >= p.drained_at + 1) {
        p.health -= 1;
        p.drained_at = s.level_time;
      }
      continue;
    }
    if (deathmatch(s))
      mega.respawn_at = s.level_time + 20;
    else
      mega.gone = true;
    p.mega = -1;
  }
}

void run_command(SequelServer& s, int slot, const sh::MoveCmd& c) {
  SequelPlayer& p = s.players[slot];
  sh::SequelMove pm;
  pm.s = p.move;
  if (s.intermission_at > 0)
    pm.s.type = sh::SequelType::freeze;
  else if (pm.s.type != sh::SequelType::spectator)
    pm.s.type = p.alive ? sh::SequelType::normal : sh::SequelType::dead;
  pm.s.gravity = static_cast<int16_t>(s.cvars.find("sv_gravity")->integer());
  pm.cmd = sh::sequel_command(c);  // the jump button the up move (sequel_move.hpp)
  if (s.opt.clients_take_view && p.forced && pm.cmd.angles[0] == p.forced_angles[0] &&
      pm.cmd.angles[1] == p.forced_angles[1] && pm.cmd.angles[2] == p.forced_angles[2]) {
    for (int16_t& d : pm.s.delta_angles) d = 0;  // the client took the turn as its own
    p.forced = false;
  }
  pm.changed = p.changed;
  pm.air_accelerate = sequel_air_accelerate(s);
  MapWorld mw{&s, p.alive ? sh::kSqMaskPlayerSolid : sh::kSqMaskDeadSolid};
  sh::sequel_move(pm, mw.world());
  p.move = pm.s;
  p.changed = false;
  for (int i = 0; i < 3; i++) p.cmd_angles[i] = pm.cmd.angles[i];
  p.buttons = c.buttons;
  p.view_angles = pm.view_angles;
  p.mins = pm.mins;
  p.maxs = pm.maxs;
  p.view_height = pm.view_height;
  p.water_level = pm.water_level;
  p.water_type = pm.water_type;
  p.on_ground = pm.ground_entity >= 0;
  p.ground = pm.ground_entity;
  if (!p.alive || s.clients[slot].spectator || s.intermission_at > 0) return;
  touch_items(s, slot);
  map_touch(s, slot);
  if (c.impulse >= 1 && c.impulse <= kSqWeaponCount) fight_choose(s, slot, c.impulse - 1);
  // The first command of the frame that presses attack steps the hand at once.
  if ((c.buttons & sh::kButtonAttack) != 0 && !p.stepped) {
    p.stepped = true;
    fight_step(s, slot, true);
  }
}

// The commands a packet brings: those a lost packet left unrun (up to the two it carries), then
// its newest. A packet older than the last one run brings nothing.
void take_packet(SequelServer& s, const sh::Packet& pk) {
  if (pk.client < 0 || pk.client >= sh::kMaxClients || !s.clients[pk.client].active) return;
  SequelPlayer& p = s.players[pk.client];
  if (p.last_sequence != 0 && pk.sequence <= p.last_sequence) return;
  const uint32_t gap = p.last_sequence == 0 ? 0 : pk.sequence - p.last_sequence - 1;
  const int lost = gap > 2 ? 2 : static_cast<int>(gap);
  for (int i = 2 - lost; i <= 2; i++) run_command(s, pk.client, pk.cmds[i]);
  p.last_sequence = pk.sequence;
}

// The intermission begins: the dead stand up, and everyone is moved to its spot (the first
// `info_player_intermission`, else the first start) and frozen there facing its angles, with no
// weapon and no model, until a button is pressed 5 s on.
void start_intermission(SequelServer& s) {
  s.intermission_at = s.level_time;
  if (deathmatch(s) || s.cvars.find("coop")->on())  // the scores, for everyone
    for (const std::string& l : fight_scores(s)) fight_print(s, -1, l + "\n");
  for (const std::string& l : fight_midair_report(s)) fight_print(s, -1, l);
  const SequelEntity* spot = nullptr;
  for (const char* name : {"info_player_intermission", "info_player_start"}) {
    for (const SequelEntity& e : s.entities)
      if (e.classname == name) {
        spot = &e;
        break;
      }
    if (spot != nullptr) break;
  }
  const sh::Vec3f at = spot != nullptr ? spot->origin : sh::Vec3f{};
  const sh::Vec3f look = spot != nullptr ? spot->angles : sh::Vec3f{};
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    SequelPlayer& p = s.players[i];
    if (!p.alive && !s.clients[i].spectator) spawn_player(s, i, false);
    const float pos[3] = {at.x, at.y, at.z};
    for (int k = 0; k < 3; k++) {
      p.move.origin[k] = static_cast<int16_t>(num::to_int32(pos[k] * 8));
      p.move.velocity[k] = 0;
    }
    p.move.type = sh::SequelType::freeze;
    const int16_t want[3] = {to_short_angle(look.x), to_short_angle(look.y), 0};
    for (int k = 0; k < 3; k++) {
      p.move.delta_angles[k] = int16_t(want[k] - p.cmd_angles[k]);
      p.forced_angles[k] = want[k];
    }
    p.forced = true;
    p.changed = true;
    p.view_angles = {look.x, look.y, 0};
  }
}

// The intermission's frame: 5 s on, any button pressed leaves for the next map.
void intermission_frame(SequelServer& s) {
  if (!s.pending_map.empty() || s.level_time + 0.001 < s.intermission_at + kIntermissionHold)
    return;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && s.players[i].buttons != 0) {
      s.pending_map = s.next_map;
      s.pending_change = MapChange::change_level;
      return;
    }
}

// One rules frame: the time advances, the dead who press attack a second after dying respawn.
void rules_frame(SequelServer& s) {
  s.frame_number++;
  s.level_time = double(s.frame_number) * kFrameSeconds;
  item_frame(s);
  if (s.intermission_at > 0) {
    intermission_frame(s);
    return;
  }
  map_frame(s);
  monster_frame(s);
  campaign_frame(s);
  fight_missiles(s);
  fight_drops(s);
  for (int i = 0; i < sh::kMaxClients; i++) {  // the hands no command stepped this frame
    SequelPlayer& p = s.players[i];
    if (s.clients[i].active && !p.stepped) fight_step(s, i, (p.buttons & sh::kButtonAttack) != 0);
    p.stepped = false;
    if (s.clients[i].active) fight_body_frame(s, i);
  }
  if (fight_limits(s) || s.exit_now) {
    s.exit_now = false;
    start_intermission(s);
    return;
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelPlayer& p = s.players[i];
    if (!s.clients[i].active || s.clients[i].spectator || p.alive) continue;
    if (s.level_time < p.died_at + kRespawnAfter - 0.001) continue;
    // By pressing attack, or at once with `dmflags` 1024 (forced respawn); by himself after 2 s in
    // midair.
    const bool by_himself =
        fight_midair(s) && s.level_time >= p.died_at + sh::kMidairRespawn - 0.001;
    if (deathmatch(s) &&
        ((p.buttons & sh::kButtonAttack) != 0 || fight_dmflag(s, 1024) || by_himself))
      spawn_player(s, i, true);
  }
}

void mirror_pool(SequelServer& s);

void fill_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  w.time = s.level_time;
  w.paused = s.paused;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& c = s.clients[i];
    const SequelPlayer& p = s.players[i];
    WorldClient& wc = w.clients[i];
    wc.active = c.active;
    wc.spectator = c.spectator;
    wc.bot = c.bot;
    wc.userid = c.userid;
    wc.name.clear();
    for (const auto& [k, v] : c.userinfo)
      if (k == "name") wc.name = v;
    wc.userinfo = c.userinfo;
    wc.ping_ms = c.ping_ms;
    wc.entered_at = c.entered_at;
    wc.numbers = {};
    WorldEntity& e = w.entities[size_t(i) + 1];
    e = {};
    if (!c.active) continue;
    wc.numbers.health = p.health;
    wc.numbers.armour = float(p.body.armour_count);
    wc.numbers.armour_type = kSqArmours[static_cast<int>(p.body.armour)].normal;
    // The HUD's four counters: shells, bullets, rockets, cells.
    const SqAmmo shown[4] = {SqAmmo::shells, SqAmmo::bullets, SqAmmo::rockets, SqAmmo::cells};
    for (int k = 0; k < 4; k++) wc.numbers.ammo[k] = float(p.body.ammo[static_cast<int>(shown[k])]);
    wc.numbers.frags = p.frags;
    wc.numbers.view_z = p.view_height;
    wc.numbers.fov = fight_fov(s, i);
    e.free = false;
    e.origin = unpack(p.move.origin);
    e.velocity = unpack(p.move.velocity);
    e.angles = {0, p.view_angles.y, 0};
    e.view_angles = p.view_angles;
    e.mins = p.mins;
    e.maxs = p.maxs;
    e.absmin = e.origin + p.mins;
    e.absmax = e.origin + p.maxs;
    e.flags = p.on_ground ? sh::kFlagOnGround : 0;
    e.waterlevel = p.water_level;
    e.health = p.health;
    const bool unseen = c.spectator || p.gibbed || s.intermission_at > 0;
    e.modelindex = unseen ? 0 : static_cast<int>(s.models.size()) - 1;
  }
  for (size_t i = 0; i < s.entities.size(); i++) {  // the items shown or hidden
    const SequelEntity& m = s.entities[i];
    if (m.item >= 0) w.entities[1 + sh::kMaxClients + i].free = m.gone || m.hidden;
  }
  map_rows(s);
  monster_rows(s);
  targets_rows(s);
  fight_rows(s);
  mirror_pool(s);
}

// The world's rows mirrored into the loop's pool (`sv.edicts`), which the classic endpoint, the
// listen's view and the hit tests still read by slot: the same entities, as the world shows them.
void mirror_pool(SequelServer& s) {
  const RulesWorld& w = s.world;
  if (s.sv.edicts.size() < w.entities.size()) return;  // sized at the map's start
  s.sv.time = s.host_time;
  s.sv.num_edicts = static_cast<int>(w.entities.size());
  for (size_t i = 0; i < w.entities.size(); i++) {
    const WorldEntity& row = w.entities[i];
    sh::Edict& e = s.sv.edicts[i];
    e.free = row.free;
    e.origin = row.origin;
    e.velocity = row.velocity;
    e.angles = row.angles;
    e.v_angle = row.view_angles;
    e.mins = row.mins;
    e.maxs = row.maxs;
    e.size = row.maxs - row.mins;
    e.absmin = row.absmin;
    e.absmax = row.absmax;
    e.flags = row.flags;
    e.waterlevel = row.waterlevel;
    e.health = row.health;
    e.modelindex = row.modelindex;
  }
  if (s.sv.clients.size() < size_t(sh::kMaxClients)) s.sv.clients.resize(sh::kMaxClients);
  for (int i = 0; i < sh::kMaxClients; i++) {
    s.sv.edicts[size_t(i) + 1].view_ofs = {0, 0, s.players[i].view_height};
    sh::ClientSlot& slot = s.sv.clients[size_t(i)];
    slot.active = s.clients[i].active;
    slot.spectator = s.clients[i].spectator;
    slot.bot = s.clients[i].bot;
    slot.last_sequence = s.players[i].last_sequence;
  }
}

void fill_lists(SequelServer& s) {
  RulesWorld& w = s.world;
  w.map_name = s.map_name;
  w.map_title.clear();
  if (!s.map->entities.empty()) w.map_title = s.map->entities[0].get("message");
  w.models = s.models;
  w.sounds = s.sounds;
  w.vwep.clear();
  w.statics.clear();
  w.max_clients = sh::kMaxClients;
  w.entities.assign(
      1 + sh::kMaxClients + s.entities.size() + kSequelMissiles + kSequelDrops + kSequelBodies,
      WorldEntity{});
  w.entity_count = static_cast<int>(w.entities.size());
  w.entities[0].free = false;
  w.entities[0].modelindex = 1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const SequelEntity& m = s.entities[i];
    WorldEntity& e = w.entities[1 + sh::kMaxClients + i];
    // Triggers, fields and the map's points are never drawn.
    e.free = m.gone || m.hidden || m.trigger || m.classname.rfind("trigger_", 0) == 0;
    e.origin = m.origin;
    e.angles = m.angles;
    e.modelindex = m.modelindex;
  }
}

bool sq_start(void* r, const MapSpec& map, std::string& error) {
  SequelServer& s = sq(r);
  if (map.brushes == nullptr) {
    error = "the sequel's rules cannot start map '" + map.name +
            "': it has no brushes (MapSpec::brushes). Read the version 38 map with "
            "brush::read_bsp and pass it whole.";
    return false;
  }
  s.map = map.brushes;
  s.tracer = std::make_unique<ge::brush::BrushTracer>(*s.map);
  s.map_name = map.name;
  s.entities.clear();
  for (size_t i = 1; i < s.map->entities.size(); i++) {
    const ge::brush::Entity& b = s.map->entities[i];
    SequelEntity e;
    e.classname = b.get("classname");
    e.targetname = b.get("targetname");
    e.target = b.get("target");
    e.model = b.get("model");
    e.origin = parse_vec(b.get("origin"));
    if (!b.get("angles").empty())
      e.angles = parse_vec(b.get("angles"));
    else if (!b.get("angle").empty())
      e.angles.y = std::strtof(b.get("angle").c_str(), nullptr);
    if (e.model.size() > 1 && e.model[0] == '*') e.modelindex = std::atoi(e.model.c_str() + 1);
    e.spawnflags = std::atoi(b.get("spawnflags").c_str());
    e.item = sq_item(e.classname);
    e.gone = left_out(s, e.spawnflags) || dm_left_out(s, e.item);
    s.entities.push_back(std::move(e));
  }
  s.models.assign(1, "");
  for (size_t i = 1; i < s.map->models.size(); i++) s.models.push_back("*" + std::to_string(i));
  for (SequelEntity& e : s.entities) {  // each item's model, once
    if (e.item < 0) continue;
    const std::string model = kSqItems[e.item].model;
    const auto found = std::find(s.models.begin(), s.models.end(), model);
    e.modelindex = static_cast<int>(found - s.models.begin());
    if (found == s.models.end()) s.models.push_back(model);
  }
  s.item_models.assign(size_t(kSqItemCount), 0);  // every item's, for those a player drops
  for (int i = 0; i < kSqItemCount; i++) {
    const std::string model = kSqItems[i].model;
    const auto found = std::find(s.models.begin(), s.models.end(), model);
    s.item_models[size_t(i)] = static_cast<int>(found - s.models.begin());
    if (found == s.models.end()) s.models.push_back(model);
  }
  s.sounds.assign(1, "");
  fight_precache(s);
  monster_precache(s);
  campaign_start(s);
  fight_clear(s);
  s.models.push_back(kSequelPlayerModel);
  drop_items(s);
  s.frame_number = 0;
  s.level_time = 0;
  s.exit_now = false;
  map_spawn(s);
  monster_spawn(s);
  s.host_time = 0;
  s.map_stopped = false;
  s.pending_map.clear();
  s.intermission_at = 0;
  s.next_map.clear();
  fill_lists(s);
  // The pool the world is mirrored into: every row the world has, and room past them.
  s.sv.set.max_edicts = std::max(s.sv.set.max_edicts, int(s.world.entities.size()) + 8);
  s.sv.edicts.assign(size_t(s.sv.set.max_edicts), sh::Edict{});
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) spawn_player(s, i, false);
  fill_rows(s);
  return true;
}

void set_info(std::vector<std::pair<std::string, std::string>>& info, const std::string& key,
              const std::string& value) {
  for (auto& kv : info)
    if (kv.first == key) {
      kv.second = value;
      return;
    }
  info.emplace_back(key, value);
}

void sq_connect(void* r, int slot, const std::string& name, bool spectator, bool bot,
                const float* /*parms: a deathmatch body starts from scratch*/) {
  SequelServer& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients) return;
  ClientInfo& c = s.clients[slot];
  c = ClientInfo{};
  c.active = true;
  c.spectator = spectator;
  c.bot = bot;
  c.userid = s.next_userid++;
  c.entered_at = s.level_time;
  set_info(c.userinfo, "name", name);
  pool_slot(s, slot).speedjump = false;
  s.players[slot] = SequelPlayer{};
  if (s.map != nullptr) spawn_player(s, slot, false);
  if (s.map != nullptr) fill_rows(s);
}

void sq_drop(void* r, int slot) {
  SequelServer& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients) return;
  s.clients[slot] = ClientInfo{};
  pool_slot(s, slot).speedjump = false;
  s.players[slot] = SequelPlayer{};
  if (s.map != nullptr) fill_rows(s);
}

// The rocket jump helper's question (rjump_rules.hpp): the launcher chosen when it is not in
// hand (its raising is waited for), and fired by the first command of a frame with the hand ready.
sh::RjumpReady sq_rocket_ready(void* r, int slot, sh::MoveCmd& /*cmd*/, std::string& why) {
  SequelServer& s = sq(r);
  SequelPlayer& p = s.players[slot];
  const int rl = int(SqWeapon::rocket_launcher);
  if (!p.alive || s.clients[slot].spectator || s.intermission_at > 0) return sh::RjumpReady::never;
  if (!sq_holds(p.body, rl)) {
    why = "You have no rocket launcher.\n";
    return sh::RjumpReady::never;
  }
  if (!sq_can_fire(p.body, rl)) {
    why = "You have no rockets.\n";
    return sh::RjumpReady::never;
  }
  if (p.hand.weapon != rl || (p.hand.pending >= 0 && p.hand.pending != rl)) {
    if (p.hand.pending != rl) fight_choose(s, slot, rl);
    return sh::RjumpReady::soon;
  }
  if (p.hand.state != SqHandState::ready || p.stepped) return sh::RjumpReady::soon;
  return sh::RjumpReady::now;
}

void sq_frame(void* r, const sh::Packet* packets, int count) {
  SequelServer& s = sq(r);
  if (s.map == nullptr || s.paused) return;
  s.rj_packets.assign(packets, packets + count);
  for (sh::Packet& p : s.rj_packets) {
    const bool shot = rjump_packet(s, p, &s, &sq_rocket_ready, s.level_time);
    if (p.client < 0 || p.client >= sh::kMaxClients) continue;
    // The view a shot turned is put back once it has fired: the view before it.
    const sh::Vec3f before = s.players[p.client].view_angles;
    take_packet(s, p);
    if (shot) s.players[p.client].view_angles = before;
  }
  s.host_time += s.sv.set.mintic;
  while (s.host_time + 1e-9 >= double(s.frame_number + 1) * kFrameSeconds) rules_frame(s);
  fill_rows(s);
}

CommandResult sq_client_command(void* r, int slot, const std::string& line) {
  SequelServer& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return CommandResult::bad;
  ge::progs::Words w;
  ge::progs::tokenize_words(line.c_str(), w);
  if (w.count == 0) return CommandResult::bad;
  const std::string cmd = ge::progs::word(w, 0);
  if (campaign_command(s, slot, cmd)) return CommandResult::mod;
  if (cmd == "score" || cmd == "players" || cmd == "help") {
    for (const std::string& l : fight_scores(s)) fight_print(s, slot, l + "\n");
    return CommandResult::mod;
  }
  if (cmd == "kill") {
    SequelPlayer& p = s.players[slot];
    if (p.alive && s.level_time >= p.spawned_at + kKillAfter) fight_kill(s, slot, slot, -1);
    fill_rows(s);
    return CommandResult::mod;
  }
  std::string rest;
  for (int i = 1; i < w.count; i++) rest.append(i > 1 ? " " : "").append(ge::progs::word(w, i));
  if (fight_command(s, slot, cmd, rest)) {
    fill_rows(s);
    return CommandResult::mod;
  }
  return CommandResult::bad;
}

bool sq_console(void* r, const std::string& line) {
  SequelServer& s = sq(r);
  ge::progs::Words w;
  ge::progs::tokenize_words(line.c_str(), w);
  if (w.count == 0) return true;
  const std::string cmd = ge::progs::word(w, 0);
  if ((cmd == "map" || cmd == "changelevel") && w.count >= 2) {
    s.pending_map = ge::progs::word(w, 1);
    s.pending_change = cmd == "map" ? MapChange::new_game : MapChange::change_level;
    return true;
  }
  if (cmd == "restart") {
    s.pending_map = s.map_name;
    s.pending_change = MapChange::restart;
    return true;
  }
  if (s.cvars.find(cmd) != nullptr) {
    if (w.count >= 2) {
      s.cvars.set(cmd, ge::progs::word(w, 1), con::Source::local);
      if (cmd == "pm_speedjump")
        s.sv.set.move.speedjump = static_cast<float>(s.cvars.find(cmd)->number);
    } else
      s.console.push_back(cmd + " is \"" + s.cvars.find(cmd)->value + "\"");
    return true;
  }
  return false;
}

void sq_user_info(void* r, int slot, const std::string& key, const std::string& value) {
  SequelServer& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return;
  set_info(s.clients[slot].userinfo, key, value);
  if (key == "cl_speedjump") pool_slot(s, slot).speedjump = value == "1";  // the easy jump's ask
  if (s.map != nullptr) fill_rows(s);
}

bool sq_pause(void* r, int /*slot*/, bool on) {
  SequelServer& s = sq(r);
  if (s.paused == on) return false;
  s.paused = on;
  s.world.paused = on;
  return true;
}

void sq_carry(void* /*r*/) {}  // a deathmatch body starts from scratch on every map

void sq_destroy(void* r) { delete static_cast<SequelServer*>(r); }

constexpr RunnerCalls kSequelCalls = {"sequel",  &sq_start,          &sq_connect, &sq_drop,
                                      &sq_frame, &sq_client_command, &sq_console, &sq_user_info,
                                      &sq_pause, &sq_carry,          &sq_destroy};

}  // namespace

SequelServer::SequelServer() = default;
SequelServer::~SequelServer() = default;

std::unique_ptr<SequelServer> make_sequel_server(const SequelServerOptions& opt) {
  auto s = std::make_unique<SequelServer>();
  s->opt = opt;
  s->rng = ge::core::make_rng(opt.seed);
  register_cvars(*s);
  return s;
}

RulesRunner sequel_runner(std::unique_ptr<SequelServer> s) {
  SequelServer* p = s.release();
  return {&kSequelCalls, p, p};
}

SequelServer* sequel_of(const RulesRunner& r) {
  return r.calls == &kSequelCalls ? static_cast<SequelServer*>(r.rules) : nullptr;
}

sh::MoveState sequel_aid_state(const SequelServer& s, int slot) {
  const SequelPlayer& p = s.players[slot];
  sh::MoveState m;
  m.origin = unpack(p.move.origin);
  m.velocity = unpack(p.move.velocity);
  m.ground = p.on_ground ? 0 : -1;
  m.waterlevel = p.water_level;
  const bool playing = p.alive && !s.clients[slot].spectator && s.intermission_at <= 0;
  m.type = playing ? sh::MoveType::normal : sh::MoveType::none;
  return m;
}

float sequel_air_accelerate(const SequelServer& s) {
  return deathmatch(s) ? static_cast<float>(s.cvars.find("sv_airaccelerate")->number) : 0;
}

}  // namespace shooters_edge::server
