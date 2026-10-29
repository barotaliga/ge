// THE GRADE'S KNOBS: a player's turn of the picture's colour (contrast, saturation, white balance,
// gamma), baked into the post pass's lookup table (render/post.hpp's `set_grade`) after the game's
// own grade, so turning them costs the frame nothing: the table is read once a pixel either way.
// Pure CPU, no GPU: the table is identity_grade's layout (sRGB-encoded colours in and out).
//
// Each knob is neutral at its default and leaves black and white where they are:
//
//   contrast     1    an S-curve about the encoded middle grey: above 1 steeper, below 1 flatter
//   saturation   1    the colour's distance from its luminance (linear light): 0 grey, 2 doubled
//   temperature  0    -1 cold (blue) .. 1 warm (amber): a white balance in linear light that
//                     keeps the luminance
//   tint         0    -1 green .. 1 magenta, the same way
//   gamma        1    the middle tones brighter above 1, darker below (encoded ^ 1 / gamma)
//
// ge:summary The grade's knobs (contrast, saturation, temperature, tint, gamma) baked into the LUT
// ge:keywords render grade colour-grade color-grade lut contrast saturation temperature tint
// ge:keywords white-balance gamma brightness apply_grade_knobs grade_neutral r_contrast
// ge:skill render
// ge:test tests/render_grade_test.cpp
#pragma once

#include <span>

namespace ge::render {

struct GradeKnobs {
  double contrast = 1;
  double saturation = 1;
  double temperature = 0;
  double tint = 0;
  double gamma = 1;
};

// True when every knob is at its neutral value (applying them changes nothing).
bool grade_neutral(const GradeKnobs& k);

// The knobs applied after `table` (identity_grade's layout, kGradeSize³ RGB triples), in place:
// each entry's colour becomes the knobs' answer for it, clamped to 0..1.
void apply_grade_knobs(std::span<float> table, const GradeKnobs& k);

// One sRGB-encoded colour through the knobs (what apply_grade_knobs does to each entry).
void grade_colour(const GradeKnobs& k, float rgb[3]);

}  // namespace ge::render
