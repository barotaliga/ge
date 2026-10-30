// FIXTURE FILES in the classic model and sprite formats, written from a `morph::Model` or a
// `morph::Sprite` so the tests and labs read real bytes without a classic file in the tree.
//
// - `write_model` packs every frame's positions into bytes over the model's bounds (the format's
//   own precision: 1/255 of the extent on each axis), joins vertices that share a position and
//   whose coordinates differ by half the skin's width into one on-seam vertex, and marks a
//   triangle back-facing when it reads such a vertex's right half. What it writes reads back
//   into the same model, positions rounded to the packing.
// - `sample_model` and `sample_sprite` are the fixtures: a small eight-sided body with a seam
//   down its back, a lone skin and a group of two, a lone frame and a group of two; a sprite with
//   a lone frame and a group of two.
//
// ge:summary Writes classic model and sprite files from the view, and the fixtures the tests read
// ge:keywords write fixture mdl spr model sprite pack seam group sample
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "morph/model.hpp"

namespace ge::morph {

std::vector<uint8_t> write_model(const Model& m);
std::vector<uint8_t> write_sprite(const Sprite& s);

Model sample_model();
Sprite sample_sprite();

}  // namespace ge::morph
