// `ge texture`: procedural texture recipes from the command line (engine/texgen,
// docs/textures.md).
//
//   ge texture kinds                    every layer kind, its keys, their defaults and ranges
//   ge texture check <file.tex>...      parse each recipe; every mistake a finding at its line
//   ge texture bake <file.tex> [--ref="seed=12 wear=0.8"] [--size=N] [--out=<dir>]
//                                       bake one variant into the cache directory (default
//                                       build/textures): the maps as PNG and the .meta; a
//                                       variant already there is reused, not baked again
//   ge texture match <dir> [--map=<file>] [--only=a,b] [--top=N] ...
//                                       the agent's loop over a converted map's textures (PNGs
//                                       in <dir>, kept outside the tree): each one's reference
//                                       written to the texture map, re-picked unless pinned,
//                                       scored, its nearest recipes listed, and sheets of the
//                                       farthest drawn beside their candidates
//
// ge:summary ge texture: list layer kinds, check recipes, bake a variant to PNG in a cache
// ge:keywords cli ge texture texgen recipe check bake kinds cache png reference match texmap
// ge:keywords convert agent-loop pin candidates sheets
// ge:skill agent-interface textures
// ge:test tests/cli_texture_test.cpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_texture_kinds();
Outcome run_texture_check(const std::filesystem::path& repo, const std::vector<std::string>& files);
struct TextureMatchOptions {
  std::string dir;        // the textures, as PNGs
  std::string map;        // the texture map (default <dir>/textures.texmap)
  std::string library;    // the recipes (default engine/texgen/library)
  std::string only;       // comma-separated texture names to draw sheets for
  std::string sheet_dir;  // where the sheets go (default <dir>/sheets)
  int top = 4;            // candidates listed and drawn per texture
  int sheets = 8;         // sheets drawn when --only is empty: the farthest textures
  int list = 20;          // textures printed, farthest first
  double near = 0.5;      // over this a texture is far
};

Outcome run_texture_match(const std::filesystem::path& repo, const TextureMatchOptions& options);
Outcome run_texture_bake(const std::filesystem::path& repo, const std::string& file,
                         const std::string& ref, const std::string& size, const std::string& out);

}  // namespace ge::cli
