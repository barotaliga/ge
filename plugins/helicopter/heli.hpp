// THE HELICOPTER IN THE AIR AND ON THE SNOW: a light helicopter flown by hand on four controls,
// with nothing of an autopilot between the hands and the rotor, integrated at the caller's step.
// Every number is `spec.hpp`'s.
//
// THE STEP, in order (`step_heli`):
// 1. THE ROTOR spooled toward full rpm or down, as the caller asks (`spool_to`).
// 2. THE AIR it meets: its way against the caller's wind and, near the map's edge, the edge's
//    wind blowing it back in (`spec::edge`). The airframe's drag on its front, side and plan.
// 3. THE COLLECTIVE: its share of what the rotor can give (`thrust_most`: momentum theory's power
//    through the induced flow, the translational lift, the parasite power, the blades' own limit,
//    ground effect), at the rotor's speed squared, answered in `flight::kLag` s, along the disc.
// 4. THE DISC tilted by the cyclic against its damping and its flapback (on the snow with the
//    weight on the skids it lies as the snow does); THE FUSELAGE swung under it as a damped
//    pendulum, hung off the centre line by a rider on a skid; THE PEDALS against the tail's and
//    the fin's damping, the main rotor's torque and the fin's weathervane.
// 5. THE MOVE, then THE SKIDS ON THE SNOW: set down, held, judged (too fast down or along, stood
//    off the snow's lean, or snow too steep to sit on is a crash); lifted off.
// 6. THE STRIKE: the disc's rim and the airframe's extremes against the snow, the disc against
//    every crown it sweeps. A crash leaves the machine down where it struck, its rotor stopped;
//    `step_heli` does nothing to it after.
//
// Nothing here draws a random number or allocates; `heli_hash` is the trajectory's digest.
//
// ge:summary The helicopter: HeliState, HeliControls, HeliEvents, step_heli, thrust_most
// ge:keywords helicopter heli step_heli flight rotor disc cyclic collective pedals torque
// ge:keywords fuselage skids landing crash strike tree ground-effect thrust_most stand_heli
// ge:keywords heli_hash heli-state edge air-bounds
// ge:skill helicopter
// ge:test tests/helicopter_scenarios_test.cpp
#pragma once

#include <cstdint>

#include "core/quat.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"

namespace ge::helicopter {

// THE CONTROLS as a pilot works them, each clamped where it is read:
// - `collective` the lever, 0 (down: no lift) to 1 (all the pitch the power turns): the thrust's
//   share of what the rotor can give. A lever, not a spring: it stays where it is left.
// - `pitch` the cyclic fore and aft, -1..1, forward positive: the disc tilted forward.
// - `roll` the cyclic side to side, -1..1, right positive.
// - `pedal` the pedals, -1..1, the nose turned right (clockwise) positive.
struct HeliControls {
  double collective = 0;
  double pitch = 0;
  double roll = 0;
  double pedal = 0;
};

// The rotor disc the cyclic flies and the thrust points along: its pitch and roll (the
// airframe's conventions) and their rates.
struct HeliDisc {
  double pitch = 0, roll = 0;
  double pitch_rate = 0, roll_rate = 0;
};

enum class HeliCrash : uint8_t { none, landing, rotor, airframe, tree };

const char* heli_crash_name(HeliCrash c);

struct HeliState {
  double x = 0, y = 0, z = 0;                          // the skid datum, world, m
  double vx = 0, vy = 0, vz = 0;                       // m/s
  double heading = 0;                                  // rad, 0 = +z, clockwise from above
  double pitch = 0, roll = 0;                          // the fuselage, hung under its rotor, rad
  double yaw_rate = 0, pitch_rate = 0, roll_rate = 0;  // rad/s
  HeliDisc disc;
  HeliControls controls;                // as flown this step
  double spool = 0;                     // the rotor's share of the full rpm, 0..1
  double rotor = 0;                     // the main rotor's turn, rad, wrapped
  double tail_rotor = 0;                // the tail rotor's, rad, wrapped
  double thrust = 0;                    // N, this step
  double collective = 0;                // the thrust's share of what the rotor could give
  bool grounded = true;                 // the skids on the snow
  double agl = 0;                       // the hub's height over the snow, m
  HeliCrash crashed = HeliCrash::none;  // down for good once set
};

// A rider (or any load) carried on the machine: his mass, kg, and his place across the airframe,
// body x, m (on a skid: half the track and a little more). The fuselage hangs off the centre line
// toward him.
struct HeliLoad {
  double mass = 0;
  double x = 0;
};

// What the machine flies over: the map's ground and trees, the trees' index, and the air moving
// at its height, m/s, world x and z.
struct HeliWorld {
  const mountain::mapgen::Level* level;
  const skier::TreeIndex* trees;
  double air_x = 0;
  double air_z = 0;
};

// What one step did, for the game to turn into its events and the labs to count.
struct HeliEvents {
  bool lifted = false;                // the skids just left the snow
  bool landed = false;                // the skids just met it and held
  double sink = 0;                    // ...how fast it came down onto them, m/s
  HeliCrash crash = HeliCrash::none;  // crashed this step
  double speed = 0;                   // ...how fast it was going, m/s
  double fall = 0;           // ...how fast it was coming down when the snow stopped it, m/s
  core::Vec3 stop{0, 0, 0};  // ...that stop in the airframe's own frame, m/s
};

// The airframe's orientation and the disc's, body to world.
core::Quat heli_quat(const HeliState& h);
core::Quat disc_quat(const HeliState& h);
// A point of the airframe, body frame (off the skid datum), in the world.
core::Vec3 heli_point(const HeliState& h, core::Vec3 p);

// WHAT THE ROTOR CAN GIVE at full rpm, N: the thrust momentum theory says the power buys going
// `along` m/s through the air and climbing `vc` m/s (P = T (v_c + v_i), Glauert's induced
// velocity, the parasite power taken off first), never more than the blades give at their full
// pitch in the air meeting them along the disc's thrust axis (`through`, m/s, climbing positive),
// times ground effect `over` m of hub height over the snow.
double thrust_most(double along, double vc, double over, double through);

// ONE STEP of `dt` s on `in`, carrying `load`, its rotor spooled toward `spool_to` (1 running,
// 0 shut down). A crashed machine only falls to the snow as a wreck. Allocates nothing.
void step_heli(HeliState& h, const HeliControls& in, const HeliLoad& load, double spool_to,
               const HeliWorld& world, double dt, HeliEvents& events);

// A machine stood on its skids at (x, z) facing `heading`, its rotor still.
HeliState stand_heli(const mountain::mapgen::Level& level, double x, double z, double heading);

// THE EDGE'S HAND: near the map's edge in the air, controls `c` blended toward the bot's flying
// the machine back toward the middle at its height, by how deep into the band it is.
HeliControls heli_turned_back(const HeliState& h, const HeliWorld& world, const HeliLoad& load,
                              const HeliControls& c);

// The machine's mass with its load, kg.
double heli_mass(const HeliLoad& load);

// FNV-1a 64 over every number of the state, folded into `h`: the trajectory's digest.
uint64_t heli_hash(const HeliState& s, uint64_t h);

}  // namespace ge::helicopter
