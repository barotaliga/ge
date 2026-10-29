// THE VILLAGE'S BUILDINGS BY KIND, AND THE RULES THEY STAND BY: the kinds a ski area's village is
// built of (the base's own buildings round the hub, the town along its streets), each one's
// footprint and heights, and the numbers its lots are laid by (buildings.hpp).
//
// THE MEASURES are a mountain village's, as built:
//   - THE BASE LODGE: a restaurant and its hall over a boarded ground floor, 32 m along its front
//     and 18 deep, its eaves at 7 m under a broad low gable to 11.
//   - THE TICKET OFFICE: a timber kiosk with a row of windows, 8 by 5, 3 m to its eaves.
//   - THE RENTAL: a shop front under a boarded upper floor, 14 by 10, its ridge at 9.
//   - THE SKI SCHOOL: a hut with two dormers, 10 by 8, its ridge at 7.
//   - FIRST AID: a rendered hut with a wide door, 9 by 7, its ridge at 6.
//   - A HOTEL: four storeys with a balcony on each under a big gable, 24 by 14, its ridge at 17.
//   - THE GARAGE: ribbed steel over a concrete plinth with three doors a groomer drives through,
//     30 by 16, under a low pitch at 8.
//   - THE PUMP HOUSE: formwork concrete under a flat roof, 10 by 8, 4.5 tall.
//   - A HOUSE: the mountain chalet, a stone or rendered ground floor and a timber storey under
//     its gable on deep eaves, 11 by 10, its ridge at 9.5.
//   - THE FLATS: three storeys and a roof storey, 18 by 13, its ridge at 14.
//   - A SHOP: display windows under an awning with a flat over, 10 by 12, its ridge at 9.5.
//   - THE CHURCH: a nave 11 wide and 23 long under a steep roof to 14, its bell tower before it
//     (5 m square) under a spire 30 m over the street.
//
// ge:summary The village's building kinds, their measures and the rules their lots are laid by
// ge:keywords village buildings building-kind building-size lodge ticket rental school
// ge:keywords first-aid hotel garage pump-house house flats shop church lots setback
// ge:skill mountain
// ge:test tests/mountain_buildings_test.cpp
#pragma once

#include <cstdint>

namespace ge::mountain::village {

enum class BuildingKind : uint8_t {
  lodge,
  ticket,
  rental,
  school,
  first_aid,
  hotel,
  garage,
  pump_house,
  house,
  flats,
  shop,
  church,
};
inline constexpr int kBuildingKinds = static_cast<int>(BuildingKind::church) + 1;

inline const char* building_kind_name(BuildingKind k) {
  switch (k) {
    case BuildingKind::lodge:
      return "lodge";
    case BuildingKind::ticket:
      return "ticket";
    case BuildingKind::rental:
      return "rental";
    case BuildingKind::school:
      return "school";
    case BuildingKind::first_aid:
      return "first-aid";
    case BuildingKind::hotel:
      return "hotel";
    case BuildingKind::garage:
      return "garage";
    case BuildingKind::pump_house:
      return "pump-house";
    case BuildingKind::house:
      return "house";
    case BuildingKind::flats:
      return "flats";
    case BuildingKind::shop:
      return "shop";
    case BuildingKind::church:
      return "church";
  }
  return "?";
}

// A kind's footprint (its WIDTH along its front, its DEPTH back from it) and heights over its
// floor: its eaves, its ridge (the flat roof's top for a flat one), and how many storeys.
struct BuildingSize {
  double width;
  double depth;
  double eaves;
  double ridge;
  int32_t floors;
};

inline constexpr BuildingSize building_size(BuildingKind k) {
  switch (k) {
    case BuildingKind::lodge:
      return {32, 18, 7, 11, 2};
    case BuildingKind::ticket:
      return {8, 5, 3, 4.5, 1};
    case BuildingKind::rental:
      return {14, 10, 6, 9, 2};
    case BuildingKind::school:
      return {10, 8, 4, 7, 1};
    case BuildingKind::first_aid:
      return {9, 7, 3.5, 6, 1};
    case BuildingKind::hotel:
      return {24, 14, 12, 17, 4};
    case BuildingKind::garage:
      return {30, 16, 7, 8, 1};
    case BuildingKind::pump_house:
      return {10, 8, 4.5, 4.5, 1};
    case BuildingKind::house:
      return {11, 10, 5.5, 9.5, 2};
    case BuildingKind::flats:
      return {18, 13, 10, 14, 4};
    case BuildingKind::shop:
      return {10, 12, 6, 9.5, 2};
    case BuildingKind::church:
      return {11, 28, 8, 14, 1};
  }
  return {10, 10, 5, 8, 1};
}

namespace buildings {

// The church's bell tower: its side, m, and its spire's tip over the floor.
inline constexpr double kTowerSide = 5;
inline constexpr double kSpire = 30;
// How far a front wall stands back from the back of its street's sidewalk, m; the front row's
// back wall from the main street's.
inline constexpr double kSetback = 3;
inline constexpr double kFrontRowSetback = 2;
// The gap between one roof and the next along a street, m, dealt between these.
inline constexpr double kGapLeast = 4;
inline constexpr double kGapMost = 9;
// The share of lots left a garden, and its width along the street, m.
inline constexpr double kGarden = 0.12;
inline constexpr double kGardenWidth = 10;
// The margin round a footprint kept off every street and every other building, m.
inline constexpr double kPad = 1;
// The most the snow may fall across a footprint, m (the rest stands on a plinth).
inline constexpr double kFall = 3.5;
// How far a building is tried along its frontage past its lot, in steps of `kNudge` m, before its
// kind is given up there.
inline constexpr double kNudge = 3;
inline constexpr double kSearch = 90;
// A footprint is tested every `kSample` m.
inline constexpr double kSample = 2;
// The most buildings a village stands, and the most hotels, shops and flats.
inline constexpr int32_t kMost = 64;
inline constexpr int32_t kHotels = 3;
inline constexpr int32_t kShops = 4;
// The share of the town's lots that are flats rather than a house.
inline constexpr double kFlats = 0.3;

}  // namespace buildings

}  // namespace ge::mountain::village
