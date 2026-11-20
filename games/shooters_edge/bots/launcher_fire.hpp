// THE LAUNCHERS FIRED OUT OF SIGHT, AS PEOPLE FIRE THEM (docs/goals/M18r.md § 18b): what a
// fighter fires with nobody in its sights, and the ledger of its finger by range.
//
// - THE ROCKET AFTER HIM (`afterfire`): the one aimed at gone from sight within `kAfter_s`, the
//   aim holding where he is carried, the launcher up and its refire over: fired once the
//   crosshair's burst lands within `kAfterReach` of where he is believed, the burst no nearer the
//   shooter than `kBlastNear` (`blastsafe`'s rule) and no mate by it; `kAfterShots` a loss.
// - THE ROCKET AT A SOUND (`earfire`): an enemy heard within `kEar_s` and not in sight, the
//   launcher up and ready: fired once the crosshair's burst lands within `kEarReach` of where he
//   was heard with a clear line to it (the view swiped there by `listen`); one a sound.
// - THE LOB ROUND A CORNER (`earlob`): the lob carried and loaded, an enemy heard or lost from
//   sight within `kLob_s`, `kLobNear` to `kLobFar` off, with no straight line to him (a rocket
//   cannot reach him): the throw's pitch off the arc (`lob_pitch`), the grenade FLOWN through the
//   world from the shooter's feet, and thrown only when its first impact lands within
//   `kLobReach` of him, no nearer the shooter than the blast's reach nor by a mate. The lob taken
//   up for it (the launcher's choice held off meanwhile), the view turned there on the hand, the
//   throw flown again from where it stands when it fires; one a sound or a loss. People fire 56 %
//   of their grenades with no enemy in view and 20 % just after losing one (se-people `gl.*`).
//
// Deterministic: pure functions of the fighter's beliefs, the map and the server's state.
//
// ge:summary The launchers fired out of sight: a rocket after one lost, at a sound, a lob round a
// ge:summary corner; the finger's ledger by range
// ge:keywords afterfire earfire earlob lob grenade round-corner heard sound lost-from-sight
// ge:keywords blind-fire finger-ledger launcher rocket arc lob_pitch
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kAfter_s = 1.2;  // (`afterfire`) a rocket after one lost this long ago at most, s
constexpr float kAfterReach = 100;  // its burst this near where he is believed, units
constexpr int32_t kAfterShots = 2;  // this many a loss
constexpr double kEar_s = 1.0;      // (`earfire`) a rocket at a sound this fresh at most, s
constexpr float kEarReach = 150;    // its burst this near where it was, units
constexpr double kLob_s = 1.5;      // (`earlob`) a lob at a sound or a loss this fresh at most, s
constexpr float kLobNear = 200, kLobFar = 700;  // thrown at one this far off, units
constexpr float kLobReach = 120;    // its first impact this near him, units (the blast's 120)
constexpr double kLobHold_s = 1.5;  // the throw given up when not made in this long, s
constexpr float kLobOn = 3;         // thrown with the view this near the throw's, degrees

// (`afterfire`) After the command: the rocket at one just lost, into `cmd`.
void after_fire(Fighter& f, const ge::shooter::BotView& s, int me, const Stats& st,
                const BotWeapon* held, double t, ge::shooter::MoveCmd& cmd);
// (`earfire`) After the command: the rocket at a sound, into `cmd`.
void ear_fire(Fighter& f, const ge::shooter::BotView& s, int me, const Stats& st,
              const BotWeapon* held, double t, ge::shooter::MoveCmd& cmd);
// (`earlob`) Before the command: a throw chosen or kept (the lob asked for, the view's look set).
void lob_look(Fighter& f, const ge::shooter::BotView& s, int me, const Stats& st, double t);
// (`earlob`) After the command: the throw made once the lob is up, ready and the view on it.
void lob_fire(Fighter& f, const ge::shooter::BotView& s, int me, const Stats& st, double t,
              ge::shooter::MoveCmd& cmd);
// 18b's ledger by range: with the one aimed at in sight (`c`), the finger pressing (or between
// its clicks), held for the burst, or another weapon up, in the band of his range (kFingerBands).
void finger_band(Fighter& f, const ge::shooter::BotView& s, int me, const ge::shooter::Contact* c,
                 FingerWhy why);

}  // namespace shooters_edge::bots
