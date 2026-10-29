// LAB `content-scan`: every sound and model a classic content set ships or its maps name, held to a
// game's asset map (content.hpp), so a game replacing that content sees what is still unmapped,
// what is missing from the set itself, and where each one is used.
//
// `--content` is a pack, or a directory read whole (its packs and its loose files, by their paths
// under it); with none, the fixture set (`content_fixture`) and its own asset map. `--assetmap`
// holds the scan to a game's asset map, `--registry` to the names its registry rows give
// (`sounds.tsv`, `models.tsv`). What comes back: a table each of sounds, models and numbered sets
// (where each is used and the name it maps to), a picture of how much of each is mapped, and
// `assetmap-draft.txt`, the unmapped assets as asset-map lines to fill in. Read a set nobody may
// publish locally: the report stays in previews/, never in the tree.
//
// ge:summary Lab: a content set's sounds, models and numbered sets held to a game's asset map
// ge:keywords lab content scan assets sounds models assetmap registry draft pak replace unmapped
// ge:skill brush free-assets labs
// ge:test tests/brush_content_test.cpp
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "brush/content.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace br = ge::brush;
namespace lab = ge::lab;
namespace fs = std::filesystem;
using lab::Picture;
using lab::Rgba;

// The set's bytes, owned: each pack whole (its files are views into it) and each loose file.
struct Loaded {
  std::vector<std::unique_ptr<std::vector<uint8_t>>> buffers;
  std::vector<br::ContentFile> files;
  int packs = 0;
};

bool read_file(const fs::path& path, std::vector<uint8_t>* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool add_pack(lab::Lab& l, Loaded& set, std::unique_ptr<std::vector<uint8_t>> bytes,
              const std::string& where) {
  ge::vfs::Pak pak;
  std::string error;
  if (!ge::vfs::read_pak(*bytes, &pak, &error)) {
    l.fail("content-scan: " + where + " is not a pack: " + error,
           "next: point --content at a pack, or a directory holding packs and loose files");
    return false;
  }
  for (const ge::vfs::PakEntry& e : pak.entries) {
    set.files.push_back({e.path, ge::vfs::pak_file(*bytes, e)});
  }
  set.buffers.push_back(std::move(bytes));
  ++set.packs;
  return true;
}

bool load(lab::Lab& l, const std::string& content, Loaded& set) {
  const fs::path root(content);
  std::error_code ec;
  if (fs::is_regular_file(root, ec)) {
    auto bytes = std::make_unique<std::vector<uint8_t>>();
    if (!read_file(root, bytes.get())) {
      l.fail("content-scan: cannot read " + content, "next: check the path and its permissions");
      return false;
    }
    return add_pack(l, set, std::move(bytes), content);
  }
  if (!fs::is_directory(root, ec)) {
    l.fail("content-scan: " + content + " is neither a pack nor a directory",
           "next: point --content at a pack or a directory of the content set");
    return false;
  }
  std::vector<fs::path> paths;
  for (const auto& e : fs::recursive_directory_iterator(root, ec)) {
    if (e.is_regular_file()) paths.push_back(e.path());
  }
  std::sort(paths.begin(), paths.end());
  for (const fs::path& p : paths) {
    auto bytes = std::make_unique<std::vector<uint8_t>>();
    if (!read_file(p, bytes.get())) continue;
    std::string ext = p.extension().string();
    for (char& c : ext) c = char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    if (ext == ".pak") {
      if (!add_pack(l, set, std::move(bytes), p.string())) return false;
      continue;
    }
    set.files.push_back({fs::relative(p, root, ec).generic_string(), *bytes});
    set.buffers.push_back(std::move(bytes));
  }
  return true;
}

// The first column of every row of a registry's tables: the names the game ships under.
std::set<std::string> registry_names(lab::Lab& l, const std::string& dir) {
  std::set<std::string> names;
  for (const char* table : {"sounds.tsv", "models.tsv"}) {
    std::ifstream in(fs::path(dir) / table);
    if (!in) {
      l.fail("content-scan: no " + std::string(table) + " in " + dir,
             "next: point --registry at the directory holding the game's sounds.tsv and "
             "models.tsv");
      continue;
    }
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') continue;
      names.insert(line.substr(0, line.find('\t')));
    }
  }
  return names;
}

std::string where(const br::AssetUse& u) { return u.map + ": " + u.classname + "." + u.key; }

// One row of the picture: a bar split into mapped, unmapped but shipped, unmapped and missing.
void bar(Picture& p, int y, const std::string& name, int mapped, int shipped, int missing) {
  const int total = std::max(1, mapped + shipped + missing);
  const int x0 = 150, w = p.width - x0 - 30;
  lab::draw_text(p, 16, y + 10, name, Rgba{230, 230, 230}, 2);
  double x = x0;
  const Rgba kinds[] = {{80, 180, 110}, {225, 170, 60}, {200, 80, 80}};
  const int counts[] = {mapped, shipped, missing};
  for (int i = 0; i < 3; ++i) {
    const double cw = double(w) * counts[i] / total;
    lab::fill_rect(p, x, y, cw, 34, kinds[i]);
    if (counts[i] > 0 && cw > 36) {
      lab::draw_text(p, x + 6, y + 10, std::to_string(counts[i]), Rgba{20, 20, 20}, 2);
    }
    x += cw;
  }
}

void run(lab::Lab& l) {
  Loaded set;
  std::string map_text;
  const std::string content = l.arg("content");
  br::ContentFixture fixture;
  if (content.empty()) {
    std::string error;
    if (!br::content_fixture(&fixture, &error)) {
      l.fail("content-scan: the fixture set did not build: " + error,
             "next: run tests/brush_content_test (skill: brush)");
      return;
    }
    auto bytes = std::make_unique<std::vector<uint8_t>>(fixture.pak);
    if (!add_pack(l, set, std::move(bytes), "the fixture pack")) return;
    map_text = fixture.asset_map;
  } else if (!load(l, content, set)) {
    return;
  }
  if (!l.arg("assetmap").empty()) {
    std::vector<uint8_t> bytes;
    if (!read_file(l.arg("assetmap"), &bytes)) {
      l.fail("content-scan: cannot read " + l.arg("assetmap"),
             "next: point --assetmap at the game's asset map");
      return;
    }
    map_text.assign(bytes.begin(), bytes.end());
  }
  br::AssetMap map;
  std::string error;
  if (!br::parse_asset_map(map_text, &map, &error)) {
    l.fail("content-scan: the asset map does not read: " + error,
           "next: fix that line (sound|model <classic> <ours>, set <class> <key>=<value> "
           "<field> <ours>); skill: free-assets");
    return;
  }
  const bool have_registry = !l.arg("registry").empty();
  const std::set<std::string> registry =
      have_registry ? registry_names(l, l.arg("registry")) : std::set<std::string>{};

  const br::ContentScan scan = br::scan_content(set.files);
  const br::ContentMatch match = br::compare_content(scan, map);
  for (const auto& [m, why] : scan.unread) {
    l.note("map not read: " + m + ": " + why);
  }

  // The tables, and the counts the picture and the metrics carry.
  struct Counts {
    int mapped = 0, shipped = 0, missing = 0, named = 0, unregistered = 0;
  };
  Counts sounds, models, sets;
  lab::LabTable tables[2] = {{"Sounds", {"name", "shipped", "uses", "first use", "ours"}, {}},
                             {"Models", {"name", "shipped", "uses", "first use", "ours"}, {}}};
  std::string draft =
      "# The unmapped sounds and models of the scanned set, as asset-map lines: replace each `?`\n"
      "# with our name (skill: free-assets) and move the line into the game's asset map.\n";
  for (size_t i = 0; i < scan.assets.size(); ++i) {
    const br::ScannedAsset& a = scan.assets[i];
    const bool sound = a.kind == br::AssetKind::sound;
    Counts& c = sound ? sounds : models;
    const std::string& ours = match.asset_ours[i];
    if (!a.uses.empty()) ++c.named;
    std::string shown = ours.empty() ? "-" : ours;
    if (!ours.empty() && have_registry && !registry.count(ours)) {
      shown += " (not in the registry)";
      ++c.unregistered;
    }
    if (!ours.empty()) {
      ++c.mapped;
    } else if (a.shipped) {
      ++c.shipped;
    } else {
      ++c.missing;
    }
    if (ours.empty()) {
      draft += std::string(sound ? "sound " : "model ") + a.name + " ?";
      draft += a.uses.empty() ? "  # shipped, named by no map\n" : "  # " + where(a.uses[0]) + "\n";
    }
    tables[sound ? 0 : 1].rows.push_back({a.name, a.shipped ? "yes" : "no",
                                          std::to_string(a.uses.size()),
                                          a.uses.empty() ? "-" : where(a.uses[0]), shown});
  }
  lab::LabTable set_table{"Numbered sets", {"class", "pick", "maps", "ours"}, {}};
  for (size_t i = 0; i < scan.sets.size(); ++i) {
    const br::SetUse& s = scan.sets[i];
    const std::string& ours = match.set_ours[i];
    ++(ours.empty() ? sets.missing : sets.mapped);
    if (ours.empty()) {
      draft += "# set " + s.classname + " " + s.key + "=" + s.value + " <field> ?  # used by " +
               std::to_string(s.maps.size()) + " map(s), first " + s.maps[0] + "\n";
    }
    set_table.rows.push_back({s.classname, s.key + "=" + s.value, std::to_string(s.maps.size()),
                              ours.empty() ? "-" : ours});
  }
  l.table(tables[0]);
  l.table(tables[1]);
  l.table(set_table);
  l.attach("assetmap-draft.txt", std::vector<uint8_t>(draft.begin(), draft.end()));

  l.metric("packs", set.packs, "count");
  l.metric("files", double(set.files.size()), "count");
  l.metric("maps", double(scan.maps.size()), "count");
  l.metric("maps-unread", double(scan.unread.size()), "count");
  for (const auto& [key, c] : {std::pair{"sounds", sounds}, std::pair{"models", models}}) {
    const std::string k = key;
    l.metric(k, c.mapped + c.shipped + c.missing, "count");
    l.metric(k + "-named-by-maps", c.named, "count");
    l.metric(k + "-mapped", c.mapped, "count");
    l.metric(k + "-unmapped", c.shipped + c.missing, "count");
    l.metric(k + "-missing", c.missing, "count");
    if (have_registry) l.metric(k + "-not-in-registry", c.unregistered, "count");
  }
  l.metric("sets", double(scan.sets.size()), "count");
  l.metric("sets-mapped", sets.mapped, "count");

  Picture p = lab::create_picture(900, 200, Rgba{24, 26, 30});
  bar(p, 24, "SOUNDS", sounds.mapped, sounds.shipped, sounds.missing);
  bar(p, 76, "MODELS", models.mapped, models.shipped, models.missing);
  bar(p, 128, "SETS", sets.mapped, 0, sets.missing);
  lab::Shot shot{"mapped",
                 "WHAT THE ASSET MAP COVERS",
                 "Each bar is the scanned set's sounds, models or numbered sets: green ones the "
                 "asset map gives our name, amber ones shipped but unmapped, red ones unmapped "
                 "and not in the set (a map names them, the set lacks them; sets: unmapped).",
                 "no amber or red where the game needs the asset; assetmap-draft.txt lists them",
                 {{"content", content.empty() ? "the fixture set" : content},
                  {"assetmap", l.arg("assetmap").empty() ? "the fixture's" : l.arg("assetmap")}}};
  l.shot(shot, p);
}

const lab::LabSpec kSpec{
    "content-scan",
    lab::LabKind::scenario,
    "plugin:brush",
    "plugins/brush/content (the content scan, the asset map)",
    "free-assets",
    "Which sounds, models and numbered sets does a content set ship or its maps name, and which "
    "of them does a game's asset map still leave without a name of its own?",
    {
        {"content", "", "a pack or a directory of the set (none: the fixture set)"},
        {"assetmap", "", "the game's asset map (none: the fixture's own)"},
        {"registry", "", "the directory of the game's sounds.tsv and models.tsv"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
