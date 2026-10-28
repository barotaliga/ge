// THE VILLAGE'S TRAFFIC, the numbers: the cars, the ski bus and the bicycles on a village's
// streets (traffic.hpp), what each class measures and weighs, how each drives, and how many of
// each a village is dealt. Measured off the classes' typical figures (no make); every number is
// metres, seconds or metres a second unless it says otherwise.
//
// ge:summary The village traffic's numbers: the vehicles' measures, the drives, the dealing
// ge:keywords traffic vehicles vehicle-kind vehicle-size drive traffic-rules bus bike car
// ge:keywords headway margin period parked-share
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace ge::mountain::village {

// The kinds of vehicle the village's streets carry.
enum class VehicleKind : uint8_t { hatch, estate, suv, van, bus, bike };

inline const char* vehicle_kind_name(VehicleKind k) {
  switch (k) {
    case VehicleKind::hatch:
      return "hatch";
    case VehicleKind::estate:
      return "estate";
    case VehicleKind::suv:
      return "suv";
    case VehicleKind::van:
      return "van";
    case VehicleKind::bus:
      return "bus";
    case VehicleKind::bike:
      return "bike";
  }
  return "?";
}

// A VEHICLE'S MEASURES: its length, width (the body, mirrors folded) and height over the road;
// its wheelbase and the front overhang ahead of the front axle (the rear's is what is left of its
// length); the wheel's radius and the track between the wheels' middles; its mass with its driver,
// kg. A bicycle's height is its rider's over the road.
//   HATCH: a small five-door, 3.6-4.1 m long, 1.6-1.75 wide, 1.4-1.5 tall, a 2.4-2.6 m wheelbase.
//   ESTATE: a compact estate, 4.4-4.75 m long, 1.8 wide, a 2.65-2.7 m wheelbase.
//   SUV: a compact crossover, 4.2-4.6 m long, 1.6-1.7 tall, a 0.7 m tyre on a 17-inch wheel.
//   VAN: a medium panel van, 5.5-5.9 m long, 2.0-2.06 wide, 2.35-2.65 tall, a 3.3 m wheelbase.
//   BUS: the ski bus, a 12 m single-deck bus at the legal 2.55 m width, 3.0-3.1 tall, the front
//     axle 2.7 m back and a 5.9 m wheelbase.
//   BIKE: a winter bicycle, 1.8-1.9 m long, its bars 0.6 wide, a 1.1 m wheelbase on fat tyres.
struct VehicleSize {
  double length;
  double width;
  double height;
  double wheelbase;
  double front;
  double wheel;
  double track;
  double mass;
};

inline constexpr VehicleSize vehicle_size(VehicleKind k) {
  switch (k) {
    case VehicleKind::hatch:
      return {4.05, 1.75, 1.47, 2.56, 0.84, 0.315, 1.5, 1150};
    case VehicleKind::estate:
      return {4.65, 1.8, 1.48, 2.69, 0.92, 0.325, 1.55, 1450};
    case VehicleKind::suv:
      return {4.45, 1.82, 1.67, 2.63, 0.88, 0.355, 1.56, 1950};
    case VehicleKind::van:
      return {5.55, 2.05, 2.45, 3.3, 1.0, 0.35, 1.72, 2200};
    case VehicleKind::bus:
      return {12, 2.55, 3.1, 5.9, 2.7, 0.48, 2.1, 12500};
    case VehicleKind::bike:
      break;
  }
  return {1.85, 0.62, 1.75, 1.1, 0.38, 0.36, 0, 95};
}

// How far a vehicle's middle stands ahead of its rear axle, m.
inline constexpr double axle_to_middle(VehicleKind k) {
  const VehicleSize v = vehicle_size(k);
  return v.length / 2 - (v.length - v.wheelbase - v.front);
}

// HOW A VEHICLE DRIVES: its top speed on each kind of street (a village's 25 km/h, the car
// park's walking pace, the road out's 50), the sideways pull it takes a bend at (a careful driver
// on a snowy street: some 15 km/h round a village corner), how hard it pulls away and how gently
// it slows, and its pace backing out of a bay. A bicycle rides at a winter cyclist's 13 km/h.
struct Drive {
  double street;
  double aisle;
  double road;
  double lateral;
  double accel;
  double brake;
  double reverse;
};

enum class Driver : uint8_t { car, bus, bike };

inline constexpr Drive drive_of(Driver d) {
  switch (d) {
    case Driver::car:
      return {25 / 3.6, 10 / 3.6, 50 / 3.6, 1.4, 1.3, 1.7, 1.4};
    case Driver::bus:
      return {22 / 3.6, 10 / 3.6, 45 / 3.6, 1.1, 0.8, 1.2, 1};
    case Driver::bike:
      break;
  }
  return {13 / 3.6, 8 / 3.6, 15 / 3.6, 1, 0.5, 1, 1};
}

inline constexpr Driver driver_of(VehicleKind k) {
  return k == VehicleKind::bus ? Driver::bus : k == VehicleKind::bike ? Driver::bike : Driver::car;
}

// THE TRAFFIC a village is dealt and how it runs.
namespace traffic {

// Everything the streets carry comes round again this often (the bus's timetable, the visitors'
// coming and going), s; how finely a plan is checked for two vehicles meeting, s.
inline constexpr double kPeriod = 600;
inline constexpr double kSample = 0.25;
// Cars round the loop, each way, dealt.
inline constexpr int kLoopLeast = 2;
inline constexpr int kLoopMost = 3;
// Visitors in off the road to a bay of the day car park, parked a while (dealt, s) and away.
inline constexpr int kVisitorsLeast = 4;
inline constexpr int kVisitorsMost = 6;
inline constexpr double kStayLeast = 90;
inline constexpr double kStayMost = 300;
// Cars in off the road and out again.
inline constexpr int kThrough = 2;
// Cyclists round the loop, and how far in from the carriageway's edge one rides.
inline constexpr int kBikesLeast = 3;
inline constexpr int kBikesMost = 4;
inline constexpr double kBikeEdge = 0.65;
// The ski bus's stand at its stop, dealt, s.
inline constexpr double kBusStayLeast = 25;
inline constexpr double kBusStayMost = 45;
// The shares of a car's class: hatch, estate, SUV, van.
inline constexpr std::array<double, 4> kKinds{0.35, 0.2, 0.3, 0.15};
// The share of the kerbs' bays taken, and of the car park's by day (the lifts' hours) and after.
inline constexpr double kParkedKerb = 0.62;
inline constexpr double kParkedDay = 0.8;
inline constexpr double kParkedNight = 0.3;
inline constexpr double kDayFrom = 8;
inline constexpr double kDayTo = 17;
// The room each vehicle keeps beside it, the gap it keeps ahead (m, and s of its speed), which
// two vehicles never close.
inline constexpr double kMarginSide = 0.25;
inline constexpr double kMarginGap = 1.2;
inline constexpr double kMarginHeadway = 0.6;
// How far short of a junction's crossing carriageway a lane is left to turn on; how far back down
// an aisle a car starts its turn into a bay.
inline constexpr double kTrim = 1.5;
inline constexpr double kApproach = 6.5;
// How far a car swings out round a cyclist on its lane at the most, and how far ahead and behind
// of him; the gap it leaves him, and the least room it keeps beside anything it meets on the way
// (an oncoming car it squeezes past: a hand's breadth, where every other meeting keeps
// `kMarginSide`).
inline constexpr double kPassSwing = 0.9;
inline constexpr double kPassAhead = 14;
inline constexpr double kPassBehind = 6;
inline constexpr double kPassClear = 0.15;
inline constexpr double kPassSide = 0.05;
// How far ahead a turn lights the indicator, and how much of a turn, rad.
inline constexpr double kSignalAhead = 28;
inline constexpr double kSignalTurn = 0.6;
// How finely a route's line is laid.
inline constexpr double kDs = 0.5;
// How many phases a vehicle is tried at before it is left off the streets, and a cyclist.
inline constexpr int kTries = 24;
inline constexpr int kBikeTries = 100;

}  // namespace traffic

// How far a car of `car` swings out round a cyclist on a lane `lane` m wide: as far as clears
// him by `kPassClear`, and no further than `kPassSwing`.
inline constexpr double pass_swing(VehicleKind car, double lane) {
  const double outer = lane / 2 + vehicle_size(car).width / 2;
  const double inner = lane - traffic::kBikeEdge - vehicle_size(VehicleKind::bike).width / 2;
  return std::clamp(outer - inner + traffic::kPassClear, 0.0, traffic::kPassSwing);
}

// What the streets carry for the traffic: a kerbside bay's length and width, how far past a
// junction's carriageway a crossing stands and how wide it is, the bus stop's shelter along the
// street and its depth.
namespace furniture {

inline constexpr double kKerbLength = 6;
inline constexpr double kKerbWidth = 2.2;
inline constexpr double kCrossingOff = 1.5;
inline constexpr double kCrossingWidth = 3;
inline constexpr double kBusAlong = 4;
inline constexpr double kBusDeep = 1.6;

}  // namespace furniture

}  // namespace ge::mountain::village
