// THE ARMY'S SNOWMOBILES' NUMBERS (army_sled.hpp moves the machines, army_drive.hpp reads the
// snow ahead of them). Every number carries its unit.
//
// THE PATTERN is the way winter troops use a light over-snow machine: a utility snowmobile two up,
// a driver and a rifleman behind him, that moves a pair of men fast over open snow and stops to
// let them fight. A machine on the move is a poor gun platform (the seat bucks under every bump),
// so the man behind fires from a SHORT HALT: the driver turns the machine side-on to the target,
// brakes to a stop, the gunner fires a few bursts off the pillion and they drive on before the
// target can answer. A skier who keeps moving, keeps something between him and them and makes
// them stop often is hard to hit from one. (The bursts themselves are gunfire's, a later row: the
// halt is built here.)
//
// THE MACHINE is the free ride's own (`plugins/snowmobile`): the same body, belt and engine,
// stepped on the same snow and the same trunks, carrying two men with their kit and the gun, and
// ridden by the same hands the bot rides the player's with (`sled_pilot`).
//
// ge:summary The army's snowmobiles' numbers: the call, the drive, the halt, the stand-off, stuck
// ge:keywords army snowmobile sled rules call respond stagger crew cruise woods short-halt
// ge:keywords stand-off dismount right-it stuck dig home terrain-read climb cliff fan
// ge:skill sim
// ge:test tests/fall_line_army_sled_test.cpp
#pragma once

#include <cstdint>

namespace fall_line::sim::army_sled {

// THE CALL: seconds from the roster asking for a machine to it leaving the village (the crew
// mounting up), and the seconds between one and the next. They come from beside the free ride's
// snowmobile spot (`sled_spot`), each `kApartM` to the side of the one before.
inline constexpr double kRespondS = 20;
inline constexpr double kStaggerS = 8;
inline constexpr double kApartM = 5;

// THE CREW: two men with their kit, kg each, and the gun, its ammunition and the crew's packs on
// the rack, kg: the load the machine carries.
inline constexpr double kManKg = 105;
inline constexpr double kLoadKg = 40;
inline constexpr double kRiderKg = 2 * kManKg + kLoadKg;

// THE DRIVE (`sled_pilot`'s hands, to an aim the terrain reading picks): the pace over open snow,
// m/s (some 60 km/h: a laden utility machine on a mountain, not a racer), among trees, and the
// pace it closes on him at from within `kSlowM` of where it stands off him; how far ahead it reads
// the snow, m, every `kStepM`; the headings it weighs either side of the way to him, `kFan` a side
// across `kSpread` rad; how often it chooses again, s; and how far ahead of him it aims, s of his
// way.
inline constexpr double kCruise = 17;
inline constexpr double kWoods = 8;
inline constexpr double kClose = 6;
inline constexpr double kSlowM = 60;
inline constexpr double kLookM = 36;
inline constexpr double kStepM = 6;
inline constexpr int32_t kFan = 9;
inline constexpr double kSpread = 1.6;
inline constexpr double kDecideS = 0.1;
inline constexpr double kLeadS = 1;
// The aim handed to the pilot, m out along the way chosen, so its hands hold the pace asked.
inline constexpr double kAimOutM = 90;

// READING THE TERRAIN (`army_drive.hpp`): a rise steeper than `kRise` (a slope) is more than a
// laden machine climbs; a drop steeper than `kDrop` it will not ride off; a wind tunnel's tube
// within `kTunnelM`, a trunk within `kTrunkM` of its line and the map's edge within `kEdgeM` are
// no way at all or cost. A heading costs `kTurnCost` for every radian off the way to him,
// `kBlockedCost` for every metre short of `kLookM` it is stopped, `kTreeCost` a trunk near its
// line, and `kKeepCost` for every radian off the heading it holds.
inline constexpr double kRise = 0.55;
inline constexpr double kDrop = 0.9;
inline constexpr double kTunnelM = 3;
inline constexpr double kTrunkM = 3;
inline constexpr double kEdgeM = 40;
inline constexpr double kTurnCost = 6;
inline constexpr double kBlockedCost = 1.2;
inline constexpr double kTreeCost = 4;
inline constexpr double kKeepCost = 1.5;
// UP THE RUNS: further than `kDirectM` from him, and him within `kWayNearM` of one of the ways
// the waves climb (the ski area's runs, read downhill to the valley), a machine below him on it
// climbs the run, the mountain's open, groomed snow, aiming `kAlongM` up it, to the arc nearest
// him; nearer, or past it, it goes straight at him.
inline constexpr double kDirectM = 220;
inline constexpr double kWayNearM = 200;
inline constexpr double kAlongM = 30;

// BLOCKED: where it is going (up a run, or him) stands higher over it than `kRise` of the way
// there, and the way straight there is stopped within this, m, by a face it cannot climb. The
// machine waits there with its crew on it (he is out of its reach until he comes down) rather than
// casting along the face's foot.
inline constexpr double kBlockedM = 2 * kStepM;

// THE SHORT HALT: with him in sight and nearer than `kHaltRangeM` (but not nearer than
// `kHaltLeastM`), it stops once it may: braked to under `kStill` m/s, the gunner settles
// `kSettleS` and has him for up to `kHoldS`, then they drive on for `kMoveS` to `kMoveS` +
// `kMoveSpreadS` before stopping again. Out of sight `kLostS`, the halt is broken off. It stops
// with him `kSide` rad off its nose to the right: the gunner's side, his line clear of the
// driver's head.
inline constexpr double kHaltRangeM = 380;
inline constexpr double kHaltLeastM = 20;
inline constexpr double kSide = 1.0;
inline constexpr double kStill = 1.5;
inline constexpr double kSettleS = 1.5;
inline constexpr double kHoldS = 8;
inline constexpr double kMoveS = 6;
inline constexpr double kMoveSpreadS = 5;
inline constexpr double kLostS = 1.5;
// THE GUN on the pillion (gunfire.hpp): the gunner's light machine gun at his shoulder, the
// muzzle `kMuzzleRightM` right, `kMuzzleUpM` up and `kMuzzleAheadM` forward of the machine's
// centre of gravity; once settled and the machine still, he fires up to `kHaltBursts` bursts. His
// gun is `kGunBase` + the machine's id.
inline constexpr double kMuzzleRightM = 0.32;
inline constexpr double kMuzzleUpM = 0.85;
inline constexpr double kMuzzleAheadM = 0.3;
inline constexpr int32_t kHaltBursts = 3;
inline constexpr int32_t kGunBase = 20000;
// The first halt waits this long after it leaves the village, s.
inline constexpr double kFirstHaltS = 4;

// HOW CLOSE IT COMES: where the army shoots to kill it keeps `kStandLethalM` off him (the gun
// does the work); where it does not it drives up to `kStandArrestM` and the crew get off to take
// him in, `kDismountM` from him once he is slow or down and the machine slower than
// `kDismountSpeed` m/s.
inline constexpr double kStandLethalM = 110;
inline constexpr double kStandArrestM = 7;
inline constexpr double kDismountM = 14;
inline constexpr double kDismountSpeed = 3;
// A man stepping off is put down this far out to the side of his seat, m: the driver to the left,
// the gunner to the right.
inline constexpr double kStepOffM = 1.3;
// The crew's eyes over the machine's centre of gravity, m.
inline constexpr double kEyeM = 1.0;
// How long a sighting is kept, s.
inline constexpr double kSawS = 0.5;

// OVER ON ITS SIDE: the crew right it after `kRightS` on its side, and it rests `kRestS` before
// driving on.
inline constexpr double kRightS = 1.5;
inline constexpr double kRestS = 3;

// NOT GETTING STUCK: its progress checked every `kWindowS`; under `kLeastM` in one it swings off
// to a side for `kDetourS` (`kDetourRad` off the way to him); a second window and the crew dig it
// out, stood `kDigM` back on open snow facing him.
inline constexpr double kWindowS = 4;
inline constexpr double kLeastM = 2;
inline constexpr double kDetourS = 3;
inline constexpr double kDetourRad = 1.3;
inline constexpr double kDigM = 6;
// The room kept round a spot it is stood on, m.
inline constexpr double kRoomM = 3;

// HOME: the hunt over, it drives back to the village and is gone within `kGoneM` of its spot, or
// after `kHomeS`; it closes on its spot at `kClose` within `kHomeSlowM`. Machines left standing
// empty are kept up to `kMostEmpty`, the oldest let go first, and an empty one is gone once the
// hunt is over and he is further than `kEmptyGoneM` from it.
inline constexpr double kHomeS = 150;
inline constexpr double kGoneM = 25;
inline constexpr double kHomeSlowM = 60;
inline constexpr int32_t kMostEmpty = 3;
inline constexpr double kEmptyGoneM = 300;

// The most machines out at once (the roster's and the waves' together, with the empty ones), kept
// in a reserve made at create so a step never allocates; and the most a wave keeps out on top of
// the roster's.
inline constexpr int32_t kMostSleds = 16;
inline constexpr int32_t kWaveSleds = 5;

// THE CREWS IN THE PURSUERS: `kPursuerBase` + the machine's id (below the soldiers' own base).
inline constexpr int32_t kPursuerBase = 1000;

}  // namespace fall_line::sim::army_sled
