// THE ARMY ON FOOT'S NUMBERS (army.hpp moves the men, army_troop.hpp carries each over the snow):
// the soldiers the wanted level's roster and ground waves send after a skier, on skis. Every
// number carries its unit.
//
// ON SKIS a soldier is a winter infantryman on long touring skis with a heavy pack: not a skier
// on the snow's physics (dozens of them would cost a step what the field does) but a point on the
// heightfield with a heading and a speed. He skis well, not fast (a disciplined descent, checked to
// a pace he can stop from), skates the flats, climbs short rises in a herringbone and long steep
// ones across them on skins, zig-zagging.
//
// UP THE LIFTS: from the star whose roster asks for them (`WantedRoster::lifted`), squads are put
// down below the load zone of the lift whose top is nearest him and well above him, ride it up and
// ski down at him from its top.
//
// THE GROUND WAVES: each wave's men on foot (`WaveMix::foot`) are put down out of his sight a
// way down the run nearest him, in a column, and climb it at him on skins.
//
// Where the army does not shoot to kill they run him down and the first man to reach him while
// he is slow or down arrests him; where it does, they close to their rifles' range and hold there
// with him in sight (their fire is gunfire's, a later row).
//
// ge:summary The army on foot's numbers: the ski pace and climb, the lift squads, the waves' column
// ge:keywords army soldier troop rules ski pace climb herringbone traverse skate lift-squad
// ge:keywords ground-wave column dismount stand-off arrest stuck detour engage sent-home
// ge:skill sim
// ge:test tests/fall_line_army_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fall_line/sim/wanted_rules.hpp"

namespace fall_line::sim::army {

// ON SKIS: the pace he holds, m/s (about 60 km/h at the most), checked at `kBrake` m/s^2; the
// tightest turn he skis, m; the snow's drag, m/s^2, and the air's, 1/m; his poles' push where the
// hill does not carry him, m/s^2; and his working pace: `kSkate` m/s on the flat, falling by
// `kClimb` m/s for every unit of slope against him to `kLeast` m/s, and a rise steeper than
// `kTraverse` (a slope) climbed across it, turned the other way every `kZigS` s.
inline constexpr double kPace = 16;
inline constexpr double kBrake = 4;
inline constexpr double kTurnM = 7;
inline constexpr double kSnowDrag = 0.45;
inline constexpr double kAirDrag = 0.0035;
inline constexpr double kPush = 1.2;
inline constexpr double kSkate = 3.4;
inline constexpr double kClimb = 7;
inline constexpr double kLeast = 0.7;
inline constexpr double kTraverse = 0.45;
inline constexpr double kZigS = 7;
inline constexpr double kGravity = 9.81;

// KEEPING CLEAR: of trunks, m past their radius, looked for `kAheadM` along his way; of each
// other, m; of the map's edge, m.
inline constexpr double kSolidM = 1.2;
inline constexpr double kAheadM = 9;
inline constexpr double kApartM = 2.5;
// How far a man on the snow moves in one step at most, m: his pace over a step, and a trunk he
// skis into pushing him out of it. The step's grid of the soldiers (army_troop.hpp) is built with
// this much slack past `kApartM`, so its cells hold every man he keeps apart from.
inline constexpr double kNearSlackM = 4;
inline constexpr double kEdgeM = 8;

// NOT GETTING STUCK: his progress checked every `kWindowS`; under `kLeastM` in one and he steps
// off sideways for `kDetourS`, the other side each time he is caught again.
inline constexpr double kWindowS = 4;
inline constexpr double kLeastM = 1.5;
inline constexpr double kDetourS = 3;

// His eyes over his feet, m (the sight test's pursuer height).
inline constexpr double kEyeM = 1.7;

// UP THE LIFTS: a squad of `kSquad`, put down `kLaneM` below the load zone `kRespondS` after the
// star asked for them, another every `kEveryS` while the roster is short; the lift taken is the
// one whose top is nearest him and at least `kAboveM` over him (else the highest over him).
inline constexpr int32_t kSquad = 4;
inline constexpr double kRespondS = 20;
inline constexpr double kEveryS = 30;
inline constexpr double kAboveM = 30;
inline constexpr double kLaneM = 18;
inline constexpr double kQueueM = 1.2;
// How many ride one carrier, by `LiftKind` (gondola, chair, drag).
inline constexpr std::array<int32_t, 3> kSeats{8, 4, 2};

// THE GROUND WAVES' COLUMN: put down `kDismountM` m down the way from him (the run nearest him
// and every run it flows into, to its foot and on past it), and further, `kSampleM` at a time,
// while he could see the place, to `kHiddenM` at the most; never nearer him than `kAwayM` m as
// the crow flies. Up to `kAbreast` men across, `kFileM` apart (no wider than the run, less
// `kMarginM` each side), ranks `kRankM` apart down the way. A man climbs to the next point of the
// way once within `kReachM` of it, and leaves the column for the fight within `kEngageM` of where
// he was last seen, or seeing him within `kStandOffM`. At most `kWaveMost` men of the waves out
// at once, by stars; one further than `kBehindM` from where he was last seen while the hunt runs,
// or than `kHomeM` from him once it is over, is sent home.
inline constexpr double kDismountM = 380;
inline constexpr double kAwayM = 300;
inline constexpr double kHiddenM = 720;
inline constexpr double kSampleM = 8;
inline constexpr int32_t kAbreast = 5;
inline constexpr double kFileM = 3.2;
inline constexpr double kMarginM = 2;
inline constexpr double kRankM = 4.5;
inline constexpr double kReachM = 6;
inline constexpr double kEngageM = 220;
inline constexpr double kBehindM = 1100;
inline constexpr double kHomeM = 450;
inline constexpr std::array<int32_t, wanted::kMaxStars + 1> kWaveMost{0, 0, 16, 32, 56, 80};

// THE CHASE: where they shoot to kill, a man with him in sight stops within `kStandOffM` and
// holds there, or within `kHoldBandM` further once he is caught behind the men already holding;
// where they do not, they run him down. Unseen, they make for where he was last seen and stand
// there once within `kSearchedM`.
inline constexpr double kStandOffM = 260;
inline constexpr double kHoldBandM = 40;
inline constexpr double kSearchedM = 15;

// THE ARREST: a man within `wanted::kArrestReachM` takes him if he is slower than this, m/s, or
// down.
inline constexpr double kArrestSpeed = 3;

// The most soldiers out at once (the roster's lift squads and the waves' men), and the first
// pursuer id a soldier is (the wanted level's sight test keys them by it).
inline constexpr size_t kMostSoldiers = 96;
inline constexpr int32_t kPursuerBase = 2000;
// The army's call, which lift a squad goes up (`lift_for`), is recorded as this agent; each
// soldier's own decisions as `kPursuerBase` + his id.
inline constexpr int32_t kCallAgent = kPursuerBase - 1;

}  // namespace fall_line::sim::army
