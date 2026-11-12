// MIDAIR, FOR ANY ARENA SHOOTER: the rocket duel where only a hit on a player in the air counts,
// and counts for more the higher he flew above the shooter. One set of rules every game's rules
// call (the sequel's today, server/sequel_fight.cpp; the classic arena's once they are C++), so the
// mode plays alike on every map family and a game adds only its glue: where a hit lands, a trace
// down, the spawn's kit in its own items, a line printed.
//
// - THE SETTINGS a server keeps: `k_midair` (1 on) and `k_midair_minheight`, a LEVEL 0..4 (1 by
//   default) naming the least height a hit counts at: 64, 128, 256, 512 or 1024 units, read as
//   "ground", "bronze", "silver", "gold" or "platinum" (`midair_min_height`,
//   `midair_level_name`). Any other level is 64.
// - IN THE AIR is the victim's HEIGHT: the bottom of his box less the floor under him, found by a
//   line `kMidairProbe` units straight down from his origin that only the world stops (other
//   players and monsters let it through); nothing under him in reach reads as the whole probe. The
//   on-ground flag and the speed are not looked at.
// - A ROCKET (direct or splash) on ANOTHER player at the least height or more KILLS him outright
//   (`kMidairKill` damage, through any armour: thrown apart). Below it the blow takes armour as
//   usual and no health. One's own rocket never takes health. Of the rest only drowning, lava,
//   slime, a telefrag and the kill command still harm a player (`MidairHarm`); falls, the blade
//   and everything else do nothing. Things that are not players take every blow as usual.
// - THE SCORE of a counted hit is by the RISE: the victim's origin less the shooter's at the
//   moment he fired (`midair_band`): above 1024 platinum (8 frags), above 512 gold (4), above 256
//   silver (2), else bronze (1), a negative rise too. Any other kill of a player gives the killer
//   nothing; suicides and the world's kills keep their usual cost.
// - THE PUSH a blow gives is worked out from its damage before the rules above take the damage
//   away, so a hit that harms nothing still throws (`midair_push`): a weak splash (under 60) on
//   another player pushes 11/8 as hard; a target `kMidairLowAir` or more above the floor is pushed
//   1 + (height − 45) / 64 times as hard (double at 109, triple at 173), his own rocket jump too;
//   one lower is also thrown upward, the up part of the blow's way times its damage times 6 (a
//   weak splash on another player) or 4, which is `lift` times the push.
// - THE SPAWN in the mode: 250 health, 200 of the best armour, the blade and the rocket launcher
//   in hand with ammunition that never runs out, no spawn shield; no item lies on the map, and a
//   dead player comes back by himself after `kMidairRespawn` seconds. Gravity and speed are as
//   they are.
// - THE LINE told to everyone for a counted hit (`midair_line`) and the match's tally per player
//   (`MidairTally`: hits by band, the highest and the mean rise), told at the match's end
//   (`midair_tally_line`). The words are a game's, never a tool's.
//
// Pure and allocation-free except the lines. Held by tests/shooter_midair_test.cpp and the
// `se-midair` lab.
//
// ge:summary Midair for any arena shooter: hits count only in the air, scored by the rise
// ge:keywords midair k_midair k_midair_minheight air-shot rocket-only height-bands bronze silver
// ge:keywords gold platinum min-height midair_band midair_push knockback game-mode endif
// ge:keywords midair_line midair-tally
// ge:skill shooter
// ge:test tests/shooter_midair_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ge::shooter {

inline constexpr int kMidairLevels = 5;         // `k_midair_minheight` 0..4
inline constexpr int kMidairDefaultLevel = 1;   // 128 units: bronze
inline constexpr float kMidairProbe = 2048;     // the line down that finds the floor, units
inline constexpr float kMidairLowAir = 45;      // under it a blow also throws upward, units
inline constexpr float kMidairKill = 9999;      // a counted hit's damage
inline constexpr float kMidairRespawn = 2;      // a dead player back by himself, seconds
inline constexpr float kMidairHealth = 250;     // a spawn's
inline constexpr float kMidairArmour = 200;     // a spawn's, of the best kind
inline constexpr float kMidairWeakSplash = 60;  // a blow under it on another player: harder push

enum class MidairBand : uint8_t { bronze, silver, gold, platinum };
inline constexpr int kMidairBands = 4;

// The least height a hit counts at, units, for level 0..4 (64 for any other).
float midair_min_height(int level);
// The level's name ("ground", "bronze", "silver", "gold", "platinum").
const char* midair_level_name(int level);
// A level from the setting's text: its whole number 0..4, the default when empty or another.
int midair_level(std::string_view text);

// The band of a counted hit by its rise (victim's origin less the shooter's when he fired).
MidairBand midair_band(float rise);
int midair_points(MidairBand b);             // 1, 2, 4, 8
const char* midair_band_name(MidairBand b);  // "bronze" ... "platinum"

// The victim's height above the floor: the bottom of his box (`origin_z + mins_z`) less where the
// line down stopped (`floor_z`; the probe's end when it met nothing).
inline float midair_height(float origin_z, float mins_z, float floor_z) {
  return origin_z + mins_z - floor_z;
}
// Whether a rocket's hit at `height` counts at `level`.
bool midair_counts(float height, int level);
// The frags a rocket hit on another player is worth: 0 when it does not count, else its band's.
int midair_score(float height, float rise, int level);

// What harms a player in the mode besides a counted rocket.
enum class MidairHarm : uint8_t { none, drowning, lava, slime, telefrag, suicide };
inline bool midair_harms(MidairHarm h) { return h != MidairHarm::none; }

// How a blow's push changes: the push the game would give (its own law: the classic game's
// damage × 8, the sequel's by mass) times `scale` along the blow's way, plus `lift` times that
// push times the way's up part, straight up.
struct MidairPush {
  float scale = 1;
  float lift = 0;
};
// For a blow of `damage` on a target `height` above the floor; `own` his own rocket.
MidairPush midair_push(float height, float damage, bool own);

// The line told to everyone for a counted hit, ending in a newline.
std::string midair_line(MidairBand b, float rise, std::string_view shooter,
                        std::string_view victim);

// A player's midairs over a match.
struct MidairTally {
  int hits[kMidairBands] = {};
  float highest = 0;
  double rise_sum = 0;
  int total() const { return hits[0] + hits[1] + hits[2] + hits[3]; }
};
void midair_note(MidairTally& t, float rise);
// "Alice: 5 midairs (bronze 2, silver 2, gold 1, platinum 0), highest 612.3, average 301.0",
// with a newline; "" for a tally with none.
std::string midair_tally_line(std::string_view name, const MidairTally& t);

// The line told to everyone when the least height is set.
std::string midair_level_line(int level);

}  // namespace ge::shooter
