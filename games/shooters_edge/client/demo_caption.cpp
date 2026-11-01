// The demo video's upper-left words (demo_caption.hpp).
//
// ge:summary demo_caption's lines from the demo's state, the glyph sheet's loaders, the drawing
// ge:keywords demo-video caption conchars glyph-sheet team-score clock draw_caption
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_demo_caption_test.cpp
#include "shooters_edge/client/demo_caption.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <utility>

#include "assets/image.hpp"
#include "hud/face.hpp"
#include "vfs/wad.hpp"

namespace shooters_edge::client {

namespace {

constexpr size_t kSlots = 32;

std::string frags_text(int32_t frags) {
  return std::to_string(frags) + (frags == 1 || frags == -1 ? " frag" : " frags");
}

}  // namespace

std::string caption_clock(double t) {
  const auto s = static_cast<long long>(std::floor(std::max(0.0, t) + 1e-6));
  char b[48];
  if (s >= 3600)
    std::snprintf(b, sizeof b, "%lld:%02lld:%02lld", s / 3600, s / 60 % 60, s % 60);
  else
    std::snprintf(b, sizeof b, "%lld:%02lld", s / 60, s % 60);
  return b;
}

int caption_size(int height) {
  return 8 * std::max(1, static_cast<int>(std::lround(static_cast<double>(height) / 360.0)));
}

std::vector<CaptionLine> demo_caption(const DemoState& s, std::string_view title, double t) {
  std::vector<CaptionLine> out;
  if (!s.map.empty()) {
    std::string line = s.map;
    // A title's line breaks and other control bytes as single spaces, trimmed.
    std::string clean;
    for (const char c : title) {
      const bool space = static_cast<unsigned char>(c) < 32 || c == ' ';
      if (space && (clean.empty() || clean.back() == ' ')) continue;
      clean += space ? ' ' : c;
    }
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (!clean.empty() && clean != s.map) line += "  " + clean;
    out.push_back({line, true});
  }
  out.push_back({caption_clock(t), false});

  if (s.format == DemoFormat::single) {
    const int32_t monsters = s.stats[kStatTotalMonsters], secrets = s.stats[kStatTotalSecrets];
    std::string line;
    if (monsters > 0)
      line += "kills " + std::to_string(s.stats[kStatMonsters]) + "/" + std::to_string(monsters);
    if (secrets > 0)
      line += (line.empty() ? "" : "  ") + std::string("secrets ") +
              std::to_string(s.stats[kStatSecrets]) + "/" + std::to_string(secrets);
    if (!line.empty()) out.push_back({line, false});
    return out;
  }

  // The tracked player: the recording's own (client) or the one the view rides (multiview).
  const auto me = static_cast<size_t>(s.player);
  if (me < kSlots && !s.names[me].empty())
    out.push_back({"watching " + s.names[me] + ", " + frags_text(s.frags[me]), false});

  // The match: the teams' frags summed when two or more teams play, else the leader.
  std::map<std::string, int32_t> teams;
  size_t named = 0;
  size_t leader = kSlots;
  for (size_t k = 0; k < kSlots; k++) {
    if (s.names[k].empty()) continue;
    named++;
    if (!s.teams[k].empty()) teams[s.teams[k]] += s.frags[k];
    if (leader == kSlots || s.frags[k] > s.frags[leader]) leader = k;
  }
  if (teams.size() >= 2) {
    std::vector<std::pair<std::string, int32_t>> order(teams.begin(), teams.end());
    std::stable_sort(order.begin(), order.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    std::string line;
    for (const auto& [team, frags] : order)
      line += (line.empty() ? "" : "  ") + team + " " + std::to_string(frags);
    out.push_back({line, false});
  } else if (named >= 2 && leader != me) {
    out.push_back({"leader " + s.names[leader] + ", " + frags_text(s.frags[leader]), false});
  }
  return out;
}

bool caption_glyphs_from_packs(const CaptionFind& find, ge::ui::Image* out, std::string* why) {
  std::vector<uint8_t> pal_store, wad_store;
  const std::span<const uint8_t> palette = find("gfx/palette.lmp", pal_store);
  if (palette.size() != 768) {
    if (why != nullptr) *why = "the packs have no gfx/palette.lmp of 768 bytes";
    return false;
  }
  const std::span<const uint8_t> wad_bytes = find("gfx.wad", wad_store);
  ge::vfs::Wad wad;
  std::string bad;
  if (wad_bytes.empty() || !ge::vfs::read_wad(wad_bytes, &wad, &bad)) {
    if (why != nullptr)
      *why = "the packs have no readable gfx.wad" + (bad.empty() ? "" : ": " + bad);
    return false;
  }
  const ge::vfs::WadEntry* e = ge::vfs::wad_find(wad, "conchars");
  ge::vfs::IndexedPicture p;
  if (e == nullptr ||
      !ge::vfs::read_raw_picture(ge::vfs::wad_lump(wad_bytes, *e), 128, 128, &p, &bad)) {
    if (why != nullptr) *why = "gfx.wad has no 128 x 128 glyph sheet `conchars`";
    return false;
  }
  out->width = p.width;
  out->height = p.height;
  out->rgba.resize(p.indices.size() * 4);
  for (size_t i = 0; i < p.indices.size(); i++) {
    const size_t at = size_t{p.indices[i]} * 3;
    out->rgba[i * 4] = palette[at];
    out->rgba[i * 4 + 1] = palette[at + 1];
    out->rgba[i * 4 + 2] = palette[at + 2];
    out->rgba[i * 4 + 3] = p.indices[i] == 0 ? 0 : 255;
  }
  return true;
}

bool caption_glyphs_from_png(const std::filesystem::path& path, ge::ui::Image* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  const std::optional<ge::assets::Image> img = ge::assets::decode_png(bytes);
  if (!img || img->width < 16 || img->width % 16 != 0 || img->height < 16 || img->height % 16 != 0)
    return false;
  out->width = img->width;
  out->height = img->height;
  out->rgba = img->rgba;
  return true;
}

void draw_caption(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                  std::span<const CaptionLine> lines) {
  (void)width;
  const int size = caption_size(height);
  const auto px = static_cast<float>(size);
  const float margin = px, step = std::round(px * 1.25f);
  const ge::ui::Color shadow = ge::ui::hex(0x000000, 0.75f);
  const float drop = std::max(1.0f, px / 8);
  float baseline = margin + px;
  if (!glyphs.empty()) {
    // The sheet's own colours, white untinted; a gold line from its second half.
    const ge::hud::Face face = ge::hud::Face::glyph_sheet(glyphs);
    for (const CaptionLine& l : lines) {
      std::string text = l.text;
      if (l.gold)
        for (char& c : text)
          if (c != ' ') c = static_cast<char>(static_cast<unsigned char>(c) | 0x80);
      ge::hud::draw_shadowed(dl, face, px, margin, baseline, text, ge::ui::hex(0xffffff), shadow,
                             drop);
      baseline += step;
    }
    return;
  }
  // The charset's cell is twice as tall as wide: a line of `px` reads as the sheet's 8 x 8 does
  // at about 1.4 times the height.
  const ge::hud::Face face = ge::hud::Face::bitmap(ge::charset::FontId::rounded);
  const float cs = std::round(px * 1.4f);
  for (const CaptionLine& l : lines) {
    ge::hud::draw_shadowed(dl, face, cs, margin, baseline, l.text,
                           l.gold ? ge::ui::hex(0xf2c060) : ge::ui::hex(0xf4f4f4), shadow, drop);
    baseline += std::round(cs * 1.1f);
  }
}

}  // namespace shooters_edge::client
