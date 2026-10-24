// A TEXTURE MAP (`<map>.texmap`): what each of a converted level's textures becomes, as a text
// file that `ge texture match` writes, an agent steers, and a map converter reads. One line a
// texture, keyword first:
//
//   texture <name> <reference>             a wall, a floor: a recipe reference
//   liquid <name> <kind> [<reference>]     a liquid (a name starting '*'): water, slime, lava, ...
//   sky <name> [<reference>]               the sky
//   surface <name> <surface reference>     a procedural surface drawn in its place (grass, ...)
//   # ...                                  a comment
//
// A reference is a texture reference (`recipe.hpp`): `red_bricks seed=7 wear=0.4 tint=#825540
// spread=#281913`. A comment starts at a '#' that begins the line or follows a space, so a tint's
// '#' is not one. A line the agent has approved or tuned ends in `# pin`: `ge texture match`
// never rewrites it, and re-picks every other texture line, appending its distance (`# 0.31`,
// `# pin 0.31`). Lines of a kind this reader does not know are kept as they are. Nothing in the
// file is anyone's pixels, so it may be committed with the level.
//
// ge:summary The texture map file: each converted texture's reference, pinned or picked
// ge:keywords texture-map texmap file convert converter pin reference agent loop liquid sky
// ge:skill textures
// ge:test tests/texgen_texture_map_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "texgen/recipe.hpp"

namespace ge::texgen {

struct TextureMapLine {
  std::string keyword;  // texture, liquid, sky, surface; "" for a comment or an unknown line
  std::string texture;  // the level's own name for it ("city2_1", "*lava1", "+0slip")
  std::string value;    // the rest: the reference, or "<kind> [<reference>]" on a liquid
  bool pinned = false;  // the agent's: never rewritten
  std::string note;     // the trailing comment past `pin`, e.g. the distance
  std::string raw;      // a comment or an unknown line, kept verbatim
};

struct TextureMap {
  std::vector<TextureMapLine> lines;  // in file order
  TextureMapLine* find(std::string_view texture);
  const TextureMapLine* find(std::string_view texture) const;
};

// Read a map; each malformed line of a known kind is a diagnostic at its line number.
TextureMap parse_texture_map(std::string_view text, std::vector<Diagnostic>& errors);
// The map as text, line for line.
std::string texture_map_text(const TextureMap& map);

}  // namespace ge::texgen
