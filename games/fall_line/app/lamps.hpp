// FALL LINE'S LAMPS AFTER DARK: the list of local lights a frame hands the renderer
// (render/lights.hpp) and the light the eye adapts to among them.
//
// - THE MOUNTAIN'S: the floodlight masts down the piste, the village's street lamps and its lit
//   windows' spill (mountain/app/night_lamps.hpp), coming on with the dark (`lamps_lit`).
// - THE HEADLAMP on every racer's helmet, lit with them: a narrow SPOT that throws far enough for
//   the speed (some 600 lm in a beam about 13 degrees off its axis: 5000 cd), and a wide FLOOD
//   round it for the snow at the tips and the piste's edges (60 degrees off the axis, dim). The
//   lamp is strapped on the helmet's brow, so it is where the posed figure's head is (the rig's
//   `head` joint, skier/app/rig_pose.hpp) and turns and bobs with it, tipped down so the spot
//   lands some seven metres ahead of a skier standing tall. Neutral white, 5500 K, so its pool
//   reads apart from the floods' and the street lamps'. The spot casts shadows (the trees' trunks
//   across its pool), the flood does not.
// - THE EYE (`eye_lamp_lux`): the lamps' light on the snow the camera looks at (a white face
//   turned to them, render/lights.hpp's `lamp_lux_at`, at points of the ground along the view),
//   their geometric mean, so a few bright pools in a dark view do not blind it, and under it (a
//   share, `kEyeKey` in lamps.cpp), so the lit snow reads bright and the moonlit mountain past the
//   lamps is still seen.
//
// ge:summary Fall Line's lamps at night: the frame's local lights, the headlamps, the eye's lux
// ge:keywords fall-line lamps headlamp head-lamp night-lights lamp-list eye-lamp-lux lamp_lux
// ge:keywords headlamp_lights headlamp_of eye_lamp_lux night_lights
// ge:skill render
// ge:test tests/fall_line_lamps_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mountain/app/night_lamps.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/lights.hpp"
#include "render/view.hpp"
#include "skier/app/figure.hpp"
#include "skier/app/rig_pose.hpp"

namespace fall_line::app {

// The headlamps' shadow keys: this base plus the racer's place.
inline constexpr uint32_t kHeadlampKeys = 0x03000000u;
inline constexpr float kHeadlampCandela = 5000;  // the spot's, at full
inline constexpr float kHeadlampFlood = 150;     // the flood's
inline constexpr double kHeadlampDip = 14;       // degrees under the head's level

// A headlamp: where it is, world m, and the unit way it shines.
struct Headlamp {
  double at[3] = {0, 0, 0};
  float dir[3] = {0, 0, 1};
};
// The headlamp on the head of a figure posed `pose` in `frame` (skier/app/figure.hpp's).
Headlamp headlamp_of(const ge::skier::FigureFrame& frame, const ge::skier::RigPose& pose);
// Append the headlamp's spot and flood at `lit` (0: nothing), the spot shadowed under `key` when
// `shadows`.
void headlamp_lights(const Headlamp& lamp, uint32_t key, double lit, bool shadows,
                     std::vector<ge::render::LocalLight>& out);

// The lux the eye adapts to among `lights` from `view` over `level`'s snow: a share of the
// geometric mean of their light at points of the ground ahead (0 when none reaches any).
double eye_lamp_lux(std::span<const ge::render::LocalLight> lights,
                    const ge::render::FrameView& view, const ge::mountain::mapgen::Level& level);

}  // namespace fall_line::app
