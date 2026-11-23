// QUAKE II'S MATCH STATS: what each player did with each weapon while a match is on, counted by
// the rules as it happens (the fight calls these) and read by the server's match for the `stats`
// words (net/q2_match.hpp). Counting is on only while `Q2Stats::on` (the match's phase), so a
// warmup counts nothing; it changes nothing in the game.
//
// - A SHOT is a pull of the trigger that spends ammunition: the shotguns' blast one, the machine
//   gun's and chaingun's bullets each, a grenade, rocket, bolt, slug or BFG ball one.
// - A HIT is a blow on an enemy, once a frame for each weapon however many pellets or how many
//   players a blast touches, but the railgun counts every player its slug passes through. A blow on
//   oneself is damage received only; on a teammate, team damage only.
// - DAMAGE dealt and received is the blow's points before armour.
// - A KILL of an enemy counts for the weapon that made it; the victim's death counts against the
//   same weapon. A suicide or a death by the world is a suicide; a teammate's kill a team kill.
//
// ge:summary Quake II's match stats: shots, hits, kills, deaths and damage a weapon, by slot
// ge:keywords q2-stats match-stats accuracy weapon-stats shots hits damage-dealt team-kills
// ge:keywords suicides stats teamstats laststats stats-table
// ge:skill shooter net
// ge:test tests/shooters_edge_q2_match_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::server {

struct Q2WeaponTally {
  int32_t shots = 0, hits = 0, kills = 0, deaths = 0, dealt = 0, received = 0;
};

struct Q2SlotStats {
  std::array<Q2WeaponTally, kSqWeaponCount> weapon{};
  int32_t kills = 0, deaths = 0, suicides = 0, team_kills = 0;
  int32_t dealt = 0, received = 0, team_dealt = 0, team_received = 0;
  std::array<int64_t, kSqWeaponCount> hit_frame{};  // the frame of a weapon's last hit (+1)
  uint32_t rail_hit = 0;                            // the players a slug hit this frame
  bool counted = false;                             // anything counted (a row in the table)
};

struct Q2Stats {
  bool on = false;  // a match is on: count
  std::array<Q2SlotStats, 32> slot{};
};

struct Q2Server;

// The fight's calls: a shot of weapon `w` (`n` shots); a blow of `points` from `attacker` on
// `target` with weapon `w` (-1 none); a death of `slot` by `killer` (-1 the world) with `w`.
void stats_shot(Q2Server& s, int slot, int w, int n);
void stats_blow(Q2Server& s, int attacker, int target, int w, int points);
void stats_death(Q2Server& s, int slot, int killer, int w, bool teammate);

// The table a player reads: "Stats for <name>:", a row a weapon used or met (accuracy rounded up,
// a dash when nothing was fired), then the totals. "" when nothing was counted.
std::string stats_table(const Q2SlotStats& p, const std::string& name);
// Accuracy as a whole percentage rounded up; -1 with no shots.
int stats_accuracy(const Q2WeaponTally& t);

}  // namespace shooters_edge::server
