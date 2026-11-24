// QUAKE II'S MATCH STATS: the shared stats every classic game counts (server/match_stats.hpp:
// accuracy, splash, mid-airs, damage, armour and power-ups, streaks, spawn frags), fed by Quake
// II's rules as things happen (the fight calls these) and read by the server's match for the
// `stats` words and the lines shown when a match ends (net/q2_match.hpp). Counting is on only while
// `Q2Stats::on` (the match's phase), so a warmup counts nothing; it changes nothing in the game.
//
// QUAKE II'S EVENTS in the shared rules:
// - ATTACKS: the shotgun's 12 pellets, the super shotgun's 20, each machine gun or chaingun bullet,
//   each bolt, grenade, rocket, slug or BFG ball.
// - HITS: each pellet, bullet or bolt that lands, a slug's every player, a rocket's or grenade's
//   direct touch, a hand grenade's or the BFG's blast once a player a frame; the BFG ball's laser
//   on its way is damage only. A rocket's or grenade's blast on an enemy is splash.
// - DAMAGE is the blow's points before armour; a blow from the world counts none. The big guns
//   (`dmg_ewep`) are the rocket launcher and the railgun.
// - ITEMS: jacket, combat and body armour, the mega health, the quad, invulnerability and power
//   armour (a screen or a shield).
// - Beside the shared columns: kills, deaths, suicides (oneself or the world) and team kills.
//
// ge:summary Quake II's match stats: the shared columns fed by the fight's calls, by slot
// ge:keywords q2-stats match-stats accuracy weapon-stats shots hits damage-dealt team-kills
// ge:keywords suicides stats teamstats laststats airgib midair splash
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_match_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/server/match_stats.hpp"
#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::server {

// Quake II's weapons and items as the shared stats count them; the weapons in SqWeapon's order.
const StatsGame& q2_stats_game();

struct Q2SlotStats {
  PlayerStats tally;  // the shared columns
  int32_t kills = 0, deaths = 0, suicides = 0, team_kills = 0;
  std::array<int64_t, kSqWeaponCount> hit_frame{};  // the frame of a blast's last hit (+1)
  std::array<uint32_t, kSqWeaponCount> hit_mask{};  // and the players it hit that frame
  bool counted = false;                             // anything counted (a row in the table)
};

struct Q2Stats {
  bool on = false;  // a match is on: count
  std::array<Q2SlotStats, 32> slot{};
  double last_time = -1;  // the level time of the last tick
};

struct Q2Server;

// The fight's calls: a shot of weapon `w` (`n` shots); a blow of `points` from `attacker` on
// `target` with weapon `w` (-1 none), landing as the server's `blow_how`; a death of `slot` by
// `killer` (-1 the world) with `w`; a player spawned; an item taken (its row).
void stats_shot(Q2Server& s, int slot, int w, int n);
void stats_blow(Q2Server& s, int attacker, int target, int w, int points);
void stats_death(Q2Server& s, int slot, int killer, int w, bool teammate);
void stats_spawn(Q2Server& s, int slot);
void stats_item(Q2Server& s, int slot, int item);
// Once a rules frame: the quad's seconds held.
void stats_tick(Q2Server& s);

}  // namespace shooters_edge::server
