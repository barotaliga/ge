// A CLASSIC MATCH'S WEAPON AND ITEM STATS (docs/accounts.md § The weapon stats): what each player
// did with each weapon, the damage they gave and took, the armour and power-ups they took, their
// streaks and spawn frags, read off the QC server every frame (it changes nothing there) and sent
// with the match's report as its columns (`kMatchStats`, named by `stat_name`).
//
// - AN ATTACK is ammunition spent on the weapon held the frame before, alive in both frames: a
//   scattergun's shell is 6 pellets, a double's two shells 14, a nail one (the heavy spiker's two
//   nails one), a grenade or rocket one, a cell one pulse of the arc. The axe's is a swing: its
//   refire time moving on while the axe is held.
// - A HIT is a blow a player took (health and armour, read every frame) from whatever last hurt
//   them (`dmg_inflictor`): a player's axe, pellets (a blow over 4 points each, four times that
//   under the quad) or pulse, or a missile that touched them: a nail, a rocket by its direct
//   blow (100, or a kill within 32 units of it), a grenade that was moving within 32 units.
//   Team mates count; oneself does not. A rocket's or grenade's blast on an enemy counts as a
//   SPLASH HIT per enemy hurt, direct or not (`rl_splash`, `gl_splash`), and its blow is summed
//   for the average rocket damage (`rl_damage`).
// - A MID-AIR is a rocket's blow on an enemy who was off the ground and out of the water the frame
//   before; their height over the floor below is summed (`midair_height`) and its most kept
//   (`midair_height_max`).
// - A KILL is credited to the weapon of the blow that brought a player's health to nothing (a
//   kill for the killer, a death by that weapon for the victim) when the killer is an enemy;
//   a telefrag, the world or a stomp credits none. A kill of an enemy within 2 s of their
//   spawn is a SPAWN FRAG. A STREAK is kills without dying; a QUAD RUN, the kills of one quad.
// - DAMAGE: a blow from an enemy is `dmg_given` for them and `dmg_taken` for the victim; from a
//   team mate `dmg_team`, from oneself `dmg_self`; `dmg_ewep` the given part dealt to an enemy
//   who carried the rocket launcher or the arc.
// - ITEMS: armour taken (its points rising, by its kind), a mega health (health rising 50 or
//   more), a quad, a pentagram or a ring (its bit set); `quad_s` the seconds held alive.
//
// Columns whose name ends `_max` are maxima: the master keeps their largest, not their sum.
//
//   stats_frame(eyes, seats, server)   once a frame, after the rules (null seats count nothing)
//   stats_close(seat)                  the runs closed into their best, at the match's end
//   stats_lines(report)                the lines every player is shown when the match ends
//
// ge:summary A classic match's per-player weapon, damage, item and streak stats, read off the
// ge:summary QC server each frame for the report's columns
// ge:keywords weapon-stats match-stats rocket-hits grenade-hits midair splash damage-given
// ge:keywords damage-taken spawn-frags streak quad-run armour-taken stats_frame stat_name
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_stats_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/report.hpp"

#include "shooter/server.hpp"

namespace shooters_edge::server {
struct QcServer;
}

namespace shooters_edge::net {

// The weapons, in a report's order.
enum class StatWeapon : uint8_t { axe, sg, ssg, ng, sng, gl, rl, lg };
constexpr int kMsWeapons = 8;

// A weapon's four columns: attacks, hits, kills, deaths by it (kMsAttacks + 4 * weapon + k).
constexpr int kMsAttacks = 0, kMsHits = 1, kMsKills = 2, kMsDeaths = 3;
enum Stat : uint8_t {
  kMsRlSplash = 4 * kMsWeapons,
  kMsGlSplash,
  kMsRlDamage,
  kMsMidair,
  kMsMidairHeight,
  kMsMidairHeightMax,
  kMsDmgGiven,
  kMsDmgTaken,
  kMsDmgTeam,
  kMsDmgSelf,
  kMsDmgEwep,
  kMsGa,
  kMsYa,
  kMsRa,
  kMsMh,
  kMsQuad,
  kMsPent,
  kMsRing,
  kMsQuadS,
  kMsSpawnFrags,
  kMsStreakMax,
  kMsQuadRunMax,
  kMsCount
};
constexpr int kMatchStats = kMsCount;
constexpr int stat_of(StatWeapon w, int k) { return 4 * static_cast<int>(w) + k; }
// The column's name in a report ("rl_hits", "dmg_given", "streak_max").
const char* stat_name(int stat);

struct SeatStats {
  std::array<int64_t, kMatchStats> n{};
  int64_t run = 0, quad_run = 0;  // the kills since the last death, and of the quad held
  double spawned = -1;            // the server's time the player last came in, s
  double quad_carry = 0;          // the part of a second of quad not yet counted, s
};

// What the last frame read, by entity.
struct StatsEyes {
  double time = -1;
  std::vector<float> life, health, armour, attack_finished;
  std::vector<int32_t> items, weapon;
  std::vector<std::array<float, 4>> ammo;
  std::vector<uint8_t> aloft;  // off the ground and out of the water
  std::vector<ge::shooter::Vec3f> at;
  std::vector<uint8_t> kind;   // a missile's kind (0 none)
  std::vector<float> speed;    // and its speed, units/s
  std::vector<int32_t> owner;  // and whose
};

void stats_frame(StatsEyes& eyes, const std::array<SeatStats*, ge::shooter::kMaxClients>& seats,
                 const server::QcServer& s);
void stats_close(SeatStats& seat);
// The match's stats as the players read them when it ends, a line each (none when the report has
// no weapon stats): per player their frags and efficiency, each weapon's accuracy, the rockets'
// average damage and mid-airs, armour and power-ups taken, damage, streaks and spawn frags.
std::vector<std::string> stats_lines(const ge::net::MatchReport& r);

}  // namespace shooters_edge::net
