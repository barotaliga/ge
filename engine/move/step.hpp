// ONE STEP OF A MOVING CHARACTER: what the player asked for (`MoveCmd`: the stick, the view, the
// buttons), the knobs (`MoveSpec`), the world (a trace) and `dt` in; the body moved, and what
// happened on the way (`MoveEvents`: a jump, a landing and how hard, a step climbed, a slide) out.
// It is a pure function of its inputs and the state, with no clock, no random draw and nothing
// allocated, so a run replays to the bit from its commands, a server and a predicting client step
// the same body alike, and a bot's command moves it the way a player's does.
//
// THE ORDER of a step (docs/movement.md § The step): the crouch (the hull changes before it
// moves), a ladder ahead (on its rungs the stick climbs and nothing else below applies), the
// jump, then by mode: WALK (friction and acceleration on
// the ground, the air's rules off it, gravity by halves either side of the move, the slide along
// whatever is hit, a step up, the snap down), FLY (three-dimensional wish, no gravity, the same
// collision) or NOCLIP (no collision). Then the ground again, so the events see the landing.
//
// THE STEP'S LENGTH is the game's: a fixed 120 Hz step, a client's usercmd's milliseconds. The
// rise and fall of a jump do not depend on it (gravity is applied by halves either side of the
// move, so a jump's apex is v^2 / 2g at any rate); the gain of strafe jumping does, as it always
// has, because it is gained a step at a time (the `move-strafe` lab charts it by rate).
//
// Swimming is not a mode here: a game hands a body in the water to its swimmer
// (docs/movement.md § Water), and back.
//
// ge:summary One step of a moving character: crouch, ground, jump, friction, accelerate, slide
// ge:keywords move movement step step_move character controller pmove walk fly noclip
// ge:keywords friction accelerate air-accelerate gravity jump land slide clip-velocity step-up
// ge:keywords ground-snap crouch edge-friction coyote-time jump-buffer air-control events
// ge:keywords landing impact wish-direction usercmd ladder climb moving-ground platform bounce
// ge:keywords carry_along gravity-lead land-clip-speed
// ge:skill move
// ge:test tests/move_step_test.cpp
#pragma once

#include <cstdint>

#include "move/spec.hpp"
#include "move/world.hpp"

namespace ge::move {

enum class Mode : uint8_t {
  walk,    // on the ground and in the air under gravity
  fly,     // free flight with collision (a spectator)
  noclip,  // free flight through everything
};

// What the player asked for this step.
struct MoveCmd {
  double forward = 0;  // -1..1: along the view's heading
  double side = 0;     // -1..1: to the right
  double up = 0;       // -1..1: flying only
  double yaw = 0;      // rad: the view's heading (0 = +z, clockwise from above)
  double pitch = 0;    // rad: the view's pitch, up positive (flying only)
  bool jump = false;
  bool crouch = false;
  bool walk = false;    // the walk button: `walk_scale`
  bool sprint = false;  // `sprint_scale`
};

struct MoveState {
  Vec3 pos{0, 0, 0};  // the feet: the bottom centre of the hull
  Vec3 vel{0, 0, 0};  // L/s
  Mode mode = Mode::walk;
  bool grounded = false;
  Vec3 ground_normal{0, 1, 0};
  bool ducked = false;       // the hull is the crouched one
  double crouch = 0;         // 0 standing .. 1 crouched: the eyes' travel (`crouch_time`)
  bool sliding = false;      // in a crouch slide
  bool jump_held = false;    // the jump button was down last step
  bool jump_rising = false;  // rising from a jump the button still holds (`jump_cut` pending)
  double since_ground = 0;   // s since the feet left the ground (0 on it)
  double buffer_left = 0;    // s an early press is still waiting
  double slide_ready = 0;    // s until a slide's boost is given again
  int32_t air_jumps_left = 0;
  // The ground's own motion (world.hpp's `solid` and `surface_vel`): standing, `vel` is relative
  // to it; leaving it, the body takes its velocity with it.
  uint32_t ground_solid = 0;
  Vec3 ground_vel{0, 0, 0};
  bool on_ladder = false;  // on a ladder's rungs: no gravity, the stick climbs
  double t = 0;            // s: the time stepped, for a lab's trace
};

enum MoveEvent : uint16_t {
  kJumped = 1u << 0,       // left the ground by a jump (`jump_speed` given)
  kAirJumped = 1u << 1,    // a jump in the air
  kLanded = 1u << 2,       // came down onto the ground (`impact` says how hard)
  kLeftGround = 1u << 3,   // walked or slid off without a jump
  kSteppedUp = 1u << 4,    // climbed a step (`stepped` says how high)
  kSteppedDown = 1u << 5,  // followed a drop down (`stepped` negative)
  kSlideStart = 1u << 6,
  kSlideEnd = 1u << 7,
  kDucked = 1u << 8,      // the crouched hull taken
  kStood = 1u << 9,       // the standing hull back
  kBlocked = 1u << 10,    // ran into something (`blocked` says how much speed it took)
  kBounced = 1u << 11,    // came down on a bouncing surface and was thrown up (`impact`)
  kLadderOn = 1u << 12,   // took hold of a ladder
  kLadderOff = 1u << 13,  // let go of it (stepped off its top or bottom, jumped off, turned away)
};

struct MoveEvents {
  uint16_t bits = 0;
  double impact = 0;   // L/s: the speed into the ground on landing
  double stepped = 0;  // L: the height climbed (up positive) this step
  double blocked = 0;  // L/s: the speed a wall took this step

  bool has(MoveEvent e) const { return (bits & e) != 0; }
};

// `MoveState::since_ground` for a body that left the ground by a jump or a push: no coyote jump.
inline constexpr double kSinceNever = 1e300;

// The hull standing or crouched, feet at the origin.
Box hull(const MoveSpec& spec, bool ducked);
// The eyes above the feet for a crouch share (the camera's view height).
double eye_height(const MoveSpec& spec, double crouch);

// A body at rest at `pos`, put down on walkable ground within a step above or below it (left at
// `pos`, in the air, when there is none).
MoveState stand_at(const MoveSpec& spec, const World& world, const Vec3& pos);

// One step; returns what happened.
MoveEvents step_move(const MoveSpec& spec, const World& world, MoveState& s, const MoveCmd& cmd,
                     double dt);

// The body carried along its own velocity for `dt` and nothing else (no friction, gravity, wish
// or ground probe): the slide along whatever it hits and, with `step_up`, the lifted try that
// climbs a step. What a game moves a body with while something else decides its velocity (a
// swimmer, docs/movement.md § Water); `kBlocked` and `kSteppedUp` are reported as a step does.
MoveEvents carry_along(const MoveSpec& spec, const World& world, MoveState& s, double dt,
                       bool step_up);

// THE ACCELERATION RULE, shared by the ground, the air and the labs that chart it: `vel` gains
// along the unit `wish_dir` up to `accel * wish_speed * dt`, but only until its speed along
// `wish_dir` reaches `cap` (the wish speed itself on the ground; the air's cap in the air).
void accelerate(Vec3& vel, const Vec3& wish_dir, double wish_speed, double cap, double accel,
                double dt);

// The wish for a command: its direction (unit, on the plan unless flying) and speed (L/s), the
// scales of walk, sprint, crouch, backwards and sideways applied. A stick is held to length 1.
struct Wish {
  Vec3 dir{0, 0, 0};
  double speed = 0;
};
Wish wish_of(const MoveSpec& spec, const MoveState& s, const MoveCmd& cmd);

}  // namespace ge::move
