// THE PICTURES A MAP IS BUILT WITH: a texture name answered from the maps a folder already has.
// Every pack (`*.pak`) and every loose `.bsp` found in the folder (and its folders one deep) is
// read, packs first, and each version 29 map's own pictures kept by name, the first found winning;
// the compiler asks the library for each texture its map names (compile.hpp `TextureFinder`). The
// pictures stay on the player's machine: a built map carries them as every map of its game does.
//
// ge:summary The textures a map is built with, taken by name from the maps a folder already has
// ge:keywords map-compiler texture library miptex pictures pak folder texture-finder
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <map>
#include <string>

#include "brush/compile/compile.hpp"

namespace ge::brush::mapc {

struct TextureLibrary {
  std::map<std::string, TextureImage> by_name;  // lower case
  size_t maps = 0;                              // the maps read
};

// The library from `folder`; false with the reason when it holds no map at all.
bool load_texture_library(const std::string& folder, TextureLibrary* out, std::string* error);
// A finder over the library (it must outlive the finder).
TextureFinder finder_of(const TextureLibrary& lib);

}  // namespace ge::brush::mapc
