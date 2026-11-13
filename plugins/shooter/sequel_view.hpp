// THE SEQUEL'S FIRST-PERSON VIEW, AS ITS SERVER WORKS IT OUT: in the sequel the view's motion is
// the rules' (the client only draws it between frames), so the server works out each player's view
// offset and kick angles once a rules frame (0.1 s) and sends them in the player state. Any game on
// the sequel's rules calls this; docs/camera.md § Shooter's Edge's view tells it beside the
// classic.
//
// - THE BOB: the horizontal speed's stride, its clock (`bob_time`) stepping 0.0625, 0.125 or 0.25
//   a frame by the speed on the ground (5..100, to 210, past it) and keeping its last step in the
//   air; under 5 units a second it stops at 0. Ducked, the clock reads 4 times as fast. The
//   stride's share is |sin(clock x pi)|: its height (share x speed x `bob_up`, at most 6 units)
//   lifts the eye; its pitch (share x speed x `bob_pitch`, x6 ducked) and roll (the same with
//   `bob_roll`, its side flipped each stride) tip the view.
// - THE RUN: the velocity along the view's forward x `run_pitch` into the pitch (forward dips the
//   view), along its right x `run_roll` into the roll (a strafe right leans right side down).
// - A BLOW (`sequel_view_blow`, summed over the frame): its count (the points it took, armour
//   included; at least 10) and its push. With a push and alive, the kick is the push x 100 / the
//   health, at least half the count, at most 50; 0.3 of it tips the roll toward the source's side
//   and the pitch away from its front (a blow from ahead tips the view up), fading linearly over
//   0.5 s. A new blow replaces the kick. `sequel_view_hard_kick` sets the same channel outright (a
//   weapon's own recoil that lasts).
// - A FALL (`sequel_view_fall`, the frame's fall severity, water already counted): 15 or more
//   dips the view: a pitch of half the severity (at most 40) nose down and the eye 0.4 of it
//   lower, fading over 0.3 s.
// - A WEAPON'S KICK (`SequelViewState::weapon_angles`, `weapon_origin`): set by the shot, held one
//   frame and cleared by `step_sequel_view`.
// - DEAD: no kick; the view lies at `kSequelDeadRoll` and looks `kSequelDeadPitch` up, toward the
//   killer (the rules give the yaw).
// - THE OFFSET is the view height, less the fall's dip, plus the bob's height and the weapon's kick
//   origin, held to -14..14 across and -22..30 up so the eye never leaves the box.
//
// One difference from the stock rules, on purpose: the bob's step in the air is each player's own
// (the stock keeps one for every player, so an airborne player took the last one processed).
//
// ge:summary The sequel's view by the server: bob, run tilt, blow and fall kicks, the view offset
// ge:keywords sequel view bob run_pitch run_roll bob_up bob_pitch bob_roll kick-angles view-offset
// ge:keywords damage-kick fall-kick weapon-kick dead-view step_sequel_view sequel_view_blow
// ge:skill shooter camera
// ge:test tests/shooter_sequel_view_test.cpp
#pragma once

#include "shooter/vec.hpp"

namespace ge::shooter {

inline constexpr float kSequelViewFrame = 0.1f;  // the rules frame, s
inline constexpr float kSequelDeadRoll = 40;     // the dead view's roll, degrees
inline constexpr float kSequelDeadPitch = -15;   // the dead view's pitch, degrees (up)

// The server settings (`run_pitch`, `run_roll`, `bob_up`, `bob_pitch`, `bob_roll`).
struct SequelViewSettings {
  float run_pitch = 0.002f, run_roll = 0.005f;
  float bob_up = 0.005f, bob_pitch = 0.002f, bob_roll = 0.002f;
};

struct SequelViewState {
  float bob_time = 0, bob_step = 0;
  float kick_pitch = 0, kick_roll = 0, kick_left = 0;  // the blow's kick and its time left, s
  float fall_value = 0, fall_left = 0;                 // the fall's, degrees and s
  int blow_count = 0, blow_push = 0;                   // this frame's blows, summed
  Vec3f blow_way;                                      // the last blow's way to its source
  Vec3f weapon_angles, weapon_origin;                  // this frame's shot's kick
};

// What the frame's player is.
struct SequelViewIn {
  Vec3f velocity;     // units/s
  Vec3f view_angles;  // pitch down, yaw, roll, degrees
  float view_height = 22;
  float health = 100;
  bool on_ground = true, ducked = false, dead = false;
};

struct SequelView {
  Vec3f offset;  // the eye over the origin, units
  Vec3f kick;    // added to the view's angles, degrees (pitch down, yaw, roll right side down)
};

// A blow taken this frame: its points (armour included), its push, its way from the player
// toward its source (any length; zero: none).
void sequel_view_blow(SequelViewState& st, int points, int push, Vec3f to_source);
// The blow's kick channel set outright (a weapon's lasting recoil), degrees, for 0.5 s.
void sequel_view_hard_kick(SequelViewState& st, float pitch, float roll);
// The frame's fall severity (the speed lost squared x 0.0001, water counted).
void sequel_view_fall(SequelViewState& st, float severity);
// The frame's view, once a rules frame at its end; the frame's blows and shot are spent.
SequelView step_sequel_view(SequelViewState& st, const SequelViewSettings& set,
                            const SequelViewIn& in);

}  // namespace ge::shooter
