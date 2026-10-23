// FALL LINE'S CAMERA LADDER: the six rungs a run is seen from, each a row of an engine rig
// (engine/camera), in the order the camera key walks them.
//
//   tips     WORN low over the ski tips: the shovels and the snow coming straight at the lens
//   helmet   WORN at his eyes: the tuck folds it down, the lean tips the horizon into the carve
//   chase    a BOOM behind and over him, the full sense of speed: the seat a run starts on
//   far      a heavier boom further back and higher
//   high     the heaviest boom, high over him: the line through the gates ahead
//   orbit    the menu's drone, round him (not on the camera key)
//
// The booms are the reference ladder's (engine/camera/ladders.hpp), which were cut to a skier
// on a mountain: they lean with the slope, keep the skier placed in the frame, stretch the lens
// with speed, look down a long fall, and are pushed off the trunks and pulled in for the posts
// (world.hpp). The worn rungs read their eye off the figure each frame (`fit_worn`, subject.hpp),
// so they ride where the drawn head and tips are.
//
// THE CAMERA KEY (`next_rung`) walks tips, helmet, chase, far, high and round again. A change is
// a HAND-OVER (`hand_over`), never a cut: between two booms the picture is inertialized (it keeps
// its own motion and settles onto the arriving boom), and to or from a worn lens it is blended in
// the skier's frame, so the lens flies into his head or out of it rather than through his body.
//
// ge:summary Fall Line's camera ladder: tips, helmet, chase, far, high, orbit; the camera key
// ge:keywords fall-line camera ladder rung tips helmet chase far high orbit camera-key hand-over
// ge:keywords next_rung hand_over make_run_camera change_rung rung_named fall_line_ladder
// ge:skill camera
// ge:test tests/fall_line_camera_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "camera/camera.hpp"

namespace fall_line::camera {

enum class Rung : uint8_t { tips, helmet, chase, far, high, orbit };
inline constexpr int kRungs = 6;

// The rows, in `Rung` order (a camera's rung index is the Rung's number).
std::span<const ge::camera::Rig> fall_line_ladder();

std::string_view rung_name(Rung r);
// The rung named `name` into `out`; false for a name not on the ladder.
bool rung_named(std::string_view name, Rung& out);

// The camera key: the next rung of tips, helmet, chase, far, high, wrapping; from the orbit, the
// chase.
Rung next_rung(Rung r);

struct HandOver {
  ge::camera::Edit edit;
  double seconds;
};
// How the picture is carried from `from` to `to`.
HandOver hand_over(Rung from, Rung to);

// A run's camera on the ladder, on `start`.
ge::camera::Camera make_run_camera(Rung start, uint32_t seed);
// The rung the camera is on.
Rung rung_of(const ge::camera::Camera& cam);
// Change rung by the hand-over `hand_over` names (nothing when it is already on it).
void change_rung(ge::camera::Camera& cam, Rung to);

}  // namespace fall_line::camera
