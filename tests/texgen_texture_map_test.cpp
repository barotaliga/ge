// engine/texgen/texture_map.hpp: a map reads and writes back to itself line for line (comments
// and unknown lines kept), a tint's '#' is not a comment, pins and notes are read off the
// trailing comment, and every malformed line of a known kind is a diagnostic at its line.
//
// ge:summary Tests the texture map file: round trip, pins, tints, liquids and skies, mistakes
// ge:keywords test texgen texture-map texmap pin parse write
// ge:skill testing textures
#include "texgen/texture_map.hpp"

#include "check.hpp"

namespace tg = ge::texgen;

TEST(a_map_reads_and_writes_back) {
  const std::string text =
      "# texture map for e1m1\n"
      "texture city2_1 red_bricks seed=7 tint=#825540 spread=#281913  # pin 0.31\n"
      "texture metal1_4 bolted_iron seed=12  # 0.21\n"
      "liquid *lava1 lava\n"
      "sky sky4\n"
      "surface grass1 grass long=0.6\n"
      "class city2_1 bricks\n"
      "traits city2_1 running bond, 8 courses a tile, #825540 red-brown, pale mortar\n"
      "judge city2_1 pass bond and courses match\n"
      "cluster red_wall city2_1 metal1_4\n"
      "light lamp1 300\n";
  std::vector<tg::Diagnostic> errors;
  const tg::TextureMap map = tg::parse_texture_map(text, errors);
  CHECK(errors.empty());
  const tg::TextureMapLine* bricks = map.find("city2_1");
  CHECK(bricks && bricks->pinned && bricks->note == "0.31");
  CHECK(bricks->value == "red_bricks seed=7 tint=#825540 spread=#281913");
  CHECK(map.find("metal1_4") && !map.find("metal1_4")->pinned);
  CHECK(map.note("cluster", "red_wall") == "city2_1 metal1_4");
  CHECK(map.find("*lava1")->value == "lava" && map.find("sky4")->value.empty());
  CHECK(map.find("lamp1") == nullptr);
  CHECK(map.note("class", "city2_1") == "bricks");
  CHECK(map.note("judge", "city2_1") == "pass bond and courses match");
  CHECK(
      map.note("traits", "city2_1") ==
      "running bond, 8 courses a tile, #825540 red-brown, pale mortar");  // another reader's line,
                                                                          // kept but not a texture
  CHECK(tg::texture_map_text(map) == text);
}

TEST(mistakes_name_their_line) {
  std::vector<tg::Diagnostic> errors;
  tg::parse_texture_map(
      "texture a brick_wall\ntexture b\ntexture c brick_wall tint=red\ntexture a concrete\n",
      errors);
  CHECK(errors.size() == 3);
  CHECK(errors[0].line == 2 && errors[1].line == 3 && errors[2].line == 4);
}
