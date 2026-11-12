// THE PILOT AT A GAP TRICK (pilot.hpp, nav_gaps.hpp): where the gap is in the hop's stretch,
// whether one over water lies close enough ahead for a curve into it from a walk, and the curve's
// own look (`gap_circle`): aimed across at the landing, or the jump now when one lands across.
// The pilot's own parts (`pilot_parts`): only pilot.cpp and pilot_gaps.cpp include this.
//
// ge:summary The pilot at a gap trick: the gap in the stretch, the curve into one over water
// ge:keywords pilot gap-trick gap_in gap_curve_near gap_curve gap-circle curve-into-gap gapbuild
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pilot.hpp"

namespace ge::shooter::pilot_parts {

inline constexpr float kGapLand = 48;  // a gap's jump lands across within this of its landing
inline constexpr float kGapUp = 32;    // and within this up or down

// The route index of the gap trick's start in the hop's stretch [k, end), or -1.
int32_t gap_in(const Pilot& p, int32_t end);

// A gap trick over water within the next few links, its start near and the player moving at a
// walk or more (`gap_circle`: a curve into it begins here).
bool gap_curve_near(const Pilot& p, const PilotSense& s);

// The hop's first jump at the gap in its stretch (`p.gap_k`) as a curve (`gap_circle`): false
// when it is no curve (switched off, far, over lava or slime); else `now` when a jump pressed now
// lands across, and `err` the aim across at its landing off the velocity (degrees, + left).
bool gap_curve(const Pilot& p, const PilotSense& s, int msec, float& err, bool& now);

}  // namespace ge::shooter::pilot_parts
