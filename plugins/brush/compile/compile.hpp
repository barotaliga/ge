// THE MAP COMPILER: a map's source (source.hpp: brushes and entities, from a `.map` file, a plan or
// an SVG floor plan) compiled into a playable BSP of the version 29 family, the way any brush
// map is: the world's brushes split into a tree (tree.hpp), sealed against the void (a leak is
// refused with where it is), faces where open space meets a surface, the player's and the large
// box's clip hulls, the faces lit by the map's lights with shadows (light.hpp), and every brush
// entity (a door, a lift, a trigger) a model of its own (`*1`, `*2` ...).
//
// Textures come from `CompileOptions::textures`, asked once a name: a game hands its own pictures
// (the player's packs' maps, say), and a name nothing answers is drawn as a pattern of its own.
// The bytes are a pure function of the source, the options and the pictures.
//
// No visibility is computed yet: every leaf sees every other (the file's rows are absent, which
// readers take as all visible; a version 38 map has one cluster that sees itself).
//
// VERSION 38 (`CompileOptions::version`): the same tree, faces and light, laid out in the
// family's nineteen lumps (write38.cpp): planes in pairs (each and its flip), texinfo naming its
// texture with the side's surface flags and value, RGB luxels, and the brushes themselves (each
// with axial bevels where it has none, so a box sweeps against it cleanly), every leaf listing the
// brushes in it; clip brushes are brushes that only stop bodies. No clip hulls: the family
// sweeps boxes through the brushes.
//
// ge:summary The map compiler: brushes and entities into a version 29 BSP with hulls and light
// ge:keywords map-compiler compile_map bsp v29 compile leak hull clip-hull lightmap brush-model
// ge:keywords texture-finder miptex map-build qbsp-like
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "brush/compile/source.hpp"

namespace ge::brush::mapc {

// A texture's picture: palette indices, four levels (each half the last), the first
// `width × height`.
struct TextureImage {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> levels[4];
};
// The picture for a texture name; false when the finder has none (the compiler draws a pattern).
using TextureFinder = std::function<bool(const std::string& name, TextureImage* out)>;

struct CompileOptions {
  bool light = true;       // false: every luxel at the world's least light
  TextureFinder textures;  // may be empty (version 29 only: a 38 map names its pictures)
  int version = 29;        // 29, or 38 (the brushes kept for the trace, RGB light, no hulls)
};

struct CompileReport {
  size_t brushes = 0, planes = 0, nodes = 0, leaves = 0, faces = 0, portals = 0;
  size_t clip_nodes = 0, models = 0, textures = 0, textures_found = 0, lights = 0, luxels = 0;
  size_t faces_without_side =
      0;  // faces that found no brush side on their plane (a texture guessed)
  std::vector<std::string> warnings;
  // A leak: the entity the flood started from (its class and place) and the way out, from its
  // origin through the middle of each opening to the void.
  bool leaked = false;
  std::string leak_entity;
  std::vector<V3> leak_path;
};

// `src` compiled into `*out` (a version 29 or 38 map, `options.version`); false with the reason in
// `*error` (a leak, no world brushes, no entity to flood from, a map past the family's limits).
bool compile_map(const Source& src, const CompileOptions& options, std::vector<uint8_t>* out,
                 CompileReport* report, std::string* error);

}  // namespace ge::brush::mapc
