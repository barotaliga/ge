// The `ge texture` commands over engine/texgen: the layer table printed, recipes checked into
// findings, and one variant baked to the cache.
//
// ge:summary ge texture's commands: the kinds table, recipe findings, a bake to the cache
// ge:keywords cli texture kinds check bake findings texgen
// ge:skill agent-interface textures
// ge:test tests/cli_texture_test.cpp
#include "cli/texture.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

#include "cli/json.hpp"
#include "core/num_text.hpp"
#include "texgen/bake.hpp"
#include "texgen/layers.hpp"
#include "texgen/library.hpp"
#include "texgen/replace.hpp"
#include "texgen/texture_map.hpp"
#include "lab/png.hpp"

namespace ge::cli {
namespace {

std::string default_text(const texgen::KeyDef& k) {
  if (k.type == texgen::KeyType::number) {
    return core::num::to_string(k.def) + " (" + core::num::to_string(k.lo) + ".." +
           core::num::to_string(k.hi) + ")";
  }
  if (k.type == texgen::KeyType::word && *k.words)
    return std::string(k.text) + " (" + k.words + ")";
  return k.text;
}

}  // namespace

Outcome run_texture_kinds() {
  Outcome o;
  std::vector<std::string> kinds;
  for (const texgen::LayerKind& kind : texgen::layer_kinds()) {
    o.text += std::string("layer ") + kind.name + "  [" + kind.group + "] " + kind.help + "\n";
    std::vector<std::string> keys;
    for (const texgen::KeyDef& k : kind.keys) {
      o.text += std::string("    ") + k.name + "=" + default_text(k) + "  " + k.help + "\n";
      keys.push_back(json_obj({{"key", json_str(k.name)},
                               {"default", json_str(default_text(k))},
                               {"help", json_str(k.help)}}));
    }
    kinds.push_back(json_obj({{"kind", json_str(kind.name)},
                              {"group", json_str(kind.group)},
                              {"help", json_str(kind.help)},
                              {"keys", json_arr(keys)}}));
  }
  o.result = json_arr(kinds);
  o.summary = std::to_string(kinds.size()) + " layer kinds";
  o.next = "write a recipe (docs/textures.md), then ge texture check <file.tex>";
  return o;
}

Outcome run_texture_check(const std::filesystem::path& repo,
                          const std::vector<std::string>& files) {
  Outcome o;
  std::vector<std::string> checked;
  for (const std::string& f : files) {
    const std::filesystem::path p = std::filesystem::absolute(repo / f);
    const texgen::ParseResult r = texgen::load_recipe(p.string());
    for (const texgen::Diagnostic& d : r.errors) {
      o.findings.push_back({Severity::error, d.message,
                            rel_path(repo, p) + (d.line ? ":" + std::to_string(d.line) : ""),
                            "fix the line (ge texture kinds lists every kind and key; skill: "
                            "textures)"});
    }
    if (r.ok()) {
      checked.push_back(
          json_obj({{"file", json_str(rel_path(repo, p))},
                    {"texture", json_str(r.recipe.name)},
                    {"layers", json_num(static_cast<long long>(r.recipe.layers.size()))}}));
      o.text += rel_path(repo, p) + ": " + r.recipe.name + ", " +
                std::to_string(r.recipe.layers.size()) + " layers, " +
                std::to_string(r.recipe.params.size()) + " params\n";
    }
  }
  o.result = json_arr(checked);
  o.summary = std::to_string(checked.size()) + " of " + std::to_string(files.size()) +
              " recipes parse clean";
  o.next =
      "ge texture bake <file.tex> --ref=\"seed=1\", then look at it in make lab "
      "LAB=texgen-specimens";
  return o;
}

Outcome run_texture_bake(const std::filesystem::path& repo, const std::string& file,
                         const std::string& ref_args, const std::string& size,
                         const std::string& out) {
  Outcome o;
  const std::filesystem::path p = std::filesystem::absolute(repo / file);
  texgen::ParseResult r = texgen::load_recipe(p.string());
  if (!r.ok()) {
    for (const texgen::Diagnostic& d : r.errors)
      o.findings.push_back({Severity::error, d.message,
                            rel_path(repo, p) + (d.line ? ":" + std::to_string(d.line) : ""),
                            "ge texture check " + file});
    return o;
  }
  if (!size.empty()) {
    const texgen::ParseResult sized =
        texgen::parse_recipe("texture t\nsize " + size + "\nlayer fill\n");
    if (!sized.ok()) {
      o.findings.push_back({Severity::error, "--size: " + sized.errors[0].message, "",
                            "pass a power of two such as --size=512"});
      return o;
    }
    r.recipe.size = sized.recipe.size;
  }
  std::vector<texgen::Diagnostic> errors;
  const texgen::TextureRef ref = texgen::parse_ref(r.recipe.name + " " + ref_args, errors);
  texgen::resolve_params(r.recipe, ref, errors);
  for (const texgen::Diagnostic& d : errors)
    o.findings.push_back({Severity::error, d.message, "", "fix --ref (\"seed=<n> <param>=<v>\")"});
  if (!errors.empty()) return o;
  const std::filesystem::path dir =
      std::filesystem::absolute(repo / (out.empty() ? "build/textures" : out));
  const auto t0 = std::chrono::steady_clock::now();
  const texgen::DiskResult d = texgen::bake_to_disk(r.recipe, ref, dir.string());
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  for (const texgen::Diagnostic& e : d.errors)
    o.findings.push_back({Severity::error, e.message, "", "check --out"});
  std::vector<std::string> files;
  for (const std::string& f : d.files) {
    files.push_back(rel_path(repo, f));
    if (!d.reused) o.written.push_back(rel_path(repo, f));
  }
  o.result = json_obj({{"ref", json_str(texgen::ref_text(ref))},
                       {"key", json_str(texgen::bake_key(r.recipe, ref))},
                       {"reused", json_bool(d.reused)},
                       {"files", json_strs(files)}});
  o.summary = texgen::ref_text(ref) +
              (d.reused ? ": already baked, reused "
                        : ": baked in " + core::num::to_fixed(ms, 1) + " ms, ") +
              std::to_string(files.size()) + " files";
  o.next = "load the maps from " + rel_path(repo, dir) +
           "; look at the recipe with make lab LAB=texgen-specimens";
  return o;
}

namespace {

struct Matched {
  std::string name;   // the texture's own name
  std::string file;   // its PNG
  lab::Picture picture;
  texgen::TextureMapLine* line = nullptr;
  double distance = -1;  // -1: not a texture line (a liquid, a sky, a surface)
  std::vector<texgen::Candidate> candidates;
};

std::string fixed2(double v) { return core::num::to_fixed(v, 2); }

std::vector<std::string> split_commas(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream in(s);
  std::string w;
  while (std::getline(in, w, ',')) {
    if (!w.empty()) out.push_back(w);
  }
  return out;
}

// One texture's sheet: the original, its current reference, then each candidate baked with the
// same seed, tint and spread, every panel labelled with its recipe and distance.
lab::Picture match_sheet(const texgen::Matcher& m, const Matched& t) {
  constexpr int kPanel = 128, kGap = 8, kLabel = 24;
  const int panels = 2 + static_cast<int>(t.candidates.size());
  lab::Picture sheet = lab::create_picture(panels * (kPanel + kGap) + kGap,
                                           kPanel + 2 * kLabel + 2 * kGap, {24, 24, 26});
  const int h = std::min(kPanel, kPanel * t.picture.height / std::max(1, t.picture.width));
  auto panel = [&](int k, const lab::Picture& p, const std::string& top, const std::string& low,
                   lab::Rgba colour) {
    const int x = kGap + k * (kPanel + kGap);
    lab::draw_text(sheet, x, kGap, top, colour);
    lab::blit(sheet, lab::resized(p, kPanel, h), x, kGap + kLabel / 2 + 2);
    lab::draw_text(sheet, x, kGap + kLabel / 2 + h + 6, low, {170, 170, 170});
  };
  panel(0, t.picture, t.name, "ORIGINAL", {220, 220, 220});
  std::vector<texgen::Diagnostic> errors;
  const texgen::TextureRef ref = texgen::parse_ref(t.line->value, errors);
  panel(1, texgen::bake_ref(m, ref, t.picture.width, t.picture.height, errors), ref.name,
        std::string(t.line->pinned ? "PINNED " : "CURRENT ") + fixed2(t.distance),
        {200, 200, 160});
  for (size_t k = 0; k < t.candidates.size(); ++k) {
    texgen::TextureRef alt = ref;
    alt.name = t.candidates[k].recipe;
    alt.params.clear();
    std::vector<texgen::Diagnostic> alt_errors;
    const double d = texgen::ref_distance(m, alt, t.picture, alt_errors);
    panel(static_cast<int>(k) + 2,
          texgen::bake_ref(m, alt, t.picture.width, t.picture.height, alt_errors), alt.name,
          "#" + std::to_string(k + 1) + " " + fixed2(d), {160, 200, 220});
  }
  return sheet;
}

}  // namespace

Outcome run_texture_match(const std::filesystem::path& repo, const TextureMatchOptions& opt) {
  namespace fs = std::filesystem;
  Outcome o;
  const fs::path dir = fs::absolute(repo / opt.dir);
  std::error_code ec;
  std::vector<Matched> textures;
  for (const auto& f : fs::directory_iterator(dir, ec)) {
    if (f.path().extension() != ".png") continue;
    std::optional<lab::Picture> p = lab::read_png(f.path().string());
    if (!p) {
      o.findings.push_back({Severity::warn, "not a PNG the reader takes (8-bit RGB or RGBA)",
                            rel_path(repo, f.path()), "write it again as 8-bit RGBA"});
      continue;
    }
    textures.push_back({texgen::texture_name_of_file(f.path().stem().string()),
                        f.path().string(), std::move(*p), nullptr, -1, {}});
  }
  if (textures.empty()) {
    o.findings.push_back({Severity::error, "no PNG textures in " + rel_path(repo, dir), "",
                          "write the map's textures as PNGs into a directory outside the tree "
                          "(liq_ for a leading '*', anim_ for '+'), e.g. ge level textures "
                          "<map> --out=<dir>"});
    return o;
  }
  std::sort(textures.begin(), textures.end(),
            [](const Matched& a, const Matched& b) { return a.name < b.name; });
  const fs::path library = fs::absolute(
      repo / (opt.library.empty() ? std::string(texgen::kLibraryDir) : opt.library));
  texgen::Matcher matcher = texgen::make_matcher(library.string());
  matcher.max_distance = opt.near;
  if (matcher.entries.empty()) {
    o.findings.push_back({Severity::error, "no recipes in " + rel_path(repo, library), "",
                          "pass --library=<dir of .tex recipes>"});
    return o;
  }

  const fs::path map_path = opt.map.empty() ? dir / "textures.texmap" : fs::absolute(repo / opt.map);
  texgen::TextureMap map;
  if (fs::exists(map_path)) {
    std::ifstream in(map_path);
    std::stringstream text;
    text << in.rdbuf();
    std::vector<texgen::Diagnostic> errors;
    map = texgen::parse_texture_map(text.str(), errors);
    for (const texgen::Diagnostic& d : errors)
      o.findings.push_back({Severity::error, d.message,
                            rel_path(repo, map_path) + ":" + std::to_string(d.line),
                            "fix the line (the format is in engine/texgen/texture_map.hpp)"});
    if (!errors.empty()) return o;
  } else {
    map.lines.push_back({"", "", "", false, "",
                         "# texture map: written by ge texture match, steered by an agent. "
                         "End a line with `# pin` to keep it."});
  }

  // Every texture's line: a pinned one or a surface kept, a liquid or a sky named, the rest
  // re-picked; then every texture line scored.
  const int top = std::max(0, opt.top);
  for (Matched& t : textures) {
    texgen::TextureMapLine* line = map.find(t.name);
    const texgen::TextureMatch m = texgen::match_texture(matcher, t.name, t.picture);
    if (line == nullptr) {
      map.lines.push_back({"", t.name, "", false, "", ""});
      line = &map.lines.back();
    }
    const bool keep = line->pinned || line->keyword == "surface" ||
                      (m.kind != texgen::TextureKind::surface && !line->keyword.empty());
    if (!keep) {
      line->keyword = m.kind == texgen::TextureKind::liquid ? "liquid"
                      : m.kind == texgen::TextureKind::sky  ? "sky"
                                                            : "texture";
      line->value = m.kind == texgen::TextureKind::liquid ? m.liquid : m.text;
      line->note.clear();
    }
  }
  // Pointers only once every line is in (the vector no longer moves).
  int far = 0, pinned = 0;
  for (Matched& t : textures) {
    t.line = map.find(t.name);
    if (t.line->pinned) ++pinned;
    if (t.line->keyword != "texture") continue;
    std::vector<texgen::Diagnostic> errors;
    const texgen::TextureRef ref = texgen::parse_ref(t.line->value, errors);
    t.distance = texgen::ref_distance(matcher, ref, t.picture, errors);
    for (const texgen::Diagnostic& d : errors)
      o.findings.push_back({Severity::error, t.name + ": " + d.message, rel_path(repo, map_path),
                            "fix the line's reference (ge texture kinds; the library's recipes)"});
    if (!errors.empty()) continue;
    t.line->note = fixed2(t.distance);
    t.candidates = texgen::nearest_refs(matcher, t.name, t.picture, top);
    if (t.distance > opt.near) ++far;
  }

  // The sheets: the textures asked for, or the farthest.
  std::vector<Matched*> shown;
  const std::vector<std::string> only = split_commas(opt.only);
  for (Matched& t : textures) {
    if (t.distance < 0) continue;
    if (only.empty() || std::find(only.begin(), only.end(), t.name) != only.end())
      shown.push_back(&t);
  }
  std::stable_sort(shown.begin(), shown.end(),
                   [](const Matched* a, const Matched* b) { return a->distance > b->distance; });
  if (only.empty() && static_cast<int>(shown.size()) > opt.sheets)
    shown.resize(static_cast<size_t>(std::max(0, opt.sheets)));
  const fs::path sheets = opt.sheet_dir.empty() ? dir / "sheets" : fs::absolute(repo / opt.sheet_dir);
  fs::create_directories(sheets, ec);
  std::vector<std::string> rows;
  for (Matched* t : shown) {
    const fs::path png = sheets / (fs::path(t->file).stem().string() + ".png");
    if (lab::write_png(png.string(), match_sheet(matcher, *t))) o.written.push_back(rel_path(repo, png));
  }

  {
    std::ofstream out(map_path);
    out << texgen::texture_map_text(map);
  }
  o.written.push_back(rel_path(repo, map_path));

  std::vector<const Matched*> order;
  for (const Matched& t : textures) order.push_back(&t);
  std::stable_sort(order.begin(), order.end(), [](const Matched* a, const Matched* b) {
    return a->distance > b->distance;
  });
  int listed = 0;
  for (const Matched* t : order) {
    std::vector<std::string> cands;
    std::string cand_text;
    for (const texgen::Candidate& c : t->candidates) {
      cands.push_back(json_obj({{"recipe", json_str(c.recipe)}, {"distance", fixed2(c.distance)}}));
      cand_text += " " + c.recipe + " " + fixed2(c.distance);
    }
    rows.push_back(json_obj({{"texture", json_str(t->name)},
                             {"kind", json_str(t->line->keyword)},
                             {"value", json_str(t->line->value)},
                             {"pinned", json_bool(t->line->pinned)},
                             {"distance", t->distance < 0 ? "null" : fixed2(t->distance)},
                             {"candidates", json_arr(cands)}}));
    if (t->distance >= 0 && listed < opt.list) {
      o.text += (t->distance > opt.near ? "far  " : "ok   ") + fixed2(t->distance) + "  " +
                (t->line->pinned ? "pin " : "    ") + t->name + " = " + t->line->value +
                "\n       nearest:" + cand_text + "\n";
      ++listed;
    }
  }
  if (listed < static_cast<int>(order.size()))
    o.text += "... " + std::to_string(order.size() - listed) +
              " more in the map (--list=<n> prints more)\n";
  o.result = json_obj({{"map", json_str(rel_path(repo, map_path))},
                       {"sheets", json_str(rel_path(repo, sheets))},
                       {"textures", json_arr(rows)}});
  o.summary = std::to_string(textures.size()) + " textures: " + std::to_string(far) +
              " far (distance over " + fixed2(opt.near) + "), " + std::to_string(pinned) +
              " pinned; map " + rel_path(repo, map_path);
  o.next = far > 0 ? "look at " + rel_path(repo, sheets) +
                         ", then in the map pin a better candidate or turn the reference's knobs "
                         "(seed, params, tint) and end the line with `# pin`; run "
                         "ge texture match again"
                   : "every texture is near: hand the map to the converter";
  return o;
}

}  // namespace ge::cli
