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

std::string plain_text(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char raw : text) {
    const auto c = static_cast<unsigned char>(static_cast<unsigned char>(raw) & 0x7f);
    if (c >= 32 && c < 127)
      out += static_cast<char>(c);
    else if (c == 16)
      out += '[';
    else if (c == 17)
      out += ']';
    else if (c >= 29 && c <= 31)
      out += '-';
    else if (c == 5 || c == 14 || c == 15 || c == 28)
      out += '.';
  }
  return out;
}

std::vector<DemoPrint> PrintJoin::feed(const DemoPrint& p) {
  std::vector<DemoPrint> out;
  if (pending.empty()) level = p.level;
  for (const char c : p.text) {
    if (c != '\n') {
      pending += c;
      continue;
    }
    if (plain_text(pending).find_first_not_of(' ') != std::string::npos)
      out.push_back({level, std::move(pending)});
    pending.clear();
    level = p.level;
  }
  return out;
}

void DemoNotices::take(const DemoState& s, double t) {
  for (const DemoPrint& p : s.printed)
    for (DemoPrint& line : join.feed(p))
      lines.push_back({std::move(line.text), line.level >= 3, t});
  while (lines.size() > kNoticeLines) lines.erase(lines.begin());
  if (s.centred) {
    centre = s.centre;
    // Long enough to read: two seconds, or a twentieth of a second a character.
    centre_until = t + std::max(kCentreSeconds, static_cast<double>(centre.size()) / 20.0);
  }
}

std::vector<CaptionLine> DemoNotices::at(double t) {
  std::erase_if(lines, [t](const Notice& n) {
    return t - n.born > (n.chat ? kChatSeconds : kNoticeSeconds) || t < n.born;
  });
  std::vector<CaptionLine> out;
  out.reserve(lines.size());
  for (const Notice& n : lines) out.push_back({n.text, n.chat});
  return out;
}

namespace {

// One line at (x, baseline) in the sheet (its own bytes; `gold` sets the second half) or the
// charset (plain ASCII, tinted). Returns its advance.
void draw_line(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, float px, float x, float baseline,
               const CaptionLine& l, ge::ui::Align align) {
  const ge::ui::Color shadow = ge::ui::hex(0x000000, 0.75f);
  const float drop = std::max(1.0f, px / 8);
  if (!glyphs.empty()) {
    std::string text = l.text;
    if (l.gold)
      for (char& c : text)
        if (c != ' ') c = static_cast<char>(static_cast<unsigned char>(c) | 0x80);
    ge::hud::draw_shadowed(dl, ge::hud::Face::glyph_sheet(glyphs), px, x, baseline, text,
                           ge::ui::hex(0xffffff), shadow, drop, align);
    return;
  }
  // The charset's cell is twice as tall as wide: a line of `px` reads as the sheet's 8 x 8 does
  // at about 1.4 times the height. Its gold is a tint, and the sheet's own gold bytes are too.
  bool gold = l.gold;
  for (const char c : l.text) gold = gold || (static_cast<unsigned char>(c) & 0x80) != 0;
  ge::hud::draw_shadowed(dl, ge::hud::Face::bitmap(ge::charset::FontId::rounded),
                         std::round(px * 1.4f), x, baseline, plain_text(l.text),
                         gold ? ge::ui::hex(0xf2c060) : ge::ui::hex(0xf4f4f4), shadow, drop, align);
}

// A line's step down the picture in either type.
float line_step(const ge::ui::Image& glyphs, float px) {
  return glyphs.empty() ? std::round(px * 1.4f * 1.1f) : std::round(px * 1.25f);
}

}  // namespace

void draw_caption(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                  std::span<const CaptionLine> lines, std::span<const CaptionLine> notices) {
  (void)width;
  const auto px = static_cast<float>(caption_size(height));
  const float step = line_step(glyphs, px), margin = px;
  float baseline = margin + (glyphs.empty() ? std::round(px * 1.4f) : px);
  for (const CaptionLine& l : lines) {
    draw_line(dl, glyphs, px, margin, baseline, l, ge::ui::Align::left);
    baseline += step;
  }
  if (!lines.empty()) baseline += step / 2;
  for (const CaptionLine& l : notices) {
    draw_line(dl, glyphs, px, margin, baseline, l, ge::ui::Align::left);
    baseline += step;
  }
}

void draw_centre(ge::ui::DrawList& dl, const ge::ui::Image& glyphs, int width, int height,
                 std::string_view text) {
  std::vector<std::string> rows;
  size_t from = 0;
  while (from <= text.size()) {
    size_t to = text.find('\n', from);
    if (to == std::string_view::npos) to = text.size();
    rows.emplace_back(text.substr(from, to - from));
    from = to + 1;
  }
  while (!rows.empty() && plain_text(rows.back()).empty()) rows.pop_back();
  if (rows.empty()) return;
  const auto px = static_cast<float>(caption_size(height));
  const float step = line_step(glyphs, px);
  float baseline =
      std::round(static_cast<float>(height) * 0.35f - step * static_cast<float>(rows.size()) / 2) +
      px;
  for (const std::string& r : rows) {
    draw_line(dl, glyphs, px, static_cast<float>(width) / 2, baseline, {r, false},
              ge::ui::Align::center);
    baseline += step;
  }
}

}  // namespace shooters_edge::client
