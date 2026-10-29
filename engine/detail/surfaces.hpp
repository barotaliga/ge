// WHAT A SURFACE IS MADE OF, from the little a level gives: its texture's name and, where the
// level carries the texture's pixels, their mean colour. The clutter a floor takes is chosen by
// this class (props.hpp's `PropRule::classes`, `clutter_rules`): grass on grass, stones on
// stone, shells on a metal deck.
//
// - BY NAME (`classify_name`): words in the name, matched as substrings of its lower case, in a
//   fixed order so a "mossy rock" is stone and a "grass edge" is grass. A name with none of the
//   words is `unknown`.
// - BY COLOUR (`classify_surface`): a green texture (green above red and blue by a margin, not
//   too dark) is grass unless its name says it is something no grass grows on (metal, wood,
//   tile, sky, liquid); so an unnamed green floor and a "ground" painted green both grow it.
//
// ge:summary What a surface is made of, from its texture's name and its mean colour
// ge:keywords detail surface class texture name colour classify grass stone dirt wood metal
// ge:keywords tile sand sky liquid classify_surface clutter
// ge:skill detail
// ge:test tests/detail_clutter_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace ge::detail {

enum class SurfaceClass : uint8_t {
  unknown,
  stone,
  dirt,
  grass,
  sand,
  wood,
  metal,
  tile,
  sky,
  liquid
};
inline constexpr int kSurfaceClasses = 10;
std::string_view surface_name(SurfaceClass c);

// A bit of a `PropRule::classes` mask.
constexpr uint32_t class_bit(SurfaceClass c) { return 1u << static_cast<int>(c); }

// The class a texture's name says, or `unknown`.
SurfaceClass classify_name(std::string_view texture_name);

// The class of a texture by its name and, when `mean_rgb` is given (linear or display 0..1, three
// values), its colour.
SurfaceClass classify_surface(std::string_view texture_name, const double* mean_rgb = nullptr);

}  // namespace ge::detail
