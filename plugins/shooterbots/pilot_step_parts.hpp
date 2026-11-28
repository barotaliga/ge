// THE PILOT'S STEP IN PARTS (pilot.hpp): what pilot.cpp's `pilot_step` shares with the files it
// is split over: the wish a command is built from (`Want`), the small reads of a link and a node
// every part makes, and the parts themselves, each defined in its own file: the route followed
// (pilot_route.cpp), the strafe's and the hop's stretch and the circle jump (pilot_hop.cpp), the
// run-up and the jump (pilot_jump.cpp), and the plan made, restarted and advanced
// (pilot_plan.cpp). The pilot's own parts (`pilot_parts`): only those and pilot.cpp include this.
//
// ge:summary The pilot's step in parts: the wish, a link's and a node's reads, the parts' calls
// ge:keywords pilot pilot-parts want facing off_segment through level on_height leaps flies lip
// ge:keywords over_floor pool_floor hops_gaps reached passed pursue approach_jump circling replan
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "ai/why.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/vec.hpp"
#include "shooter/world.hpp"
#include "shooterbots/look_ahead.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/pilot.hpp"
#include "shooterbots/pilot_movers.hpp"
#include "shooterbots/pilot_parts.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots::pilot_parts {

inline constexpr float kSlow = 50;        // under this a velocity's way is not read, units/s
inline constexpr float kLipSpeed = 200;   // `lipjump`: a hop at this speed or more jumps at a lip
inline constexpr float kUpdraft = 400;    // rising faster than this on a walk or a jump: an updraft
inline constexpr float kUpdraftHeld = 6;  // and held within this of the link's start across, units
inline constexpr float kPlatformBrake = 120;  // faster than this on a moving platform: keys let go
inline constexpr float kAtFloor = 2;  // an elevator this near a floor's stop stands at it, units

// How much of a run toward `want` the view at `now` already faces: all of it within 60 degrees
// (a hand does not ease off for the lag of its own view round a corner), none past 120, between
// by the cosine (late hands sweep wide circles round a point they run at while turning round).
inline float facing(float want, float now) {
  const double c = core::libm::cos(static_cast<double>(want - now) * (core::kPi / 180));
  return c >= 0.5 ? 1.0f : c > -0.5 ? static_cast<float>(c + 0.5) : 0.0f;
}

// Across from `p` to the segment a..b, and how far along it (0..1) the nearest point lies.
inline float off_segment(Vec3f p, Vec3f a, Vec3f b, float* along = nullptr) {
  const float ex = b.x - a.x, ey = b.y - a.y;
  const float ee = ex * ex + ey * ey;
  float f = ee > 0 ? ((p.x - a.x) * ex + (p.y - a.y) * ey) / ee : 0;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  if (along) *along = f;
  return across(p, {a.x + ex * f, a.y + ey * f, 0});
}

// A link the pursuit runs on through: a walk with no mover.
// A walk passed through on the run (a teleporter's is not: it is stood in until it sends).
inline bool through(const NavLink& l) {
  return l.kind == NavKind::walk && l.mover < 0 && !l.teleport;
}

// Two nodes within a step of one height (a stair is a level walk; a drop is not); with `slopes`
// within the run between them too (a stair of 20 a tread between nodes 32 apart).
inline bool level(const NavGraph& g, int32_t a, int32_t b, bool slopes) {
  const float dz = node(g, a).origin.z - node(g, b).origin.z;
  const float rise =
      slopes ? std::max(kStepHeight, across(node(g, a).origin, node(g, b).origin)) : kStepHeight;
  return dz <= rise && dz >= -rise;
}

// The player stands at the height of the segment a..b where it lies `f` along it (a stair's step
// or a ramp's slope between two nodes is no more than a step off the line between them).
inline bool on_height(const PilotSense& s, Vec3f a, Vec3f b, float f) {
  const float dz = s.origin.z - (a.z + (b.z - a.z) * f);
  return dz <= kStandUp + 2 && dz >= -kStepHeight - 2;
}

struct Want {
  float yaw = 0, forward = 0, side = 0;  // degrees; shares of maxspeed
  float pitch = 0;                       // the view's pitch, down positive (not on the hands)
  bool jump = false, attack = false;
  bool duck = false;  // held down (a crawlway's scripted way)
};

// A leap off the ground at its start: a jump, or a rocket jump played the same way.
inline bool leaps(NavKind k) { return k == NavKind::jump || k == NavKind::rocket; }
// A flight off the ground on the link: a leap, or a grenade jump (its start played apart).
inline bool flies(NavKind k) { return leaps(k) || k == NavKind::grenade; }

// A swimmer over a pool's floor (PilotSettings::floor_swim): in the water at most `floor_swim_up`
// over the floor's height `z` (and no more than a step under it).
inline bool over_floor(const Pilot& p, const PilotSense& s, float z) {
  const float dz = s.origin.z - z;
  return p.set.floor_swim && s.waterlevel >= 2 && !s.onground && dz <= p.set.floor_swim_up &&
         dz >= -kStepHeight - 2;
}

// A pool's floor node, as `floor_swim` swims over it: wet, not a cell's centre, and not on a mover
// (a plat in the water is boarded standing on it).
inline bool pool_floor(const NavNode& n) { return n.wet && !n.swim && n.mover < 0 && n.ride < 0; }

// A lip over lava or slime to this pilot: a `brink` link, or with `sidebrink` one whose side step
// falls in (`NavLink::brink_side`).
inline bool lip(const Pilot& p, const NavLink& l) {
  return l.brink || (p.set.brink_side && l.brink_side);
}

// Whether a hop runs on into a gap trick: a hopper at its speed, or one building it (`gapbuild`;
// the takeoff's look, `gap_takeoff`, still gives the gap up when no jump from the lip lands).
inline bool hops_gaps(const Pilot& p) { return p.set.jumper.gaps || p.set.jumper.gap_build_s >= 0; }

// THE ROUTE FOLLOWED (pilot_route.cpp).
bool reached(const Pilot& p, const PilotSense& s, int32_t n);
bool passed(const Pilot& p, const PilotSense& s, size_t k);
bool nearer_on(const Pilot& p, const PilotSense& s);
Vec3f lane_target(Pilot& p, float dt, float t, Vec3f target);
Vec3f pursue(const Pilot& p, Vec3f at, float look);
Vec3f via_aim(const Pilot& p, const PilotSense& s, Vec3f at, float look, Vec3f aim);
void via_check(Pilot& p, const PilotSense& s, float speed);
Vec3f aim_clear(const Pilot& p, const MoveWorld& world, Vec3f o, Vec3f at, Vec3f aim, Vec3f target);
WalkWay foresee(const Pilot& p, const PilotSense& s, Vec3f aim, Vec3f near, Want& w, ai::Why* why,
                bool& fall);

// THE STRAFE'S AND THE HOP'S STRETCH, THE HEADROOM AND THE CIRCLE JUMP (pilot_hop.cpp).
int32_t strafe_stretch(const Pilot& p, Vec3f at);
int32_t hop_stretch(const Pilot& p, Vec3f at);
// Why `hop_stretch` found none from `at` (for a trace): the rule that ended the stretch first.
const char* hop_stretch_why(const Pilot& p, Vec3f at);
// What the hop's stretch ends at (the link after `strafe_end_node`): "goal" (the route's end),
// "climb", "leap" (a jump next), "mover", "teleport", "lip", "side" (a brink's side), "link".
const char* hop_stretch_end(const Pilot& p);
// (`airend`) Whether a stretch's end is flown by the hop to the landing when met in the air: a
// climb, the goal or a plain link (a leap's, a mover's, a teleport's, a lip's or a side's
// approach is the walk's to make).
bool hop_end_flown(const Pilot& p);
// The node the hop follows and aims to: the stretch's end, or with its end met in the air
// (`hop_out`) two nodes past it, so the flight's aim runs on round the turn there.
int32_t hop_end_node(const Pilot& p);
float hop_follow(Pilot& p, Vec3f at, int32_t end);
Vec3f hop_aim(const Pilot& p, Vec3f at, float look, int32_t end);
bool jump_blocked(const Pilot& p, const MoveWorld& world, Vec3f o, float speed, float horizon);
bool land_walled(const MoveWorld& world, Vec3f o, Vec3f v, float dt, float horizon);
bool circling(Pilot& p, const PilotSense& s, float vh, float speed, float err, float dt, int msec,
              Want& w);

// THE RUN-UP AND THE JUMP (pilot_jump.cpp).
void missed_jump(Pilot& p, NavExtraLink j);
float flip_lead_s(const Pilot& p);
float view_when_keys_land(const Pilot& p, float want, int msec);
bool flip_ahead(const Pilot& p, const PilotSense& s, float& back, float& v);
float d_to_flip(const Pilot& p, const PilotSense& s);
bool shot_view_ready(const Pilot& p);
float route_heading(const Pilot& p, Vec3f at, float fallback);
float lip_crossing(const MoveWorld& world, Vec3f o, float vh, float way);
void strafe_across(Pilot& p, float vh, float off, Want& w, float dt);
bool lip_ahead(const MoveWorld& world, Vec3f o, Vec3f v, int msec);
bool approach_jump(Pilot& p, const PilotSense& s, Vec3f S, Vec3f T, float v, Want& w, Vec3f& target,
                   const char*& because, bool flip = false, int msec = 13, bool plain = false,
                   bool still = false);

// THE PLAN MADE, RESTARTED AND ADVANCED (pilot_plan.cpp).
bool landed_out(const Pilot& p, const PilotSense& s);
// `why` counts a copied hop start under way that it ends (PilotStats::recipe_ends).
void restart(Pilot& p, const PilotSense& s, RecipeEnd why = RecipeEnd::other);
void advance(Pilot& p, const PilotSense& s);
bool replan(Pilot& p, const PilotSense& s);

}  // namespace ge::shooterbots::pilot_parts
