// The classicenhance lab's own textures: a 256-colour palette in the classic layout (sixteen
// ramps of sixteen shades, the last two ramps fullbright) and six 64 × 64 pictures painted in
// code with it (brick, a metal panel with a glowing strip, wood planks, hazard stripes, rock,
// water), every one tiling. They stand in for the classic pack, whose pixels never enter the
// tree, so the lab's pictures and digest are the same everywhere; a held weapon made of boxes; and
// a whole pack made of the brush and morph plugins' fixtures, for the labs that run the pipeline.
//
// ge:summary Fixture palette and six painted, tiling textures for the classicenhance lab
// ge:keywords fixture palette texture brick metal wood hazard rock water tile painted pack
// ge:skill classicenhance labs
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "classicenhance/texture.hpp"
#include "morph/model.hpp"

namespace ge::classicenhance::labs {

struct FixtureTexture {
  std::string name;
  int width = 64, height = 64;
  std::vector<uint8_t> indexed;
};

Palette fixture_palette();
std::vector<FixtureTexture> fixture_textures(const Palette& palette);
// The index of the palette colour nearest (r, g, b) among the entries below the fullbright ones.
uint8_t nearest_index(const Palette& palette, int r, int g, int b);
// The labs' held weapon: a receiver, a barrel and a glowing sight made of boxes, authored at the
// eye as the classic files author theirs, a rest frame and a recoiled one, written as a classic
// model file and read back (so the labs go through the reader the pack's weapons go through).
morph::Model fixture_weapon(const Palette& palette);
// A pack the pipeline runs every stage on: the palette, two maps of the brush plugin's fixture,
// a looping sound, the morph plugin's fixture model and sprite, and a picture in a wad.
std::vector<uint8_t> fixture_pack(const Palette& palette);

}  // namespace ge::classicenhance::labs
