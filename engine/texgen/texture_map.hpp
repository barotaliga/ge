// A TEXTURE MAP (`<map>.texmap`): what each of a converted level's textures becomes, as a text
// file that `ge texture match` writes, an agent steers, and a map converter reads. One line a
// texture, keyword first:
//
//   texture <name> <reference>             a wall, a floor: a recipe reference
//   liquid <name> <kind> [<reference>]     a liquid (a name starting '*'): water, slime, lava, ...
//   sky <name> [<reference>]               the sky
//   surface <name> <surface reference>     a procedural surface drawn in its place (grass, ...)
//   class <name> <class>                   1. what the texture is: bricks, blocks, planks,
//                                          panels, plate, tiles, rock, metal, ... (a recipe tag)
//   traits <name> <description>            2. how a texture of that class looks, and this one:
//                                          the laying pattern, the pieces' size and count a
//                                          tile, the joints, the colour
//   judge <name> pass|fail <reason>        4. the clone (3, the texture line) held against
//                                          those traits, trait by trait
//   # ...                                  a comment
//
// A reference is a texture reference (`recipe.hpp`): `red_bricks seed=7 wear=0.4 tint=#825540
// spread=#281913`. A comment starts at a '#' that begins the line or follows a space, so a tint's
// '#' is not one. A line the agent has approved or tuned ends in `# pin`, and has all three notes:
// `ge texture match` never rewrites it, and re-picks every other texture line, appending its
// distance (`# 0.31`,
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
  std::string
      keyword;  // texture, liquid, sky, surface, class, traits, judge; "" for a comment or another
  std::string texture;  // the level's own name for it ("city2_1", "*lava1", "+0slip")
  std::string value;    // the rest: the reference, or "<kind> [<reference>]" on a liquid
  bool pinned = false;  // the agent's: never rewritten
  std::string note;     // the trailing comment past `pin`, e.g. the distance
  std::string raw;      // a comment or an unknown line, kept verbatim
};

struct TextureMap {
  std::vector<TextureMapLine> lines;  // in file order
  // The texture's line (texture, liquid, sky or surface), or null.
  TextureMapLine* find(std::string_view texture);
  const TextureMapLine* find(std::string_view texture) const;
  // The texture's `class`, `traits` or `judge` text, or "".
  std::string note(std::string_view keyword, std::string_view texture) const;
};

// Read a map; each malformed line of a known kind is a diagnostic at its line number.
TextureMap parse_texture_map(std::string_view text, std::vector<Diagnostic>& errors);
// The map as text, line for line.
std::string texture_map_text(const TextureMap& map);

}  // namespace ge::texgen
