// A MATCH'S PLAYER STATS, THE SAME FOR EVERY CLASSIC GAME (Quake and QuakeWorld, Quake II): what
// each player did with each weapon, the damage they gave and took, the armour and power-ups they
// took, mid-airs, splash, streaks and spawn frags. Each game finds the events its own way (the
// QuakeWorld server reads them off its QC program every frame, net/qw_stats.hpp; Quake II's rules
// call them as they happen, server/q2_stats.hpp) and hands them here, so every game counts by the
// same rules into the same columns and shows the same lines.
//
// THE GAME'S PART (`StatsGame`): its weapons' short labels in its report's order (at most
// `kStatWeaponsMost`), which of them is the rocket launcher and the grenade launcher (their blast
// is counted as splash), the order the accuracy line shows them in, and its armours' and power-ups'
// labels.
//
// THE COLUMNS, in a report's order: four a weapon (`<weapon>_attacks`, `_hits`, `_kills`,
// `_deaths`), then the rest (`StatRest`): rl_splash, gl_splash, rl_damage, midair, midair_height,
// midair_height_max, dmg_given, dmg_taken, dmg_team, dmg_self, dmg_ewep, the three armours, mh,
// the three power-ups, quad_s, spawn_frags, streak_max, quad_run_max. A column whose name ends
// `_max` is a maximum: the master keeps the largest, not the sum.
//
// THE RULES every game counts by:
// - AN ATTACK is a shot's worth of a weapon: a pellet of a shotgun, a bullet, a nail, a missile, a
//   slug, one pulse of a beam, a swing. A HIT is a blow on another player (a team mate counts,
//   oneself never) from one: each pellet, bullet, nail or pulse that lands, a missile's direct
//   touch, a slug's every player. A hit proves its attack: attacks never fall below hits.
// - SPLASH: a rocket's or grenade's blow on an enemy, direct or by its blast, one a player hurt
//   (`rl_splash`, `gl_splash`); a rocket's points summed for its average (`rl_damage`).
// - A MID-AIR is a rocket's blow on an enemy off the ground and out of the water; their height over
//   the floor below is summed and its most kept.
// - DAMAGE: from an enemy, `dmg_given` for the dealer and `dmg_taken` for the victim; from a team
//   mate `dmg_team`; from oneself `dmg_self`; `dmg_ewep` the given part dealt to an enemy who
//   carried the game's big guns (the rocket launcher, and the arc or the railgun).
// - A KILL of an enemy counts for the weapon of the killing blow, a death by it for the victim. A
//   kill within 2 s of the victim's spawn is a SPAWN FRAG; a STREAK is kills without dying; a
//   QUAD RUN the kills of one quad. A death ends the victim's runs.
// - ITEMS: each armour, mega health and power-up taken; `quad_s` the whole seconds a quad was held
//   alive.
//
// ge:summary Every classic game's match stats: the columns, the counting rules and the lines shown
// ge:keywords match-stats weapon-stats accuracy airgib midair splash rl-damage damage-given
// ge:keywords damage-taken spawn-frags streak quad-run armour-taken stats_lines stats_game
// ge:keywords shared-stats top-scores stats_top_lines stats_kill stats_hit stats_attack stats_blow
// ge:skill net shooter
// ge:test tests/shooters_edge_match_stats_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/report.hpp"

namespace shooters_edge::server {

constexpr int kStatWeaponsMost = 12;
// A weapon's four columns: attacks, hits, kills, deaths by it (4 * weapon + part).
constexpr int kStatAttacks = 0, kStatHits = 1, kStatKills = 2, kStatDeaths = 3;

// The columns after the weapons', in a report's order.
enum StatRest : uint8_t {
  kStatRlSplash,
  kStatGlSplash,
  kStatRlDamage,
  kStatMidair,
  kStatMidairHeight,
  kStatMidairHeightMax,
  kStatDmgGiven,
  kStatDmgTaken,
  kStatDmgTeam,
  kStatDmgSelf,
  kStatDmgEwep,
  kStatArmourLight,   // QuakeWorld's green, Quake II's jacket
  kStatArmourMiddle,  // yellow, combat
  kStatArmourHeavy,   // red, body
  kStatMega,
  kStatQuad,
  kStatPowerB,  // QuakeWorld's pentagram, Quake II's invulnerability
  kStatPowerC,  // QuakeWorld's ring, Quake II's power armour
  kStatQuadS,
  kStatSpawnFrags,
  kStatStreakMax,
  kStatQuadRunMax,
  kStatRestCount
};
constexpr int kStatColumnsMost = 4 * kStatWeaponsMost + kStatRestCount;

// The kinds of item a player takes, counted under the game's labels.
enum class StatItem : uint8_t {
  armour_light,
  armour_middle,
  armour_heavy,
  mega,
  quad,
  power_b,
  power_c
};

struct StatsGame {
  int weapons = 0;                                    // how many (at most kStatWeaponsMost)
  std::array<const char*, kStatWeaponsMost> label{};  // "rl", "lg"
  int rl = -1, gl = -1;                               // the launchers whose blast is splash
  std::array<uint8_t, kStatWeaponsMost> shown{};      // the accuracy line's order
  int shown_count = 0;
  std::array<const char*, 7> item{};   // the items' column names, by StatItem ("ga", "quad")
  std::array<const char*, 7> words{};  // and as a player reads them ("green", "quad")
};

struct PlayerStats {
  std::array<int64_t, kStatColumnsMost> n{};  // by column (stat_rest for the rest)
  int64_t run = 0, quad_run = 0;              // the kills since the last death, and of the quad
  double spawned = -1;                        // the game's time the player last came in, s
  double quad_carry = 0;                      // the part of a second of quad not yet counted, s
};

// The game's columns: how many, a column's name ("rl_hits", "dmg_given"), where a rest column is.
int stats_columns(const StatsGame& g);
std::string stats_column_name(const StatsGame& g, int column);
constexpr int stat_rest(const StatsGame& g, StatRest r) { return 4 * g.weapons + r; }
constexpr int stat_weapon(int weapon, int part) { return 4 * weapon + part; }

// The events (a null player counts nothing; `weapon` -1 is no weapon of the list).
void stats_spawned(PlayerStats* p, double now);
void stats_attack(PlayerStats* p, int weapon, int64_t n);
void stats_hit(PlayerStats* p, int weapon, int64_t n);
// A blow of `points` from `by` on `victim`: by oneself, a team mate or an enemy (`enemy`);
// `big_gun` the victim carried one of the game's big guns.
void stats_blow(const StatsGame& g, PlayerStats* by, PlayerStats* victim, bool self, bool enemy,
                int64_t points, bool big_gun);
// A rocket's or grenade's blow on an enemy (`weapon` g.rl or g.gl), of `points`.
void stats_splash(const StatsGame& g, PlayerStats* by, int weapon, int64_t points);
void stats_midair(const StatsGame& g, PlayerStats* by, int64_t height);
// A death: `killer` the enemy credited (null none), `enemy` whether one killed, with `weapon`;
// `quad` the killer held the quad; `now` the game's time, s.
void stats_died(const StatsGame& g, PlayerStats* victim, PlayerStats* killer, bool enemy,
                int weapon, bool quad, double now);
void stats_took(const StatsGame& g, PlayerStats* p, StatItem item);
// `dt` seconds of the quad held alive.
void stats_quad_held(const StatsGame& g, PlayerStats* p, double dt);
// The runs closed into their best (a death, the match's end).
void stats_close(const StatsGame& g, PlayerStats& p);
// The game's columns of `p`, its runs closed, in a report's order.
std::vector<int64_t> stats_row(const StatsGame& g, const PlayerStats& p);

// The lines a player reads for one player of a report whose columns begin with the game's: their
// frags and efficiency, each weapon's accuracy, the rockets' average damage and mid-airs, armour
// and power-ups, damage, streaks and spawn frags. None when the report has no such columns.
std::vector<std::string> stats_player_lines(const StatsGame& g, const ge::net::MatchReport& r,
                                            const ge::net::ReportPlayer& p);
// "Player statistics:", every player's lines, then the top scores.
std::vector<std::string> stats_lines(const StatsGame& g, const ge::net::MatchReport& r);
// THE TOP SCORES, with two players or more: "Top scores:" and a line a measure that someone
// scored above 0, the best and every player tied with him ("  Streak: ana, ben (5)"): frags,
// efficiency, damage given, mid-airs, the highest mid-air, streak, quad run, spawn frags.
std::vector<std::string> stats_top_lines(const StatsGame& g, const ge::net::MatchReport& r);

}  // namespace shooters_edge::server
