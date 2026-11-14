// WHAT A SURFACE IS MADE OF, from its texture's name: a hit's impact (sparks off metal, dust off
// stone, splinters off wood) is chosen by this class.
//
// `classify_name` looks for words in the name, matched as substrings of its lower case, in a fixed
// order so a "mossy rock" is stone and a "grass edge" is grass. A name with none of the words is
// `unknown`.
//
// ge:summary What a surface is made of, from its texture's name
// ge:keywords detail surface class texture name classify grass stone dirt wood metal
// ge:keywords tile sand sky liquid classify_name
// ge:skill detail
// ge:test tests/detail_surfaces_test.cpp
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

// The class a texture's name says, or `unknown`.
SurfaceClass classify_name(std::string_view texture_name);

}  // namespace ge::detail
