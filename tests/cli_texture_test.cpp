// engine/cli/texture.hpp: `ge texture kinds` lists the layer table, `check` turns a recipe's
// mistakes into findings at their lines, `bake` writes a variant once and reuses it after, and
// `match` writes a texture map, keeps the agent's pinned lines and asks for their argument.
//
// ge:summary Tests ge texture: kinds, check findings, bake and reuse in a cache directory
// ge:keywords test cli texture kinds check bake findings cache
// ge:skill testing agent-interface textures
#include "cli/texture.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"
#include "lab/png.hpp"
#include "texgen/bake.hpp"
#include "texgen/library.hpp"
#include "texgen/preview.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

TEST(kinds_lists_every_layer_kind) {
  const Outcome o = run_texture_kinds();
  CHECK(o.text.find("layer bricks") != std::string::npos);
  CHECK(o.text.find("layer glow") != std::string::npos);
  CHECK(exit_of(o) == 0);
}

TEST(check_and_bake_through_the_cli) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_texture_test";
  fs::remove_all(repo);
  fs::create_directories(repo);
  std::ofstream(repo / "good.tex") << "texture good\nsize 32\nparam age 0.5 0 1\n"
                                      "layer bricks rows=2 cols=2\nlayer grime amount=$age\n";
  std::ofstream(repo / "bad.tex") << "texture bad\nlayer fill colr=#ffffff\n";
  const Outcome bad = run_texture_check(repo, {"good.tex", "bad.tex"});
  CHECK(exit_of(bad) == 1);
  CHECK(bad.findings.size() == 1 && bad.findings[0].where == "bad.tex:2");
  const Outcome first = run_texture_bake(repo, "good.tex", "seed=4 age=0.9", "", "cache");
  CHECK(exit_of(first) == 0);
  CHECK(first.written.size() == 5);  // albedo, normal, orm, height, meta: it does not glow
  const Outcome again = run_texture_bake(repo, "good.tex", "seed=4 age=0.9", "", "cache");
  CHECK(again.written.empty() && again.summary.find("reused") != std::string::npos);
  const Outcome wrong = run_texture_bake(repo, "good.tex", "wear=1", "", "cache");
  CHECK(exit_of(wrong) == 1);
  const Outcome sized = run_texture_bake(repo, "good.tex", "", "100", "cache");
  CHECK(exit_of(sized) == 1);
  fs::remove_all(repo);
}

TEST(match_writes_a_map_keeps_pins_and_argues) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_texture_match_test";
  fs::remove_all(repo);
  fs::create_directories(repo / "tex");
  // Two "level" textures: the library's own bricks and planks under another seed.
  for (const char* name : {"brick_wall", "wood_planks"}) {
    ge::texgen::ParseResult r =
        ge::texgen::load_recipe(ge::test::root_path(ge::texgen::kLibraryDir) + "/" + name + ".tex");
    r.recipe.size = 32;
    const ge::texgen::Maps maps = ge::texgen::bake(r.recipe, {name, 9u, {}}).maps;
    CHECK(ge::lab::write_png((repo / "tex" / (std::string(name) + ".png")).string(),
                             ge::texgen::map_picture(maps, ge::texgen::MapKind::albedo, 32)));
  }
  TextureMatchOptions opt;
  opt.dir = "tex";
  opt.library = ge::test::root_path(ge::texgen::kLibraryDir);
  opt.top = 2;
  const Outcome first = run_texture_match(repo, opt);
  CHECK(exit_of(first) == 0);
  CHECK(fs::exists(repo / "tex" / "textures.texmap"));
  CHECK(fs::exists(repo / "tex" / "sheets" / "brick_wall.png"));
  CHECK(first.text.find("original: ") != std::string::npos);
  CHECK(first.text.find(" courses, pieces a course") != std::string::npos);
  // The agent pins the planks as bricks without arguing: kept, and warned about.
  {
    std::ofstream out(repo / "tex" / "textures.texmap", std::ios::app);
    out << "class brick_wall bricks\n";
  }
  std::ifstream in(repo / "tex" / "textures.texmap");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  const size_t at = text.find("texture wood_planks ");
  CHECK(at != std::string::npos);
  text.replace(at, text.find('\n', at) - at, "texture wood_planks brick_wall seed=3  # pin");
  std::ofstream(repo / "tex" / "textures.texmap") << text;
  const Outcome second = run_texture_match(repo, opt);
  std::ifstream again(repo / "tex" / "textures.texmap");
  const std::string after((std::istreambuf_iterator<char>(again)),
                          std::istreambuf_iterator<char>());
  CHECK(after.find("texture wood_planks brick_wall seed=3  # pin") != std::string::npos);
  CHECK(after.find("class brick_wall bricks") != std::string::npos);
  bool warned = false, uncoloured = false;
  for (const Finding& f : second.findings) {
    warned |= f.what.find("wood_planks is pinned without its argument") != std::string::npos;
    uncoloured |=
        f.what.find("wood_planks is pinned without the original's colour") != std::string::npos &&
        f.next.find("texture wood_planks brick_wall seed=3 tint=#") != std::string::npos;
  }
  CHECK(warned);
  CHECK(uncoloured);
  fs::remove_all(repo);
}

TEST(match_takes_real_names_from_a_level_export_list) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_texture_list_test";
  fs::remove_all(repo);
  fs::create_directories(repo / "tex");
  ge::texgen::ParseResult r =
      ge::texgen::load_recipe(ge::test::root_path(ge::texgen::kLibraryDir) + "/brick_wall.tex");
  r.recipe.size = 32;
  const ge::texgen::Maps maps = ge::texgen::bake(r.recipe, {"brick_wall", 5u, {}}).maps;
  CHECK(ge::lab::write_png((repo / "tex" / "anim_0wall_2.png").string(),
                           ge::texgen::map_picture(maps, ge::texgen::MapKind::albedo, 32)));
  std::ofstream(repo / "tex" / "textures.txt")
      << "# name file size\n+0wall-2 anim_0wall_2.png 32x32\n";
  TextureMatchOptions opt;
  opt.dir = "tex";
  opt.library = ge::test::root_path(ge::texgen::kLibraryDir);
  opt.sheets = 0;
  CHECK(exit_of(run_texture_match(repo, opt)) == 0);
  std::ifstream in(repo / "tex" / "textures.texmap");
  const std::string map((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(map.find("texture +0wall-2 ") != std::string::npos);  // the listed name, not the file's
  in.close();
  // Called a logo, it is never cloned: re-picked from the blank recipes, and a pinned lookalike
  // is warned about.
  std::ofstream(repo / "tex" / "textures.texmap", std::ios::app) << "class +0wall-2 logo\n";
  CHECK(exit_of(run_texture_match(repo, opt)) == 0);
  std::ifstream blank(repo / "tex" / "textures.texmap");
  std::string text((std::istreambuf_iterator<char>(blank)), std::istreambuf_iterator<char>());
  blank.close();
  CHECK(text.find("texture +0wall-2 blank_plate ") != std::string::npos);
  const size_t at = text.find("texture +0wall-2 ");
  text.replace(at, text.find('\n', at) - at, "texture +0wall-2 brick_wall  # pin");
  std::ofstream(repo / "tex" / "textures.texmap") << text;
  bool lookalike = false;
  for (const Finding& f : run_texture_match(repo, opt).findings)
    lookalike |= f.what.find("a mark is never cloned") != std::string::npos;
  CHECK(lookalike);
  fs::remove_all(repo);
}

TEST(match_judges_a_cluster_and_draws_tiled_sheets) {
  const fs::path repo = fs::temp_directory_path() / "ge_cli_texture_cluster_test";
  fs::remove_all(repo);
  fs::create_directories(repo / "tex");
  for (const char* name : {"brick_wall", "red_bricks"}) {
    ge::texgen::ParseResult r =
        ge::texgen::load_recipe(ge::test::root_path(ge::texgen::kLibraryDir) + "/" + name + ".tex");
    r.recipe.size = 32;
    const ge::texgen::Maps maps = ge::texgen::bake(r.recipe, {name, 6u, {}}).maps;
    CHECK(ge::lab::write_png((repo / "tex" / (std::string(name) + ".png")).string(),
                             ge::texgen::map_picture(maps, ge::texgen::MapKind::albedo, 32)));
  }
  std::ofstream(repo / "tex" / "textures.texmap")
      << "cluster walls brick_wall red_bricks nowhere\n";
  TextureMatchOptions opt;
  opt.dir = "tex";
  opt.library = ge::test::root_path(ge::texgen::kLibraryDir);
  const Outcome o = run_texture_match(repo, opt);
  CHECK(exit_of(o) == 0);
  CHECK(fs::exists(repo / "tex" / "sheets" / "cluster_walls.png"));
  CHECK(o.text.find("cluster walls: brick_wall red_bricks") != std::string::npos);
  bool unjudged = false, unknown = false;
  for (const Finding& f : o.findings) {
    unjudged |= f.what == "cluster walls is not judged";
    unknown |= f.what.find("`nowhere` is not a texture line") != std::string::npos;
  }
  CHECK(unjudged && unknown);
  fs::remove_all(repo);
}
