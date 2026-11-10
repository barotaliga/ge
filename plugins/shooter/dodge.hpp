// THE DODGE: a player in a fight does not run a straight line under the enemy's crosshair. He
// strafes across the line between them, left and right, turning back after a fraction of a second
// at moments the enemy cannot read, and now and then runs straight for a beat. How often he turns,
// how hard he presses the side key and how often he forgets to dodge at all is his skill (the
// `dodge` trait, traits.hpp, read through `dodge_skill`).
//
// A side step that would walk him off a ledge, into a wall or into a liquid is not taken: the
// other side is, or none (`dodge_clear` sweeps the player's box a step to that side and looks for
// the floor under it). The caller works the side out under the view, which a fight holds on the
// enemy, so the side key is across the line to him.
//
// Deterministic: the turns come off the dodge's own `core::Rng`.
//
// ge:summary The dodge in a fight: side steps across the enemy's line, turned at unreadable times
// ge:keywords dodge jink strafe-dodge side-step fight movement dodge_skill dodge_step dodge_clear
// ge:skill shooter bots
// ge:test tests/shooter_dodge_test.cpp
#pragma once

#include <cstdint>

#include "core/prng.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

struct Server;

// How a player dodges: each side step held `hold_s` on average (drawn from half to one and a half
// times it), the side key pressed `press` of the way (0..1), and the chance a stretch is run
// straight instead of across (`straight`).
struct DodgeSkill {
  double hold_s = 0.4;
  float press = 1;
  double straight = 0.1;
};

// The skill a `dodge` trait value (1..10) gives: a novice turns slowly, presses half the key and
// often runs straight at his enemy; an expert turns two or three times a second and seldom forgets.
DodgeSkill dodge_skill(uint8_t trait);

struct Dodge {
  core::Rng rng{1};
  double until = -1e9;  // the stretch under way ends then
  int32_t dir = 0;      // -1 left, +1 right, 0 straight
};

void dodge_init(Dodge& d, uint32_t seed);

// The side the dodge presses at server time `t` (-1 left, +1 right, 0 none), each side clear or not
// (`dodge_clear`): a stretch ends on its time or when its side stops being clear, and the next is
// drawn: across to the other side, or straight. `d.dir` keeps the answer.
int dodge_step(Dodge& d, const DodgeSkill& k, double t, bool left_clear, bool right_clear);

// Whether a side step `dir` (-1 left, +1 right) of the view's yaw `yaw_deg` from `origin` is safe
// for player entity `me`: the box goes most of the way, the floor is there under it (no drop
// deeper than a step and a little) and no liquid is.
bool dodge_clear(const Server& sv, Vec3f origin, float yaw_deg, int dir, int me);

}  // namespace ge::shooter
