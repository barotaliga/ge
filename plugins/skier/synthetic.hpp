// THE HAND-BUILT MAPS a skier is staged on: nothing the generator built, so the skier's tests
// and scenarios hold with the generator deleted, and a move in the generator never moves them.
//
// - `synthetic_slope()`: THE SLOPE, a 640 m square face falling down +z at 17 degrees, a groomed
//   piste 40 m wide down its middle (x = 320) and powder either side; a set of rollers from
//   z = 150 to 250, a lone tree beside the piste's line at (328, 260), and a kicker (K1) whose
//   lip stands 2.5 m over the face at z = 380, its ramp 10 m long, landing back on the face.
// - `flat_strip(pitch, packed)`: A DRAG STRIP, 4096 m square, one constant pitch down +z (0 is
//   the dead flat) and one snow all over it (1 packed, 0 powder).
//
// Both carry a straight piste down x = the middle (a point every 2 m), a start gate and a finish
// line, so anything that reads the piste reads one here too.
//
// ge:summary The hand-built maps the skier is staged on: the slope and the drag strip
// ge:keywords synthetic slope flat strip drag-strip test-map hand-built synthetic_slope flat_strip
// ge:keywords rollers kicker lone-tree powder packed
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"

namespace ge::skier {

// The slope's landmarks, m, for anything that stages a skier at one.
namespace slope {
inline constexpr double kSize = 640;
inline constexpr double kCentre = 320;   // the piste's line, x
inline constexpr double kPitch = 0.297;  // rad, 17 degrees
inline constexpr double kRollersFrom = 150;
inline constexpr double kRollersTo = 250;
inline constexpr double kTreeX = 328;
inline constexpr double kTreeZ = 260;
inline constexpr double kKickerLip = 380;  // z of the lip
inline constexpr double kKickerRamp = 10;  // m, foot to lip
inline constexpr double kKickerHeight = 2.5;
}  // namespace slope

mountain::mapgen::Level synthetic_slope();

// The strip's size, m; its piste runs down x = kStripSize / 2.
inline constexpr double kStripSize = 4096;

mountain::mapgen::Level flat_strip(double pitch, double packed);

}  // namespace ge::skier
