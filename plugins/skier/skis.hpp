// A PAIR OF SKIS AND THE SKIER ON THEM: the numbers that are this pair's own (its length, its
// sidecut, how far over it can be stood) and the skier's (his mass in his kit, his height, his
// drag area stood tall and tucked). The numbers every pair shares are `tuning.hpp`'s.
//
// `kReferenceSkis` is the all-mountain pair every shared number was tuned on, each value inside
// the class's measured band: 1.78 m long on an 88 mm waist, an 18 m sidecut, at most 60 degrees
// of edge, and an 80 kg skier in 8.5 kg of kit whose drag area is 0.9 m^2 stood tall and 0.48 m^2
// in a recreational tuck (a racer's is 0.25-0.35).
//
// ge:summary A pair of skis and the skier on them: SkiSpec and the reference pair
// ge:keywords skis ski-spec pair sidecut edge-max mass drag-area cda tuck reference-skis
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

namespace ge::skier {

struct SkiSpec {
  double length;       // m
  double waist;        // m, the narrowest width under the boot
  double sidecut;      // m, the radius the ski's side is cut on
  double edge_max;     // rad, the most edge the skier can stand the pair on at a crawl
  double mass;         // kg, the skier and his kit together
  double cog;          // m, the centre of mass over the snow, stood tall
  double cda_upright;  // m^2, drag area stood tall
  double cda_tuck;     // m^2, drag area in a full tuck
};

inline constexpr SkiSpec kReferenceSkis{
    1.78,   // length
    0.088,  // waist
    18.0,   // sidecut
    1.05,   // edge_max: 60 degrees
    88.5,   // mass: 80 kg skier + 8.5 kg kit
    1.0,    // cog
    0.9,    // cda_upright
    0.48,   // cda_tuck
};

}  // namespace ge::skier
