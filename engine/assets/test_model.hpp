// THE TEST MODEL: the smallest model that exercises every part of the path from a file to a
// posed mesh on the GPU, built in code so the tests and the `models` lab need no file in the
// tree. A LIMB: a six-sided tube 1 m tall standing on the origin, skinned to three joints in a
// chain up +y (`root` at 0, `elbow` at 0.4 m, `wrist` at 0.8 m) with the weights blended across
// each joint; a FIN (a rigid box on the wrist, its own material, bound whole to a fourth joint,
// `fin`, under the wrist); and two clips: `wave` (1 s: the elbow bends to the right and back,
// the wrist follows half a beat later) and `twist` (0.5 s: the root turns a quarter round about
// +y). Two materials: `skin` (the tube) and `fin`.
//
// ge:summary The test model: a skinned limb with a rigid fin and two clips, built in code
// ge:keywords assets test-model limb tube fin wave twist fixture make_test_model
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#pragma once

#include "assets/model.hpp"

namespace ge::assets {

Model make_test_model();

}  // namespace ge::assets
