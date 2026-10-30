// LAB `pak`: the search path (engine/vfs/search.hpp) over game directories. Which source wins
// each file, which files a later source hides, does every file read back, and do the palette, the
// colour map and every picture read clean? Without `--root` it mounts a fixture tree it writes
// itself (two game directories: packs numbered past ten, a pk3, a pk3dir, loose files, a mod over
// the base), so the run is deterministic; with `--root=<dir> --game=<name>` it mounts the content
// the user points it at, which is read at run time and never committed. The lookup's speed is
// measured.
//
// ge:summary Lab: the search path's winners, hidden files, reads, palette, colour map and pictures
// ge:keywords lab pak pk3 pk3dir search-path gamedir winner override hidden palette colormap
// ge:keywords picture lmp wad lookup speed free-content
// ge:skill vfs labs
// ge:test tests/vfs_search_test.cpp
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "vfs/palette.hpp"
#include "vfs/search.hpp"
#include "vfs/wad.hpp"

namespace {

namespace fs = std::filesystem;
namespace lab = ge::lab;
namespace vfs = ge::vfs;
using lab::Det;
using lab::Picture;

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

constexpr lab::Rgba kPaper{246, 244, 238, 255};
constexpr lab::Rgba kInk{30, 32, 38, 255};

std::string fold(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    if (c == '\\') c = '/';
  }
  return out;
}

void put(const fs::path& p, const std::vector<uint8_t>& bytes) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// A palette of 16 ramps of 16 shades, and a colour map that darkens each index toward the
// darkest shade of its ramp.
vfs::Palette fixture_palette() {
  vfs::Palette p;
  const uint8_t hues[16][3] = {{255, 255, 255}, {150, 110, 70}, {120, 120, 150}, {90, 140, 70},
                               {200, 60, 50},   {230, 170, 60}, {170, 120, 90},  {200, 160, 130},
                               {140, 100, 150}, {150, 90, 110}, {200, 190, 160}, {80, 140, 140},
                               {230, 220, 60},  {70, 90, 200},  {250, 120, 40},  {250, 240, 200}};
  for (int i = 0; i < 256; ++i) {
    const int ramp = i / 16, shade = i % 16;
    for (int k = 0; k < 3; ++k)
      p.rgb[size_t(i * 3 + k)] = uint8_t(hues[ramp][k] * (16 - shade) / 16);
  }
  return p;
}

vfs::Colormap fixture_colormap() {
  vfs::Colormap c;
  for (int level = 0; level < int(vfs::kColormapLevels); ++level) {
    for (int i = 0; i < 256; ++i) {
      const int shade = std::min(15, i % 16 + level / 4);
      c.map[size_t(level * 256 + i)] = uint8_t(i / 16 * 16 + shade);
    }
  }
  return c;
}

vfs::IndexedPicture fixture_picture(int w, int h, int ramp) {
  vfs::IndexedPicture p;
  p.width = w, p.height = h;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) p.indices.push_back(uint8_t(ramp * 16 + (x / 4 + y / 4) % 16));
  }
  return p;
}

bool write_fixture(const fs::path& root, std::string* error) {
  fs::remove_all(root);
  const fs::path base = root / "base", mod = root / "mod";
  const auto pak = [&](const fs::path& p, const Files& files) {
    std::vector<uint8_t> b;
    if (!vfs::write_pak(files, &b, error)) return false;
    put(p, b);
    return true;
  };
  bool ok =
      pak(base / "pak0.pak", {{"gfx/palette.lmp", vfs::write_palette(fixture_palette())},
                              {"gfx/colormap.lmp", vfs::write_colormap(fixture_colormap())},
                              {"gfx/menu.lmp", vfs::write_picture(fixture_picture(64, 32, 4))},
                              {"default.cfg", text("bind w +forward")},
                              {"maps/start.bsp", text("pak0 start")}});
  ok = ok &&
       pak(base / "pak1.pak", {{"maps/start.bsp", text("pak1 start")},
                               {"gfx/bar.lmp", vfs::write_picture(fixture_picture(96, 16, 13))}});
  ok = ok && pak(base / "pak2.pak", {{"sound/door.wav", text("pak2 door")}});
  ok = ok && pak(base / "pak10.pak", {{"sound/door.wav", text("pak10 door")}});
  put(base / "zz.pk3",
      vfs::write_stored_zip({{"textures/wall.tex", text("zz.pk3")},
                             {"gfx/face.lmp", vfs::write_picture(fixture_picture(24, 24, 7))}}));
  put(base / "aa.pk3dir" / "textures" / "wall.tex", text("aa.pk3dir"));
  put(base / "aa.pk3dir" / "scripts" / "Sky.txt", text("aa.pk3dir sky"));
  put(base / "Default.CFG", text("loose cfg"));
  ok = ok && pak(mod / "pak0.pak", {{"maps/start.bsp", text("mod start")}});
  put(mod / "progs.dat", text("mod progs"));
  return ok;
}

std::string kind_word(vfs::SourceKind k) {
  return k == vfs::SourceKind::pack ? "pack" : k == vfs::SourceKind::zip ? "zip" : "directory";
}

std::string short_path(const std::string& path, const std::string& root) {
  if (path.rfind(root, 0) == 0) {
    std::string s = path.substr(root.size());
    while (!s.empty() && (s[0] == '/' || s[0] == '\\')) s.erase(0, 1);
    return s.empty() ? "." : s;
  }
  return path;
}

// Every folded name a source holds.
std::vector<std::string> names_of(const vfs::Source& s) {
  std::vector<std::string> out;
  if (s.kind == vfs::SourceKind::pack) {
    for (const vfs::PakEntry& e : s.pak.entries) out.push_back(fold(e.path));
  } else if (s.kind == vfs::SourceKind::zip) {
    for (const vfs::ZipEntry& e : s.zip.entries) {
      if (!e.name.empty() && e.name.back() != '/') out.push_back(fold(e.name));
    }
  } else {
    for (const std::string& f : s.files) out.push_back(fold(f));
  }
  return out;
}

Picture palette_picture(const vfs::Palette& pal) {
  const int cell = 16;
  Picture p = lab::create_picture(cell * 16, cell * 16, kPaper);
  for (int i = 0; i < 256; ++i) {
    const lab::Rgba c{pal.rgb[size_t(i * 3)], pal.rgb[size_t(i * 3 + 1)],
                      pal.rgb[size_t(i * 3 + 2)], 255};
    lab::fill_rect(p, (i % 16) * cell, (i / 16) * cell, cell, cell, c);
  }
  return p;
}

Picture colormap_picture(const vfs::Colormap& cm, const vfs::Palette& pal) {
  const int sx = 2, sy = 4;
  Picture p = lab::create_picture(256 * sx, int(vfs::kColormapLevels) * sy, kPaper);
  for (int level = 0; level < int(vfs::kColormapLevels); ++level) {
    for (int i = 0; i < 256; ++i) {
      const uint8_t k = cm.at(level, uint8_t(i));
      const lab::Rgba c{pal.rgb[size_t(k) * 3], pal.rgb[size_t(k) * 3 + 1],
                        pal.rgb[size_t(k) * 3 + 2], 255};
      lab::fill_rect(p, i * sx, level * sy, sx, sy, c);
    }
  }
  return p;
}

Picture indexed_picture(const vfs::IndexedPicture& ip, const vfs::Palette& pal) {
  Picture p = lab::create_picture(ip.width, ip.height, kPaper);
  for (size_t i = 0; i < ip.indices.size(); ++i) {
    const uint8_t k = ip.indices[i];
    for (size_t c = 0; c < 3; ++c) p.rgba[i * 4 + c] = pal.rgb[size_t(k) * 3 + c];
    p.rgba[i * 4 + 3] = 255;
  }
  return p;
}

void run(lab::Lab& lab) {
  std::string root = lab.arg("root");
  std::vector<std::string> games;
  const bool fixture = root.empty();
  std::string error;
  if (fixture) {
    root = (fs::temp_directory_path() / "ge_pak_lab").string();
    if (!write_fixture(root, &error)) {
      lab.fail("the fixture tree could not be written: " + error,
               "Run tests/vfs_search_test.cpp. Skill: vfs.");
      return;
    }
    games = {"base", "mod"};
  } else {
    if (lab.arg("game").empty()) {
      lab.fail("--root was given without --game",
               "Name the base game directory under --root with --game=<name>. Skill: vfs.");
      return;
    }
    games.push_back(lab.arg("game"));
    if (!lab.arg("mod").empty()) games.push_back(lab.arg("mod"));
  }

  vfs::SearchPath sp;
  for (const std::string& g : games) {
    if (!vfs::add_game(&sp, root, g, &error)) {
      lab.fail("game directory " + g + " was refused: " + error,
               "Point --root at the directory that holds the game directories, and --game at "
               "one of them. Skill: vfs.");
      return;
    }
  }
  const std::vector<vfs::Listed> listed = vfs::list(sp);

  // Which sources hold each name, and who wins it.
  std::map<std::string, std::vector<uint32_t>> holders;
  for (uint32_t s = 0; s < sp.sources.size(); ++s) {
    for (const std::string& n : names_of(sp.sources[s])) holders[n].push_back(s);
  }
  std::vector<size_t> wins(sp.sources.size(), 0), hidden(sp.sources.size(), 0);
  for (const vfs::Listed& l : listed) {
    ++wins[l.ref.source];
    const auto it = holders.find(std::string(l.name));
    if (it == holders.end()) continue;
    for (uint32_t s : it->second) {
      if (s != l.ref.source) ++hidden[s];
    }
  }

  lab::LabTable sources{"the sources, lowest first (the last mounted wins)",
                        {"#", "game", "kind", "path", "files", "wins", "hidden"},
                        {}};
  size_t packs = 0, zips = 0, dirs = 0;
  for (size_t s = 0; s < sp.sources.size(); ++s) {
    const vfs::Source& src = sp.sources[s];
    packs += src.kind == vfs::SourceKind::pack;
    zips += src.kind == vfs::SourceKind::zip;
    dirs += src.kind == vfs::SourceKind::directory;
    sources.rows.push_back({std::to_string(s), src.game, kind_word(src.kind),
                            short_path(src.path, root), std::to_string(names_of(src).size()),
                            std::to_string(wins[s]), std::to_string(hidden[s])});
  }
  lab.table(std::move(sources));

  lab::LabTable overrides{
      "every name more than one source holds, and its winner", {"name", "held by", "winner"}, {}};
  size_t shadowed = 0;
  for (const vfs::Listed& l : listed) {
    const auto it = holders.find(std::string(l.name));
    if (it == holders.end() || it->second.size() < 2) continue;
    ++shadowed;
    std::string by;
    for (uint32_t s : it->second) {
      if (!by.empty()) by += ", ";
      by += short_path(sp.sources[s].path, root);
    }
    if (overrides.rows.size() < 40) {
      overrides.rows.push_back(
          {std::string(l.name), by, short_path(sp.sources[l.ref.source].path, root)});
    }
  }
  lab.table(std::move(overrides));

  // Every file read back; the pictures, the palette and the colour map read as what they are.
  size_t read_ok = 0, bytes = 0, pictures = 0, pictures_bad = 0;
  std::vector<uint8_t> b;
  std::vector<std::pair<std::string, vfs::IndexedPicture>> shown;
  for (const vfs::Listed& l : listed) {
    if (!vfs::read(sp, l.ref, &b, &error)) {
      lab.fail(std::string(l.name) + " did not read: " + error, "Skill: vfs.");
      continue;
    }
    ++read_ok;
    bytes += b.size();
    const std::string_view n = l.name;
    const bool lmp = n.size() > 4 && n.substr(n.size() - 4) == ".lmp";
    if (lmp && n != "gfx/palette.lmp" && n != "gfx/colormap.lmp") {
      vfs::IndexedPicture ip;
      if (vfs::read_picture(b, &ip, &error)) {
        ++pictures;
        if (shown.size() < 12) shown.emplace_back(std::string(n), std::move(ip));
      } else {
        ++pictures_bad;
        lab.note(std::string(n) + " is not a picture: " + error);
      }
    }
  }

  lab.metric("files", double(listed.size()), "count");
  lab.metric("sources", double(sp.sources.size()), "count");
  lab.metric("sources.packs", double(packs), "count");
  lab.metric("sources.zips", double(zips), "count");
  lab.metric("sources.directories", double(dirs), "count");
  lab.metric("files.shadowed", double(shadowed), "count");
  lab.metric("files.read", double(read_ok), "count");
  lab.metric("files.bytes", double(bytes), "B");
  lab.metric("pictures", double(pictures), "count");
  lab.metric("pictures.refused", double(pictures_bad), "count");

  // The lookup's speed: every name, in a spelling of its own (upper case, back slashes).
  std::vector<std::string> spelled;
  for (const vfs::Listed& l : listed) {
    std::string s(l.name);
    for (char& c : s) {
      if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
      if (c == '/') c = '\\';
    }
    spelled.push_back(std::move(s));
  }
  size_t found = 0;
  const int rounds = std::max(1, int(lab.arg_num("rounds")));
  const auto t0 = std::chrono::steady_clock::now();
  for (int r = 0; r < rounds; ++r) {
    for (const std::string& s : spelled) found += vfs::find(sp, s) != nullptr;
  }
  const double ns =
      std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() /
      double(std::max<size_t>(1, spelled.size() * size_t(rounds)));
  lab.metric("lookup.ns", ns, "ns", Det::measured);
  if (found != spelled.size() * size_t(rounds)) {
    lab.fail("a name spelled in upper case with back slashes was not found",
             "Names fold case and slashes as they are hashed: tests/vfs_search_test.cpp. Skill: "
             "vfs.");
  }
  lab.note("The lookup is timed over every name spelled in upper case with back slashes, " +
           std::to_string(rounds) + " rounds; time it on PRESET=release.");

  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"root", fixture ? "the lab's fixture" : "--root"},
      {"games", games.size() > 1 ? games[0] + " then " + games[1] : games[0]}};
  const Det det = fixture ? Det::deterministic : Det::measured;

  vfs::Palette pal;
  const vfs::FileRef* pf = vfs::find(sp, "gfx/palette.lmp");
  const bool have_palette =
      pf && vfs::read(sp, *pf, &b, &error) && vfs::read_palette(b, &pal, &error);
  lab.metric("palette", have_palette ? 1 : 0, "bool");
  if (!have_palette) {
    lab.note("No palette read (gfx/palette.lmp): " + (pf ? error : std::string("not found")) +
             "; the pictures are drawn in grey.");
    for (int i = 0; i < 256; ++i) {
      for (int k = 0; k < 3; ++k) pal.rgb[size_t(i * 3 + k)] = uint8_t(i);
    }
  } else {
    lab.shot({"palette", "THE PALETTE",
              "The 256 colours of gfx/palette.lmp, index 0 at the top left, sixteen to a row.",
              "the rows run as ramps from light to dark; nothing is missing or shifted", inputs},
             palette_picture(pal), det);
  }

  vfs::Colormap cm;
  const vfs::FileRef* cf = vfs::find(sp, "gfx/colormap.lmp");
  const bool have_cm = cf && vfs::read(sp, *cf, &b, &error) && vfs::read_colormap(b, &cm, &error);
  lab.metric("colormap", have_cm ? 1 : 0, "bool");
  if (have_cm) {
    lab.shot(
        {"colormap", "THE COLOUR MAP THROUGH THE PALETTE",
         "gfx/colormap.lmp: a column an index, a row a light level from full bright at the "
         "top to dark at the bottom, each cell the colour that index is drawn with at that "
         "level.",
         "every column darkens smoothly downward; full-bright columns, if any, stay lit", inputs},
        colormap_picture(cm, pal), det);
  } else {
    lab.note("No colour map read (gfx/colormap.lmp): " + (cf ? error : std::string("not found")));
  }

  if (!shown.empty()) {
    const int cell = 160, label_h = 14, cols = 4;
    const int rows = (int(shown.size()) + cols - 1) / cols;
    Picture sheet = lab::create_picture(cols * cell, rows * (cell + label_h), kPaper);
    for (size_t i = 0; i < shown.size(); ++i) {
      const auto& [name, ip] = shown[i];
      Picture pic = indexed_picture(ip, pal);
      const double scale = std::min(1.0 * (cell - 8) / std::max(1, ip.width),
                                    1.0 * (cell - 8) / std::max(1, ip.height));
      const double fit = std::min(scale, 4.0);
      pic = lab::resized(pic, std::max(1, int(ip.width * fit)), std::max(1, int(ip.height * fit)));
      const int x = int(i % cols) * cell, y = int(i / cols) * (cell + label_h);
      lab::draw_text(sheet, x + 4, y + 3, name, kInk);
      lab::blit(sheet, pic, x + 4, y + label_h);
    }
    lab.shot({"pictures", "THE FIRST PICTURES ON THE SEARCH PATH",
              "Up to twelve .lmp pictures in name order, each read off the source that wins its "
              "name and drawn through the palette.",
              "each picture is whole and upright, its colours its own (not a grey ramp)", inputs},
             sheet, det);
  }
}

const lab::LabSpec kSpec{
    "pak",
    lab::LabKind::sim,
    "engine",
    "engine/vfs/search.hpp, engine/vfs/palette.hpp",
    "vfs",
    "Which source wins each file of a search path, what does a later source hide, and does every "
    "file, the palette, the colour map and every picture read back clean?",
    {
        {"root", "", "the directory that holds the game directories (empty: the lab's fixture)"},
        {"game", "", "the base game directory under --root (needed with --root)"},
        {"mod", "", "a game directory mounted over the base (optional)"},
        {"rounds", "200", "lookup rounds timed"},
    },
    "--rounds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
