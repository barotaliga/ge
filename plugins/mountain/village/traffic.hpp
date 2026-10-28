// THE VILLAGE'S TRAFFIC: the cars, the ski bus and the bicycles on a village's streets
// (streets.hpp), and the cars parked along its kerbs and in its day car park. Planned once a map:
//   - CARS ROUND THE LOOP, each way, each going round in a whole number of laps a period;
//   - VISITORS in off the road out to a bay of the day car park, parked a while, backed out and
//     away down the road again (off the map's edge until their time comes round);
//   - CARS THROUGH, in off the road and out again (round the loop where there is one road out,
//     across to the other where there are two);
//   - THE SKI BUS, in off the road to the stop on the square, standing there a while, and away;
//   - CYCLISTS round the loop at the carriageway's edge.
// Every route comes round in a whole fraction of `traffic::kPeriod`, so the whole village's
// traffic does too, and each vehicle is put on its route at a PHASE chosen so that, over the
// whole period, it never comes within its room (its width and the gap it keeps ahead) of any
// vehicle placed before it, tried at the phases dealt off the seed in turn; a vehicle with no
// such phase is not on the streets. A car overtaking a cyclist on his lane is the one meeting not
// held to: it swings out round him (`vehicle_at`).
//
// Where each vehicle is at a moment (`vehicle_at`) is a PURE FUNCTION OF THE PLAN AND THE CLOCK,
// as a lift's carriers are: nothing stepped, nothing stored on a game's state, nothing drawn from
// any stream; the plan is dealt off hashes of the map's seed on a salt of its own (`kTrafficSalt`),
// so nothing a map builds or a run steps moves for it.
//
// ge:summary The village's traffic: cars, the ski bus and bikes as a pure function of the clock
// ge:keywords traffic plan-traffic vehicle-at route-at vehicle parked bus bike car visitor
// ge:keywords through loop phase period traffic-hash swing-out
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/furniture.hpp"
#include "mountain/village/streets.hpp"
#include "mountain/village/traffic_route.hpp"
#include "mountain/village/traffic_rules.hpp"

namespace ge::mountain::village {

// The traffic's own salt on the map's seed.
inline constexpr int32_t kTrafficSalt = 7331;

// What a route carries a vehicle as.
enum class Role : uint8_t { loop, visitor, through, bus, bike };

// A LEG of a route: driven along a line, stood where the last left off, or off the map. `start`
// is its time into the route's period, `odo` the distance the wheels have rolled by then (backing
// counts back), `driven` its line (an index into the route's, the last driven for a wait).
enum class LegKind : uint8_t { drive, wait, hide };
struct Leg {
  LegKind kind;
  double start;
  double duration;
  double odo;
  int32_t driven;
};

// A ROUTE: its legs round one period of its own (a whole fraction of `traffic::kPeriod`), their
// lines, and the wheels' roll over it.
struct Route {
  std::vector<Leg> legs;
  std::vector<Driven> driven;
  double period;
  double roll;
  Role role;
};

// A VEHICLE ON THE STREETS: its kind, its route and its phase on it, s.
struct Vehicle {
  int32_t id;
  VehicleKind kind;
  int32_t route;
  double phase;
  Role role;
};

// A CAR PARKED in a bay all the while.
struct Parked {
  int32_t id;
  VehicleKind kind;
  int32_t bay;
  double x;
  double y;
  double z;
  double heading;
};

// THE VILLAGE'S TRAFFIC: the furniture it drives (the lanes and bays), its routes, the vehicles
// on them, the cars parked, and the box round everything it drives.
struct TrafficPlan {
  double period;
  Furniture furniture;
  std::vector<Route> routes;
  std::vector<Vehicle> vehicles;
  std::vector<Parked> parked;
  double x0;
  double z0;
  double x1;
  double z1;
};

// A rear axle's pose on a route at a moment: whether it is on the map, where, the way the body
// faces and the speed along it (negative backing), the line's curvature as the body goes, the
// lane, the acceleration, the turn ahead and how far the wheels have rolled.
struct RoutePose {
  bool shown = false;
  double x = 0;
  double z = 0;
  double facing = 0;
  double speed = 0;
  double bend = 0;
  int32_t lane = -1;
  double accel = 0;
  double turn = 0;
  double odo = 0;
};

// A VEHICLE AT A MOMENT: whether it is on the map (a car off down the road out is not), its
// middle on the snow, the way its body faces and its speed along that (negative backing), the
// front wheels' turn (rad, positive toward the heading's growing side), how far its wheels have
// rolled (m), its brake lights, its indicator (-1, 0, 1: the side it turns to, by the heading's
// sign) and the lane it is on (-1 between them).
struct VehiclePose {
  bool shown = false;
  VehicleKind kind = VehicleKind::hatch;
  double x = 0;
  double y = 0;
  double z = 0;
  double heading = 0;
  double speed = 0;
  double steer = 0;
  double roll = 0;
  bool brake = false;
  int32_t signal = 0;
  int32_t lane = -1;
};

// THE TRAFFIC on `plan`'s streets, or nothing where the streets carry no lanes.
std::optional<TrafficPlan> plan_traffic(const mapgen::Level& level, const Streets& plan);

// THE REAR AXLE'S POSE on `route` `tau` s into its period (any `tau`: whole periods are rolled
// into the odometer).
RoutePose route_at(const Route& route, double tau);

// VEHICLE `k` OF THE PLAN AT `t` s of the run's clock: off its route at its phase, its middle the
// rear axle's line carried forward, and (a car beside a cyclist on its lane) swung out round him.
VehiclePose vehicle_at(const mapgen::Level& level, const TrafficPlan& plan, int32_t k, double t);

// Whether (x, z) is within `pad` m of anything the traffic drives.
bool near_traffic(const TrafficPlan& plan, double x, double z, double pad = 0);

// The plan folded into `h` (FNV-1a over every number's bits), for goldens and lab digests.
uint64_t traffic_hash(const TrafficPlan& plan, uint64_t h);

}  // namespace ge::mountain::village
