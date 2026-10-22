// A MODEL WRITTEN AS glTF: a `Model` back out as a binary glTF 2.0 file (.glb), mirrored back
// into glTF's frame. It is the loader's other half for the tests and the labs: the test model
// (assets/test_model.hpp) goes through a real file and the loader on its way to the GPU, and a
// load of a write poses as the model it was written from (tests/assets_gltf_test.cpp).
//
// The file: one node a joint (its TRS) and one more a mesh (a root, named after the mesh, with
// the skin of every joint), one primitive a material a mesh, the inverse binds, and one
// animation a clip with a LINEAR channel for each joint's translation, rotation and scale at the
// clip's baked frames. The loader turns each mesh node into a joint of its own, so a load has
// the written joints first and one more a mesh after them.
//
// ge:summary A Model written as a binary glTF 2.0 file, for the tests and the labs
// ge:keywords assets gltf glb write export writer round-trip save_glb write_glb
// ge:skill assets
// ge:test tests/assets_gltf_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "assets/model.hpp"

namespace ge::assets {

std::vector<uint8_t> write_glb(const Model& model);

}  // namespace ge::assets
