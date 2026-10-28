// A WORLD OF CONVEX SOLIDS a mover can be tested in: boxes, ramps (wedges rising along an axis)
// and stairs, each a brush of planes, swept against the mover's box by the plane-expansion method
// (each plane pushed out by the box's reach along its normal, the segment clipped against the
// pushed-out solid). It answers `World::trace` (world.hpp) for the `move-*` labs, the tests, and
// a game's prototype before its own collision exists.
//
// EXACT FOR THE SOLIDS IT MAKES: a box's planes are axial, and a ramp's slanted face has its
// normal in a coordinate plane with the wedge's axial bounds as bevel planes, which makes the
// pushed-out solid the true sum of the wedge and the box. A ramp is built only that way.
//
// Built once before a run (it allocates while adding); a trace allocates nothing and visits every
// brush (a lab's world has tens).
//
// ge:summary A test world of boxes, ramps and stairs that answers the mover's swept-box trace
// ge:keywords move movement box-world brush brushes planes convex ramp wedge stairs trace sweep
// ge:keywords minkowski plane-expansion bevel skin add_box add_ramp add_stairs outline
// ge:skill move
// ge:test tests/move_box_world_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "move/world.hpp"

namespace ge::move {

struct Plane {
  Vec3 n;    // outward unit normal
  double d;  // a point p is inside the solid's half-space when dot(n, p) <= d
};

// Which way a ramp climbs: along +x, -x, +z or -z.
enum class Rise : uint8_t { px, nx, pz, nz };

// What a brush's faces are, reported on every hit of it (world.hpp's surface fields).
struct Surface {
  Vec3 vel{0, 0, 0};  // L/s: how it is moving (set by whoever moves it, `move_brush`)
  uint8_t flags = 0;  // SurfaceFlag bits
  double bounce = 0;  // 0..1
};

struct Brush {
  uint32_t first_plane = 0, planes = 0;
  uint32_t first_corner = 0, corners = 0;  // the solid's corners, for drawing it
  Vec3 lo{0, 0, 0}, hi{0, 0, 0};           // its bounds
  Surface surface;
};

struct BoxWorld {
  double skin = 0.03125;  // L: how far a trace stops short of a surface
  std::vector<Brush> brushes;
  std::vector<Plane> planes;
  std::vector<Vec3> corners;
};

// A solid box between two corners. Returns its index in `brushes`; a hit on it reports
// `solid` = index + 1.
size_t add_box(BoxWorld& w, const Vec3& lo, const Vec3& hi, const Surface& surface = {});
// A wedge on the footprint `lo`..`hi` (x, z), its top rising from `lo.y` at the start of `rise`
// to `hi.y` at its end; under it solid down to `lo.y`.
void add_ramp(BoxWorld& w, const Vec3& lo, const Vec3& hi, Rise rise);
// `count` steps of `rise` up and `tread` deep climbing along +z from `start` (the first step's
// near bottom-left corner), `width` across in +x, each step a box down to `start.y`.
void add_stairs(BoxWorld& w, const Vec3& start, int count, double rise, double tread, double width);

// Brush `index` moved by `delta` (its planes, corners and bounds), its surface's velocity set to
// `delta / dt`: a moving platform, a lift, a door, stepped by its owner before the bodies move.
void move_brush(BoxWorld& w, size_t index, const Vec3& delta, double dt);

// The swept box against every brush: the nearest touch (world.hpp's contract).
Hit trace_box_world(const BoxWorld& w, const Box& box, const Vec3& from, const Vec3& to);

// The world as the mover's `World`; `w` must outlive it.
World as_world(const BoxWorld& w);

}  // namespace ge::move
