// Quake II's rules as a runner (q2_server.hpp states the rules): the map's start, the
// players' slots, their commands through Quake II's move, the 10 Hz frame, `kill` and the
// respawn, the console's settings, and the world the host reads.
//
// ge:summary Quake II's runner: the map started, players spawned and moved, the frame, the world
// ge:keywords q2 q2-server q2-runner spawn-point respawn kill deathmatch frame-100ms
// ge:keywords q2_runner runner-calls fill-world
// ge:skill shooter
// ge:test tests/shooters_edge_q2_server_test.cpp
#include "shooters_edge/server/q2_server.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "brush/light_probe.hpp"
#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "classic/coop.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "progs/text.hpp"
#include "shooters_edge/server/q2_actor.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_blend.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_commands.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_mods.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_screens.hpp"
#include "shooters_edge/server/q2_spectate.hpp"
#include "shooters_edge/server/q2_targets.hpp"
#include "shooters_edge/server/rjump_rules.hpp"
#include "shooters_edge/server/shot_help.hpp"

namespace shooters_edge::server {

namespace {

// The weapon impulse `n` picks (1 the first): with Quake's weapons worn, Quake's `n`th.
int impulse_weapon(const Q2Server& s, int n) { return (s.quake_arms ? kSqQuakeFirst : 0) + n - 1; }

}  // namespace

namespace {

namespace num = ge::core::num;
namespace con = ge::console;

constexpr double kFrameSeconds = 0.1;
constexpr double kKillAfter = 5;      // `kill` is refused this long after a spawn, s
constexpr double kRespawnAfter = 1;   // the dead may respawn this long after dying, s
constexpr uint8_t kRespawnHold = 14;  // the deathmatch respawn's teleport hold, in 8 ms
constexpr int kSqItemBackEvent = 1;   // the entity event an item that shows again carries
constexpr float kSpawnHealth = 100;
constexpr double kIntermissionHold = 5;  // the intermission's least time before a button leaves, s
// Its most in a deathmatch of bots alone (no person to press a button), s. A match with a person
// in it, single player and coop wait for a button as long as it takes.
constexpr double kIntermissionMost = 20;

Q2Server& sq(void* rules) { return *static_cast<Q2Server*>(rules); }

int16_t to_short_angle(float degrees) {
  return static_cast<int16_t>(num::to_int32(double(degrees) * (65536.0 / 360.0)));
}

sh::Vec3f parse_vec(const std::string& s) {
  sh::Vec3f v;
  if (std::sscanf(s.c_str(), "%f %f %f", &v.x, &v.y, &v.z) != 3) return {};
  return v;
}

sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void register_cvars(Q2Server& s) {
  s.cvars.add({.name = "deathmatch",
               .fallback = "1",
               .flags = con::kServer,
               .help = "The deathmatch rules, with spawn points and respawns by attack.",
               .values = "0: the single-player or coop game; 1: deathmatch (the default)"});
  s.cvars.add({.name = "coop",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether everyone plays on one team, cooperatively.",
               .values = "0: not coop (the default); 1: coop"});
  // A dead player's running powerups left for others, off by default: Quake's quad drop by
  // the same name (`dmflags` 16384 drops it too), and one for each of Quake II's other timed
  // ones.
  for (const auto& [name, what] : {std::pair{"sv_dropquad", "the Quad Damage"},
                                   std::pair{"sv_droppent", "the Invulnerability"},
                                   std::pair{"sv_dropsuit", "the Environment Suit"},
                                   std::pair{"sv_dropbreather", "the Rebreather"},
                                   std::pair{"sv_dropsilencer", "the Silencer"}}) {
    const bool quad = std::string_view(name) == "sv_dropquad";
    s.cvars.add({.name = name,
                 .fallback = "0",
                 .flags = con::kServer,
                 .help = std::string("Whether a dead player leaves ") + what + " they held, with " +
                         (std::string_view(name) == "sv_dropsilencer" ? "the shots" : "the time") +
                         " it had left.",
                 .values = quad ? "0: it is lost unless dmflags has 16384 (the default); 1: left "
                                  "for whoever takes it first"
                                : "0: it is lost (the default); 1: left for whoever takes it "
                                  "first"});
  }
  for (const ge::classic::CoopSetting& c : ge::classic::kCoopSettings)  // both games' (coop.hpp)
    s.cvars.add({.name = c.name,
                 .fallback = c.fallback,
                 .flags = con::kServer,
                 .help = c.help,
                 .values = c.values});
  s.cvars.add({.name = "sv_gravity",
               .fallback = "800",
               .flags = con::kServer,
               .help = "The pull down on players, in units a second a second.",
               .values = "N: that pull (800 the default)"});
  s.cvars.add({.name = "skill",
               .fallback = "1",
               .flags = con::kServer,
               .help = "Which entities a map spawns and how many powerups are held.",
               .values = "0: easy; 1: normal (the default); 2: hard; 3: hardest"});
  s.cvars.add(
      {.name = "dmflags",
       .fallback = "0",
       .flags = con::kServer,
       .help = "Deathmatch's rule bits.",
       .values =
           "0: none (the default); N: the sum of the bits, such as 8192 for infinite ammunition"});
  s.cvars.add({.name = "fraglimit",
               .fallback = "0",
               .flags = con::kServer,
               .help = "The frags that end the level.",
               .values = "0: no limit (the default); 1..N: that many frags"});
  s.cvars.add({.name = "timelimit",
               .fallback = "0",
               .flags = con::kServer,
               .help = "The minutes that end the level.",
               .values = "0: no limit (the default); 1..N: that many minutes"});
  s.cvars.add(
      {.name = "sv_maplist",
       .fallback = "",
       .flags = con::kServer,
       .help = "The maps a level's end goes through in turn.",
       .kind = con::CvarKind::text,
       .values =
           "\"\": the same map again (the default); TEXT: these maps, spaces or commas between"});
  s.cvars.add({.name = "sv_wear",
               .fallback = "",
               .flags = con::kServer,
               .help = "Whose weapons, armour, ammunition and health the rules wear, from the next "
                       "map (classic remix).",
               .kind = con::CvarKind::text,
               .values = "\"\": the game's own (the default); q1: Quake's"});
  s.cvars.add({.name = "sv_physics",
               .fallback = "",
               .flags = con::kServer,
               .help = "Whose physics the players' bodies take (running, friction, the air, the "
                       "landing, swimming, harm from a fall), from the next map (classic remix).",
               .kind = con::CvarKind::text,
               .values = "\"\": the game's own (the default); q1: Quake's"});
  s.cvars.add({.name = "sv_speedcheck",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether a player whose moves add up to more time than passed is held back.",
               .values =
                   "0: not checked (the default; a server sets 1); 1: a move past his time budget "
                   "is not run"});
  s.cvars.add({.name = "sv_fps",
               .fallback = "10",
               .flags = con::kServer,
               .help = "The rules' frames a second, from the next map: the doors and lifts, the "
                       "shots and what falls step this often.",
               .values = "10: the classic beat (the default); 20, 30, 40, 50 or 60: that many "
                         "(another number is taken down to its ten, 10 to 60)"});
  s.cvars.add({.name = "k_midair",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether rockets count only on a player in the air, in deathmatch.",
               .values = "0: every hit counts (the default); 1: midair only"});
  s.cvars.add(
      {.name = "k_midair_minheight",
       .fallback = "1",
       .flags = con::kServer,
       .help = "The least height a midair hit counts at.",
       .values =
           "0: 64 units; 1: 128 units (the default); 2: 256 units; 3: 512 units; 4: 1024 units"});
  // The jump's two leftovers lifted for clients that predict our move (q2_move_feel).
  s.cvars.add({.name = "sv_landjump",
               .fallback = "1",
               .flags = con::kServer,
               .help = "Whether a player may jump again at once after landing from a fast fall.",
               .values = "0: a landing faster than 200 units a second holds the next jump for 144 "
                         "or 200 ms; 1: no hold for players whose game predicts it (the default); "
                         "2: no hold for anyone"});
  s.cvars.add({.name = "sv_waterjump",
               .fallback = "1",
               .flags = con::kServer,
               .help = "Whether jump held under water lifts a swimmer at a fixed speed.",
               .values = "0: the fixed lift (100 in water, 80 in slime, 50 in lava); 1: only the "
                         "swim's own up move for players whose game predicts it (the default); 2: "
                         "the same for anyone"});
  s.cvars.add({.name = "sv_airaccelerate",
               .fallback = "0",
               .flags = con::kServer,
               .help = "The air move's acceleration in deathmatch.",
               .values = "0: the walking rule (the default); N: that acceleration"});
  s.cvars.add(
      {.name = "sv_aimhelp_x",
       .fallback = "24",
       .flags = con::kServer,
       .help = "The aim help's leeway sideways for a player granted it, in units at the target.",
       .values = "0: no sideways help; N: that many units (24 the default)"});
  s.cvars.add(
      {.name = "sv_aimhelp_y",
       .fallback = "48",
       .flags = con::kServer,
       .help = "The aim help's leeway up and down for a player granted it, in units at the target.",
       .values = "0: no vertical help; N: that many units (48 the default)"});
  // The view's motion (plugins/classic/q2_view.hpp): the run's tilt and the stride's bob.
  for (const auto& [name, fallback, help] :
       {std::tuple{"run_pitch", "0.002", "How far running forward dips the view, a unit of speed."},
        std::tuple{"run_roll", "0.005",
                   "How far running sideways rolls the view, a unit of speed."},
        std::tuple{"bob_up", "0.005", "How far the stride lifts the eye, a unit of speed."},
        std::tuple{"bob_pitch", "0.002", "How far the stride nods the view, a unit of speed."},
        std::tuple{"bob_roll", "0.002", "How far the stride rocks the view, a unit of speed."}})
    s.cvars.add({.name = name,
                 .fallback = fallback,
                 .flags = con::kServer,
                 .help = help,
                 .values = std::string("0: none; N: that much (") + fallback + " the default)"});
  add_rjump_settings(s.cvars);  // sv_rjump, sv_rrjump (rjump_rules.hpp)
  commands_cvars(s);            // cheats and the chat's flood guard (q2_commands.hpp)
  q2_save_cvars(s);             // sv_noreload (q2_save.hpp)
  const std::string share =
      ge::core::num::to_string(num::round(double(s.opt.speedjump) * 1000) / 1000);
  s.cvars.add({.name = "pm_speedjump",
               .fallback = share,
               .flags = con::kServer,
               .help = "The share of the best strafe's gain a client with cl_speedjump 1 strafes "
                       "at for its player.",
               .min = 0,
               .max = 1,
               .values = "0: off; 0..1: that share"});
  s.sv.set.move.speedjump = s.opt.speedjump;
}

// The slots the loop's pool keeps (the endpoint and the listen read them by slot).
sh::ClientSlot& pool_slot(Q2Server& s, int slot) {
  if (s.sv.clients.size() < size_t(sh::kMaxClients)) s.sv.clients.resize(sh::kMaxClients);
  return s.sv.clients[size_t(slot)];
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const Q2Server& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }

// `sv_fps` as a level starts: 10 to 60, taken down to its ten.
int level_fps(const Q2Server& s) {
  const con::Cvar* c = s.cvars.find("sv_fps");
  const double v = c != nullptr ? c->number : 10;
  if (!(v >= 10)) return 10;
  return v >= 60 ? 60 : static_cast<int>(v / 10) * 10;
}

// The squared distance from `p` to the nearest living player other than `skip`; huge for none.
double nearest_player(const Q2Server& s, sh::Vec3f p, int skip) {
  double best = 1e30;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const Q2Player& o = s.players[i];
    if (i == skip || !s.clients[i].active || !o.alive) continue;
    const sh::Vec3f d = unpack(o.move.origin) - p;
    const double sq = double(d.x) * d.x + double(d.y) * d.y + double(d.z) * d.z;
    if (sq < best) best = sq;
  }
  return best;
}

// The spot a player spawns at: in deathmatch a random `info_player_deathmatch` not among a pair
// near a living player (any of them with two or fewer): walking the spots in map order, one
// nearer than the nearest so far takes its place, one nearer only than the second the second's;
// else, or with none, the
// first `info_player_start`; else the map's origin.
const Q2Entity* spawn_spot(Q2Server& s, int slot) {
  if (deathmatch(s)) {
    std::vector<const Q2Entity*> spots;
    for (const Q2Entity& e : s.entities)
      if (e.classname == "info_player_deathmatch") spots.push_back(&e);
    if (!spots.empty() && fight_dmflag(s, 512)) {  // spawn farthest from every living player
      const Q2Entity* best = spots[0];
      double far = -1;
      for (const Q2Entity* e : spots) {
        const double d = nearest_player(s, e->origin, slot);
        if (d > far) {
          far = d;
          best = e;
        }
      }
      return best;
    }
    // g_spawn_mode (a match server's): 0 the game's own pick, 1 the true two nearest left out,
    // 2 any spot at random.
    const con::Cvar* mode_cvar = s.cvars.find("g_spawn_mode");
    const int mode = mode_cvar != nullptr ? mode_cvar->integer() : 0;
    if (!spots.empty() && mode == 2) return spots[ge::core::pick_index(s.rng, spots.size())];
    if (!spots.empty()) {
      const Q2Entity* near1 = nullptr;
      const Q2Entity* near2 = nullptr;
      if (spots.size() > 2) {
        double d1 = 1e31, d2 = 1e31;
        for (const Q2Entity* e : spots) {
          const double d = nearest_player(s, e->origin, slot);
          if (d < d1) {  // mode 0: the old nearest is not moved to second, so the pair left out
            if (mode == 1) {  // is not always the two nearest; mode 1 moves it
              d2 = d1;
              near2 = near1;
            }
            d1 = d;
            near1 = e;
          } else if (d < d2) {
            d2 = d;
            near2 = e;
          }
        }
      }
      std::vector<const Q2Entity*> allowed;
      for (const Q2Entity* e : spots)
        if (e != near1 && e != near2) allowed.push_back(e);
      return allowed[ge::core::pick_index(s.rng, allowed.size())];
    }
  }
  return campaign_spot(s, slot);
}

void arrival_flash(Q2Server& s, int slot, const char* name);

// A match server's warmup (the lobby's phase in the local info, `ge_phase`, before the match)
// with `g_warmup_loadout` 1: every weapon but the BFG10K, each ammunition full and body armour,
// the rocket launcher in hand, so the warmup is spent fighting rather than gathering.
void warmup_kit(Q2Server& s, int slot) {
  const con::Cvar* on = s.cvars.find("g_warmup_loadout");
  Q2Player& p = s.players[slot];
  if (on == nullptr || !on->on() || !deathmatch(s) || !p.alive) return;
  std::string_view phase;
  for (const auto& [k, v] : s.localinfo)
    if (k == "ge_phase") phase = v;
  if (phase != "warmup" && phase != "countdown") return;
  for (const char* w :
       {"weapon_shotgun", "weapon_supershotgun", "weapon_machinegun", "weapon_chaingun",
        "weapon_grenadelauncher", "weapon_rocketlauncher", "weapon_hyperblaster", "weapon_railgun"})
    if (const int row = sq_item(w); row >= 0) p.body.counts[row] = 1;
  const std::pair<SqAmmo, int> full[] = {{SqAmmo::bullets, 200}, {SqAmmo::shells, 100},
                                         {SqAmmo::rockets, 50},  {SqAmmo::grenades, 50},
                                         {SqAmmo::cells, 200},   {SqAmmo::slugs, 50}};
  for (const auto& [kind, n] : full) {
    const auto k = static_cast<size_t>(kind);
    p.body.max_ammo[k] = std::max(p.body.max_ammo[k], n);
    p.body.ammo[k] = n;
  }
  p.body.armour = SqArmour::body;
  p.body.armour_count = 100;
  p.hand = SqHand{};
  p.hand.weapon = int(SqWeapon::rocket_launcher);
}

void spawn_player(Q2Server& s, int slot, bool respawn) {
  Q2Player& p = s.players[slot];
  if (respawn) fight_bury(s, slot);
  const SqBody died = p.body;  // a co-op respawn keeps the keys he held
  const Q2Entity* spot = spawn_spot(s, slot);
  const sh::Vec3f at = spot != nullptr ? spot->origin : sh::Vec3f{};
  const float yaw = spot != nullptr ? spot->angles.y : 0;
  ge::classic::Q2State st;
  st.type =
      s.clients[slot].spectator ? ge::classic::Q2Type::spectator : ge::classic::Q2Type::normal;
  st.gravity = static_cast<int16_t>(s.cvars.find("sv_gravity")->integer());
  // 9 above the spot, then 1 higher; packed to the eighth, truncated.
  const float pos[3] = {at.x, at.y, at.z + 9 + 1};
  for (int i = 0; i < 3; i++) st.origin[i] = static_cast<int16_t>(num::to_int32(pos[i] * 8));
  // The delta angles turn the player's own view to the spot's yaw.
  const int16_t want[3] = {0, to_short_angle(yaw), 0};
  for (int i = 0; i < 3; i++) st.delta_angles[i] = int16_t(want[i] - p.cmd_angles[i]);
  p.forced = true;
  for (int i = 0; i < 3; i++) p.forced_angles[i] = want[i];
  if (respawn && (deathmatch(s) || coop(s))) {
    st.flags |= ge::classic::kSqTimeTeleport;
    st.time = kRespawnHold;
    // The teleport's burst where he comes back, at his box's middle.
    if (!s.clients[slot].spectator) fight_effect(s, "te_teleport", {at.x, at.y, at.z + 10});
  }
  p.move = st;
  p.changed = true;
  p.alive = !s.clients[slot].spectator;
  p.health = p.alive ? kSpawnHealth : 0;
  p.spawned_at = s.level_time;
  p.respawn_time = respawn ? s.level_time : 0;  // a first placing counts from the map's start
  p.pressed = 0;
  p.view_angles = {0, yaw, 0};
  p.body = sq_new_body(s.opt.pack);
  if (s.quake_arms) sq_quake_body(p.body);  // Quake's weapons (q2_quake_arms.hpp)
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
  pose_spawn(s, slot);
  campaign_body(s, slot, respawn);
  gz_entered(s, slot);  // the pack's last level hands out the A-M bomb
  if (respawn && coop(s))
    for (int i = 0; i < kSqItemCount; i++)
      if (kSqItems[i].kind == SqKind::key) p.body.counts[i] = died.counts[i];
  fight_midair_kit(s, slot);
  warmup_kit(s, slot);
  q2_mods_spawn(s, slot);
  commands_spawn(s, slot);
  if (p.alive) targets_telefrag(s, slot);  // whatever stands where he appears dies
}

int skill(const Q2Server& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}

// Whether item row `item` is left out by `dmflags`: health (1), powerups (2), armour (2048), and
// with infinite ammo (8192) the ammo and the BFG.
// The bits of `g_removed_weapons` and `g_removed_powerups`, in the items' order.
constexpr std::string_view kRemovableWeapons[] = {"weapon_shotgun",        "weapon_supershotgun",
                                                  "weapon_machinegun",     "weapon_chaingun",
                                                  "ammo_grenades",         "weapon_grenadelauncher",
                                                  "weapon_rocketlauncher", "weapon_hyperblaster",
                                                  "weapon_railgun",        "weapon_bfg"};
constexpr std::string_view kRemovablePowerups[] = {
    "item_quad",     "item_invulnerability", "item_power_screen", "item_power_shield",
    "item_silencer", "item_breather",        "item_enviro"};

int removed_bits(const Q2Server& s, const char* setting) {
  const ge::console::Cvar* c = s.cvars.find(setting);
  return c != nullptr ? c->integer() : 0;
}

bool weapon_removed(int bits, std::string_view name) {
  for (size_t i = 0; i < std::size(kRemovableWeapons); i++)
    if (kRemovableWeapons[i] == name) return ((bits >> i) & 1) != 0;
  return false;
}

// A map's item a match server takes off it (`g_removed_weapons`, `g_removed_powerups`): a weapon
// or powerup named, and an ammunition box once nothing left on the map fires it (the hand
// grenades' box is their pickup: it goes with both grenade bits).
bool match_removed(const Q2Server& s, const SqItem& it) {
  const int w = removed_bits(s, "g_removed_weapons"), p = removed_bits(s, "g_removed_powerups");
  const std::string_view name = it.classname;
  if (w == 0 && p == 0) return false;
  for (size_t i = 0; i < std::size(kRemovablePowerups); i++)
    if (kRemovablePowerups[i] == name) return ((p >> i) & 1) != 0;
  if (it.kind == SqKind::weapon) return weapon_removed(w, name);
  if (it.kind != SqKind::ammo) return false;
  const auto both = [w](std::string_view a, std::string_view b) {
    return weapon_removed(w, a) && weapon_removed(w, b);
  };
  switch (it.ammo) {
    case SqAmmo::shells:
      return both("weapon_shotgun", "weapon_supershotgun");
    case SqAmmo::bullets:
      return both("weapon_machinegun", "weapon_chaingun");
    case SqAmmo::grenades:
      return both("ammo_grenades", "weapon_grenadelauncher");
    case SqAmmo::rockets:
      return weapon_removed(w, "weapon_rocketlauncher");
    case SqAmmo::slugs:
      return weapon_removed(w, "weapon_railgun");
    case SqAmmo::cells:
      return both("weapon_hyperblaster", "weapon_bfg") && ((p >> 2) & 3) == 3;
    default:
      return false;
  }
}

bool dm_left_out(const Q2Server& s, int item) {
  if (s.opt.pack == Q2Pack::ground_zero && gz_left_out(s, item)) return true;  // in every mode
  if (item >= 0 && gz_tag_left_out(s, item)) return true;  // the tag token, outside tag
  if (item < 0 || !deathmatch(s)) return false;
  if (fight_midair(s)) return true;  // nothing lies on the map in midair
  const SqItem& it = kSqItems[item];
  const std::string_view name = it.classname;
  return match_removed(s, it) || (fight_dmflag(s, 1) && it.kind == SqKind::health) ||
         (fight_dmflag(s, 2) && it.kind == SqKind::powerup) ||
         (fight_dmflag(s, 2048) &&
          (it.kind == SqKind::armour || it.kind == SqKind::power_armour)) ||
         (fight_dmflag(s, 8192) && (it.kind == SqKind::ammo || name == "weapon_bfg"));
}

// Whether the map's entity is left out in this mode by its spawn flags.
bool left_out(const Q2Server& s, int flags) {
  if (deathmatch(s)) return (flags & 0x800) != 0;
  if (const int gz = gz_coop_left_out(s, flags); gz >= 0) return gz != 0;  // Ground Zero's co-op
  const int k = skill(s);
  return (k == 0 && (flags & 0x100) != 0) || (k == 1 && (flags & 0x200) != 0) ||
         (k >= 2 && (flags & 0x400) != 0);
}

constexpr float kItemHalf = 15;            // an item's box, ±15 each way
constexpr int kItemTargetsUsed = 0x10000;  // spawnflags: its first touch has fired its targets

// The first entity of the map (in its order) with this `team`, of any kind.
size_t team_master(const Q2Server& s, const std::string& team) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].team == team) return i;
  return s.entities.size();
}

// An item shows again: alone, or for its team one member chosen at random (itself or another, a
// member removed at the start counted and showing nothing), with the respawn event.
void show_item(Q2Server& s, size_t i) {
  s.entities[i].respawn_at = 0;
  const std::string team = s.entities[i].team;
  size_t shown = i;
  if (!team.empty()) {
    std::vector<size_t> members;
    for (size_t k = 0; k < s.entities.size(); k++)
      if (s.entities[k].item >= 0 && s.entities[k].team == team) members.push_back(k);
    if (!members.empty()) shown = members[ge::core::pick_index(s.rng, members.size())];
  }
  Q2Entity& e = s.entities[shown];
  if (e.gone) return;
  e.hidden = false;
  e.back_frame = s.frame_number;
  if (gz_random_respawn(s, e) && dm_left_out(s, e.item)) e.gone = true;  // swapped for a banned one
}

// Each item dropped up to 128 units onto the floor, the map's or a solid mover's (a key resting on
// a train or a wall's ledge), 0.2 s into the level, once the trains stand at their first corners;
// one that starts in a solid is removed.
// Item `i` dropped up to `reach` units down onto the floor; removed, and said, if it starts in a
// solid. Whether it found a floor.
bool drop_item(Q2Server& s, size_t i, float reach) {
  const sh::Vec3f mins{-kItemHalf, -kItemHalf, -kItemHalf}, maxs{kItemHalf, kItemHalf, kItemHalf};
  Q2Entity& e = s.entities[i];
  const ge::classic::Q2Trace t =
      map_trace(s, e.origin, mins, maxs, e.origin - sh::Vec3f{0, 0, reach},
                sh::kSqSolid | sh::kSqWindow, static_cast<int>(i));
  if (t.start_solid) {
    s.console.push_back("droptofloor: " + e.classname + " startsolid at " + q2_pos(e.origin));
    e.gone = true;
    return false;
  }
  e.origin = t.end;
  // Its row too (built as the map started, before the drop): drawn where it lies, as a loaded
  // game, whose rows are built after the drop, draws it.
  if (const size_t row = 1 + sh::kMaxClients + i; row < s.world.entities.size())
    s.world.entities[row].origin = e.origin;
  return t.fraction < 1;
}

// Ground Zero's item spawned by a trigger (TRIGGER_SPAWN, 1; never the power cube's): left where
// it was placed, unseen, until used.
bool gz_trigger_spawned(const Q2Server& s, const Q2Entity& e) {
  return s.opt.pack == Q2Pack::ground_zero && e.item >= 0 && (e.spawnflags & 1) != 0 &&
         e.classname != "key_power_cube";
}

void drop_items(Q2Server& s) {
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    if (e.item >= 0 && !e.gone && !gz_trigger_spawned(s, e)) drop_item(s, i, 128);
  }
  // Item teams (items sharing a `team`): every member hidden as they land, and 0.1 s later the
  // team's first entity (when it is an item) shows one of them at random.
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    if (e.item < 0 || e.team.empty()) continue;
    e.hidden = true;
    e.respawn_at = 0;
    if (team_master(s, e.team) == i && !e.gone) e.respawn_at = s.level_time + 0.1;
  }
}

}  // namespace

void q2_item_used(Q2Server& s, size_t i) {
  Q2Entity& e = s.entities[i];
  if (!e.hidden || (e.spawnflags & 1) == 0) return;
  const bool dropped = gz_trigger_spawned(s, e);
  e.hidden = false;
  e.spawnflags &= ~1;
  // Ground Zero's drops now, once used: 128 units down, and on to the floor below if none is
  // that near (it falls; here it lands at once).
  if (dropped && !drop_item(s, i, 128) && !e.gone) drop_item(s, i, 8192);
}

namespace {

// The items a living player's box touches, taken by their rule.
void touch_items(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    if (e.item < 0 || e.hidden || e.gone || (e.spawnflags & 2) != 0) continue;  // NO_TOUCH
    if (lo.x > e.origin.x + kItemHalf || hi.x < e.origin.x - kItemHalf ||
        lo.y > e.origin.y + kItemHalf || hi.y < e.origin.y - kItemHalf ||
        lo.z > e.origin.z + kItemHalf || hi.z < e.origin.z - kItemHalf)
      continue;
    // The first touch by a player fires the item's targets and message, taken or not, once.
    if ((e.spawnflags & kItemTargetsUsed) == 0) {
      e.spawnflags |= kItemTargetsUsed;
      if (!e.target.empty() || !e.message.empty()) {
        map_use_targets(s, static_cast<int>(i), slot);
        if (s.entities[i].gone || s.entities[i].hidden) continue;
      }
    }
    Q2Entity& item = s.entities[i];
    if (gz_tag_take(s, slot, item.item)) {  // Ground Zero's tag token: it never comes back
      item.gone = true;
      continue;
    }
    const SqItem& it = kSqItems[item.item];
    // A weapon that stays (`dmflags` 4; always in coop) is refused to one who holds it, and stays
    // when taken; in coop with `coop_pickup_weapons` a holder may take a copy nobody has yet.
    const bool coop = !deathmatch(s) && s.cvars.find("coop")->on();
    const std::string_view cn = it.classname;
    const bool stays =
        (it.kind == SqKind::weapon && (fight_anyflag(s, 4) || coop)) ||
        (coop && (it.kind == SqKind::key || cn == "item_breather" || cn == "item_enviro"));
    const int cube = (item.spawnflags >> 8) & 0xff;  // a coop power cube's bit, once a player
    if (coop && it.kind == SqKind::key && item.classname == "key_power_cube") {
      if ((p.body.cubes & cube) != 0) continue;
    } else if (stays && p.body.counts[item.item] > 0 &&
               !(coop && it.kind == SqKind::weapon && !item.coop_taken &&
                 s.cvars.find("coop_pickup_weapons")->on())) {
      continue;
    }
    p.body.health = static_cast<int>(p.health);
    int count = 0;  // ammo the map gives a `count` of
    if (it.kind == SqKind::ammo && item.block >= 0 && size_t(item.block) < s.map->entities.size())
      count = std::atoi(s.map->entities[size_t(item.block)].get_any_case("count").c_str());
    const int grenades = p.body.ammo[static_cast<int>(it.ammo)];  // its kind's before
    if (s.opt.pack == Q2Pack::ground_zero && gz_sphere_refused(s, slot, item.item)) continue;
    if (!sq_pick_up(item.item, p.body, deathmatch(s), skill(s), s.level_time, count)) continue;
    p.health = static_cast<float>(p.body.health);
    if (it.kind == SqKind::weapon && fight_anyflag(s, 8192)) {  // infinite ammo: a full stock
      const int k = static_cast<int>(it.ammo);
      p.body.ammo[k] = std::max(p.body.ammo[k], std::min(1000, p.body.max_ammo[k]));
    }
    if (coop && item.classname == "key_power_cube") p.body.cubes |= cube;
    p.taken.push_back(item.item);
    fight_took(s, slot, item.item, grenades);
    if (stays && coop) item.coop_taken = true;
    if (stays) continue;
    if (std::string_view(it.classname) == "item_health_mega") {
      item.hidden = true;  // shows again 20 s after its taker is back to his maximum
      item.respawn_at = 0;
      item.picked = true;
      p.mega = static_cast<int>(i);
      p.drained_at = s.level_time + 4;  // the first point goes 5 s after
    } else if (deathmatch(s) && it.respawn_s > 0) {
      item.hidden = true;
      item.respawn_at = s.level_time + it.respawn_s;
      item.picked = true;
    } else {
      item.gone = true;
    }
  }
  fight_touch_drops(s, slot);
}

// The frame's items: those whose time came show again; a mega counts its taker down.
void item_frame(Q2Server& s) {
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    if (e.hidden && e.respawn_at > 0 && s.level_time + 0.001 >= e.respawn_at) show_item(s, i);
  }
  for (Q2Player& p : s.players) {
    if (p.mega < 0) continue;
    Q2Entity& mega = s.entities[size_t(p.mega)];
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

void run_command(Q2Server& s, int slot, const sh::MoveCmd& c) {
  q2_mods_pre_think(s, slot);
  Q2Player& p = s.players[slot];
  if (s.clients[slot].spectator && s.intermission_at <= 0 && spectate_command(s, slot, c)) return;
  ge::classic::Q2Move pm;
  pm.s = p.move;
  if (s.intermission_at > 0 || (!p.alive && p.hunter_cam != -1))  // or riding a hunter sphere
    pm.s.type = ge::classic::Q2Type::freeze;
  else if (p.noclip && p.alive)  // `noclip` (q2_commands.hpp): the spectator's flight, no walls
    pm.s.type = ge::classic::Q2Type::spectator;
  else if (pm.s.type != ge::classic::Q2Type::spectator || !s.clients[slot].spectator)
    pm.s.type = p.alive    ? ge::classic::Q2Type::normal
                : p.gibbed ? ge::classic::Q2Type::gib  // his head: the gib's box, no input
                           : ge::classic::Q2Type::dead;
  pm.s.gravity = static_cast<int16_t>(float(s.cvars.find("sv_gravity")->integer()) * p.gravity);
  pm.cmd = ge::classic::q2_command(c);  // the jump button the up move (q2_move.hpp)
  if (s.opt.clients_take_view && p.forced && pm.cmd.angles[0] == p.forced_angles[0] &&
      pm.cmd.angles[1] == p.forced_angles[1] && pm.cmd.angles[2] == p.forced_angles[2]) {
    for (int16_t& d : pm.s.delta_angles) d = 0;  // the client took the turn as its own
    p.forced = false;
  }
  pm.changed = p.changed;
  ge::classic::q2_feel(pm, q2_move_feel(s, slot));
  if (s.clients[slot].protocol != 0)  // a stock client's flight, as it predicts it
    ge::classic::q2_fly_for_protocol(pm, s.clients[slot].protocol);
  MapWorld mw{&s, p.alive ? sh::kSqMaskPlayerSolid : sh::kSqMaskDeadSolid};
  const bool was_on_ground = p.on_ground;
  ge::classic::q2_move(pm, mw.world());
  p.move = pm.s;
  p.changed = false;
  for (int i = 0; i < 3; i++) p.cmd_angles[i] = pm.cmd.angles[i];
  if (!p.alive) p.pressed = static_cast<uint8_t>(p.pressed | (c.buttons & ~p.buttons));
  p.buttons = c.buttons;
  p.view_angles = pm.view_angles;
  p.mins = pm.mins;
  p.maxs = pm.maxs;
  p.view_height = pm.view_height;
  p.water_level = pm.water_level;
  p.water_type = pm.water_type;
  p.on_ground = pm.ground_entity >= 0;
  p.ground = pm.ground_entity;
  if (p.noclip && p.alive) {  // flying: no water, no falls, no triggers, no pickups
    p.water_level = 0;
    p.water_type = 0;
    p.old_fall = 0;
  }
  if (!p.alive || s.clients[slot].spectator || s.intermission_at > 0) return;
  gz_gravity_touch(s, slot);  // Ground Zero's gravity triggers, for his next move
  if (p.noclip) {
    if (c.impulse >= 1 && c.impulse <= kSqWeaponCount)
      fight_choose(s, slot, impulse_weapon(s, c.impulse));
    if ((c.buttons & sh::kButtonAttack) != 0 && !p.stepped) {
      p.stepped = true;
      fight_step(s, slot, true);
    }
    return;
  }
  fight_moved(s, slot, was_on_ground, pm.cmd.up);
  touch_items(s, slot);
  map_touch(s, slot);
  if (c.impulse >= 1 && c.impulse <= kSqWeaponCount)
    fight_choose(s, slot, impulse_weapon(s, c.impulse));
  // The first command of the frame that presses attack steps the hand at once.
  if ((c.buttons & sh::kButtonAttack) != 0 && !p.stepped) {
    p.stepped = true;
    fight_step(s, slot, true);
  }
}

// The commands a packet brings: those a lost packet left unrun (up to the two it carries), then
// its newest. A packet older than the last one run brings nothing.
void take_packet(Q2Server& s, const sh::Packet& pk) {
  if (pk.client < 0 || pk.client >= sh::kMaxClients || !s.clients[pk.client].active) return;
  Q2Player& p = s.players[pk.client];
  if (p.last_sequence != 0 && pk.sequence <= p.last_sequence) return;
  const uint32_t gap = p.last_sequence == 0 ? 0 : pk.sequence - p.last_sequence - 1;
  const int lost = gap > 2 ? 2 : static_cast<int>(gap);
  const bool check = s.cvars.find("sv_speedcheck")->on();
  for (int i = 2 - lost; i <= 2; i++) {
    p.time_budget_ms -= pk.cmds[i].msec;
    if (check && p.time_budget_ms < 0) {
      s.moves_held++;
      continue;
    }
    run_command(s, pk.client, pk.cmds[i]);
    spectate_follow(s, pk.client);  // the chase cameras on him, behind his new view
  }
  p.last_sequence = pk.sequence;
  q2_mods_post_think(s, pk.client);
}

// The intermission begins: the dead stand up, and everyone is moved to its spot (the first
// `info_player_intermission`, else the first start) and frozen there facing its angles, with no
// weapon and no model, until a button is pressed 5 s on.
void start_intermission(Q2Server& s) {
  s.intermission_at = s.level_time;
  screens_intermission(s);
  for (const std::string& l : fight_midair_report(s)) fight_print(s, -1, l);
  // One of the first four intermission spots: 0..3 steps on from the first, wrapping round
  // (so with three, the first twice as often); none, the first start.
  const Q2Entity* spot = nullptr;
  std::vector<const Q2Entity*> spots;
  for (const Q2Entity& e : s.entities)
    if (e.classname == "info_player_intermission") spots.push_back(&e);
  if (!spots.empty()) {
    const auto steps = static_cast<size_t>(ge::core::int_in(s.rng, 0, 3));
    spot = spots[steps % spots.size()];
  }
  for (const Q2Entity& e : s.entities)
    if (spot == nullptr && e.classname == "info_player_start") spot = &e;
  sh::Vec3f at = spot != nullptr ? spot->origin : sh::Vec3f{};
  if (q2_same_name(s.map_name, "fact1")) at = {1037, 1100, 222};  // a portal there garbled it
  const sh::Vec3f look = spot != nullptr ? spot->angles : sh::Vec3f{};
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    Q2Player& p = s.players[i];
    if (!p.alive && !s.clients[i].spectator) spawn_player(s, i, false);
    const float pos[3] = {at.x, at.y, at.z};
    for (int k = 0; k < 3; k++) {
      p.move.origin[k] = static_cast<int16_t>(num::to_int32(pos[k] * 8));
      p.move.velocity[k] = 0;
    }
    p.move.type = ge::classic::Q2Type::freeze;
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

// The intermission's frame: past 5 s, any button pressed leaves for the next map, however long
// it takes; a deathmatch with no person in it (bots alone) leaves by itself after 20 s.
void intermission_frame(Q2Server& s) {
  if (s.game_over || !s.pending_map.empty() ||
      s.level_time <= s.intermission_at + kIntermissionHold + 0.001)
    return;
  bool people = false;
  for (const ClientInfo& c : s.clients) people = people || (c.active && !c.bot);
  const bool over =
      deathmatch(s) && !people && s.level_time + 0.001 >= s.intermission_at + kIntermissionMost;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (over || (s.clients[i].active && s.players[i].buttons != 0)) {
      s.pending_map = s.next_map;
      s.cinematic = s.next_cinematic;
      s.game_over = s.next_map.empty();
      s.pending_change = MapChange::change_level;
      return;
    }
}

// A living player's figure as others see it: a third of the view's pitch, the view's yaw, and a
// lean into a strafe of four times `sv_rollangle` at `sv_rollspeed` sideways (less below it).
sh::Vec3f figure_angles(const Q2Server& s, const Q2Player& p) {
  float pitch = p.view_angles.x;
  if (pitch > 180) pitch -= 360;
  const float most = static_cast<float>(s.cvars.find("sv_rollangle")->number);
  const float at = static_cast<float>(s.cvars.find("sv_rollspeed")->number);
  const float side = sh::dot(unpack(p.move.velocity), sh::angle_vectors(p.view_angles).right);
  const float size = side < 0 ? -side : side;
  const float lean = size < at ? size * most / at : most;
  return {pitch / 3, p.view_angles.y, (side < 0 ? -lean : lean) * 4};
}

// The player's view this frame (plugins/classic/q2_view.hpp): the bob, the tilts and the kicks
// into `Q2Player::seen`; dead, the view lying on its side, looking up at the killer.
void view_frame(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  ge::classic::Q2ViewSettings set;
  set.run_pitch = static_cast<float>(s.cvars.find("run_pitch")->number);
  set.run_roll = static_cast<float>(s.cvars.find("run_roll")->number);
  set.bob_up = static_cast<float>(s.cvars.find("bob_up")->number);
  set.bob_pitch = static_cast<float>(s.cvars.find("bob_pitch")->number);
  set.bob_roll = static_cast<float>(s.cvars.find("bob_roll")->number);
  set.gun_x = static_cast<float>(s.cvars.find("gun_x")->number);
  set.gun_y = static_cast<float>(s.cvars.find("gun_y")->number);
  set.gun_z = static_cast<float>(s.cvars.find("gun_z")->number);
  ge::classic::Q2ViewIn in;
  in.velocity = unpack(p.move.velocity);
  in.view_angles = p.view_angles;
  in.view_height = p.view_height;
  in.health = p.health;
  in.on_ground = p.on_ground;
  in.ducked = (p.move.flags & ge::classic::kSqDucked) != 0;
  in.dead = !p.alive;
  const float scale = in.ducked ? 4.0f : 1.0f;  // the stride's clock, as the view reads it
  const int stride = static_cast<int>(p.view.bob_time * scale);
  p.seen = ge::classic::step_q2_view(p.view, set, in);
  if (static_cast<int>(p.view.bob_time * scale) != stride) fight_stride(s, slot);
  if (p.alive) return;
  float yaw = p.view_angles.y;
  if (p.killer >= 0 && p.killer != slot && p.killer < sh::kMaxClients) {
    const sh::Vec3f to = unpack(s.players[p.killer].move.origin) - unpack(p.move.origin);
    if (to.x != 0 || to.y != 0)
      yaw = static_cast<float>(ge::core::libm::atan2(to.y, to.x) * 180 / 3.14159265358979323846);
  }
  p.dead_angles = {ge::classic::kQ2DeadPitch, yaw, ge::classic::kQ2DeadRoll};
}

// The dead who may respawn, on the first frame past 1 s after dying: by a fresh press of attack
// since he died (deathmatch; holding it through the death does not count) or of any button
// (co-op), or at once with `dmflags` 1024 (forced respawn, deathmatch only); by himself after 2 s
// in midair.
void respawn_frame(Q2Server& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& p = s.players[i];
    if (!s.clients[i].active || s.clients[i].spectator || p.alive) continue;
    if (s.level_time <= p.died_at + kRespawnAfter + 0.001) continue;
    const bool by_himself =
        fight_midair(s) && s.level_time >= p.died_at + sh::kMidairRespawn - 0.001;
    // Single player asks for the load menu instead (q2_save.hpp).
    const bool attack = (p.pressed & sh::kButtonAttack) != 0;
    if ((deathmatch(s) && (attack || fight_dmflag(s, 1024) || by_himself)) ||
        (coop(s) && p.pressed != 0)) {
      // A respawn takes his wish as it stands: to watching, or back, with no check and no word.
      if (deathmatch(s)) s.clients[i].spectator = p.want_spectator;
      spawn_player(s, i, true);
    }
  }
}

// The level time the next rules frame starts at: the next tenth, or a step into this one.
double next_frame_time(const Q2Server& s) {
  if (s.sub + 1 >= s.fps / 10) return double(s.frame_number + 1) * kFrameSeconds;
  return double(s.frame_number) * kFrameSeconds + double(s.sub + 1) * double(s.step);
}

// A frame between tenths (`sv_fps` past 10): the movers, the shots, the drops and the pieces step,
// the dead may respawn and a limit may end the level; the 10 Hz world waits for its tenth.
void fast_frame(Q2Server& s) {
  s.fast_number++;
  s.sub++;
  s.tenth = false;
  s.level_time = double(s.frame_number) * kFrameSeconds + double(s.sub) * double(s.step);
  if (s.intermission_at > 0) return;
  map_frame(s);
  fight_missiles(s);
  fight_drops(s);
  gibs_frame(s);
  if (fight_limits(s) || s.exit_now) {
    s.exit_now = false;
    start_intermission(s);
    return;
  }
  respawn_frame(s);
}

// One rules frame: the time advances, the dead who press attack a second after dying respawn.
void rules_frame(Q2Server& s) {
  if (s.sub + 1 < s.fps / 10) {
    fast_frame(s);
    return;
  }
  s.sub = 0;
  s.tenth = true;
  s.fast_number++;
  if (s.frame_number % kTimeBudgetFrames == 0)
    for (Q2Player& p : s.players) p.time_budget_ms = kTimeBudgetMs;
  s.frame_number++;
  s.level_time = double(s.frame_number) * kFrameSeconds;
  for (Q2Player& p : s.players) p.pushed = {};
  q2_mods_start_frame(s);
  bool placed[sh::kMaxClients] = {};
  spectate_frame(s, placed);  // before anything else moves
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!placed[i]) continue;
    spawn_player(s, i, false);  // no body left behind, at a spawn spot as a respawn picks
    s.players[i].respawn_time = s.level_time;
    if (s.clients[i].spectator) continue;
    arrival_flash(s, i, "sq_login");  // back to play: the login flash and a short hold
    s.players[i].move.flags |= ge::classic::kSqTimeTeleport;
    s.players[i].move.time = kRespawnHold;
  }
  item_frame(s);
  screens_frame(s);
  if (s.intermission_at > 0) {
    intermission_frame(s);
    return;
  }
  map_frame(s);
  if (s.frame_number == 2) {
    drop_items(s);
    gz_tag_start(s);  // Ground Zero's tag: a token where the map placed none
  }
  monster_frame(s);
  campaign_frame(s);
  fight_missiles(s);
  fight_drops(s);
  gz_tag_frame(s);
  gibs_frame(s);
  for (int i = 0; i < sh::kMaxClients; i++) {  // the hands no command stepped this frame
    Q2Player& p = s.players[i];
    if (s.clients[i].active && !p.stepped) fight_step(s, i, (p.buttons & sh::kButtonAttack) != 0);
    p.stepped = false;
    if (s.clients[i].active) fight_body_frame(s, i);
  }
  for (int i = 0; i < sh::kMaxClients; i++)  // the bodies' frames, once the frame is done
    if (s.clients[i].active && !s.clients[i].spectator) pose_player(s, i);
  commands_frame(s);  // a completed switch leaves the last weapon for `weaplast`
  for (int i = 0; i < sh::kMaxClients; i++)  // and what each sees of it, and its tint
    if (s.clients[i].active) {
      view_frame(s, i);
      blend_frame(s, i);
    }
  if (fight_limits(s) || s.exit_now) {
    s.exit_now = false;
    start_intermission(s);
    return;
  }
  respawn_frame(s);
  q2_save_frame(s);  // single player's dead: a button asks for the load menu
}

void mirror_pool(Q2Server& s);

void fill_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  w.time = s.level_time;
  w.paused = s.paused;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& c = s.clients[i];
    const Q2Player& p = s.players[i];
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
    if (wc.layout != p.layout) wc.layout = c.active ? p.layout : std::string();
    if (wc.inventory != p.inventory) wc.inventory = c.active ? p.inventory : std::vector<int16_t>();
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
    // The bots' numbers: the weapon in hand and those carried as bits (1 << the weapon's index),
    // and every ammunition from shells on.
    wc.numbers.held = int32_t(1) << p.hand.weapon;
    for (int k = 0; k < kSqWeaponCount; k++)
      if (sq_holds(p.body, k)) wc.numbers.carried |= int32_t(1) << k;
    for (int k = 1; k < kSqAmmoKinds; k++) wc.numbers.stock[k - 1] = float(p.body.ammo[k]);
    wc.numbers.frags = p.frags;
    wc.numbers.view_z = p.view_height;
    wc.numbers.view_set = true;
    const bool still = s.intermission_at > 0 || c.spectator;  // no bob, no kick
    const sh::Vec3f off =
        still ? sh::Vec3f{0, 0, s.intermission_at > 0 ? 0 : p.view_height} : p.seen.offset;
    const sh::Vec3f kick = still ? sh::Vec3f{} : p.seen.kick;
    const float offs[3] = {off.x, off.y, off.z}, kicks[3] = {kick.x, kick.y, kick.z};
    const sh::Vec3f go = p.seen.gun_offset, ga = p.seen.gun_angles;
    const float guns[6] = {go.x, go.y, go.z, ga.x, ga.y, ga.z};
    for (int k = 0; k < 3; k++) {
      wc.numbers.view_offset[k] = offs[k];
      wc.numbers.kick[k] = kicks[k];
      wc.numbers.gun_offset[k] = still ? 0 : guns[k];
      wc.numbers.gun_angles[k] = still ? 0 : guns[k + 3];
    }
    wc.numbers.fov = fight_fov(s, i);
    if (gz_sphere_view(s, i)) {  // riding a hunter sphere: its wide eye, 8 up
      wc.numbers.fov = 140;
      wc.numbers.view_z = 8;
      wc.numbers.view_offset[0] = wc.numbers.view_offset[1] = 0;
      wc.numbers.view_offset[2] = 8;
    }
    wc.numbers.rdflags = sq_power_shown(p.ir_until - s.level_time) ? 4 : 0;  // IR goggles
    for (int k = 0; k < 4; k++) {
      const bool none = s.intermission_at > 0;
      wc.numbers.blend[k] = none ? 0 : p.blend[k];
      wc.numbers.blend_apart[k] = none ? 0 : p.blend_apart[k];
      wc.numbers.damage_blend[k] = none ? 0 : p.damage_blend[k];
    }
    e.free = false;
    e.origin = unpack(p.move.origin);
    e.velocity = unpack(p.move.velocity);
    e.angles = p.alive ? figure_angles(s, p) : sh::Vec3f{0, p.view_angles.y, 0};
    e.view_angles = p.alive || c.spectator ? p.view_angles : p.dead_angles;  // dead: lying there
    e.mins = p.mins;
    e.maxs = p.maxs;
    e.absmin = e.origin + p.mins;
    e.absmax = e.origin + p.maxs;
    e.flags = p.on_ground ? sh::kFlagOnGround : 0;
    e.waterlevel = p.water_level;
    e.health = p.health;
    e.frame = p.pose.frame;
    const bool unseen = c.spectator || s.intermission_at > 0;
    e.modelindex = unseen ? 0 : static_cast<int>(s.models.size()) - 1;
    e.carried_model = unseen || !p.alive ? 0 : s.carried_models[p.hand.weapon];
    if (p.gibbed && !unseen) {  // thrown apart: his head lies where his body is
      gibs_player_head(s, i, e.modelindex, e.skin);
      e.frame = 0;
      e.origin.z += p.mins.z;
      e.angles = {0, p.dead_angles.y, 0};
    } else if (p.gibbed) {
      e.modelindex = 0;
    }
    if (!p.alive && p.hunter_cam != -1) e.modelindex = 0;          // his view rides a hunter sphere
    if (s.opt.pack == Q2Pack::ground_zero) e.render_fx |= 0x8000;  // seen through IR goggles
    if (p.disguised) e.render_fx |= 1 << 18;  // Ground Zero's disguise: its folder's skin
    if (p.alive && s.intermission_at <= 0) {  // the powers held, blinking as they run out
      for (const SqPower& pw : sq_powers())
        if ((pw.effects | pw.render) != 0 && sq_power_shown(p.*pw.until - s.level_time)) {
          e.effects |= pw.effects;
          e.render_fx |= pw.render;
        }
      if (p.god) {  // god mode: a white shell
        e.effects |= kSqColourShellEffect;
        e.render_fx |= kSqGodShellRender;
      }
      if (gz_tag_holder(s, i)) e.effects |= kGzTagTrailEffect;  // the token's holder
      if (s.level_time < p.power_flash) {  // power armour that took a blow: its plate or shell
        if (p.body.counts[sq_item("item_power_shield")] > 0) {
          e.effects |= kSqColourShellEffect;
          e.render_fx |= kSqShieldShellRender;
        } else {
          e.effects |= kSqPowerScreenEffect;
        }
      }
    }
  }
  // The frame's sounds by the names the players were told (`sv_wear`).
  if (!s.looks.empty())
    for (NetEvent& ev : s.out.events)
      if (ev.kind == EventKind::sound)
        if (const profiles::Look* l = profiles::look_of(s.looks, ev.text); l != nullptr)
          ev.text = std::string(l->to);
  for (size_t i = 0; i < s.entities.size(); i++) {  // the items shown or hidden
    const Q2Entity& m = s.entities[i];
    if (m.item < 0) continue;
    sh::WorldEntity& e = w.entities[1 + sh::kMaxClients + i];
    e.free = m.gone || m.hidden;
    // Where it lies: dropped to the floor 0.2 s in, as a loaded level has it (a worn look's own
    // offset kept, sq_dress).
    e.origin = m.origin;
    if (e.modelindex > 0 && size_t(e.modelindex) < s.model_looks.size())
      if (const profiles::Look* l = s.model_looks[size_t(e.modelindex)]; l != nullptr)
        e.origin = e.origin + sh::Vec3f{l->offset[0], l->offset[1], l->offset[2]};
    // Its glow, once: on the tenth it showed (a frame between tenths shows none).
    e.event = !e.free && s.tenth && m.back_frame == s.frame_number ? kSqItemBackEvent : 0;
    // Taken in deathmatch: how long until it shows again, and the model it will show.
    const bool waits = e.free && !m.gone && m.hidden && m.respawn_at > s.level_time;
    e.back_in = waits ? static_cast<float>(m.respawn_at - s.level_time) : -1;
    e.back_model = waits ? m.modelindex : 0;
  }
  map_rows(s);
  monster_rows(s);
  targets_rows(s);
  fight_rows(s);
  fight_power_frame(s);
  bar_rows(s);
  mirror_pool(s);
}

// The pool's trace (`sh::Server::world_trace`): the map and its solid movers as a shot meets them
// (solid and windows), the hit as the world's row (0 the world, a mover's row) or -1.
sh::HullTrace pool_trace(const void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                         sh::Vec3f end) {
  const auto& s = *static_cast<const Q2Server*>(ctx);
  const ge::classic::Q2Trace t = map_trace(s, start, mins, maxs, end, sh::kSqSolid | sh::kSqWindow);
  sh::HullTrace tr;
  tr.allsolid = t.all_solid;
  tr.startsolid = t.start_solid;
  tr.inopen = !t.all_solid;
  tr.fraction = t.fraction;
  tr.endpos = t.all_solid ? start : t.end;
  tr.plane.normal = t.normal;
  tr.plane.dist = t.normal.x * t.end.x + t.normal.y * t.end.y + t.normal.z * t.end.z;
  tr.plane.type = sh::plane_type(t.normal);
  tr.ent = t.entity;
  return tr;
}

// The world's rows mirrored into the loop's pool (`sv.edicts`), which the QuakeWorld endpoint, the
// listen's view and the hit tests still read by slot: the same entities, as the world shows them.
void mirror_pool(Q2Server& s) {
  const RulesWorld& w = s.world;
  if (s.sv.edicts.size() < w.entities.size()) return;  // sized at the map's start
  s.sv.time = s.host_time;
  if (s.tracer != nullptr) {
    const ge::brush::Model& m = s.map->models[0];
    s.world_solid = sh::brushes_solid(*s.tracer, 0, {}, {m.mins[0], m.mins[1], m.mins[2]},
                                      {m.maxs[0], m.maxs[1], m.maxs[2]});
    s.sv.brushes = &s.world_solid;
    s.sv.world_ctx = &s;
    s.sv.world_trace = &pool_trace;
  }
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
    e.solid = sh::kSolidNot;
  }
  // A live monster blocks as its box (the bots' senses take only a solid one as a foe; a body
  // lying dead does not block).
  for (const Q2Monster& m : s.monsters) {
    const size_t row = size_t(map_row(m.entity));
    const Q2Entity& me = s.entities[size_t(m.entity)];
    if (row < w.entities.size() && !me.gone && !me.hidden && me.health > 0)
      s.sv.edicts[row].solid = sh::kSolidSlidebox;
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

void fill_lists(Q2Server& s) {
  RulesWorld& w = s.world;
  w.map_name = s.map_name;
  w.map_title.clear();
  if (!s.map->entities.empty()) w.map_title = s.map->entities[0].get_any_case("message");
  // The music: the world's `music` (a track's name) when set, else its `sounds` (a number).
  if (!s.map->entities.empty()) {
    w.cd_track = s.map->entities[0].get_any_case("music");
    if (w.cd_track.empty()) w.cd_track = s.map->entities[0].get_any_case("sounds");
  } else {
    w.cd_track.clear();
  }
  // The other game's things the rules wear (`sv_wear`): every name as the players are told it,
  // in the lists the window and the wire both read; the names were looked up as the map started.
  s.model_looks.assign(s.models.size(), nullptr);
  for (size_t i = 0; i < s.models.size(); i++)
    if (const profiles::Look* l = profiles::look_of(s.looks, s.models[i]); l != nullptr) {
      s.model_looks[i] = l;
      s.models[i] = std::string(l->to);
    }
  for (std::string& name : s.sounds)
    if (const profiles::Look* l = profiles::look_of(s.looks, name); l != nullptr)
      name = std::string(l->to);
  w.models = s.models;
  w.sounds = s.sounds;
  w.vwep.clear();
  w.statics.clear();
  w.max_clients = sh::kMaxClients;
  w.entities.assign(
      1 + sh::kMaxClients + s.entities.size() + kQ2Missiles + kQ2Drops + kQ2Bodies + kQ2Gibs,
      WorldEntity{});
  w.entity_count = static_cast<int>(w.entities.size());
  bar_start(s);
  w.entities[0].free = false;
  w.entities[0].modelindex = 1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& m = s.entities[i];
    WorldEntity& e = w.entities[1 + sh::kMaxClients + i];
    // Triggers, fields and the map's points are never drawn.
    e.free =
        m.gone || m.hidden || ((m.trigger || m.classname.rfind("trigger_", 0) == 0) && m.pad == 0);
    e.origin = m.origin;
    e.angles = m.angles;
    e.modelindex = m.modelindex;
    // An item turns where it lies, unless the map placed it untouchable (NO_TOUCH, 2).
    e.turns = m.item >= 0 && sq_turns(m.item) && (m.spawnflags & 2) == 0;
    e.effects = e.turns ? kSqTurnEffect : 0;
    e.render_fx = m.item >= 0 && (m.spawnflags & 2) == 0 ? kSqGlowRender : 0;  // its light pulses
    if (gz_tag_token(m.item)) e.effects |= kGzTagTrailEffect;
    if (m.pad != 0) {  // a teleporter's disc: the pad's skin and specks, the exit plain
      e.skin = m.pad == 1 ? 1 : 0;
      e.effects = m.pad == 1 ? kSqTeleporterEffect : 0;
      e.mins = kDiscMins;
      e.maxs = kDiscMaxs;
      e.solid_box = true;
    }
    sq_dress(s, e);
  }
}

void announce_entry(Q2Server& s, int slot);

bool sq_start(void* r, const MapSpec& map, std::string& error) {
  Q2Server& s = sq(r);
  if (map.brushes == nullptr) {
    error = "Quake II's rules cannot start map '" + map.name +
            "': it has no brushes (MapSpec::brushes). Read the version 38 map with "
            "brush::read_bsp and pass it whole.";
    return false;
  }
  s.restored = 0;
  s.cvars.apply_latched();  // the settings that wait for a new map (cheats, gamename, ...)
  // A load asked (q2_save.hpp) sets the save's game first.
  if (!s.load_file.empty() && !q2_load_begin(s, error)) return false;
  q2_leave(s);  // the level left kept for a return within its unit, or the unit's forgotten
  s.map = map.brushes;
  s.tracer = std::make_unique<ge::brush::BrushTracer>(*s.map);
  s.light = std::make_unique<ge::brush::LightProbe>(ge::brush::make_light_probe(*s.map));
  s.map_name = map.name;
  s.looks = {};
  if (const profiles::RemixLooks* worn =
          profiles::remix_looks("q2", s.cvars.find("sv_wear")->value);
      worn != nullptr)
    s.looks = worn->looks;
  s.q1_physics = s.cvars.find("sv_physics")->value == "q1";
  s.quake_arms = quake_arms_worn(s);
  s.entities.clear();
  int cubes = 0;                // the power cubes so far (each its own bit in coop)
  std::vector<char> inhibited;  // by the map's order: left out by skill or mode
  for (size_t i = 1; i < s.map->entities.size(); i++) {
    const ge::brush::Entity& b = s.map->entities[i];
    Q2Entity e;
    e.classname = b.get_any_case("classname");
    e.targetname = b.get_any_case("targetname");
    e.target = b.get_any_case("target");
    e.model = b.get_any_case("model");
    e.origin = parse_vec(b.get_any_case("origin"));
    if (!b.get_any_case("angles").empty())
      e.angles = parse_vec(b.get_any_case("angles"));
    else if (!b.get_any_case("angle").empty())
      e.angles.y = std::strtof(b.get_any_case("angle").c_str(), nullptr);
    if (e.model.size() > 1 && e.model[0] == '*') e.modelindex = std::atoi(e.model.c_str() + 1);
    e.spawnflags = std::atoi(b.get_any_case("spawnflags").c_str());
    // The map `command`'s trigger *27 is there on hard too (the map left it out by mistake).
    if (q2_same_name(s.map_name, "command") && q2_same_name(e.classname, "trigger_once") &&
        q2_same_name(e.model, "*27"))
      e.spawnflags &= ~0x400;
    // fact2's spawner at (-1504, 512, 72) stood in the floor: 8 units down.
    if (q2_same_name(s.map_name, "fact2") && e.classname == "target_spawner" &&
        e.origin.x == -1504 && e.origin.y == 512 && e.origin.z == 72)
      e.origin.z = 64;
    if (s.opt.pack == Q2Pack::ground_zero) e.classname = std::string(gz_classname(e.classname));
    e.item = sq_item(e.classname, s.opt.pack);
    if (e.item >= 0) e.team = b.get_any_case("team");
    inhibited.push_back(left_out(s, e.spawnflags) ? 1 : 0);
    e.gone = inhibited.back() != 0 || dm_left_out(s, e.item);
    // An item keeps its own flags only as a power cube: TRIGGER_SPAWN (1) hides it until used,
    // NO_TOUCH (2) leaves it lying there, never taken. Any other item's are cleared, and said.
    // Ground Zero's items keep TRIGGER_SPAWN alone (gz_trigger_spawned).
    const bool gz_spawned = s.opt.pack == Q2Pack::ground_zero && (e.spawnflags & 0xff) == 1;
    if (e.item >= 0 && (e.spawnflags & 0xff) != 0 && e.classname != "key_power_cube" &&
        !gz_spawned) {
      if (!e.gone)
        s.console.push_back(e.classname + " at " + q2_pos(e.origin) +
                            " has invalid spawnflags set");
      e.spawnflags &= ~0xff;
    }
    if (e.item >= 0 && (e.spawnflags & 1) != 0) e.hidden = true;
    if (e.classname == "key_power_cube" && s.cvars.find("coop")->on())  // its own bit in coop
      e.spawnflags |= 1 << (8 + cubes++);
    s.entities.push_back(std::move(e));
  }
  s.models.assign(1, "");
  for (size_t i = 1; i < s.map->models.size(); i++) s.models.push_back("*" + std::to_string(i));
  for (Q2Entity& e : s.entities) {  // each item's model, once
    if (e.item < 0) continue;
    const std::string model = kSqItems[e.item].model;
    const auto found = std::find(s.models.begin(), s.models.end(), model);
    e.modelindex = static_cast<int>(found - s.models.begin());
    if (found == s.models.end()) s.models.push_back(model);
  }
  s.item_models.assign(size_t(kSqItemCount), 0);  // every item's, for those a player drops
  for (int i = 0; i < kSqItemCount; i++) {
    if (!q2_pack_has(s.opt.pack, kSqItems[i].pack)) continue;  // a pack's under it alone
    const std::string model = kSqItems[i].model;
    const auto found = std::find(s.models.begin(), s.models.end(), model);
    s.item_models[size_t(i)] = static_cast<int>(found - s.models.begin());
    if (found == s.models.end()) s.models.push_back(model);
  }
  s.sounds.assign(1, "");
  fight_precache(s);
  monster_precache(s);
  gz_decoys_reserve(s);  // Ground Zero's deathmatch: a Doppleganger's row for each slot
  campaign_start(s);
  fight_clear(s);
  s.models.emplace_back(kSqPowerScreenModel);  // the plate a power screen shows when struck
  map_precache(s);
  s.models.push_back(kQ2PlayerModel);
  s.frame_number = 0;
  s.level_time = 0;
  s.fps = level_fps(s);
  s.sub = 0;
  s.fast_number = 0;
  s.step = s.fps == 10 ? 0.1f : static_cast<float>(1.0 / double(s.fps));
  s.tenth = true;
  s.exit_now = false;
  // Single player's gravity is the map's (worldspawn's `gravity`; 800 without it).
  if (!s.cvars.find("deathmatch")->on() && !s.map->entities.empty()) {
    const std::string& g = s.map->entities[0].get_any_case("gravity");
    s.cvars.set("sv_gravity", g.empty() ? "800" : g, con::Source::local);
  }
  q2_spawn_lines(s, inhibited);
  map_spawn(s);
  monster_spawn(s);
  actor_spawn(s);  // an actor's own health (q2_actor.hpp)
  q2_count_lines(s, static_cast<int>(std::count(inhibited.begin(), inhibited.end(), 1)));
  q2_start_lines(s);
  s.host_time = 0;
  s.map_stopped = false;
  s.pending_map.clear();
  s.intermission_at = 0;
  s.next_map.clear();
  fill_lists(s);
  // The pool the world is mirrored into: every row the world has, and room past them.
  s.sv.set.max_edicts = std::max(s.sv.set.max_edicts, int(s.world.entities.size()) + 8);
  s.sv.edicts.assign(size_t(s.sv.set.max_edicts), sh::Edict{});
  // A level of this unit entered again, or a saved game's, read over the fresh map; the autosave.
  if (!q2_enter(s, error)) return false;
  for (int i = 0; i < sh::kMaxClients; i++) {
    s.players[i].last_sequence = 0;  // a client numbers its packets afresh on every map
    if (s.clients[i].active && deathmatch(s)) s.players[i].frags = 0;  // each map scores afresh
    if (s.clients[i].active) spawn_player(s, i, false);
  }
  if (s.loading && !q2_load_end(s, error)) return false;  // the save's players in their bodies
  fill_rows(s);
  s.entered_told = 0;
  for (int i = 0; i < sh::kMaxClients; i++)  // every player entered again on the new map
    if (s.clients[i].active) {
      announce_entry(s, i);
      s.entered_told |= 1u << i;
    }
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

// A player's entry or exit flash ("sq_login", "sq_logout") where the world last showed him: the
// window throws its burst and light, the wire sends it as the protocol's flash (q2_wire.cpp).
void arrival_flash(Q2Server& s, int slot, const char* name) {
  const size_t row = size_t(slot) + 1;
  if (row >= s.world.entities.size() || s.world.entities[row].free) return;
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = 0xffffffffu;  // everyone
  ev.ent = slot + 1;
  ev.origin = s.world.entities[row].origin;
  ev.values[3] = 1;
  ev.text = name;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

std::string info_name(const Q2Server& s, int slot) {
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return "someone";
}

// A player placed on the map, said to everyone at the high level with his login flash: in
// deathmatch always (on joining and on every new map), in co-op and single player only when the
// server has room for more than one.
void announce_entry(Q2Server& s, int slot) {
  if (!deathmatch(s) && s.sv.set.maxclients <= 1) return;
  if (!s.clients[slot].spectator) arrival_flash(s, slot, "sq_login");
  fight_print(s, -1, info_name(s, slot) + " entered the game\n", 2);
}

void sq_connect(void* r, int slot, const std::string& name, bool spectator, bool bot,
                const float* /*parms: a deathmatch body starts from scratch*/) {
  Q2Server& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients) return;
  ClientInfo& c = s.clients[slot];
  if (c.active && (s.restored & (1u << slot)) != 0) {  // his body from a load: he takes it over
    s.restored &= ~(1u << slot);
    set_info(c.userinfo, "name", name);
    if (s.map != nullptr) fill_rows(s);
    return;
  }
  // Seated again after a level change (the network server's `begin`): the slot the rules kept
  // through the change is his, placed on the new map with what he carried and his score.
  if (c.active && c.spectator == spectator && c.bot == bot && s.map != nullptr) {
    set_info(c.userinfo, "name", name);
    fill_rows(s);
    if ((s.entered_told & (1u << slot)) == 0) announce_entry(s, slot);  // said at the load
    s.entered_told &= ~(1u << slot);
    return;
  }
  c = ClientInfo{};
  c.active = true;
  if (s.sv.set.maxclients > 1) s.console.push_back(name + " connected");  // the console's alone
  c.spectator = spectator;
  c.bot = bot;
  c.userid = s.next_userid++;
  c.entered_at = s.level_time;
  set_info(c.userinfo, "name", name);
  pool_slot(s, slot).speedjump = false;
  s.players[slot] = Q2Player{};
  s.players[slot].time_budget_ms = kTimeBudgetMs;
  s.players[slot].want_spectator = spectator && deathmatch(s);
  if (s.map != nullptr) q2_mods_connect(s, slot);
  if (s.map != nullptr) spawn_player(s, slot, false);
  if (s.map != nullptr) fill_rows(s);
  if (s.map != nullptr) announce_entry(s, slot);
}

void sq_drop(void* r, int slot) {
  Q2Server& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients) return;
  if (s.map != nullptr && s.clients[slot].active) q2_mods_disconnect(s, slot);
  if (s.map != nullptr && s.clients[slot].active && !s.clients[slot].spectator)
    arrival_flash(s, slot, "sq_logout");
  if (s.map != nullptr && s.clients[slot].active)
    fight_print(s, -1, info_name(s, slot) + " disconnected\n", 2);
  if (s.map != nullptr && s.clients[slot].active) gz_tag_left(s, slot);
  s.clients[slot] = ClientInfo{};
  pool_slot(s, slot).speedjump = false;
  s.players[slot] = Q2Player{};
  if (s.map != nullptr) fill_rows(s);
}

// The rocket jump helper's question (rjump_rules.hpp): the launcher chosen when it is not in
// hand (its raising is waited for), and fired by the first command of a frame with the hand ready.
sh::RjumpReady sq_rocket_ready(void* r, int slot, sh::MoveCmd& /*cmd*/, std::string& why) {
  Q2Server& s = sq(r);
  Q2Player& p = s.players[slot];
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
  Q2Server& s = sq(r);
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
  while (s.host_time + 1e-9 >= next_frame_time(s)) rules_frame(s);
  fill_rows(s);
}

CommandResult sq_client_command(void* r, int slot, const std::string& line) {
  Q2Server& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return CommandResult::bad;
  ge::progs::Words w;
  ge::progs::tokenize_words(line.c_str(), w);
  if (w.count == 0) return CommandResult::bad;
  const std::string cmd = ge::progs::word(w, 0);
  // The intermission takes the scores, the help and the chat alone (q2_commands.hpp).
  const bool between = s.intermission_at > 0 && !commands_in_intermission(cmd);
  if (!between && screens_command(s, slot, cmd)) {
    fill_rows(s);
    return CommandResult::mod;
  }
  if (!between && cmd == "kill") {
    Q2Player& p = s.players[slot];
    if (p.alive && s.level_time >= p.respawn_time + kKillAfter) {
      p.god = false;
      if (s.opt.pack == Q2Pack::ground_zero) gz_sphere_remove(s, slot);  // his sphere goes too
      fight_kill(s, slot, slot, -1);
    }
    fill_rows(s);
    return CommandResult::mod;
  }
  std::string rest;
  for (int i = 1; i < w.count; i++) rest.append(i > 1 ? " " : "").append(ge::progs::word(w, i));
  if (!between && (cmd == "use" || cmd == "drop") &&
      commands_refuse_item(s, slot, cmd == "use", rest))
    return CommandResult::mod;
  if (!between && fight_command(s, slot, cmd, rest)) {
    fill_rows(s);
    return CommandResult::mod;
  }
  commands_run(s, slot, line);  // the chat, the cheats, the gestures; an unknown word is said
  fill_rows(s);
  return CommandResult::mod;
}

bool sq_console(void* r, const std::string& line) {
  Q2Server& s = sq(r);
  ge::progs::Words w;
  ge::progs::tokenize_words(line.c_str(), w);
  if (w.count == 0) return true;
  const std::string cmd = ge::progs::word(w, 0);
  if ((cmd == "map" || cmd == "changelevel" || cmd == "gamemap") && w.count >= 2) {
    // A level string (q2_campaign.hpp): its shows first, the map, the spawn spot; a leading `!`
    // (stop the server when the map is missing) is taken off.
    std::string text(ge::progs::word(w, 1));
    if (!text.empty() && text.front() == '!') text.erase(0, 1);
    const LevelString ls = level_string(text);
    s.pending_map = ls.map;
    s.cinematic = ls.shows;
    s.next_spawn = ls.spawn;
    s.pending_change = cmd == "map" ? MapChange::new_game : MapChange::change_level;
    return true;
  }
  if (cmd == "say" && w.count >= 2) {  // the operator's chat line, to everyone, as "console:"
    std::string text = line.substr(line.find_first_not_of(" \t", line.find("say") + 3));
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
      text = text.substr(1, text.size() - 2);
    NetEvent ev;
    ev.kind = EventKind::print;
    ev.to = 0xffffffffu;  // everyone
    ev.reliable = true;
    ev.level = 3;  // the chat level
    ev.text = "console: " + text + "\n";
    ev.time = s.level_time;
    s.console.push_back("console: " + text);
    s.out.events.push_back(std::move(ev));
    s.out.events_total++;
    return true;
  }
  if (cmd == "sv") {  // the game's own server words
    const std::string word = w.count >= 2 ? std::string(ge::progs::word(w, 1)) : std::string();
    if (q2_same_name(word, "test")) {
      s.console.push_back("Svcmd_Test_f()");
      return true;
    }
    for (const char* ip : {"addip", "removeip", "listip", "writeip"})
      if (q2_same_name(word, ip)) return false;  // the network server's filter answers these
    s.console.push_back("Unknown server command \"" + word + "\"");
    return true;
  }
  if (cmd == "restart") {
    s.pending_map = s.map_name;
    s.pending_change = MapChange::restart;
    return true;
  }
  if (cmd == "serverinfo" && w.count >= 3) {  // a key every client is told (classic remix's)
    const std::string key(ge::progs::word(w, 1)), value(ge::progs::word(w, 2));
    auto at = std::find_if(s.serverinfo.begin(), s.serverinfo.end(),
                           [&](const auto& kv) { return kv.first == key; });
    if (at == s.serverinfo.end())
      s.serverinfo.emplace_back(key, value);
    else
      at->second = value;
    return true;
  }
  if (cmd == "set" && w.count >= 3 && s.cvars.find(ge::progs::word(w, 1)) != nullptr)
    return sq_console(r, std::string(ge::progs::word(w, 1)) + " \"" + ge::progs::word(w, 2) + "\"");
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
  Q2Server& s = sq(r);
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return;
  set_info(s.clients[slot].userinfo, key, value);
  if (key == "spectator") s.players[slot].want_spectator = spectate_wish(s, slot);
  if (key == "cl_speedjump") pool_slot(s, slot).speedjump = value == "1";  // the easy jump's ask
  if (key == kAimHelpKey) pool_slot(s, slot);  // the grant lands on a seat the pool keeps
  note_aim_help_info(s, slot, key, value);
  if (s.map != nullptr) fill_rows(s);
}

bool sq_pause(void* r, int /*slot*/, bool on) {
  Q2Server& s = sq(r);
  if (s.paused == on) return false;
  s.paused = on;
  s.world.paused = on;
  return true;
}

void sq_carry(void* /*r*/) {}  // a deathmatch body starts from scratch on every map

void sq_destroy(void* r) { delete static_cast<Q2Server*>(r); }

void sq_aid(const void* r, int slot, sh::MoveState& state, sh::SpeedJumpRules& air) {
  const Q2Server& s = *static_cast<const Q2Server*>(r);
  state = q2_aid_state(s, slot);
  air = sh::q2_jump_rules(q2_air_accelerate(s));
}

std::string sq_save_problem(const void* r) {
  return q2_save_problem(*static_cast<const Q2Server*>(r));
}

bool sq_save(const void* r, std::vector<uint8_t>& out, std::string& error, int64_t saved_unix) {
  return q2_save(*static_cast<const Q2Server*>(r), out, error, saved_unix);
}

std::vector<uint8_t> sq_take_autosave(void* r) {
  return std::exchange(static_cast<Q2Server*>(r)->autosave, std::vector<uint8_t>());
}

uint32_t sq_last_sequence(const void* r, int slot) {
  return static_cast<const Q2Server*>(r)->players[slot].last_sequence;
}

constexpr RunnerCalls kQ2Calls = {"q2",
                                  &sq_start,
                                  &sq_connect,
                                  &sq_drop,
                                  &sq_frame,
                                  &sq_client_command,
                                  &sq_console,
                                  &sq_user_info,
                                  &sq_pause,
                                  &sq_carry,
                                  &sq_destroy,
                                  false,
                                  &q2_start_mods,
                                  true,
                                  true,
                                  &sq_aid,
                                  &sq_save_problem,
                                  &sq_save,
                                  &sq_take_autosave,
                                  &sq_last_sequence};

}  // namespace

Q2Server::Q2Server() = default;
Q2Server::~Q2Server() = default;

std::unique_ptr<Q2Server> make_q2_server(const Q2ServerOptions& opt) {
  auto s = std::make_unique<Q2Server>();
  s->opt = opt;
  s->rng = ge::core::make_rng(opt.seed);
  register_cvars(*s);
  if (opt.pack == Q2Pack::ground_zero) gz_cvars(*s);
  add_aim_help(*s);
  s->console.push_back("==== InitGame ====");
  return s;
}

RulesRunner q2_runner(std::unique_ptr<Q2Server> s) {
  Q2Server* p = s.release();
  return {&kQ2Calls, p, p};
}

Q2Server* q2_of(const RulesRunner& r) {
  return r.calls == &kQ2Calls ? static_cast<Q2Server*>(r.rules) : nullptr;
}

void q2_hold_speed(const Q2Server& s, sh::Vec3f& velocity) {
  const float most = static_cast<float>(s.cvars.find("sv_maxvelocity")->number);
  velocity.x = std::clamp(velocity.x, -most, most);
  velocity.y = std::clamp(velocity.y, -most, most);
  velocity.z = std::clamp(velocity.z, -most, most);
}

sh::MoveState q2_aid_state(const Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  sh::MoveState m;
  m.origin = unpack(p.move.origin);
  m.velocity = unpack(p.move.velocity);
  m.ground = p.on_ground ? 0 : -1;
  m.waterlevel = p.water_level;
  const bool playing = p.alive && !s.clients[slot].spectator && s.intermission_at <= 0;
  m.type = playing ? sh::MoveType::normal : sh::MoveType::none;
  return m;
}

float q2_air_accelerate(const Q2Server& s) {
  return deathmatch(s) ? static_cast<float>(s.cvars.find("sv_airaccelerate")->number) : 0;
}

profiles::MoveFeel q2_move_feel(const Q2Server& s) {
  if (s.q1_physics) return profiles::kQ1Feel;  // Quake's ground and air
  profiles::MoveFeel f;
  f.air_accelerate = q2_air_accelerate(s);
  f.land_pause = s.cvars.find("sv_landjump")->integer() == 0;
  f.surface_lift = s.cvars.find("sv_waterjump")->integer() == 0;
  return f;
}

profiles::MoveFeel q2_move_feel(const Q2Server& s, int slot) {
  profiles::MoveFeel f = q2_move_feel(s);
  const ClientInfo& c = s.clients[slot];
  // A client that is not told the feel predicts the stock landing and lift: at 1 they stay his.
  bool told = c.bot;
  for (const auto& [k, v] : c.userinfo)
    if (k == "*ge_ext") told = told || (std::atoi(v.c_str()) & 64) != 0;
  if (!told && s.cvars.find("sv_landjump")->integer() < 2) f.land_pause = true;
  if (!told && s.cvars.find("sv_waterjump")->integer() < 2) f.surface_lift = true;
  return f;
}

void sq_dress(const Q2Server& s, WorldEntity& e) {
  if (e.modelindex <= 0 || size_t(e.modelindex) >= s.model_looks.size()) return;
  const profiles::Look* l = s.model_looks[size_t(e.modelindex)];
  if (l == nullptr) return;
  e.skin = l->skin;
  e.origin = e.origin + sh::Vec3f{l->offset[0], l->offset[1], l->offset[2]};
  if (l->to.ends_with(".bsp")) {
    e.turns = false;
    e.effects = 0;
    e.render_fx = 0;
  }
}

void q2_frames_alone(Q2Server& s, int frames) {
  bool active[sh::kMaxClients];
  for (int i = 0; i < sh::kMaxClients; i++) {
    active[i] = s.clients[i].active;
    s.clients[i].active = false;
  }
  const int64_t until = s.frame_number + frames;  // tenths, whatever `sv_fps` is
  while (s.frame_number < until) rules_frame(s);
  for (int i = 0; i < sh::kMaxClients; i++) s.clients[i].active = active[i];
  s.host_time = s.level_time;
  fill_rows(s);
}

void q2_rebuild(Q2Server& s) {
  fill_lists(s);
  s.sv.set.max_edicts = std::max(s.sv.set.max_edicts, int(s.world.entities.size()) + 8);
  s.sv.edicts.assign(size_t(s.sv.set.max_edicts), sh::Edict{});
  fill_rows(s);
}

void q2_spawn(Q2Server& s, int slot) {
  spawn_player(s, slot, false);
  fill_rows(s);
}

bool q2_console(Q2Server& s, const std::string& line) { return sq_console(&s, line); }

void q2_user_info(Q2Server& s, int slot, const std::string& key, const std::string& value) {
  sq_user_info(&s, slot, key, value);
}

void q2_connect(Q2Server& s, int slot, const std::string& name) {
  sq_connect(&s, slot, name, false, false, nullptr);
}

bool sq_give(Q2Server& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || dm_left_out(s, item)) return false;
  Q2Player& p = s.players[slot];
  if (!p.alive) return false;
  const SqItem& it = kSqItems[item];
  p.body.health = static_cast<int>(p.health);
  const int grenades = p.body.ammo[static_cast<int>(it.ammo)];  // its kind's before
  if (!sq_pick_up(item, p.body, deathmatch(s), skill(s), s.level_time)) return false;
  p.health = static_cast<float>(p.body.health);
  if (it.kind == SqKind::weapon && fight_anyflag(s, 8192)) {  // infinite ammo: a full stock
    const int k = static_cast<int>(it.ammo);
    p.body.ammo[k] = std::max(p.body.ammo[k], std::min(1000, p.body.max_ammo[k]));
  }
  p.taken.push_back(item);
  fight_took(s, slot, item, grenades);
  return true;
}

}  // namespace shooters_edge::server
