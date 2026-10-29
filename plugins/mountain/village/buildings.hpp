// THE VILLAGE'S BUILDINGS ON ITS STREETS: the base's own buildings and the town round them, stood
// on lots fronting the streets the plan lays (streets.hpp), as one compact village:
//   - THE FRONT ROW, between the main street and the hub, faces the runs with its back to the
//     street: from the square one way the base lodge, the rental and the ski school; the other
//     way the ticket office and first aid; the hotels further out, looking up the mountain.
//   - ACROSS THE MAIN STREET, facing it: the CHURCH opposite the square, the SHOPS either side of
//     it, flats and houses further out.
//   - ALONG THE BACK STREET, both sides, facing it: houses, now and then flats.
//   - ON THE ROAD OUT: the garage and the pump house, where the machines come and go without
//     crossing the village.
// Lots are laid along each side of a street outward, a gap dealt between one roof and the next
// and now and then a garden left open. Each building is tried at its lot and on along the street
// until it fits: its footprint (and `buildings::kPad` round it) off every street and open place
// (ground.hpp), clear of everything the streets keep away from (`Clear`: the runs, the lifts,
// the hub's groomed snow), off every other building, and on ground that falls no more than
// `buildings::kFall` across it. The trees on a footprint and `lots::kFell` m round it are felled.
//
// A pure function of the map and its plan, off hashes of its seed: nothing drawn from any
// stream.
//
// ge:summary The village's buildings stood on lots along its streets, their trees felled
// ge:keywords village buildings place-buildings building lot frontage front-row church shops
// ge:keywords hotels garage pump-house in-building fell-for-buildings buildings-hash
// ge:skill mountain
// ge:test tests/mountain_buildings_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/building_rules.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/streets.hpp"

namespace ge::mountain::village {

// ONE BUILDING: the middle of its footprint, the way its front faces (heading 0 = +z, clockwise
// from above), its kind's measures, its floor's height (the highest snow under its footprint; it
// stands on a plinth down to `low`, the lowest), and the street it fronts.
struct Building {
  int32_t id;
  BuildingKind kind;
  double x;
  double y;
  double z;
  double heading;
  double width;
  double depth;
  double eaves;
  double ridge;
  double low;
  int32_t street;
};

// The buildings of `plan`'s village on `level`, off the streets in `mask`, by `clear` (what the
// streets keep away from, `clearance_of`). None for a plan with no main street.
std::vector<Building> place_buildings(const mapgen::Level& level, const Streets& plan,
                                      const GroundMask& mask, const Clear& clear);

// Whether (x, z) is inside `b`'s footprint grown by `pad` m.
bool in_building(const Building& b, double x, double z, double pad = 0);

// Fell the trees of `level` on each building's footprint and `lots::kFell` m round it: their
// entries of `felled` (one a tree) set to 1.
void fell_for_buildings(const mapgen::Level& level, std::span<const Building> buildings,
                        std::vector<uint8_t>& felled);

// The buildings folded into `h` (FNV-1a over every number's bits), for goldens and lab digests.
uint64_t buildings_hash(std::span<const Building> buildings, uint64_t h);

}  // namespace ge::mountain::village
