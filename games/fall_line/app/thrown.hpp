// WHAT THE FRAME THROWS INTO THE AIR: the racers' spray off their skis, their breath in the cold
// (skier/app/spray.hpp, breath.hpp) and the scene's area emitters (render/emitters.hpp), each a
// frame's births handed to the particle pool.
//
// ge:summary Fall Line's particles each frame: the spray, the breath, the area emitters
// ge:keywords fall-line particles spray breath throw_spray breathe emit_areas births
// ge:skill render skier
#pragma once

#include "fall_line/app/frame.hpp"

namespace fall_line::app {

// The racers' spray over the `dt` s since the last frame.
void throw_spray(GameFrame& f, const FrameScene& scene, double dt);
// THE BREATH in the cold: each racer's puffs over `dt` s, as thick as the air at his mouth shows a
// breath (wind::breath_of of the map's day at the sky's solar `hour`).
void breathe(GameFrame& f, const FrameScene& scene, double hour, double dt);
// The scene's area emitters' births over [t0, t1) s.
void emit_areas(GameFrame& f, const FrameScene& scene, double t0, double t1);

}  // namespace fall_line::app
