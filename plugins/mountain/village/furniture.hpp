// WHAT A VILLAGE'S STREETS CARRY for its traffic, worked out from the street plan (streets.hpp):
// the LANES traffic keeps to (right-hand, a lane each way), the LOOP round the village, the
// CROSSINGS, the PARKING BAYS (along the back street's kerb and in the day car park) and the BUS
// STOP on the square's edge. Pure functions of the plan; nothing drawn from any stream.
//
// ge:summary The village streets' lanes, loop, crossings, parking bays and bus stop
// ge:keywords village furniture lane lanes loop bay bays kerb car-park crossing bus-stop
// ge:keywords trim-at offset-line furnish-streets
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/streets.hpp"

namespace ge::mountain::village {

// ONE LANE of a street, the way traffic runs on it: its street (an index into the plan's
// streets), which way (+1 from the street's first junction to its last, -1 back), the junctions
// it leaves and reaches, its line (the lane's middle, on the right of its way as a driver sees
// it), its length and its width. Lane `2k` is street `k`'s +1, `2k + 1` its -1 (`lane_of`).
struct Lane {
  std::string id;
  int32_t street;
  int32_t dir;
  int32_t from;
  int32_t to;
  std::vector<StreetPoint> points;
  double length;
  double width;
};

inline int32_t lane_of(int32_t street, int32_t dir) { return street * 2 + (dir > 0 ? 0 : 1); }

// A PARKING BAY: its middle, the way a car parked in it faces, its length and width, the street
// it is reached from and whether it is a kerbside bay or one of the car park's.
enum class BayKind : uint8_t { kerb, lot };
struct Bay {
  std::string id;
  BayKind kind;
  int32_t street;
  double x;
  double y;
  double z;
  double heading;
  double length;
  double width;
};

// A CROSSING: its street and its middle's arc on it, the street's heading there, how far it runs
// across (the carriageway and any parking) and its width along the street.
struct Crossing {
  int32_t street;
  double s;
  double x;
  double y;
  double z;
  double heading;
  double across;
  double width;
};

// THE BUS STOP: the shelter's middle and the way its open front faces (toward the road), and
// where on its lane the bus stands (`lane`, `s` along the lane's line).
struct BusStop {
  double x;
  double y;
  double z;
  double heading;
  int32_t street;
  int32_t lane;
  double s;
};

// Everything the streets carry for the traffic. `loop` is the lanes, in order, that run round the
// village (the main street east, the east end's cross street down, the back street west, the west
// end's cross street up).
struct Furniture {
  std::vector<Lane> lanes;
  std::vector<int32_t> loop;
  std::vector<Crossing> crossings;
  std::vector<Bay> bays;
  std::optional<BusStop> bus;
};

// How far short of its junction at `end` (0 its first, 1 its last) a street's sidewalks stop
// (`walk`: past the other streets' reach there) and its carriageway's patch reaches (`road`).
struct Trim {
  double walk;
  double road;
};
Trim trim_at(const Streets& plan, const Street& st, int end);

// The line `lat` m to the right of a street's centreline from arc `s0` to `s1` (reversed when
// `s0 > s1`), every `step` m.
std::vector<StreetPoint> offset_line(const mapgen::Level& level, const Street& st, double lat,
                                     double s0, double s1, double step = 4);

// THE FURNITURE of a village's streets.
Furniture furnish_streets(const mapgen::Level& level, const Streets& plan);

}  // namespace ge::mountain::village
