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
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

#include "cli/json.hpp"
#include "core/num_text.hpp"
#include "lab/png.hpp"
#include "texgen/bake.hpp"
#include "texgen/layers.hpp"
#include "texgen/library.hpp"
#include "texgen/replace.hpp"
#include "texgen/texture_map.hpp"

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
  std::string name;  // the texture's own name
  std::string file;  // its PNG
  lab::Picture picture;
  texgen::TextureMapLine* line = nullptr;
  double distance = -1;              // -1: not a texture line (a liquid, a sky, a surface)
  std::string klass, traits, judge;  // the agent's notes
  texgen::Evidence original, ours;
  std::vector<std::string> mismatches;
  std::vector<texgen::Candidate> candidates;
  bool argued() const { return !klass.empty() && !traits.empty() && !judge.empty(); }
  bool failed() const { return judge.starts_with("fail"); }
};

std::string fixed2(double v) { return core::num::to_fixed(v, 2); }

// A trademark, a logo or an emblem is never cloned: its texture becomes a blank recipe (tagged
// `blank`) and is judged on keeping the place's surface, not the mark.
bool is_mark(const std::string& klass) {
  return klass == "logo" || klass == "trademark" || klass == "emblem";
}

// Whether the class lays its pieces in courses (an unnamed class might): the courses and pieces
// counted on a plain, rock or metal surface are noise.
bool is_coursed(const std::string& klass) {
  return klass != "plain" && klass != "rock" && klass != "metal" && klass != "flagstones" &&
         klass != "scales" && klass != "stripes" && klass != "plate";
}

// The class the candidates are drawn from: a mark's blanks, else the agent's own class.
std::string search_class(const std::string& klass) { return is_mark(klass) ? "blank" : klass; }

bool recipe_tagged(const texgen::Matcher& m, const std::string& name, const std::string& tag) {
  for (const texgen::Matcher::Entry& e : m.entries)
    if (e.recipe.name == name) return texgen::recipe_has_class(e.recipe, tag);
  return false;
}

std::vector<std::string> split_commas(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream in(s);
  std::string w;
  while (std::getline(in, w, ',')) {
    if (!w.empty()) out.push_back(w);
  }
  return out;
}

// `text` cut into lines of at most `width` characters at spaces.
std::vector<std::string> wrap(const std::string& text, size_t width) {
  std::vector<std::string> out;
  std::string line;
  std::stringstream in(text);
  std::string word;
  while (in >> word) {
    if (!line.empty() && line.size() + 1 + word.size() > width) {
      out.push_back(line);
      line.clear();
    }
    line += (line.empty() ? "" : " ") + word;
  }
  if (!line.empty()) out.push_back(line);
  return out;
}

// `p` repeated n x n: how a tile reads on a wall, its seams in plain sight.
lab::Picture tiled(const lab::Picture& p, int n) {
  lab::Picture out = lab::create_picture(p.width * n, p.height * n, {0, 0, 0});
  for (int ty = 0; ty < n; ++ty)
    for (int tx = 0; tx < n; ++tx) lab::blit(out, p, tx * p.width, ty * p.height);
  return out;
}

// One texture's sheet: the original, its current reference, then each candidate baked with the
// same seed, tint and spread, every panel labelled with its recipe and distance (after the clone,
// the original and the clone tiled 3x3, for the seams); under them the agent's argument (class,
// traits, judgement), both textures' evidence and every mismatch.
lab::Picture match_sheet(const texgen::Matcher& m, const Matched& t) {
  constexpr int kPanel = 128, kGap = 8, kLabel = 24, kLine = 10;
  const int panels = 4 + static_cast<int>(t.candidates.size());
  const int width = panels * (kPanel + kGap) + kGap;
  const size_t chars = static_cast<size_t>((width - 2 * kGap) / lab::kGlyphW);
  std::vector<std::pair<std::string, lab::Rgba>> notes;
  auto add = [&](const std::string& head, const std::string& body, lab::Rgba c) {
    const std::vector<std::string> lines = wrap(head + " " + body, chars);
    for (const std::string& l : lines) notes.emplace_back(l, c);
  };
  add("1 CLASS", t.klass.empty() ? "(not argued: write `class <name> <class>`)" : t.klass,
      {220, 220, 220});
  add("2 TRAITS", t.traits.empty() ? "(not argued: write `traits <name> ...`)" : t.traits,
      {220, 220, 220});
  add("3 CLONE", t.line->value, {200, 200, 160});
  add("4 JUDGE", t.judge.empty() ? "(not judged: write `judge <name> pass|fail ...`)" : t.judge,
      t.failed() ? lab::Rgba{230, 120, 120} : lab::Rgba{140, 220, 140});
  add("ORIGINAL", texgen::evidence_text(t.original), {170, 170, 170});
  add("OURS", texgen::evidence_text(t.ours), {170, 170, 170});
  for (const std::string& mm : t.mismatches) add("MISMATCH", mm, {230, 170, 90});

  const int h = std::min(kPanel, kPanel * t.picture.height / std::max(1, t.picture.width));
  const int text_top = kGap + kLabel + h + kGap;
  lab::Picture sheet = lab::create_picture(
      width, text_top + static_cast<int>(notes.size()) * kLine + kGap, {24, 24, 26});
  auto panel = [&](int k, const lab::Picture& p, const std::string& top, const std::string& low,
                   lab::Rgba colour) {
    const int x = kGap + k * (kPanel + kGap);
    const int half = kLabel / 2;
    const int image_y = kGap + half + 2;
    const int low_y = kGap + half + h + 4;
    lab::draw_text(sheet, x, kGap, top, colour);
    lab::blit(sheet, lab::resized(p, kPanel, h), x, image_y);
    lab::draw_text(sheet, x, low_y, low, {170, 170, 170});
  };
  panel(0, t.picture, t.name, "ORIGINAL", {220, 220, 220});
  std::vector<texgen::Diagnostic> errors;
  const texgen::TextureRef ref = texgen::parse_ref(t.line->value, errors);
  const lab::Picture ours = texgen::bake_ref(m, ref, t.picture.width, t.picture.height, errors);
  panel(1, ours, ref.name,
        std::string(t.line->pinned ? "PINNED " : "CURRENT ") + fixed2(t.distance), {200, 200, 160});
  panel(2, tiled(t.picture, 3), "3X3", "ORIGINAL TILED", {220, 220, 220});
  panel(3, tiled(ours, 3), "3X3", "OURS TILED", {200, 200, 160});
  for (size_t k = 0; k < t.candidates.size(); ++k) {
    texgen::TextureRef alt = ref;
    alt.name = t.candidates[k].recipe;
    alt.params.clear();
    std::vector<texgen::Diagnostic> alt_errors;
    panel(static_cast<int>(k) + 4,
          texgen::bake_ref(m, alt, t.picture.width, t.picture.height, alt_errors), alt.name,
          "#" + std::to_string(k + 1) + " " + fixed2(t.candidates[k].distance), {160, 200, 220});
  }
  for (size_t i = 0; i < notes.size(); ++i)
    lab::draw_text(sheet, kGap, text_top + static_cast<int>(i) * kLine, notes[i].first,
                   notes[i].second);
  return sheet;
}

// Textures seen together, judged as a set: the joins between them in the order they meet and
// one look across them.
struct Cluster {
  std::string name, judge;
  std::vector<const Matched*> members;
  std::vector<std::string> mismatches;
  bool argued() const { return !judge.empty(); }
  bool failed() const { return judge.starts_with("fail"); }
};

// The mean luma of a picture's strip three texels wide along its left or right edge.
double edge_luma(const lab::Picture& p, bool right) {
  double sum = 0;
  int n = 0;
  for (int y = 0; y < p.height; ++y)
    for (int k = 0; k < std::min(3, p.width); ++k) {
      const int x = right ? p.width - 1 - k : k;
      const size_t o = (static_cast<size_t>(y) * p.width + x) * 4;
      sum += (0.299 * p.rgba[o] + 0.587 * p.rgba[o + 1] + 0.114 * p.rgba[o + 2]) / 255;
      ++n;
    }
  return n > 0 ? sum / n : 0;
}

// What differs between the originals as a set and the clones as a set.
void cluster_mismatches(Cluster& c, const std::vector<lab::Picture>& ours) {
  const size_t n = c.members.size();
  for (size_t i = 0; i < n; ++i)
    for (size_t j = i + 1; j < n; ++j) {
      const double o = c.members[i]->original.brightness - c.members[j]->original.brightness;
      const double u = c.members[i]->ours.brightness - c.members[j]->ours.brightness;
      if (std::fabs(o) > 0.03 && o * u < 0)
        c.mismatches.push_back(c.members[i]->name + " is " + (o > 0 ? "lighter" : "darker") +
                               " than " + c.members[j]->name + " in the originals but " +
                               (u > 0 ? "lighter" : "darker") + " in ours");
    }
  double olo = 1, ohi = 0, ulo = 1, uhi = 0;
  for (const Matched* t : c.members) {
    olo = std::min(olo, t->original.brightness), ohi = std::max(ohi, t->original.brightness);
    ulo = std::min(ulo, t->ours.brightness), uhi = std::max(uhi, t->ours.brightness);
  }
  if (n > 1 && ohi - olo > 0.02 &&
      ((uhi - ulo) > 2 * (ohi - olo) || (uhi - ulo) < 0.5 * (ohi - olo)))
    c.mismatches.push_back("the set's brightness spans " + fixed2(ohi - olo) +
                           " in the originals and " + fixed2(uhi - ulo) +
                           " in ours: the family does not read as one");
  for (size_t i = 0; i + 1 < n; ++i) {
    const double jo = std::fabs(edge_luma(c.members[i]->picture, true) -
                                edge_luma(c.members[i + 1]->picture, false));
    const double ju = std::fabs(edge_luma(ours[i], true) - edge_luma(ours[i + 1], false));
    if (ju > 2 * jo + 0.03)
      c.mismatches.push_back("the join from " + c.members[i]->name + " to " +
                             c.members[i + 1]->name + " jumps " + fixed2(ju) + " in ours, " +
                             fixed2(jo) + " in the originals");
  }
}

// A cluster's sheet: the originals side by side in the order they meet, each tiled 2x2, and the
// clones under them the same way; then the judgement and every mismatch.
lab::Picture cluster_sheet(const Cluster& c, const std::vector<lab::Picture>& ours) {
  constexpr int kPanel = 128, kGap = 8, kLabel = 14, kLine = 10;
  const int n = static_cast<int>(c.members.size());
  const int width = std::max(n * kPanel + 2 * kGap, 360);
  const size_t chars = static_cast<size_t>((width - 2 * kGap) / lab::kGlyphW);
  std::vector<std::pair<std::string, lab::Rgba>> notes;
  for (const std::string& l : wrap(
           "JUDGE " + (c.judge.empty()
                           ? std::string("(not judged: write `judge ") + c.name + " pass|fail ...`)"
                           : c.judge),
           chars))
    notes.emplace_back(l, c.failed() ? lab::Rgba{230, 120, 120} : lab::Rgba{140, 220, 140});
  for (const std::string& mm : c.mismatches)
    for (const std::string& l : wrap("MISMATCH " + mm, chars))
      notes.emplace_back(l, lab::Rgba{230, 170, 90});
  const int top = kGap + kLabel, row = kPanel + kLabel;
  lab::Picture sheet = lab::create_picture(
      width, top + 2 * row + kGap + static_cast<int>(notes.size()) * kLine + kGap, {24, 24, 26});
  lab::draw_text(sheet, kGap, kGap, "CLUSTER " + c.name + ": ORIGINALS, THEN OURS",
                 {220, 220, 220});
  for (int k = 0; k < n; ++k) {
    const int x = kGap + k * kPanel;
    lab::blit(sheet,
              lab::resized(tiled(c.members[static_cast<size_t>(k)]->picture, 2), kPanel, kPanel), x,
              top);
    lab::blit(sheet, lab::resized(tiled(ours[static_cast<size_t>(k)], 2), kPanel, kPanel), x,
              top + row);
    lab::draw_text(sheet, x, top + kPanel + 2, c.members[static_cast<size_t>(k)]->name,
                   {170, 170, 170});
  }
  for (size_t i = 0; i < notes.size(); ++i)
    lab::draw_text(sheet, kGap, top + 2 * row + kGap + static_cast<int>(i) * kLine, notes[i].first,
                   notes[i].second);
  return sheet;
}

}  // namespace

Outcome run_texture_match(const std::filesystem::path& repo, const TextureMatchOptions& opt) {
  namespace fs = std::filesystem;
  Outcome o;
  const fs::path dir = fs::absolute(repo / opt.dir);
  std::error_code ec;
  std::vector<Matched> textures;
  // A level's export may list each file's real name (`textures.txt`: `<name> <file> <w>x<h>`,
  // '#' lines skipped); without it the name is decoded from the file's.
  std::map<std::string, std::string> list_names;
  if (std::ifstream list(dir / "textures.txt"); list) {
    std::string line;
    while (std::getline(list, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream words(line);
      std::string name, file;
      if (words >> name >> file) list_names[file] = name;
    }
  }
  for (const auto& f : fs::directory_iterator(dir, ec)) {
    if (f.path().extension() != ".png") continue;
    std::optional<lab::Picture> p = lab::read_png(f.path().string());
    if (!p) {
      o.findings.push_back({Severity::warn, "not a PNG the reader takes (8-bit RGB or RGBA)",
                            rel_path(repo, f.path()), "write it again as 8-bit RGBA"});
      continue;
    }
    Matched t;
    const auto named = list_names.find(f.path().filename().string());
    t.name = named != list_names.end() ? named->second
                                       : texgen::texture_name_of_file(f.path().stem().string());
    t.file = f.path().string();
    t.picture = std::move(*p);
    textures.push_back(std::move(t));
  }
  if (textures.empty()) {
    o.findings.push_back({Severity::error, "no PNG textures in " + rel_path(repo, dir), "",
                          "write the map's textures as PNGs into a directory outside the tree "
                          "(liq_ for a leading '*', anim_ for '+'), e.g. labs bsp --pak=<pak> "
                          "--map=<map> --textures-out=<dir>"});
    return o;
  }
  std::sort(textures.begin(), textures.end(),
            [](const Matched& a, const Matched& b) { return a.name < b.name; });
  const fs::path library =
      fs::absolute(repo / (opt.library.empty() ? std::string(texgen::kLibraryDir) : opt.library));
  texgen::Matcher matcher = texgen::make_matcher(library.string());
  matcher.max_distance = opt.near;
  if (matcher.entries.empty()) {
    o.findings.push_back({Severity::error, "no recipes in " + rel_path(repo, library), "",
                          "pass --library=<dir of .tex recipes>"});
    return o;
  }

  const fs::path map_path =
      opt.map.empty() ? dir / "textures.texmap" : fs::absolute(repo / opt.map);
  const std::string map_rel = rel_path(repo, map_path);
  texgen::TextureMap map;
  if (fs::exists(map_path)) {
    std::ifstream in(map_path);
    std::stringstream text;
    text << in.rdbuf();
    std::vector<texgen::Diagnostic> errors;
    map = texgen::parse_texture_map(text.str(), errors);
    for (const texgen::Diagnostic& d : errors)
      o.findings.push_back({Severity::error, d.message, map_rel + ":" + std::to_string(d.line),
                            "fix the line (the format is in engine/texgen/texture_map.hpp)"});
    if (!errors.empty()) return o;
  } else {
    map.lines.push_back({"", "", "", false, "",
                         "# texture map: written by ge texture match, argued by an agent. Per "
                         "texture: class, traits, the clone, judge; end the clone's line with "
                         "`# pin` to keep it."});
  }

  // Every texture's line: a pinned one or a surface kept, a liquid or a sky named, the rest
  // re-picked (within the agent's class when it named one); then every texture line scored and
  // its evidence held against the original's.
  const int top = std::max(0, opt.top);
  for (Matched& t : textures) {
    t.klass = map.note("class", t.name);
    t.traits = map.note("traits", t.name);
    t.judge = map.note("judge", t.name);
    texgen::TextureMapLine* line = map.find(t.name);
    const texgen::TextureMatch m = texgen::match_texture(matcher, t.name, t.picture);
    if (line == nullptr) {
      map.lines.push_back({"", t.name, "", false, "", ""});
      line = &map.lines.back();
    }
    const bool keep = line->pinned || line->keyword == "surface" ||
                      (m.kind != texgen::TextureKind::surface && !line->keyword.empty());
    if (keep) continue;
    line->note.clear();
    if (m.kind == texgen::TextureKind::liquid) {
      line->keyword = "liquid";
      line->value = m.liquid;
    } else if (m.kind == texgen::TextureKind::sky) {
      line->keyword = "sky";
      line->value.clear();
    } else {
      line->keyword = "texture";
      texgen::TextureRef ref = m.ref;
      if (!t.klass.empty()) {
        const std::vector<texgen::Candidate> in_class =
            texgen::nearest_refs(matcher, t.name, t.picture, 1, search_class(t.klass));
        if (!in_class.empty()) ref.name = in_class[0].recipe;
      }
      line->value = texgen::ref_text(ref);
    }
  }
  int far = 0, pinned = 0, argued = 0, failed = 0;
  for (Matched& t : textures) {
    t.line = map.find(t.name);  // only now: the vector no longer moves
    if (t.line->pinned) ++pinned;
    if (t.line->keyword != "texture") continue;
    std::vector<texgen::Diagnostic> errors;
    const texgen::TextureRef ref = texgen::parse_ref(t.line->value, errors);
    const lab::Picture ours =
        texgen::bake_ref(matcher, ref, t.picture.width, t.picture.height, errors);
    for (const texgen::Diagnostic& d : errors)
      o.findings.push_back({Severity::error, t.name + ": " + d.message, map_rel,
                            "fix the line's reference (ge texture kinds; the library's recipes)"});
    if (!errors.empty()) continue;
    t.distance =
        texgen::likeness_distance(texgen::likeness_of(t.picture), texgen::likeness_of(ours));
    t.original = texgen::evidence_of(t.picture);
    t.ours = texgen::evidence_of(ours);
    if (!is_mark(t.klass))
      t.mismatches = texgen::evidence_mismatches(t.original, t.ours, is_coursed(t.klass));
    t.line->note = fixed2(t.distance);
    const std::string within = search_class(t.klass);
    t.candidates = texgen::nearest_refs(matcher, t.name, t.picture, top, within);
    if (is_mark(t.klass) && !recipe_tagged(matcher, ref.name, "blank")) {
      o.findings.push_back({Severity::warn,
                            t.name + " is a " + t.klass + " but `" + ref.name +
                                "` is not a blank recipe: a mark is never cloned",
                            map_rel,
                            "map it to a recipe tagged `blank` (e.g. blank_plate), recoloured to "
                            "the surface around the mark (skill: texture-references)"});
    }
    if (!within.empty() && t.candidates.empty()) {
      o.findings.push_back({Severity::warn, t.name + ": no recipe is tagged `" + within + "`",
                            map_rel,
                            "write one (the texture-references skill), tag it `" + within +
                                "`, or name another class"});
      t.candidates = texgen::nearest_refs(matcher, t.name, t.picture, top);
    }
    if (t.distance > opt.near) ++far;
    if (t.argued()) ++argued;
    if (t.failed()) ++failed;
    if (t.line->pinned && !t.argued()) {
      o.findings.push_back({Severity::warn,
                            t.name + " is pinned without its argument (class, traits, judge)",
                            map_rel,
                            "add `class " + t.name + " <class>`, `traits " + t.name +
                                " <how it looks>` and `judge " + t.name +
                                " pass|fail <trait by trait>` (skill: texture-references)"});
    }
    if (t.line->pinned && !ref.tint) {
      texgen::TextureRef coloured = texgen::colour_ref(ref.name, t.picture);
      coloured.name = ref.name;
      if (ref.seed) coloured.seed = ref.seed;
      coloured.params = ref.params;
      o.findings.push_back(
          {Severity::warn, t.name + " is pinned without the original's colour", map_rel,
           "write the line as `texture " + t.name + " " + texgen::ref_text(coloured) + "  # pin`"});
    }
    if (t.line->pinned && !t.failed()) {
      for (const std::string& mm : t.mismatches)
        o.findings.push_back({Severity::warn, t.name + " (pinned): " + mm, map_rel,
                              "answer it in the judge line or turn the reference's knobs"});
    }
  }

  // The clusters: each judged as a set once its members are measured.
  std::vector<Cluster> clusters;
  std::vector<std::vector<lab::Picture>> cluster_ours;
  for (const texgen::TextureMapLine& l : map.lines) {
    if (l.keyword != "cluster") continue;
    Cluster c;
    c.name = l.texture;
    c.judge = map.note("judge", l.texture);
    std::vector<lab::Picture> baked;
    std::istringstream words(l.value);
    std::string member;
    while (words >> member) {
      const auto it = std::find_if(textures.begin(), textures.end(),
                                   [&](const Matched& t) { return t.name == member; });
      if (it == textures.end() || it->distance < 0) {
        o.findings.push_back({Severity::warn,
                              "cluster " + c.name + ": `" + member + "` is not a texture line here",
                              map_rel, "name the textures as the map's texture lines name them"});
        continue;
      }
      std::vector<texgen::Diagnostic> errors;
      baked.push_back(texgen::bake_ref(matcher, texgen::parse_ref(it->line->value, errors),
                                       it->picture.width, it->picture.height, errors));
      c.members.push_back(&*it);
    }
    cluster_mismatches(c, baked);
    if (!c.argued())
      o.findings.push_back({Severity::warn, "cluster " + c.name + " is not judged", map_rel,
                            "look at the cluster's sheet and write `judge " + c.name +
                                " pass|fail <the joins, the family's look>`"});
    else if (!c.failed())
      for (const std::string& mm : c.mismatches)
        o.findings.push_back({Severity::warn, "cluster " + c.name + ": " + mm, map_rel,
                              "answer it in the judge line or retune a member"});
    clusters.push_back(std::move(c));
    cluster_ours.push_back(std::move(baked));
  }

  // The sheets: the textures asked for, or the ones most in need of an argument (failed, then
  // unargued and farthest).
  std::vector<Matched*> shown;
  const std::vector<std::string> only = split_commas(opt.only);
  for (Matched& t : textures) {
    if (t.distance < 0) continue;
    if (only.empty() || std::find(only.begin(), only.end(), t.name) != only.end())
      shown.push_back(&t);
  }
  auto need = [](const Matched* t) {
    return (t->failed() ? 100.0 : 0.0) + (t->argued() ? 0.0 : 10.0) + t->distance;
  };
  std::stable_sort(shown.begin(), shown.end(),
                   [&](const Matched* a, const Matched* b) { return need(a) > need(b); });
  if (only.empty() && static_cast<int>(shown.size()) > opt.sheets)
    shown.resize(static_cast<size_t>(std::max(0, opt.sheets)));
  const fs::path sheets =
      opt.sheet_dir.empty() ? dir / "sheets" : fs::absolute(repo / opt.sheet_dir);
  fs::create_directories(sheets, ec);
  for (Matched* t : shown) {
    const fs::path png = sheets / (fs::path(t->file).stem().string() + ".png");
    if (lab::write_png(png.string(), match_sheet(matcher, *t)))
      o.written.push_back(rel_path(repo, png));
  }

  for (size_t k = 0; k < clusters.size(); ++k) {
    const fs::path png = sheets / ("cluster_" + clusters[k].name + ".png");
    if (lab::write_png(png.string(), cluster_sheet(clusters[k], cluster_ours[k])))
      o.written.push_back(rel_path(repo, png));
  }

  {
    std::ofstream out(map_path);
    out << texgen::texture_map_text(map);
  }
  o.written.push_back(map_rel);

  std::vector<const Matched*> order;
  order.reserve(textures.size());
  for (const Matched& t : textures) order.push_back(&t);
  std::stable_sort(order.begin(), order.end(),
                   [&](const Matched* a, const Matched* b) { return need(a) > need(b); });
  std::vector<std::string> rows;
  int listed = 0;
  for (const Matched* t : order) {
    std::vector<std::string> cands;
    std::string cand_text;
    for (const texgen::Candidate& c : t->candidates) {
      cands.push_back(json_obj({{"recipe", json_str(c.recipe)}, {"distance", fixed2(c.distance)}}));
      cand_text += " " + c.recipe + " " + fixed2(c.distance);
    }
    const bool scored = t->distance >= 0;
    rows.push_back(
        json_obj({{"texture", json_str(t->name)},
                  {"kind", json_str(t->line->keyword)},
                  {"value", json_str(t->line->value)},
                  {"pinned", json_bool(t->line->pinned)},
                  {"class", json_str(t->klass)},
                  {"traits", json_str(t->traits)},
                  {"judge", json_str(t->judge)},
                  {"distance", scored ? fixed2(t->distance) : "null"},
                  {"original", json_str(scored ? texgen::evidence_text(t->original) : "")},
                  {"ours", json_str(scored ? texgen::evidence_text(t->ours) : "")},
                  {"mismatches", json_strs(t->mismatches)},
                  {"candidates", json_arr(cands)}}));
    if (scored && listed < opt.list) {
      o.text += std::string(t->failed()   ? "FAIL "
                            : t->argued() ? "ok   "
                                          : "argue") +
                " " + fixed2(t->distance) + "  " + (t->line->pinned ? "pin " : "") + t->name +
                " = " + t->line->value + "\n";
      if (!t->klass.empty()) o.text += "       class: " + t->klass + "\n";
      if (!t->traits.empty()) o.text += "       traits: " + t->traits + "\n";
      if (!t->judge.empty()) o.text += "       judge: " + t->judge + "\n";
      o.text += "       original: " + texgen::evidence_text(t->original) + "\n";
      o.text += "       ours:     " + texgen::evidence_text(t->ours) + "\n";
      for (const std::string& mm : t->mismatches) o.text += "       mismatch: " + mm + "\n";
      o.text += "       nearest:" + cand_text + "\n";
      ++listed;
    }
  }
  std::vector<std::string> cluster_rows;
  for (const Cluster& c : clusters) {
    std::string names;
    std::vector<std::string> members;
    for (const Matched* t : c.members) {
      names += " " + t->name;
      members.push_back(json_str(t->name));
    }
    o.text += std::string(c.failed()   ? "FAIL "
                          : c.argued() ? "ok   "
                                       : "argue") +
              " cluster " + c.name + ":" + names + "\n";
    if (!c.judge.empty()) o.text += "       judge: " + c.judge + "\n";
    for (const std::string& mm : c.mismatches) o.text += "       mismatch: " + mm + "\n";
    cluster_rows.push_back(json_obj({{"cluster", json_str(c.name)},
                                     {"members", json_arr(members)},
                                     {"judge", json_str(c.judge)},
                                     {"mismatches", json_strs(c.mismatches)}}));
  }
  if (listed < static_cast<int>(order.size()))
    o.text += "... " + std::to_string(order.size() - listed) +
              " more in the map (--list=<n> prints more)\n";
  o.result = json_obj({{"map", json_str(map_rel)},
                       {"sheets", json_str(rel_path(repo, sheets))},
                       {"textures", json_arr(rows)},
                       {"clusters", json_arr(cluster_rows)}});
  o.summary = std::to_string(textures.size()) + " textures: " + std::to_string(argued) +
              " argued (" + std::to_string(failed) + " judged fail), " + std::to_string(pinned) +
              " pinned, " + std::to_string(far) + " far (distance over " + fixed2(opt.near) +
              "); map " + map_rel;
  o.next =
      "per texture, in the map: 1 `class`, 2 `traits` (how that class looks and this one), "
      "3 pick or tune the reference and end it `# pin`, 4 `judge` it trait by trait against "
      "the sheet in " +
      rel_path(repo, sheets) +
      " (its 3x3 panels for the seams); group the textures that meet with `cluster <name> "
      "<texture> ...` and judge each set on its sheet; run ge texture match again (skill: "
      "texture-references)";
  return o;
}

}  // namespace ge::cli
