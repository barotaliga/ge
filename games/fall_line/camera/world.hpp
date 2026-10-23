// WHAT FALL LINE'S CAMERA ASKS OF THE MOUNTAIN: the three queries of engine/camera/world.hpp over
// a map and its trees' index, with no allocation.
//
//   ground      the map's own surface (the bilinear sample the skier stands on)
//   obstacles   the TRUNKS round a point (skier/trees.hpp's grid), from the ground at each to its
//               top: a boom is pushed sideways off them, never pulled in at the skier for a tree
//               flicking past
//   clear       the course's POSTS: each gate's two, `kPostHeight` m tall and padded to
//               `kPostPad` m, which a boom pulls its arm in for, so no lens stands inside one
//
// A trunk is answered by `obstacles` alone and a post by `clear` alone (world.hpp's rule: a
// thing in both would be pulled onto while being pushed off).
//
// ge:summary The mountain's answers to the camera: ground, the trunks to push off, the posts
// ge:keywords fall-line camera world ground trunks obstacles posts gates clear line-of-sight
// ge:keywords camera_world post-pad
// ge:skill camera
// ge:test tests/fall_line_camera_test.cpp
#pragma once

#include "camera/world.hpp"
#include "skier/skier.hpp"

namespace fall_line::camera {

// A gate's post as the camera keeps clear of it: its height over the gate's ground and the
// radius the lens is kept out of, m (a post's own few centimetres and a near plane's margin).
inline constexpr double kPostHeight = 1.8;
inline constexpr double kPostPad = 0.3;

// The camera's world over `skier_world`, which must outlive it.
ge::camera::World camera_world(const ge::skier::World& skier_world);

}  // namespace fall_line::camera
