// A FIGHTER'S KEYBOARD (feature `keys`): the movement keys and the weapon keys its command carries
// come off a hand on a keyboard (ai/keys.hpp), not off the wish.
//
// - THE KEY SET: the finished command's wish (forward and side under the view) is turned into the
//   one of the eight key sets nearest its direction (W, W+D, D, ... the classic client's full
//   speed), or none under `kPressMin` of a run's speed in the air (`kCreep` on the ground); the set
//   held is kept until the wish lies `kHold` degrees past its edge, so a wish on the line between
//   two sets does not flicker. STEERED (`keysteer`): instead the two sets either side of the
//   wish, the further one tapped in for its share of the time (a tally of the way wished less the
//   way pressed, a set past `kSteer`), as a person holds W and taps D to bear right: the way
//   pressed over a few taps is the wish's, so a turn short of a set's 45 degrees is made. A CURVE
//   (`curvekeys`): a pilot's curve on the ground (a circle jump's, a run to a gap's lip) is its
//   keys, W and the side key, held whole: read off the hands' slewed side axis, turned under a
//   view a few degrees behind, it was tapped (the speed swung 320-375 instead of building). The
//   jump is the jump key (held from a jump asked for until it lands, `jumpkey`: a pilot lets go
//   between two asks, shorter than a finger's press), a weapon's impulse a TAP of its key: the
//   impulse goes out on the command whose key goes down, not on the command it was asked for.
// - FEATHERING: on the ground a wish short of a run (a run at a jump at the speed it was proven
//   at, slowing into a point) is held by tapping: the keys pressed while the speed along the wish
//   is under the speed wished less `kBand` (a few units crept by a tap), let go once it is over it
//   plus `kBand` or the body's whole speed is over it plus `kSlide` (sliding off the wish, the
//   speed along it stays low while the body runs on past the point it slows into); the finger's own
//   times make the rhythm. A wish of `kFull` or more, or in the air, is simply held.
// - THE BINDINGS by the `moves` trait (people's ways): a novice on WASD with the weapons on the
//   number row (5 to 8 a reach off the hand), an average player with 1-4 there and the launchers
//   and the arc on Q, E, R and F, an expert jumping on the mouse's right button with the
//   scattergun's second barrel on the space bar.
// - THE HAND's times by the same trait (an expert quicker in each).
// - THE AXES built as the classic client builds them from keys: 400 a key each way, both of a pair
//   held cancelling; the swim's up axis passed through.
//
// ge:summary A fighter's keyboard: the wish as a key set, weapon impulses as key taps, by the hand
// ge:keywords keyboard keys key-set wish octant bindings weapon-key tap impulse fighter_keyboard
// ge:keywords press_keys curvekeys curve-keys key-chatter
// ge:skill bots
// ge:test tests/shooters_edge_footwork_test.cpp
#pragma once

#include "shooter/pmove.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

// The hand's bindings and times by `moves` (traits.hpp), seeded off `seed`.
void key_hand_init(KeyHand& k, uint8_t moves, uint32_t seed);
void fighter_keyboard_init(Fighter& f, uint8_t moves, uint32_t seed);

// The finished `cmd` pressed through the hand: its axes, jump and impulse what the keys held.
// `velocity` the player's own, `onground` whether he stands. `curve`: a pilot's curve on the
// ground meant as keys (shooter/pilot.hpp `Pilot::curve_keys`: forward and the side key, -1 left,
// 1 right, 0 forward alone), held whole, not read off the axes (`curvekeys`); `kNoCurveKeys` none.
// `brinkkeys` (with `near`: the world, the nav with its harm, where the player stands): near lava
// or slime a key whose step (64 units under the view) ends over it is not wanted, and a run that
// carries the box over it within a quarter second (80 units at most) wants the keys against it.
struct KeysNear {
  const sh::MoveWorld* world = nullptr;
  const sh::NavGraph* g = nullptr;
  sh::Vec3f origin;
};
void press_keys(KeyHand& k, const sh::Features& features, sh::MoveCmd& cmd, sh::Vec3f velocity,
                bool onground, int8_t curve = sh::kNoCurveKeys, const KeysNear* near = nullptr);
// The fighter's: its pilot's curve held when the command is still the pilot's (a hop on its
// heading, the brink guard not in it: `brinked`).
void fighter_keyboard(Fighter& f, sh::MoveCmd& cmd, sh::Vec3f velocity, bool onground,
                      bool brinked = false, sh::Vec3f origin = {});

}  // namespace shooters_edge::bots
