// THE ARMY'S WEAPONS AND THE GUNFIRE'S OWN NUMBERS (gunfire.hpp): what each gun fires, how fast,
// how often, how far it carries and how wide it throws. No weapon here is any maker's: each row is
// a CLASS, and its numbers are the band the service weapons of that class sit in, restated from
// the open references (field manuals' tables, ammunition data sheets, marksmanship studies).
//
// THE ROUND'S FLIGHT. A bullet leaves at its muzzle speed and the air slows it as v' = -k v^2, so
// its speed falls with the distance flown as v(x) = v0 e^(-k x); `drag` is fitted to the speed
// each class still carries at its effective range. Gravity pulls it down the whole way, and the
// shooter holds over for it.
//
// WHERE THE ROUNDS GO. Three spreads, each the standard deviation of an angle in MILS (a
// thousandth of the distance: 1 mil is 1 m at 1 km):
// - `dispersion`: the weapon and its ammunition fired from a rest. A service rifle groups in 2 to
//   4 minutes of angle (about 0.4 mil a standard deviation); a machine gun's cone of fire on its
//   mount is wider (2 to 3 mil).
// - `hold`: the shooter's own aim, drawn once a BURST: his breath, his heart, the ground or the
//   machine under him, the stress of a fight. Range studies put a trained soldier at half his
//   rounds on a man-sized target at 300 m on a range and about a tenth in the field: a hold of
//   about 2 mil.
// - `climb`: what each round of a burst adds to the muzzle's walk off the aim (the recoil lifting
//   it), so a rifle's third round lands well over its first.
// And `lead`: the share of the lead he misjudges, drawn once a burst: a man judging a target's
// crossing speed is out by a quarter or so.
//
// WHAT A ROUND CARRIES INTO WHAT IT HITS is its kinetic energy, mv^2/2, at the speed it still has
// there: the `pierce` dose the body's ladders read (skier/anatomy.hpp).
//
// ge:summary The army's weapons (rifle, light machine gun, door gun) and the gunfire's numbers
// ge:keywords weapons weapon rifle lmg light-machine-gun door-gun muzzle drag dispersion hold
// ge:keywords climb lead burst pause cyclic-rate tracer warning-fire snap mils gunfire rules
// ge:skill sim
// ge:test tests/fall_line_gunfire_test.cpp
#pragma once

#include <array>
#include <cstdint>

namespace fall_line::sim {

enum class WeaponKind : uint8_t { rifle, lmg, door_gun };
inline constexpr int32_t kWeaponKinds = 3;

// A GUN that fires bullets.
struct GunDef {
  double mass;                      // the bullet, kg
  double muzzle;                    // its speed leaving the muzzle, m/s
  double drag;                      // its drag constant, 1/m (v(x) = v0 e^(-k x))
  double rpm;                       // the cyclic rate: rounds a minute inside a burst
  int32_t burst_least, burst_most;  // a burst's rounds (one: a single aimed shot)
  double pause_least, pause_most;   // s between bursts: the aim taken again
  int32_t tracer;                   // every `tracer`th round of the belt glows (0: none)
  double range;                     // m it is aimed at a man out to (its effective range)
  double reach;                     // m: the farthest a round is followed
  double dispersion;                // mil, 1 sigma: the weapon and its ammunition
  double hold;                      // mil, 1 sigma: the shooter's aim, once a burst
  double climb;                     // mil a round of the burst: the muzzle's walk
  double lead;                      // the lead's share misjudged, 1 sigma, once a burst
};

namespace weapons {

inline constexpr std::array<GunDef, kWeaponKinds> kGuns{{
    // A SERVICE ASSAULT RIFLE, 5.56 mm class: a 4 g bullet at 920 to 950 m/s, still about 650 at
    // 300 m and 510 at 500 (`drag` fitted there); 700 to 950 rounds a minute, fired in short
    // bursts or single shots; effective against a man to 300 to 500 m. A man aiming off his feet
    // at a moving target holds about 2 mil, and the muzzle climbs 3 to 4 mil a round off the
    // shoulder.
    {0.004, 930, 0.00123, 800, 1, 3, 0.6, 1.4, 0, 400, 900, 0.4, 2, 3.5, 0.25},
    // A LIGHT MACHINE GUN, 5.56 mm class: a section's belt-fed automatic weapon on the rifle's
    // round; 700 to 1000 rounds a minute in bursts of three to six; a tracer every 4th or 5th of
    // the belt; effective to about 600 m off its bipod, its cone of fire about 2 mil. Fired off a
    // sling from the pillion of a halted snowmobile (no bipod, the seat rocking on the idling
    // engine and the soft snow, a man at the end of a fast ride) the gunner holds about 5 mil,
    // and the heavier gun climbs about 1.5 mil a round.
    {0.004, 915, 0.00123, 850, 3, 6, 0.8, 1.6, 4, 600, 1000, 2, 5, 1.5, 0.3},
    // A DOOR-MOUNTED GENERAL-PURPOSE MACHINE GUN, 7.62 mm class: a 9.5 g bullet at about
    // 840 m/s, 600 at 500 m; 650 to 950 rounds a minute in bursts of six to nine; a tracer every
    // 4th or 5th; effective to about 800 m on its mount. Its cone of fire about 2.5 mil, the
    // gunner's hold on a pintle in a moving helicopter about 3.
    {0.0095, 840, 0.00067, 750, 6, 9, 0.8, 1.8, 4, 800, 1600, 2.5, 3, 0.6, 0.25},
}};

inline const GunDef& gun_def(WeaponKind k) { return kGuns[static_cast<size_t>(k)]; }

// THE BODY a round is tested against: each bone a capsule of this radius round the body's
// points (gunfire_body.hpp), m: the head a ball, the trunk a thick one, the limbs a clothed
// limb's.
inline constexpr double kHeadR = 0.11;
inline constexpr double kNeckR = 0.06;
inline constexpr double kTrunkR = 0.17;
inline constexpr double kPelvisR = 0.15;
inline constexpr double kUpperArmR = 0.06;
inline constexpr double kForearmR = 0.05;
inline constexpr double kHandR = 0.05;
inline constexpr double kThighR = 0.09;
inline constexpr double kShinR = 0.065;
inline constexpr double kFootR = 0.07;

// WARNING FIRE (the `warn` rules, and every burst where the army does not shoot to kill): put
// down in the snow at least `kWarnNearM` and at most `kWarnFarM` to a side of him, and
// `kWarnAheadM` along his way; never tested against his body.
inline constexpr double kWarnNearM = 6;
inline constexpr double kWarnFarM = 14;
inline constexpr double kWarnAheadM = 8;

// A round struck into a trunk stops in it; into the snow, under it. Rounds are followed no longer
// than `kLifeS`, and at most `kMostRounds` are in flight (the oldest dropped past it).
inline constexpr double kLifeS = 4;
inline constexpr int32_t kMostRounds = 400;
// The most guns armed at once (the reserve is never outgrown: a gun past it is not armed).
inline constexpr int32_t kMostGuns = 128;
// A round faster than sound passing him nearer than this, m, is heard as its crack: `snap`.
inline constexpr double kSnapM = 6;
inline constexpr double kSoundMs = 340;
// A round is tested against his body only within this of his middle, m.
inline constexpr double kBodyReachM = 2.5;

// THE HOLD AND THE PACE BY STARS: early in a hunt the men sent are told to bring him in. They fire
// to pin him and stop him, from far off and on the move, so the shooter's hold is widened by
// `kHoldByStars` and the pause between bursts drawn out by `kPaceByStars`, tightening as the
// stars climb. Index: the stars (0 to 5).
inline constexpr std::array<double, 6> kHoldByStars{1, 1, 1, 8, 4, 2};
inline constexpr std::array<double, 6> kPaceByStars{1, 1, 1, 4, 2.5, 1.4};

}  // namespace weapons

}  // namespace fall_line::sim
