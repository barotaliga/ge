// THROWN DEBRIS THAT BOUNCES AND SETTLES: small rigid pieces a game throws into a level made of
// triangles (receivers.hpp): a gun's spent cases, chips off a wall. Each falls, bounces off the
// surfaces it meets, slides and rolls to a stop, lies there for its kind's `life_s` and fades over
// the last `fade_s`; the pool is made once (`make_debris_pool`), a throw takes a free slot or the
// oldest piece's, so a frame allocates nothing. Presentation's own: a game draws the pieces and
// hears their landings, and nothing it does feeds back into a simulation.
//
// - A PIECE is a short cylinder whose long axis is its body +x (`turn` carries body axes into the
//   level's: y up). It touches a surface at its `radius_m` round its centre: its motion each step
//   is a ray (receivers.hpp's `raycast`) reaching that far past where it goes.
// - A BOUNCE keeps `restitution` of the speed into the surface and loses `friction` of the speed
//   along it; the spin keeps `spin_keep` and picks up a tumble of its own (the piece's stream,
//   seeded by its serial), so two cases thrown alike land apart. A bounce at or over `heard_mps`
//   is a landing (`DebrisLanding`): the game's sound, by the surface's triangle.
// - ON A FLOOR (a surface whose up is at least `kFloorUp`), a touch too slow to bounce lays the
//   piece down along the floor (its long axis in the floor's plane, its roll about it kept) and it
//   slides, slowed by `slide_mps2`, rolling as a cylinder does; under `rest_mps` it lies still.
// - A piece still in the air after `fly_s` (fallen out of the level, or none to land on) is let go.
//
// Deterministic: exact arithmetic, the turn through core::libm, the tumble from core::Rng.
//
// ge:summary Thrown debris that falls, bounces, slides, rests and fades, in a fixed pool
// ge:keywords detail debris spent-case shell casing bounce restitution friction roll rest fade
// ge:keywords throw_debris step_debris debris-pool landing make_debris_pool debris_shown
// ge:skill detail
// ge:test tests/detail_debris_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/prng.hpp"
#include "core/quat.hpp"
#include "detail/receivers.hpp"

namespace ge::detail {

// A surface whose up (its normal's y) is at least this is a floor a piece can lie on.
inline constexpr double kFloorUp = 0.7;

// What a kind of piece is like.
struct DebrisShape {
  double radius_m = 0.01;     // how far round its centre it touches a surface
  double restitution = 0.35;  // the share of the speed into a surface kept away from it
  double friction = 0.3;      // the share of the speed along a surface lost at a bounce
  double spin_keep = 0.5;     // the share of the spin kept at a bounce
  double tumble = 40;         // the spin a bounce adds, rad/s for each m/s of the blow
  double slide_mps2 = 3;      // how fast a sliding piece slows on a floor
  double heard_mps = 0.5;     // a bounce this hard or harder is a landing
  double rest_mps = 0.08;     // on a floor slower than this it lies still
  double life_s = 10;         // how long it lies once still
  double fade_s = 1;          // the last part of that over which it fades
  double fly_s = 6;           // let go if still in the air after this long
};

struct Debris {
  Vec3 at{0, 0, 0}, vel{0, 0, 0};  // m, m/s
  core::Quat turn{0, 0, 0, 1};     // body to level
  Vec3 spin{0, 0, 0};              // body rad/s
  double born = 0;                 // when thrown, s
  double still = -1;               // when it came to rest, s (-1: moving)
  uint32_t serial = 0;             // 0: a free slot
  core::Rng rng{0};                // its tumbles' stream
  uint16_t kind = 0;               // its shape's index
  uint8_t bounces = 0;
  bool lying = false;  // on a floor, laid along it (sliding or still)
};

struct DebrisPool {
  std::vector<Debris> pieces;
  std::vector<DebrisShape> shapes;  // by kind
  uint32_t serial = 0;              // the last one handed out
};

// A pool of `capacity` pieces of the kinds `shapes` (a piece's `kind` indexes it).
DebrisPool make_debris_pool(uint32_t capacity, std::span<const DebrisShape> shapes);

struct DebrisThrow {
  Vec3 at{0, 0, 0}, vel{0, 0, 0};
  core::Quat turn{0, 0, 0, 1};
  Vec3 spin{0, 0, 0};  // body rad/s
  uint16_t kind = 0;   // a kind the pool has (one it lacks: the first)
};

// The piece thrown at `now_s` into a free slot, else the oldest piece's: its slot.
int throw_debris(DebrisPool& pool, const DebrisThrow& t, double now_s);

// A bounce hard enough to hear.
struct DebrisLanding {
  Vec3 at{0, 0, 0}, normal{0, 1, 0};
  uint32_t tri = 0;  // the receivers' mesh's triangle it struck (its tag: the game's surface)
  double speed = 0;  // m/s into the surface
  uint16_t kind = 0;
  uint32_t serial = 0;
  uint8_t bounce = 0;  // its piece's bounce count, from 1
};

// Every piece moved from `now_s` - `dt` to `now_s` (in steps of at most `step_s`) under gravity
// `gravity_mps2` (down the level's -y) against `r` (null: nothing to strike), the landings
// appended to `landings` (null: none), the pieces past their life let go.
void step_debris(DebrisPool& pool, const Receivers* r, ReceiverScratch& scratch, double now_s,
                 double dt, std::vector<DebrisLanding>* landings, double gravity_mps2 = 9.81,
                 double step_s = 1.0 / 120);

// How much of a piece shows at `now_s`: 1 until the last `fade_s` of its life, then down to 0.
double debris_shown(const DebrisPool& pool, const Debris& d, double now_s);

// The live pieces.
size_t debris_count(const DebrisPool& pool);

// The rotation whose body axes are `x`, `y` and their cross (each unit, `x` square to `y`).
core::Quat turn_of_axes(const Vec3& x, const Vec3& y);

}  // namespace ge::detail
