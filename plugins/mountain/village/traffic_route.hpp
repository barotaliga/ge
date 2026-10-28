// THE WAYS THE VILLAGE'S TRAFFIC DRIVES (traffic.hpp): a route through the village's lanes
// (furniture.hpp) laid as one line the REAR AXLE follows (each lane's line between the junctions,
// trimmed short of the crossing carriageway and joined to the next by a curve round the corner;
// a bay's turn in off its aisle) and the speed it is driven at along it (the street's top speed,
// eased for every bend by the sideways pull it takes, pulled away from and slowed into),
// integrated once into the time it reaches every point, so where a vehicle is at any moment is a
// pure function of that moment. With the rear axle on the line and the body along it, the front
// swings wide of a corner as a real car's does, and the wheels' turn is the bend's
// (tan d = wheelbase x curvature).
//
// Pure functions of the plan and its furniture; nothing drawn from any stream.
//
// ge:summary The traffic's routes: lane lines joined round corners, laid every half metre, driven
// ge:keywords traffic route path lay-path drive-path raw-line bay-turn visit lane-offset
// ge:keywords curvature corner-speed rear-axle
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/furniture.hpp"
#include "mountain/village/streets.hpp"
#include "mountain/village/traffic_rules.hpp"

namespace ge::mountain::village {

// A raw point of a route before it is laid: where, the lane it is on (-1 between lanes) and the
// top speed there.
struct RawPoint {
  double x;
  double z;
  int32_t lane;
  double top;
};

// A ROUTE'S LINE, every `traffic::kDs` m or so: where, the way it runs, its curvature (smoothed
// over a few metres, signed: positive turning the way the heading grows), the lane each point is
// on, the top speed there and what of a turn lies ahead (the heading's change over the next
// `traffic::kSignalAhead` m, rad).
struct Path {
  int32_t n = 0;
  std::vector<double> x;
  std::vector<double> z;
  std::vector<double> h;
  std::vector<double> k;
  std::vector<int32_t> lane;
  std::vector<double> top;
  std::vector<double> turn;
  bool closed = false;
};

// A stretch of a route on one lane: the lane, and its street's arc from `s0` to `s1` (falling
// when the lane runs against the street).
struct Visit {
  int32_t lane;
  double s0;
  double s1;
};

// What a vehicle on a lane keeps to: the lane's middle, or a cyclist's line in from the
// carriageway's edge.
enum class Keep : uint8_t { middle, edge };

// A LEG of a route as driven: its line, whether the vehicle backs along it, the speed at every
// point and the time it reaches each from the leg's start.
struct Driven {
  Path path;
  double ds = 0;
  bool reverse = false;
  std::vector<double> v;
  std::vector<double> t;
  double duration = 0;
};

// The lateral off a street's centreline a vehicle keeping `keep` on `lane` drives at.
double lane_offset(const Street& st, const Lane& lane, Keep keep);

// The top speed on a street of `kind` for `drive`.
double top_of(StreetKind kind, const Drive& drive);

// How far short of the junction at its `end` a lane on `st` is trimmed: past the crossing
// carriageway.
double trim_of(const Streets& plan, const Street& st, int end);

// A lane's whole visit: from past the junction it leaves to short of the one it reaches.
Visit whole_visit(const Streets& plan, const Furniture& f, int32_t lane);

// The street's arc nearest (x, z), to half a metre.
double arc_nearest(const Street& st, double x, double z);

// THE RAW LINE of a run of visits, the corners between them rounded (and the last joined back to
// the first when `closed`).
std::vector<RawPoint> raw_line(const mapgen::Level& level, const Streets& plan, const Furniture& f,
                               std::span<const Visit> visits, Keep keep, const Drive& drive,
                               bool closed);

// A BAY'S TURN: from `a` on the aisle (going `ta`) into the bay, the rear axle brought to `b`
// with the body along `fb`.
std::vector<RawPoint> bay_turn(mapgen::PlanPoint a, mapgen::PlanPoint ta, mapgen::PlanPoint b,
                               mapgen::PlanPoint fb, double top);

// THE LINE LAID every `traffic::kDs` m or so, its headings, curvatures and turns ahead.
Path lay_path(std::span<const RawPoint> raw, bool closed);

// The step between a path's points, m.
double step_of(const Path& p);

// THE SPEED ALONG A LINE and the time it reaches every point: the top there, eased for the bend by
// `drive.lateral` (v^2 k <= a), pulled away at `drive.accel` and slowed at `drive.brake`, from
// `v0` at its start to `v1` at its end (a closed line round and round). `scale` stretches the
// time (a loop fitted to its period).
Driven drive_path(Path path, const Drive& drive, double v0, double v1, bool reverse = false,
                  double scale = 1);

// The length a driven leg rolls, m.
double roll_of(const Driven& d);

}  // namespace ge::mountain::village
