// THE WANTED LEVEL'S NUMBERS (wanted.hpp has the rules that read them): how hard the army hunts
// a skier who broke the law, star by star. Every number carries its unit.
//
// Five stars, each sending more and heavier pursuers; the stars SOLID while a pursuer has him in
// sight and FLASHING while they only search where he was last seen; dropped all at once once he
// has stayed out of every pursuer's sight long enough, longer the more stars he carries. The
// roster climbs the way a military response is escalated: a helicopter to find and shadow him,
// then a fire team, a squad, pursuit over the snow, and at the top several aircraft at once; the
// rules of fire step up from presence to warning shots to aimed fire.
//
// The roster and the ground waves are DATA here: the pursuers that fly, ski and drive them are
// later rows of docs/goals.md (M15c to M15e) and read these tables.
//
// ge:summary The wanted level's numbers: the roster, the ground waves, the climb, sight, offences
// ge:keywords wanted wanted-level stars heat roster ground-waves climb search lose sight offence
// ge:keywords pursuer-kind wanted-cause fire-rule arrest cover sight-line rules
// ge:skill sim
// ge:test tests/fall_line_wanted_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace fall_line::sim {

// Who is hunting him: what a pursuer is, which sets how far he sees.
enum class PursuerKind : uint8_t {
  helicopter,  // a utility helicopter's crew, scanning from aloft
  gunner,      // a door gunner, from the same seat
  soldier,     // a soldier on foot or on skis
  snowmobile,  // a snowmobile's crew, low on the snow
  vehicle,     // a ground vehicle's (a carrier's, a truck's, a tank's), from its hatch
  balloon,     // a balloon's basket, hanging high and still
  paramotor,   // a soldier flying a paramotor
};
inline constexpr size_t kPursuerKinds = 7;

// Whether a pursuer of `kind` looks from the snow: the woods hide the quarry from him, and a
// crown over the quarry's head does not (it hides him only from aloft).
inline constexpr bool looks_from_snow(PursuerKind kind) {
  return kind == PursuerKind::soldier || kind == PursuerKind::snowmobile ||
         kind == PursuerKind::vehicle;
}

// WHAT HE DID: each offence a row of `wanted::kOffences`, in this order.
enum class WantedCause : uint8_t {
  stole_ambulance,    // flew off in the landed air ambulance
  stole_police,       // flew off in the police helicopter
  downed_helicopter,  // brought an army helicopter down
  hit_soldier,        // ran a soldier down
  stole_snowmobile,   // rode off on a patrol's or the army's snowmobile
  stole_vehicle,      // drove off in an army carrier or truck
  stole_army_heli,    // flew off in an army helicopter
  stole_tank,         // drove off in a tank
  dev,                // raised by hand (a lab, the developer's page)
};
inline constexpr size_t kWantedCauses = 9;

// The rules of fire: shadow him with no shots, warning fire aimed off him, or aimed fire.
enum class FireRule : uint8_t { hold, warn, aim };

// What one star sends: the most of each the army has out at once.
struct WantedRoster {
  int32_t helicopters;  // utility helicopters
  int32_t gunners;      // door gunners across them (two doors a machine)
  int32_t soldiers;     // soldiers on skis, set down by the helicopters
  int32_t snowmobiles;  // two up: a driver and a gunner behind him
  int32_t lifted;       // soldiers who ride the ski area's lifts up and ski down after him
  int32_t balloons;     // balloons drifting in on the wind
  int32_t rocketeers;   // soldiers in the baskets with shoulder-launched rockets
  int32_t paramotors;   // riflemen flying paramotors beside the balloons
  FireRule fire;
};

// One GROUND WAVE up the mountain from the valley: snowmobiles (two up), tracked over-snow
// carriers (a section in the back), trucks (on the roads as far as they go), tanks, and soldiers
// climbing on foot.
struct WaveMix {
  int32_t snowmobiles;
  int32_t tracked;
  int32_t trucks;
  int32_t tanks;
  int32_t foot;
};

// The same, as the fractional counts a wave's growth is stated in.
struct WaveRate {
  double snowmobiles;
  double tracked;
  double trucks;
  double tanks;
  double foot;
};

// A star's GROUND WAVES: the first `first_s` after he reached the star, then one every
// `every_s`, each `base` plus `grow` for every wave before it (rounded down), up to `cap` a kind.
struct GroundWaves {
  bool sends;  // false: this star sends none
  double first_s;
  double every_s;
  WaveRate base;
  WaveRate grow;
  WaveMix cap;
};

// What an offence is worth: `add` stars of heat on top of what he has, and `floor` the stars it
// raises him to at the least.
struct Offence {
  double add;
  int32_t floor;
};

namespace wanted {

inline constexpr int32_t kMaxStars = 5;
inline constexpr double kNever = std::numeric_limits<double>::infinity();

// THE ROSTER by stars (index 0 is nobody). A utility helicopter seats a section of about a
// dozen; a fire team is four, a squad two of them; at three stars soldiers ride the lifts up to
// ski down at him; at four a second helicopter comes and a balloon drifts in with rocketeers in
// its basket and riflemen on paramotors beside it; at five a third helicopter, a second balloon
// and more of both.
inline constexpr std::array<WantedRoster, kMaxStars + 1> kRoster{{
    {0, 0, 0, 0, 0, 0, 0, 0, FireRule::hold},
    {1, 1, 0, 0, 0, 0, 0, 0, FireRule::hold},
    {1, 2, 4, 0, 0, 0, 0, 0, FireRule::warn},
    {1, 2, 8, 1, 4, 0, 0, 0, FireRule::aim},
    {2, 4, 8, 2, 8, 1, 2, 2, FireRule::aim},
    {3, 6, 12, 3, 12, 2, 4, 4, FireRule::aim},
}};

// THE GROUND WAVES by stars (none at 0 and 1: one helicopter shadows him first). A carrier seats
// about a section, a truck a squad or two, and the men on foot come in platoons that grow with
// every wave: the longer he lasts, the more of the mountain is soldiers.
inline constexpr std::array<GroundWaves, kMaxStars + 1> kWaves{{
    {false, 0, 0, {}, {}, {}},
    {false, 0, 0, {}, {}, {}},
    {true, 90, 75, {1, 0, 0, 0, 4}, {0.5, 0, 0, 0, 2}, {2, 0, 1, 0, 12}},
    {true, 60, 60, {2, 1, 1, 0, 8}, {0.5, 0.5, 0.5, 0, 4}, {4, 2, 2, 0, 24}},
    {true, 45, 50, {2, 2, 2, 0, 16}, {0.5, 0.5, 0.5, 0.25, 6}, {5, 4, 3, 1, 40}},
    {true, 30, 40, {3, 2, 2, 1, 24}, {0.5, 0.5, 0.5, 0.5, 8}, {6, 6, 4, 3, 64}},
}};

// THE LONGER HE SURVIVES, THE WORSE IT GETS: seconds hunted IN SIGHT to climb from each star to
// the next (index = the stars he has; the last never climbs), one to five in under four minutes
// seen. Out of sight the heat climbs at `kSearchClimb` of that pace: evading is survival too,
// only slower to anger.
inline constexpr std::array<double, kMaxStars + 1> kClimbS{kNever, 40, 50, 60, 75, kNever};
inline constexpr double kSearchClimb = 0.5;

// LOSING THEM: seconds out of every pursuer's sight before the stars are dropped, by stars.
inline constexpr std::array<double, kMaxStars + 1> kLoseS{0, 12, 18, 25, 32, 40};

// THE FIX an offence gives them: for this long after one they know where he is, s, whatever
// they see (the report of the crime).
inline constexpr double kFixS = 10;

// HOW FAR EACH KIND SEES a man on the snow in clear day, m, by `PursuerKind`.
inline constexpr std::array<double, kPursuerKinds> kSightM{900, 900, 350, 400, 500, 1200, 900};

// Nearer than this, m, he is heard and seen whatever is between.
inline constexpr double kCloseM = 25;

// How often the pursuers look, s: a sight line is marched over the ground at this cadence, not
// every step.
inline constexpr double kLookEveryS = 0.25;

// THE SIGHT LINE: sampled every `kLineStepM` over the ground and blocked where the ground rises
// within `kLineClearM` of it; it ends at his chest, `kChestM` over the point he stands on. From
// the snow, a line through the crowns of `kWoods` trees is blocked, read every `kWoodStepM`
// (a crown a cone from the trunk's foot, the tree's own crown radius across at its base).
inline constexpr double kLineStepM = 6;
inline constexpr double kLineClearM = 0.4;
inline constexpr double kChestM = 1.1;
inline constexpr int32_t kWoods = 3;
inline constexpr double kWoodStepM = 3;

// UNDER COVER: a man slower than `kCoverSpeed` m/s stood under a crown (within `kCoverCrown` of
// its reach at his head, `kCoverHeadM` over his feet) is not seen from the AIR further than
// `kCoverNearM` away: the snow-loaded boughs hide him from a crew looking down.
inline constexpr double kCoverSpeed = 2.5;
inline constexpr double kCoverHeadM = 1.6;
inline constexpr double kCoverCrown = 1.2;
inline constexpr double kCoverNearM = 60;

// THE ARREST: where the army does not shoot to kill, a soldier who reaches him within this
// takes him in, m (`arrest_quarry`), which ends the run.
inline constexpr double kArrestReachM = 2;

// WHAT EACH OFFENCE IS WORTH, by `WantedCause`. The army's own machines are each worse than the
// last: a carrier or a truck, its helicopter off the snow, a tank.
inline constexpr std::array<Offence, kWantedCauses> kOffences{{
    {1, 1},    // stole_ambulance
    {1, 2},    // stole_police
    {1, 3},    // downed_helicopter
    {0.5, 1},  // hit_soldier
    {0.5, 2},  // stole_snowmobile
    {1, 3},    // stole_vehicle
    {1, 4},    // stole_army_heli
    {1.5, 4},  // stole_tank
    {1, 1},    // dev
}};

// The most pursuers the hunt tracks at once (the roster's top row and the ground waves' caps).
inline constexpr size_t kMaxPursuers = 192;

}  // namespace wanted

}  // namespace fall_line::sim
