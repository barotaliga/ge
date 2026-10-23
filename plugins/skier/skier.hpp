// THE SKIER ON THE SNOW: one step of a skier and his skis, a body of `SkiSpec::mass` integrated
// semi-implicitly at the caller's timestep. This is the whole model; every number is
// `tuning.hpp`'s or the pair's (`skis.hpp`), and every ceiling a pilot plans on is `limits.hpp`'s.
//
// THE STEP, in order (`step_skier`):
// 1. THE SNOW under him: the packed share, and the sink easing toward what his speed asks
//    (`snow.hpp`). His support is the ground less the sink.
// 2. THE CONTROLS: the edge rolls toward the steer's share of the most he can stand on at this
//    speed and tuck (`limits.hpp`'s `edge_most`); the body folds toward the tuck.
// 3. THE FREE FORCES: gravity, the air's drag on the drag area the tuck leaves him (stood tall to
//    tucked), and on the snow at a crawl the push he makes himself (skating and poling, the tuck
//    held), power-limited and gone by a run's pace.
// 4. ON THE SNOW, A VELOCITY-LEVEL CONTACT: the velocity into the snow is taken out, and that
//    normal impulse is the currency every friction is paid from. Across the skis the edge's grip
//    (`side_grip`) takes out the sideways slide as far as it can (a carve when it can, a skid when
//    it cannot); along them the base's friction, the brake, the plough and the powder drag slow
//    him without ever reversing him. So a lateral load can never exceed what the snow holds,
//    and a skid costs speed because the slide it takes out is energy lost.
// 5. THE YAW: the skis turn at the carve's rate (the speed along them times the curvature the
//    edge cuts, `carve_curvature`) held to what the grip can hold, plus the pivot (the steer
//    twisting the skis under him, strong at a crawl and slight at speed), plus the skis
//    tracking their own way (a ski slid sideways is turned back toward its travel). The jump
//    loads while held and springs straight off the snow's normal when let go.
// 6. IN THE AIR: ballistic but for the drag; the steer turns him about the vertical.
// 7. THE MOVE, then THE GROUND: below the support he is put back on it, and coming down from a
//    flight that is a LANDING whose load is 1 + v_n^2 / (2 stroke g) over his legs' stroke; past
//    `kHarshG`, or landed across his way, it is a WIPEOUT. The snow falling away more than
//    `kLeaveGap` from under him is a TAKE-OFF: a kicker's lip, a roller's crest or a cliff is a
//    flight because the ground curved away faster than gravity could follow, never by a flag.
// 8. THE TRUNKS (`trees.hpp`): met faster than `kTreeSpeed`, a wipeout; slower, a glance.
//
// THERE IS NO MODE: a launch, a landing and a fall are read off the same forces every step is
// made of, and the events say so after the fact. A skier DOWN (wiped out) slides on his body
// until he is still; the game decides when to stand him up again.
//
// Sign conventions (core/math.hpp): heading 0 = +z, clockwise from above; `steer` and `edge`
// positive to the right; the skis' forward is (sin h, cos h) and their right (cos h, -sin h).
//
// ge:summary The skier on the snow: SkierState, SkierInput, the events and step_skier
// ge:keywords skier physics step step_skier carve skid edge pivot yaw tuck drive skate pole
// ge:keywords jump pop flight air takeoff landing load wipeout crash trees contact impulse
// ge:keywords friction grip sink skier-state skier-input skier-events skier_hash world
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include <cstdint>

#include "mountain/mapgen/types.hpp"
#include "skier/skis.hpp"
#include "skier/trees.hpp"

namespace ge::skier {

// What the skier is asked to do this step. Each axis is clamped where it is read.
struct SkierInput {
  double steer = 0;   // -1 full left .. 1 full right
  double tuck = 0;    // 0 tall .. 1 full tuck; at a crawl, the push
  double brake = 0;   // 0 .. 1 the skis pivoted across
  bool jump = false;  // held to load the legs, let go to spring
  template <class Io>
  friend void describe(Io& io, SkierInput& v) {
    io(v.steer);
    io(v.tuck);
    io(v.brake);
    io(v.jump);
  }
};

// `hurt`: mortally hurt on his skis (`body.hpp`), he goes down.
enum class Wipeout : uint8_t { none, tree, landing, across, hurt };

const char* wipeout_name(Wipeout w);

struct SkierState {
  double x = 0, y = 0, z = 0;     // the skis' base under the boots, m
  double vx = 0, vy = 0, vz = 0;  // m/s
  double heading = 0;             // the skis' way, rad
  double yaw_rate = 0;            // rad/s, positive turning right
  double edge = 0;                // rad, positive on the right edges
  double tuck = 0;                // 0..1 how far the body is folded
  double lean = 0;                // rad, the body's inclination into the turn, + right
  double sink = 0;                // m, how far the skis sit below the untouched surface
  double packed = 1;              // the snow's packed share under him this step
  double speed = 0;               // |v|, vertical included, m/s
  double g_lat = 0;               // the sideways load the snow put on him this step, g (+ right)
  double jump_held = 0;           // s the jump has been held on the snow
  double air_time = 0;            // s since the snow fell away (0 on it)
  double air_peak = 0;            // m, the most clearance over the snow this flight
  double down_time = 0;           // s lying still after a wipeout
  bool on_snow = true;
  bool down = false;  // wiped out: sliding on his body, the controls dead
  Wipeout cause = Wipeout::none;
  template <class Io>
  friend void describe(Io& io, SkierState& v) {
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.vx);
    io(v.vy);
    io(v.vz);
    io(v.heading);
    io(v.yaw_rate);
    io(v.edge);
    io(v.tuck);
    io(v.lean);
    io(v.sink);
    io(v.packed);
    io(v.speed);
    io(v.g_lat);
    io(v.jump_held);
    io(v.air_time);
    io(v.air_peak);
    io(v.down_time);
    io(v.on_snow);
    io(v.down);
    io(v.cause);
  }
};

// What one step did, for the game to turn into its events and for the labs to count.
struct SkierEvents {
  bool took_off = false;   // the flight just passed `kFlightMin`
  bool landed = false;     // back on the snow after a flight
  bool wiped_out = false;  // went down this step (`cause` says why)
  bool glanced = false;    // brushed a trunk and kept going
  Wipeout cause = Wipeout::none;
  double landing_g = 0;    // the landing's load, g
  double flight_s = 0;     // how long the landing's flight lasted, s
  double landing_off = 0;  // rad the skis were off his way over the snow as they landed (0..pi/2)
  int32_t tree = -1;       // the trunk met, if any
  double closing = 0;      // m/s he was closing on that trunk as he met it
};

// What the skier skis on: the map's ground, snow and trees, and the trees' index.
struct World {
  const mountain::mapgen::Level* level;
  const TreeIndex* trees;
};

// ONE STEP of `dt` seconds. Allocates nothing.
void step_skier(SkierState& s, const SkierInput& in, const World& world, const SkiSpec& spec,
                double dt, SkierEvents& events);

// PUT HIM DOWN for `cause`, as a wipeout does: sliding on his body, the controls dead.
void put_down(SkierState& s, Wipeout cause);

// Stand a skier on the snow at (x, z) facing `heading`, moving `speed` m/s along it, at rest in
// every other respect, his skis at the sink his speed asks for. `lift` m stands him that far
// over the snow instead (in the air, falling from there).
SkierState stand_skier(const World& world, double x, double z, double heading, double speed,
                       double lift = 0);

// FNV-1a 64 over every number of the state, folded into `h`: the trajectory's digest.
uint64_t skier_hash(const SkierState& s, uint64_t h);

}  // namespace ge::skier
