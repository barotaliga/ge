// THE SNOWMOBILE ON THE SNOW: a deep-snow mountain sled and its rider as one rigid body,
// integrated semi-implicitly at the caller's step (velocity first, then position; the body's
// rates first, then the orientation). Every number is `spec.hpp`'s; the drive is `drive.hpp`'s.
//
// THE STEP, in order (`step_sled`):
// 1. THE CONTROLS through their lags: the throttle and the brake eased, the skis swung toward
//    the bars' lock (`sled_lock_at`), the rider's weight following the bars across (on the
//    groomer as far as the bend asks, in powder where the bars send it) and the lean fore and
//    aft.
// 2. THE SUSPENSION (`body.cpp`): every probe a spring-damper along the body's down axis against
//    the snow's SUPPORT (the surface less the sink the speed allows, at the belt's and the sled
//    skis' own scale), with a bump stop past its travel; the snow answers along its normal.
// 3. THE GRIP at each probe, in the snow's tangent plane: the belt driving along its length off
//    its slip and holding sideways, one budget spent along the combined slip (a belt spinning
//    under full throttle has little left to hold the tail, and the tail walks out); in powder
//    the paddles shear the snow's cohesion and a belt rolled onto its edge CARVES toward its low
//    side; the skis holding along their steered line; every probe's rolling resistance, plough
//    and powder drag against its way, and only the belt's front cutting fresh snow.
// 4. THE BELT AND THE ENGINE (`drive.hpp`), THE RIDER'S WEIGHT off-centre, THE AIR.
// 5. ON THE SNOW: the roll held toward the lean the bars ask (in loose snow the soft side giving
//    and the rider balancing against it; past THE TIP the righting fades and it goes over), and
//    the arcade's hand on the yaw. IN THE AIR: the lean pitches it, the belt's gyro lifts the
//    nose on the throttle and drops it on the brake, the rider levels the roll, and the nose is
//    set for the landing (`assist`), let go the moment he leans or brakes.
// 6. EULER'S EQUATIONS, the velocities, then THE HULL: unsprung points meeting the snow as
//    impulses when the springs run out (how it lies on its side and rolls over), then the move.
// 7. THE TRUNKS (a capsule down its length) and THE MAP'S EDGE, then WHAT THROWS THE RIDER: a
//    trunk met hard, rolled or looped past `crash::kOver`, a landing too hard or set down off its
//    belt, the way stopped dead. Nothing else on snow throws him.
//
// Nothing here draws a random number or allocates; `sled_hash` is the trajectory's digest.
//
// ge:summary The snowmobile on the snow: SledState, SledControls, SledEvents, step_sled
// ge:keywords snowmobile sled step_sled rigid-body probes suspension grip belt carve roll tip
// ge:keywords air landing hull trunks crash thrown stand_sled sled_hash sled-state controls
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/quat.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"
#include "snowmobile/spec.hpp"

namespace ge::snowmobile {

// THE CONTROLS as the rider works them: the thumb throttle 0..1, the brake lever 0..1, the bars
// -1..1 (right positive), and his weight fore and aft -1..1 (+1 back: the nose lifted, the
// climber's stance; -1 over the bars). Each is clamped where it is read.
struct SledControls {
  double throttle = 0;
  double brake = 0;
  double steer = 0;
  double lean = 0;
  template <class Io>
  friend void describe(Io& io, SledControls& v) {
    io(v.throttle);
    io(v.brake);
    io(v.steer);
    io(v.lean);
  }
};

// One probe on the snow as a renderer stamps it: on the surface under the probe (world, m), sunk
// how deep, carrying what, and whether it is touching.
struct SledContact {
  double x = 0, y = 0, z = 0;
  double sink = 0;
  double load = 0;
  bool touching = false;
  template <class Io>
  friend void describe(Io& io, SledContact& v) {
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.sink);
    io(v.load);
    io(v.touching);
  }
};

struct SledState {
  double x = 0, y = 0, z = 0;     // the CoG of the machine and its rider, world, m
  double vx = 0, vy = 0, vz = 0;  // m/s
  core::Quat q{0, 0, 0, 1};       // body to world (core/quat.hpp's conventions)
  double wx = 0, wy = 0, wz = 0;  // the body's rates, rad/s
  // Derived each step: heading, pitch (nose up +) and roll (right side down +), rad; |v|; the
  // way (the speed along the nose, flat), m/s.
  double heading = 0, pitch = 0, roll = 0;
  double speed = 0, way = 0;
  SledControls controls;   // as they stand after their lags
  double ski_angle = 0;    // the skis' turn off the nose, rad, right positive
  double rider_right = 0;  // the rider's weight moved across, m
  double rider_aft = 0;    // ...and back, m
  double roll_aim = 0;     // the roll the rider is taking it to off the snow's plane, rad
  double rpm = 0;
  double tread_speed = 0;  // the belt over its rails, m/s
  bool running = false;    // the engine
  double slip = 0;         // the belt's mean slip over the snow, m/s
  double packed = 0;       // the packed share under it, weighted by load
  std::array<SledContact, spec::kProbes> contacts{};
  std::array<double, spec::kProbes> sinks{};  // each probe's settled sink, m
  std::array<double, spec::kProbes> comps{};  // each probe's last compression, m
  std::array<double, 2> ski_comp{};           // the skis' compression, m (left, right)
  double tread_comp = 0;                      // the belt's mean compression, m
  bool airborne = false;
  double air_time = 0;  // s
  double over_for = 0;  // s it has stood past rolled or looped
  double hit_wait = 0;  // s before another trunk is reported
  template <class Io>
  friend void describe(Io& io, SledState& v) {
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.vx);
    io(v.vy);
    io(v.vz);
    io(v.q);
    io(v.wx);
    io(v.wy);
    io(v.wz);
    io(v.heading);
    io(v.pitch);
    io(v.roll);
    io(v.speed);
    io(v.way);
    io(v.controls);
    io(v.ski_angle);
    io(v.rider_right);
    io(v.rider_aft);
    io(v.roll_aim);
    io(v.rpm);
    io(v.tread_speed);
    io(v.running);
    io(v.slip);
    io(v.packed);
    io(v.contacts);
    io(v.sinks);
    io(v.comps);
    io(v.ski_comp);
    io(v.tread_comp);
    io(v.airborne);
    io(v.air_time);
    io(v.over_for);
    io(v.hit_wait);
  }
};

enum class Thrown : uint8_t { none, tree, rolled, landing, wall };

const char* thrown_name(Thrown t);

// What one step did, for the game to turn into its events and the labs to count.
struct SledEvents {
  bool took_off = false;         // the flight just passed `air::kCounts`
  bool landed = false;           // back on the snow after a flight that counted
  double flight_s = 0;           // how long the landing's flight lasted, s
  double into = 0;               // the landing's speed into the snow along its normal, m/s
  double off = 0;                // ...and how far past what the rider rides out (1 throws him)
  double hit = 0;                // a trunk met this step, m/s along the blow (0 none worth saying)
  int32_t tree = -1;             // ...which
  Thrown thrown = Thrown::none;  // the rider put off this step (ridden machines only)
};

// What the machine rides on: the map's ground, snow and trees, the trees' index, the air moving
// over it (m/s, world x and z), and how much the machine helps (`assist`, 0..1: the nose set for
// a landing).
struct SledWorld {
  const mountain::mapgen::Level* level;
  const skier::TreeIndex* trees;
  double air_x = 0;
  double air_z = 0;
  double assist = 1;
};

// The full ski lock at `speed` m/s, rad: full at a standstill, halved by `steer::kFadeSpeed`.
double sled_lock_at(double speed);

// ONE STEP of `dt` s for a machine carrying a rider of `rider` kg (0 with nobody on it).
// `ridden` false is a machine left to itself: its controls are let go with the parking brake on,
// and nothing throws anyone. Allocates nothing.
void step_sled(SledState& s, const SledControls& in, double rider, bool ridden,
               const SledWorld& world, double dt, SledEvents& events);

// A machine stood on its belt at (x, z) facing `heading`, pitched and rolled to the snow, at
// rest at its rest sag, its engine off.
SledState stand_sled(const mountain::mapgen::Level& level, double x, double z, double heading);

// Where the rider's feet stand: the boards' middle, his weight carried as far across and back as
// he has moved it, world m (the skier's origin is his boot soles).
core::Vec3 boards_at(const SledState& s);

// FNV-1a 64 over every number of the state, folded into `h`: the trajectory's digest.
uint64_t sled_hash(const SledState& s, uint64_t h);

}  // namespace ge::snowmobile
