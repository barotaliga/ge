// A TEXTURE'S LOOK IN NUMBERS: what an eye reads off a tile, measured off its pixels, so a
// recipe can be steered toward a reference without copying a pixel of it. The reference is only
// ever read for these few numbers and looked at beside the bake; nothing of it enters a recipe
// but what an agent writes after looking (`texgen-compare`, the `texture-references` skill).
//
//   brightness  the mean luma, 0..1
//   contrast    the luma's spread (its standard deviation), 0..0.5
//   saturation  the mean chroma (max - min channel), 0..1
//   warmth      the mean red minus blue, -1..1
//   grain       the mean step from a texel to the mean of its eight neighbours, over the mean
//               luma: the fine, per-texel noise (high on speckled metal, low on smooth paint)
//   edges       the share of texels where the luma steps by more than 0.08 to a neighbour: how
//               much line and structure there is
//   colours     how many distinct colours at five bits a channel, over the texels: near 0 for a
//               few-colour palette, toward 1 for a smooth gradient image
//
// ge:summary A texture's look in numbers (brightness, contrast, saturation, grain, edges)
// ge:keywords look stats statistics compare reference brightness contrast saturation warmth
// ge:keywords grain edges palette colours distance texgen look_of look_distance
// ge:skill textures
// ge:test tests/texgen_look_test.cpp
#pragma once

#include <array>
#include <string>

#include "lab/raster.hpp"

namespace ge::texgen {

struct Look {
  double brightness = 0, contrast = 0, saturation = 0, warmth = 0, grain = 0, edges = 0,
         colours = 0;
};

inline constexpr std::array<const char*, 7> kLookNames = {
    "brightness", "contrast", "saturation", "warmth", "grain", "edges", "colours"};

// The look of a picture (its alpha ignored), read as a tile: neighbours wrap.
Look look_of(const lab::Picture& p);

// The numbers in kLookNames' order.
std::array<double, 7> look_values(const Look& l);

// How far apart two looks are: each number's difference over its usual spread, the root of the
// mean square. Under about 0.5 two tiles read as the same kind of surface.
double look_distance(const Look& a, const Look& b);

}  // namespace ge::texgen
