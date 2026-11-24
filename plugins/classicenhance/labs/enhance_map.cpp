// The enhance lab's map views (enhance_map.hpp).
//
// ge:summary The enhance lab's map views: each start drawn with the pack's and the cache's textures
// ge:keywords lab enhance map views start cache cached_texture own enhanced side-by-side
// ge:skill classicenhance labs
// ge:test tests/lab_runner_test.cpp
#include "classicenhance/labs/enhance_map.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <span>
#include <vector>

#include "brush/labs/textured_view.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/cached.hpp"
#include "classicenhance/labs/fixtures.hpp"
#include "classicenhance/source.hpp"
#include "lab/raster.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance::labs {
namespace {

namespace br = ge::brush;
using lab::Picture;

constexpr int kPanelW = 480, kPanelH = 300, kLabel = 16;

// Texture `t` of the map is one the enhancer replaces: not the sky's, not a liquid's.
bool replaceable(const br::Texture& t) {
  return !t.name.empty() && t.name[0] != '*' && t.name.rfind("sky", 0) != 0 && t.width > 0 &&
         t.height > 0;
}

// The enhanced albedo with alpha 255 where the emissive map glows, as `cached_texture` gives it.
br::labs::TrueColour true_colour(const Enhanced& e) {
  br::labs::TrueColour out;
  out.width = uint32_t(e.albedo.width), out.height = uint32_t(e.albedo.height);
  out.rgba = e.albedo.rgba;
  const bool fits =
      e.glows && e.emissive.width == e.albedo.width && e.emissive.height == e.albedo.height;
  for (size_t i = 0; i + 3 < out.rgba.size(); i += 4) {
    const uint8_t* g = fits ? &e.emissive.rgba[i] : nullptr;
    out.rgba[i + 3] = g != nullptr && (g[0] > 24 || g[1] > 24 || g[2] > 24) ? 255 : 0;
  }
  return out;
}

// Each named view of `map` drawn twice, the pack's textures on the left and `replaced` on the
// right, a row a view.
Picture views_of(const br::Map& map, std::span<const uint8_t> palette,
                 std::span<const br::labs::TrueColour> replaced) {
  const auto eyes = br::labs::start_eyes(map);
  const int rows = int(eyes.size());
  Picture p = lab::create_picture(kPanelW * 2 + 6, rows * (kPanelH + kLabel), {24, 24, 28});
  for (int i = 0; i < rows; ++i) {
    const int y = i * (kPanelH + kLabel);
    lab::draw_text(p, 4, y + 3, eyes[size_t(i)].first + "  OWN", {230, 230, 230});
    lab::draw_text(p, kPanelW + 10, y + 3, eyes[size_t(i)].first + "  ENHANCED", {230, 230, 230});
    br::labs::draw_textured(p, 0, y + kLabel, kPanelW, kPanelH, map, palette,
                            eyes[size_t(i)].second);
    br::labs::draw_textured(p, kPanelW + 6, y + kLabel, kPanelW, kPanelH, map, palette,
                            eyes[size_t(i)].second, replaced);
  }
  return p;
}

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower(uint8_t(c)));
  return s;
}

}  // namespace

bool map_views(lab::Lab& lab, const std::string& source, const std::string& maps,
               const std::string& cache_root, const EnhanceOptions& options) {
  struct Loaded {
    std::string name;
    br::Map map;
    std::vector<br::labs::TrueColour> replaced;
  };
  std::vector<Loaded> loaded;
  std::vector<uint8_t> palette;
  std::string error;

  if (source.empty()) {
    // The brush plugin's fixture map in the lab's painted textures, enhanced here.
    const Palette pal = fixture_palette();
    palette.assign(pal.rgb.begin(), pal.rgb.end());
    std::vector<FixtureTexture> painted = fixture_textures(pal);
    // Walls and floors only: the painted water is a liquid's, which keeps the pack's picture.
    std::erase_if(
        painted, [](const FixtureTexture& f) { return f.name.find("water") != std::string::npos; });
    Loaded l{"fixture", {}, {}};
    std::vector<uint8_t> bytes;
    if (!br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error) ||
        !br::read_bsp(bytes, &l.map, &error)) {
      lab.fail("enhance: the fixture map: " + error,
               "next: run tests/brush_read_test; skill: brush.");
      return false;
    }
    l.replaced.resize(l.map.textures.size());
    for (size_t k = 0; k < l.map.textures.size(); ++k) {
      br::Texture& t = l.map.textures[k];
      if (!replaceable(t) || painted.empty()) continue;
      const FixtureTexture& f = painted[k % painted.size()];
      t.width = uint32_t(f.width), t.height = uint32_t(f.height), t.indexed = f.indexed;
      l.replaced[k] =
          true_colour(enhance_texture(f.name, t.indexed, f.width, f.height, pal, options));
    }
    loaded.push_back(std::move(l));
  } else {
    Source src;
    if (!open_source(source, &src, &error)) {
      lab.fail(error, "next: give --source a release zip, an installed folder or its pack.");
      return false;
    }
    std::vector<std::span<const uint8_t>> packs = {src.pack};
    for (const LaterPack& p : src.later) packs.push_back(p.bytes);
    const std::filesystem::path dir = find_cache(packs, cache_root);
    if (dir.empty()) {
      lab.fail("enhance: no enhanced cache of these packs under " +
                   (cache_root.empty() ? std::string("the default cache root") : cache_root),
               "next: run `ge tool classicenhance -- run " + source +
                   "` (pass --cache to the same root), then rerun with --cache=<that root>.");
      return false;
    }
    ge::vfs::Pak pak;
    if (!ge::vfs::read_pak(src.content(), &pak, &error)) {
      lab.fail(error, "next: check the pack with `ge tool classicenhance -- verify`.");
      return false;
    }
    const ge::vfs::PakEntry* pe = ge::vfs::pak_find(pak, "gfx/palette.lmp");
    if (pe == nullptr || pe->size != 768) {
      lab.fail("enhance: the pack has no gfx/palette.lmp", "next: give --source the classic pack.");
      return false;
    }
    const auto pb = ge::vfs::pak_file(src.content(), *pe);
    palette.assign(pb.begin(), pb.end());
    size_t from = 0;
    while (from <= maps.size()) {
      const size_t comma = std::min(maps.find(',', from), maps.size());
      const std::string name = lower(maps.substr(from, comma - from));
      from = comma + 1;
      if (name.empty()) continue;
      Loaded l{name, {}, {}};
      const std::string where = "maps/" + name + ".bsp";
      const ge::vfs::PakEntry* me = ge::vfs::pak_find(pak, where);
      if (me == nullptr || !br::read_bsp(ge::vfs::pak_file(src.content(), *me), &l.map, &error)) {
        lab.fail("enhance: no readable " + where + (error.empty() ? "" : ": " + error),
                 "next: pick maps the pack carries with --maps (start, e1m1 .. e1m8).");
        return false;
      }
      l.replaced.resize(l.map.textures.size());
      for (size_t k = 0; k < l.map.textures.size(); ++k) {
        const br::Texture& t = l.map.textures[k];
        if (!replaceable(t)) continue;
        std::vector<uint8_t> rgba;
        if (!cached_texture(dir, t.name, t.width, t.height, &rgba)) continue;
        // The cache's multiple of the pack's size: 1, 2, 4 or 8.
        const size_t area = rgba.size() / 4 / (size_t(t.width) * size_t(t.height));
        uint32_t m = 1;
        while (m < 8 && size_t(m) * m < area) m *= 2;
        l.replaced[k] = {t.width * m, t.height * m, std::move(rgba)};
      }
      loaded.push_back(std::move(l));
    }
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on this "
        "machine");
  }

  for (const Loaded& l : loaded) {
    size_t replaceable_n = 0, replaced_n = 0;
    for (size_t k = 0; k < l.map.textures.size(); ++k) {
      replaceable_n += replaceable(l.map.textures[k]);
      replaced_n += !l.replaced[k].rgba.empty();
    }
    lab.metric("map-" + l.name + ".textures", double(replaceable_n), "count");
    lab.metric("map-" + l.name + ".from-cache", double(replaced_n), "count");
    if (br::labs::start_eyes(l.map).empty()) {
      lab.note("map " + l.name + " has no start to look from; no views drawn");
      continue;
    }
    lab.shot(
        {"map-" + l.name,
         "THE MAP " + l.name + ", OWN AND ENHANCED",
         "The map from its player start and up to three other starts at eye height (the bsp "
         "lab's named views), each drawn twice: on the left with the pack's own textures "
         "through its palette, on the right with the enhanced copies " +
             std::string(source.empty() ? "(the lab's painted textures enhanced here)"
                                        : "read from the cache the pipeline wrote") +
             ", both lit by the map's lightmaps, sky and liquids the pack's own.",
         "the right-hand view is the left-hand one with finer texels: the same light, the same "
         "colours, no picture shifted or scaled on a face; glowing texels glow in both",
         {{"source", source.empty() ? "fixtures" : "local pack"},
          {"from-cache", std::to_string(replaced_n) + " of " + std::to_string(replaceable_n)}}},
        views_of(l.map, palette, l.replaced));
  }
  return true;
}

}  // namespace ge::classicenhance::labs
