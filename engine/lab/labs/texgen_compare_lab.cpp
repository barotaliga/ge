// LAB `texgen-compare`: our recipes beside the reference textures they are steered toward, and
// the look of each in numbers. A reference is a PNG an agent keeps OUTSIDE the tree for one
// session (another game's wall, a photograph): the lab reads it only to draw it beside the bake
// and to measure its look (`look.hpp`), so a recipe moves toward it by what an agent writes,
// never by its pixels. Each pair is drawn three ways at the same scale, two tiles across with
// the nearest pixel (the reference, our colour, our colour lit), and a table gives both looks
// and the distance between them. The loop is the `texture-references` skill.
//
// With no --refs, the "reference" of each pair is the recipe itself under seed 2: the smoke run
// and its digest need nothing outside the tree.
//
// ge:summary Lab: our recipes beside reference textures, with both looks measured in numbers
// ge:keywords lab texgen texture compare reference side-by-side look stats inspiration
// ge:keywords iterate recipe grain contrast palette
// ge:skill textures labs
// ge:test tests/texgen_look_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "texgen/bake.hpp"
#include "texgen/library.hpp"
#include "texgen/look.hpp"
#include "texgen/preview.hpp"

namespace {

using namespace ge;

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    size_t end = s.find(sep, at);
    if (end == std::string::npos) end = s.size();
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

// `p` two tiles across at `px` a side, nearest pixel.
lab::Picture tiled(const lab::Picture& p, int px) {
  lab::Picture two = lab::create_picture(p.width * 2, p.height * 2, {0, 0, 0});
  for (int ty = 0; ty < 2; ++ty)
    for (int tx = 0; tx < 2; ++tx) lab::blit(two, p, tx * p.width, ty * p.height);
  return lab::resized(two, px, px * p.height / std::max(1, p.width));
}

std::string fixed(double v) { return core::num::to_fixed(v, 3); }

void run(lab::Lab& lab) {
  const int panel = static_cast<int>(lab.arg_num("panel"));
  const std::string refs = lab.arg("refs");
  lab::LabTable table{"LOOK: REFERENCE / OURS", {"pair"}, {}};
  for (const char* n : texgen::kLookNames) table.columns.emplace_back(n);
  table.columns.emplace_back("distance");
  double worst = 0, total = 0;
  int pairs = 0;
  for (const std::string& pair : split(lab.arg("pairs"), ',')) {
    const std::vector<std::string> parts = split(pair, ':');
    const std::string name = parts.empty() ? pair : parts[0];
    const std::string ref_name = parts.size() > 1 ? parts[1] : name;
    const std::string path = lab.arg("library") + "/" + name + ".tex";
    texgen::ParseResult parsed = texgen::load_recipe(path);
    if (!parsed.ok()) {
      lab.fail(texgen::format_diagnostics(parsed.errors, path),
               "Fix the recipe or the --pairs list (recipe:reference,...). Skill: textures.");
      continue;
    }
    texgen::Recipe& recipe = parsed.recipe;
    lab::Picture ref;
    if (refs.empty()) {
      ref = texgen::map_picture(texgen::bake(recipe, {recipe.name, 2u, {}}).maps,
                                texgen::MapKind::albedo, recipe.size);
    } else {
      std::string ref_path = refs;
      ref_path += "/";
      ref_path += ref_name;
      ref_path += ".png";
      std::optional<lab::Picture> read = lab::read_png(ref_path);
      if (!read) {
        lab.fail("no reference PNG at " + ref_path,
                 "Write the references as PNGs into --refs (outside the tree), named as in "
                 "--pairs. Skill: texture-references.");
        continue;
      }
      ref = *read;
      // Bake at the reference's own size where it is a power of two, so texel meets texel.
      if (ref.width == ref.height && ref.width >= 16 && (ref.width & (ref.width - 1)) == 0)
        recipe.size = ref.width;
    }
    const texgen::Maps maps = texgen::bake(recipe, {}).maps;
    const lab::Picture ours = texgen::map_picture(maps, texgen::MapKind::albedo, recipe.size);
    const texgen::Look a = texgen::look_of(ref), b = texgen::look_of(ours);
    const double dist = texgen::look_distance(a, b);
    worst = std::max(worst, dist);
    total += dist;
    ++pairs;
    std::string label_pair = name;
    label_pair += " / ";
    label_pair += ref_name;
    std::vector<std::string> row{label_pair};
    const auto va = texgen::look_values(a), vb = texgen::look_values(b);
    for (size_t k = 0; k < va.size(); ++k) row.push_back(fixed(va[k]) + " / " + fixed(vb[k]));
    row.push_back(fixed(dist));
    table.rows.push_back(row);
    lab.metric(name + ".distance", dist, "count");

    const int gap = 8, label = 14;
    lab::Picture sheet =
        lab::create_picture(3 * panel + 4 * gap, panel + label + 2 * gap + 12, {24, 24, 26});
    const char* titles[3] = {"REFERENCE", "OURS", "OURS LIT"};
    const lab::Picture shots[3] = {tiled(ref, panel), tiled(ours, panel),
                                   texgen::lit_preview(maps, panel, 2)};
    for (int c = 0; c < 3; ++c) {
      const int x = gap + c * (panel + gap);
      lab::draw_text(sheet, x, gap, titles[c], {220, 220, 220});
      lab::blit(sheet, shots[c], x, gap + label);
    }
    lab::draw_text(sheet, gap, gap + label + panel + 4, "LOOK DISTANCE " + fixed(dist),
                   {200, 200, 160});
    std::string caption = "The recipe ";
    caption += path;
    caption += " baked at ";
    caption += std::to_string(recipe.size);
    caption += " px beside the reference '";
    caption += ref_name;
    caption +=
        "', both two tiles across at the nearest pixel, then ours lit. The reference is read "
        "only to be drawn and measured; look distance ";
    caption += fixed(dist);
    caption += " (under about 0.5 reads as the same kind of surface).";
    lab.shot({name,
              "COMPARE " + name,
              caption,
              "the same kind of surface at a glance: brightness, colour, grain, how much "
              "structure; ours tiles; nothing of the reference's own drawing is copied",
              {{"recipe", name}, {"reference", ref_name}}},
             sheet);
  }
  lab.metric("pairs", pairs, "count");
  if (pairs > 0) {
    lab.metric("distance.mean", total / pairs, "count");
    lab.metric("distance.worst", worst, "count");
  }
  lab.table(table);
}

const lab::LabSpec kSpec{
    "texgen-compare",
    lab::LabKind::picture,
    "engine",
    "engine/texgen",
    "textures",
    "How close does each recipe come to the reference it is steered toward?",
    {
        {"library", texgen::kLibraryDir, "the directory of .tex recipes"},
        {"refs", "", "a directory of reference PNGs outside the tree (empty: seed 2 of each)"},
        {"pairs", "brick_wall,metal_panels", "recipe:reference pairs, comma-separated"},
        {"panel", "256", "each panel's side, px"},
    },
    "--panel=64 --pairs=brick_wall",
    run,
};
GE_LAB(kSpec);

}  // namespace
