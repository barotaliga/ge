// THE THROWN BODY: the skier off his skis after a wipeout, until he is stood up. Thirteen POINTS
// (the hips, the shoulders, the head, the knees, the feet, the elbows, the hands), each with a
// mass and a radius (`tuning.hpp`'s `ragdoll`), moved by Verlet (a point's velocity is where it is
// less where it was a step ago) under gravity, and held together by what a body is:
// - the TORSO rigid: the hips, the shoulders and the head held at every distance between them;
// - the LIMBS two bones each, hung off the hips and the shoulders, free to swing but not through
//   what a joint allows: a knee bends only forward and an elbow never ahead of the chest, neither
//   folds tighter than its two bones meet, a thigh goes only so far back, up, out or across, a
//   hand never folds into its shoulder, and no limb passes through the chest. A limit puts a
//   point right and carries no velocity, and whatever energy a pass of the joints would add to the
//   body's motion about its centre of mass is taken back off it.
//
// THE MUSCLES: every limb is driven toward a pose in the trunk's own frame by a damped spring. In
// the air it is the protective reflex (the hands thrown out toward the snow, the knees drawn
// up); once his trunk is down it gives way to the slack pose a body lies in. The muscles are
// inside the body: whatever push and twist the drive would put on the whole of it is taken back
// off, so only the snow and the trunks turn him.
//
// THE AIR slows his centre of mass on his drag area. THE SNOW meets every point on its own: a
// point below the surface (settled into powder) is put back on it, its way into the snow gone and
// its way out no faster than `kPushOut`; along it, Coulomb friction on the weight it bears and on
// its arrival, and the plough through powder, a share of every point's loss taken off the body as
// a whole; and while a trunk's weight of him lies on it the snow scrubs off his turning. A TRUNK
// meets every point too, pushed out along the line of centres. Every point's arrival on the snow
// (`impact`) and fresh closing speed on a trunk (`struck`, the way it came into the step with) this
// step are kept for the body
// (`body.hpp`).
//
// Nothing draws a random number: the body is a pure function of the moment it left the skis.
//
// ge:summary The thrown body: thirteen Verlet points, joints, muscles, the air, the snow, trunks
// ge:keywords ragdoll thrown throw_body throw_of step_ragdoll verlet joints muscles brace lie
// ge:keywords tumble points hips shoulders head knees feet elbows hands impact struck
// ge:skill skier
// ge:test tests/skier_ragdoll_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"

namespace ge::skier {

// The body's points, in the order `Ragdoll::p` keeps them (x y z each).
enum Point : int32_t {
  kHipL,
  kHipR,
  kShoulderL,
  kShoulderR,
  kHead,
  kKneeL,
  kKneeR,
  kFootL,
  kFootR,
  kElbowL,
  kElbowR,
  kHandL,
  kHandR,
  kPoints
};

const char* point_name(int32_t point);
double point_mass(int32_t point);    // kg
double point_radius(int32_t point);  // m

using PointArray = std::array<double, 3 * kPoints>;

struct Ragdoll {
  bool active = false;            // thrown and not yet stood up
  PointArray p{};                 // where each point is, m
  PointArray last{};              // ...and where it was a step ago
  double x = 0, y = 0, z = 0;     // the centre of mass, m
  double vx = 0, vy = 0, vz = 0;  // its velocity, m/s
  double t = 0;                   // s since the throw
  double tumble = 0;              // rad the spine has turned through
  bool touching = false;          // a point on the snow this step
  uint16_t planted = 0;           // the points on the snow this step, a bit each
  uint16_t leaning = 0;           // the points against a trunk this step, a bit each
  double down = -1;               // s since his trunk met the snow, -1 not yet
  double still = 0;               // s lain still on the snow
  double aloft = 0;               // s since any point touched the snow
  double ground = 0;              // s on the snow since his last flight
  // This step's blows: each point's speed into the snow as it arrived (0 if it was already
  // lying there), and its closing speed on a trunk, m/s.
  std::array<double, kPoints> impact{};
  std::array<double, kPoints> struck{};
  std::array<int32_t, kPoints> struck_by{};  // the trunk each point met hardest, -1 none
};

// HOW A BODY GOES OVER: the share of his tumble head over heels along his way (negative back),
// the share over onto a side, and the climb he leaves with, m/s.
struct Over {
  double pitch = 0;
  double side = 0;
  double up = 0;
};

// The body as it stands on the skis at (x, y, z) (the skis' base under the boots), facing
// `heading`, rolled `lean` (right side down positive), sent off at (vx, vy, vz) m/s turning at
// (wx, wy, wz) rad/s (world frame) about its centre of mass, for steps of `dt` s.
Ragdoll throw_body(double x, double y, double z, double heading, double lean, double vx, double vy,
                   double vz, double wx, double wy, double wz, double dt);

// A BODY THROWN OFF ITS SKIS: standing as `throw_body` stands it, going at (vx, vy, vz) along
// `heading`, over onto `side` (-1 left, 1 right) by `how`, turning about the vertical at
// `yaw_rate`: he keeps `kKeep` of his way and leaves with a turn at his speed over
// `kTumbleRadius`, capped.
Ragdoll throw_of(double x, double y, double z, double heading, double lean, double vx, double vy,
                 double vz, double yaw_rate, double side, const Over& how, double dt);

// ONE STEP of `dt` s of the thrown body on `level` among its trunks (`trees`, may be null).
// Allocates nothing.
void step_ragdoll(Ragdoll& b, const mountain::mapgen::Level& level, const TreeIndex* trees,
                  double dt);

// The centre of mass of a set of points.
mountain::mapgen::Vec3 centre_of(const PointArray& p);

// FNV-1a 64 over the body, folded into `h`.
uint64_t ragdoll_hash(const Ragdoll& b, uint64_t h);

}  // namespace ge::skier
