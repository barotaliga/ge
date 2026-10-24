// LAB `texgen-replace`: a level's painted textures beside the procedural replacements the
// likeness matcher picks for them (`replace.hpp`). The originals are PNGs an agent keeps OUTSIDE
// the tree for one session (a map's own textures, written out by a tool of its own): the lab
// reads each one, asks the matcher for its replacement, and draws the two side by side with the
// recipe it picked and how unalike they were, a page of pairs at a time, plus a table. A texture
// that keeps its own pixels (a liquid, a sky, nothing near enough) is drawn with KEPT beside it.
//
// Files are named after their textures, with "liq_" for a leading '*' and "anim_" for a '+'.
//
// With no --refs, the originals are the library's own recipes under seed 9, each tinted, so the
// smoke run and its digest need nothing outside the tree and show the recolouring at work.
//
// ge:summary Lab: a level's painted textures beside their likeness replacements, with the pick
// ge:keywords lab texgen replace replacement likeness match side-by-side original procedural
// ge:keywords option level-textures miptex
// ge:skill textures labs
// ge:test tests/texgen_replace_test.cpp
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "texgen/bake.hpp"
#include "texgen/library.hpp"
#include "texgen/preview.hpp"
#include "texgen/replace.hpp"

namespace {

using namespace ge;

struct Original {
  std::string name;
  lab::Picture picture;
};

// The library's own bakes under seed 9, each tinted one of four ways.
std::vector<Original> library_originals(const texgen::Matcher& m) {
  static const int kTint[4][3] = {{100, 80, 60}, {70, 90, 100}, {100, 100, 100}, {90, 75, 95}};
  std::vector<Original> out;
  int k = 0;
  for (const auto& e : m.entries) {
    texgen::Recipe r = e.recipe;
    r.size = 64;
    lab::Picture p =
        texgen::map_picture(texgen::bake(r, {r.name, 9u, {}}).maps, texgen::MapKind::albedo, 64);
    const int* t = kTint[k++ % 4];
    for (size_t i = 0; i < p.rgba.size(); i += 4)
      for (int c = 0; c < 3; ++c) p.rgba[i + c] = static_cast<uint8_t>(p.rgba[i + c] * t[c] / 100);
    out.push_back({r.name, std::move(p)});
  }
  return out;
}

// A texture's name from its file's: a file system cannot always hold '*' or '+', so a leading
// "liq_" stands for a liquid's '*' and "anim_" for an animation's '+'.
std::string texture_name(const std::string& stem) {
  if (stem.starts_with("liq_")) return "*" + stem.substr(4);
  if (stem.starts_with("anim_")) return "+" + stem.substr(5);
  return stem;
}

std::string fixed(double v) { return core::num::to_fixed(v, 2); }

void run(lab::Lab& lab) {
  const int panel = static_cast<int>(lab.arg_num("panel"));
  const int per_page = std::max(1, static_cast<int>(lab.arg_num("per-page")));
  const std::string refs = lab.arg("refs");
  texgen::Matcher matcher = texgen::make_matcher(lab.arg("library"));
  matcher.max_distance = lab.arg_num("max-distance");
  if (matcher.entries.empty()) {
    lab.fail("no recipes in " + lab.arg("library"),
             "Point --library at a directory of .tex recipes. Skill: textures.");
    return;
  }

  std::vector<Original> originals;
  if (refs.empty()) {
    originals = library_originals(matcher);
  } else {
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& f : std::filesystem::directory_iterator(refs, ec))
      if (f.path().extension() == ".png") paths.push_back(f.path().string());
    std::sort(paths.begin(), paths.end());
    for (const std::string& path : paths) {
      std::optional<lab::Picture> p = lab::read_png(path);
      if (p)
        originals.push_back(
            {texture_name(std::filesystem::path(path).stem().string()), std::move(*p)});
    }
    if (originals.empty()) {
      lab.fail("no PNGs in " + refs,
               "Write the level's textures as PNGs into --refs, outside the tree. Skill: "
               "texture-references.");
      return;
    }
  }
  const int limit = static_cast<int>(lab.arg_num("limit"));
  if (limit > 0 && static_cast<int>(originals.size()) > limit) originals.resize(limit);

  lab::LabTable table{
      "REPLACEMENTS",
      {"texture", "size", "repeat x", "repeat y", "recipe", "distance", "reference a level stores"},
      {}};
  int replaced = 0, kept = 0;
  double total = 0;
  const int gap = 6, label = 12, cell_w = 2 * panel + 3 * gap, cell_h = panel + 2 * label + gap;
  const int cols = std::max(1, static_cast<int>(lab.arg_num("cols")));
  const int pages = (static_cast<int>(originals.size()) + per_page - 1) / per_page;
  for (int page = 0; page < pages; ++page) {
    const int first = page * per_page;
    const int count = std::min(per_page, static_cast<int>(originals.size()) - first);
    const int rows = (count + cols - 1) / cols;
    lab::Picture sheet =
        lab::create_picture(cols * cell_w + gap, rows * cell_h + gap, {24, 24, 26});
    for (int k = 0; k < count; ++k) {
      const Original& o = originals[first + k];
      const auto r = texgen::replace_texture(matcher, o.name, o.picture);
      const int x = gap + (k % cols) * cell_w, y = gap + (k / cols) * cell_h;
      const int h = panel * o.picture.height / std::max(1, o.picture.width);
      lab::blit(sheet, lab::resized(o.picture, panel, std::min(panel, h)), x, y + label);
      lab::draw_text(sheet, x, y, o.name, {220, 220, 220});
      std::string pick = "KEPT";
      if (r) {
        lab::blit(sheet, lab::resized(r->picture, panel, std::min(panel, h)), x + panel + gap,
                  y + label);
        pick = r->match.ref.name;
        ++replaced;
        total += r->match.distance;
      } else {
        ++kept;
      }
      lab::draw_text(sheet, x + panel + gap, y, pick,
                     r ? lab::Rgba{200, 200, 160} : lab::Rgba{200, 120, 120});
      if (r)
        lab::draw_text(sheet, x, y + label + panel + 2, "D " + fixed(r->match.distance),
                       {160, 160, 160});
      const texgen::Likeness l = texgen::likeness_of(o.picture);
      // What a converter writes for it, near or not.
      const texgen::TextureMatch m =
          r ? r->match : texgen::match_texture(matcher, o.name, o.picture);
      const std::string stored = m.kind == texgen::TextureKind::liquid ? "(liquid " + m.liquid + ")"
                                 : m.kind == texgen::TextureKind::sky  ? "(sky)"
                                                                       : m.text;
      table.rows.push_back(
          {o.name, std::to_string(o.picture.width) + "x" + std::to_string(o.picture.height),
           fixed(l.period_x) + " @" + fixed(l.repeat_x),
           fixed(l.period_y) + " @" + fixed(l.repeat_y), pick, r ? fixed(r->match.distance) : "-",
           stored});
    }
    const std::string id = "page" + std::to_string(page + 1);
    lab.shot({id,
              "REPLACEMENTS " + std::to_string(page + 1) + "/" + std::to_string(pages),
              "Each texture (left) beside the library recipe the likeness matcher picked for it "
              "(right), baked at the texture's size under a seed of its name and recoloured to "
              "its mean and spread; D is how unalike they were, KEPT a texture left as it was.",
              "the replacement reads as the same kind of surface in the same colours: a brick "
              "for a brick, a riveted plate for a riveted plate; no texture replaced by "
              "something of another kind",
              {{"page", std::to_string(page + 1)}}},
             sheet);
  }
  lab.metric("replaced", replaced, "count");
  lab.metric("kept", kept, "count");
  if (replaced > 0) lab.metric("distance.mean", total / replaced, "count");
  lab.table(table);
}

const lab::LabSpec kSpec{
    "texgen-replace",
    lab::LabKind::picture,
    "engine",
    "engine/texgen",
    "textures",
    "Which procedural recipe replaces each of a level's painted textures, and does it read "
    "the same?",
    {
        {"library", texgen::kLibraryDir, "the directory of .tex recipes to pick from"},
        {"refs", "",
         "a directory of the level's textures as PNGs, outside the tree (empty: the "
         "library's own bakes, tinted)"},
        {"limit", "0", "at most this many textures (0: all)"},
        {"max-distance", "1.6", "no recipe nearer than this: the texture keeps its pixels"},
        {"panel", "96", "each picture's side, px"},
        {"cols", "4", "pairs across a page"},
        {"per-page", "24", "pairs on a page"},
    },
    "--panel=32 --limit=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
