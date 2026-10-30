// THE BOT'S AIM: the view's yaw and pitch turned onto what its senses found, through hands of a
// SKILL (ai/hands.hpp), and the pilot's movement kept going the way it meant while the view looks
// elsewhere.
//
// - WHERE TO LOOK (`aim_angles`): the yaw and pitch (pitch down positive, the protocol's) from the
//   eye to a point. The point is the contact's middle where it was last seen, carried along its
//   velocity for the time since and the profile's `lead_s` (a novice aims where it is, an expert
//   a little ahead of where it goes).
// - THE HANDS: yaw (degrees, unwrapped) and pitch (degrees, held to the view's -70..80) pass
//   through the skill's hands. Acquiring a contact, the hands start from where the view already is
//   (`ai::Hands::reset`), so the reaction is the hands' own delay; let go, the view turns back to
//   the pilot's way and hands back to it once within `kRelease` degrees.
// - THE MOVE UNDER THE VIEW (`turn_axes`): the classic move takes forward and side relative to the
//   view, so the pilot's axes are turned by the difference between its yaw and the view's: the
//   same wish in the world, pressed from a different facing (on the protocol's grid of 4).
//
// THE PROFILES (`aim_profile`): `perfect` passes its hands untouched (the measuring stick);
// `novice`, `average` and `expert` react in 0.30, 0.22 and 0.17 s, wander 3, 1.5 and 0.6 degrees,
// turn at most 360, 600 and 1000 degrees a second and lead 0, 0.1 and 0.2 s. They are stated, not
// yet fitted to people's logs (docs/goals.md M16b's corpus replaces them).
//
// ge:summary The bot's aim: yaw and pitch at a contact through skill hands; axes under the view
// ge:keywords aim aiming skill profile novice average expert perfect lead reaction yaw pitch
// ge:keywords hands turn_axes aim_angles view strafe-under-view
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ai/hands.hpp"
#include "shooter/usercmd.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

constexpr float kRelease = 3;  // degrees: a view this near the pilot's hands back to it

enum class AimSkill : uint8_t { perfect, novice, average, expert };
const char* aim_skill_name(AimSkill s);

struct AimProfile {
  std::vector<ai::AxisHands> hands;  // yaw, pitch
  float lead_s = 0;                  // aims this far ahead along the contact's velocity
  float hold_s = 1;                  // keeps aiming at a contact lost this long
};
AimProfile aim_profile(AimSkill s);

// The yaw and pitch (degrees; pitch down positive) from `eye` to `at`.
void aim_angles(Vec3f eye, Vec3f at, float& yaw, float& pitch);

// The forward and side axes `forward`, `side` meant from a view at `from_yaw`, pressed from a view
// at `to_yaw` so the wish in the world is the same, on the protocol's grid.
void turn_axes(int16_t& forward, int16_t& side, float from_yaw, float to_yaw);

}  // namespace ge::shooter
