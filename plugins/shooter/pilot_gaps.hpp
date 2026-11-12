// THE PILOT AT A GAP TRICK (pilot.hpp, nav_gaps.hpp): where the gap is in the hop's stretch,
// whether one over water lies close enough ahead for a curve into it from a walk, and the curve's
// own look (`gap_circle`): aimed across at the landing, or the jump now when one lands across;
// and short of the lip, the run on to it that brings a place a jump lands from (`gap_run`).
// The pilot's own parts (`pilot_parts`): only pilot.cpp and pilot_gaps.cpp include this.
//
// ge:summary The pilot at a gap trick: the gap in the stretch, the curve and the run to its lip
// ge:keywords pilot gap-trick gap_in gap_curve_near gap_curve gap-circle curve-into-gap gapbuild
// ge:keywords gap_run gap_lands run-to-the-lip ground-strafe side-to-side rollout
// ge:skill shooter bots
// ge:test tests/shooter_nav_gaps_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pilot.hpp"

namespace ge::shooter::pilot_parts {

inline constexpr float kGapLand = 48;        // a gap's jump lands across within this of its landing
inline constexpr float kGapUp = 32;          // and within this up or down
inline constexpr float kGapCurveFrom = 160;  // a curve into a gap begun this near its start, units

// Whether a jump pressed at `at` moving at `velocity` comes down across, by the gap's landing `T`:
// flown by the move (nav_gaps.hpp `fly_jump`), forward at the landing all the way.
bool gap_lands(const MoveWorld& world, const MoveSettings& move, Vec3f at, Vec3f velocity, Vec3f T,
               int msec);

// Under a keyboard (`curve_keys`): whether a jump asked now still lands across by `T` when its key
// goes down one or two commands late, the keys held now (the curve's side key) and the view still.
bool gap_lands_late(const Pilot& p, const PilotSense& s, int msec, Vec3f T);

// The route index of the gap trick's start in the hop's stretch [max(k, from), end), or -1.
int32_t gap_in(const Pilot& p, int32_t end, size_t from = 0);

// A gap trick over water within the next few links, its start near and the player moving at a
// walk or more (`gap_circle`: a curve into it begins here).
bool gap_curve_near(const Pilot& p, const PilotSense& s);

// The hop's first jump at the gap in its stretch (`p.gap_k`) as a curve (`gap_circle`): false
// when it is no curve (switched off, far, over lava or slime); else `now` when a jump pressed now
// lands across, and `err` the aim across at its landing off the velocity (degrees, + left).
bool gap_curve(const Pilot& p, const PilotSense& s, int msec, float& err, bool& now);

// THE RUN TO A GAP'S LIP (`gap_circle`), asked on the ground near a gap when no jump from here
// lands across: the move rolled on from here a command at a time straight at the lip, or curving on
// the ground to either side with the wish at the best angle for speed (people curve into a gap at a
// walk and take off at 430-490), or strafing on the ground side to side along the line (the side
// key swapped every few commands: the speed of a curve without leaving the line), a jump tried
// from each place near the lip. The way whose jump lands across soonest: its first command's view
// and side key (0 straight), and the commands to its jump; `found` false when none does.
struct GapRun {
  bool found = false;
  int8_t side = 0;
  int flip = 0;  // the side key swapped every this many commands (0: one curve)
  int way = -1;  // which way (`Pilot::gap_way` keeps it, and its commands so far)
  int n = 0;
  float yaw = 0;
};
GapRun gap_run(const Pilot& p, const PilotSense& s, int msec);

}  // namespace ge::shooter::pilot_parts
