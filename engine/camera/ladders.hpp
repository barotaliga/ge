// THE REFERENCE LADDER: one rig of each kind at sensible numbers, the rows a game starts its own
// ladder from and the set the camera's labs, tests and goldens run. A game copies a row and
// changes what makes its camera its own; it never edits these to tune a game (they hold
// tests/golden/camera_scenarios.txt).
//
//   eyes       first person, standing eyes at 1.62 m, the classic stride, roll and kick
//   chase      a boom 4.3 m behind and 1.7 m up, with the full sense of speed
//   far        a boom 10 m back and 3.8 m up, heavier
//   high       a boom 13 m back and 10 m up, the heaviest, no stretch
//   helmet     mounted at the head, a neck under it
//   tips       mounted low over the feet, looking down the way
//   orbit      the menu's drone
//   trackside  planted 12 m off the line 30 m along it, zooming to hold a 4 m frame
//
// ge:summary The reference ladder: one rig of each kind, the rows games start from and labs run
// ge:keywords camera ladder reference rows defaults eyes chase far high helmet tips orbit
// ge:keywords trackside reference_ladder
// ge:skill camera
// ge:test tests/camera_camera_test.cpp
#pragma once

#include <span>

#include "camera/camera.hpp"

namespace ge::camera {

std::span<const Rig> reference_ladder();

}  // namespace ge::camera
