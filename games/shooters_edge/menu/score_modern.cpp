// The scores in Shooter's Edge's own look (score_modern.hpp).
//
// ge:summary Draws the modern board, the level card and the solo strip in screen pixels
// ge:keywords draw_modern_board draw_modern_level_card draw_modern_solo_strip player_colour
// ge:keywords ping-tint rows chips tiles bars
// ge:skill ui
// ge:test tests/shooters_edge_scoreboard_test.cpp
#include "shooters_edge/menu/score_modern.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

// The modern menu's colours: the slab, the rust accent, the warm inks.
const ui::Color kSlab = ui::hex(0x15100c, 0.84f);
const ui::Color kSlabEdge = ui::hex(0xf2e8dc, 0.08f);
const ui::Color kAccent = ui::hex(0xd8703c);
const ui::Color kInk = ui::hex(0xf2e8dc);
const ui::Color kMuted = ui::hex(0xa8998b);
const ui::Color kStripe = ui::hex(0xffffff, 0.035f);
const ui::Color kSelf = ui::hex(0xd8703c, 0.2f);
const ui::Color kTrack = ui::hex(0xffffff, 0.1f);
const ui::Color kGood = ui::hex(0x8bcf7c), kFair = ui::hex(0xe2b955), kPoor = ui::hex(0xe56a4e);

// The game's own hues a player colour, 0..13.
constexpr uint32_t kHues[14] = {0xd9d4cc, 0x8b6a3e, 0x8a9bd6, 0x5f8f4a, 0xc0473a,
                                0xb09a3a, 0xd27a32, 0xd690a6, 0x9b62b0, 0xa0708f,
                                0x6fb0ae, 0xe0c060, 0x4f6ee0, 0xe6dc4a};

float unit_of(int height) { return std::clamp(static_cast<float>(height) / 720.0f, 0.55f, 3.0f); }

const ui::Font& bold() { return ui::ui_font(ui::Weight::semibold); }
const ui::Font& plain() { return ui::ui_font(ui::Weight::medium); }

void slab(ui::DrawList& dl, ui::Rect r, float u) {
  ui::Box b;
  b.rect = r;
  b.radius = 10 * u;
  b.fill = ui::solid(kSlab);
  b.border = std::max(1.0f, u);
  b.border_color = kSlabEdge;
  dl.box(b);
}

ui::Color ping_tint(int ping) { return ping <= 50 ? kGood : ping <= 120 ? kFair : kPoor; }

std::string clock_text(int minutes, int seconds) {
  const int s = std::clamp(seconds, 0, 59);
  return std::to_string(std::max(minutes, 0)) + ":" + (s < 10 ? "0" : "") + std::to_string(s);
}

// A bar `share` (0..1) full.
void bar(ui::DrawList& dl, ui::Rect r, float share) {
  dl.rect(r, kTrack, r.h / 2);
  const float f = std::clamp(share, 0.0f, 1.0f);
  if (f > 0) dl.rect({r.x, r.y, std::max(r.h, r.w * f), r.h}, kAccent, r.h / 2);
}

float share(int part, int whole) {
  return whole > 0 ? static_cast<float>(part) / static_cast<float>(whole) : 0.0f;
}

// The players' column edges inside a card from x0 to x1.
struct Columns {
  float rank, chip, name, team, frags, time, ping, loss;  // left (rank, chip, name, team) or right
};

Columns columns(float x0, float x1, float pad, float u, bool teams) {
  const float w = x1 - x0;
  Columns c{};
  c.rank = x0 + pad + 22 * u;  // right edge
  c.chip = x0 + pad + 34 * u;
  c.name = c.chip + 30 * u;
  c.team = x0 + w * 0.46f;
  c.loss = x1 - pad;
  c.ping = c.loss - 70 * u;
  c.time = c.ping - 70 * u;
  c.frags = c.time - (teams ? 74 : 90) * u;
  return c;
}

}  // namespace

ui::Color player_colour(int colour) { return ui::hex(kHues[std::clamp(colour, 0, 13)]); }

void draw_modern_board(ui::DrawList& dl, int width, int height, const Scoreboard& b, bool teams) {
  const float u = unit_of(height);
  const float fw = static_cast<float>(width), fh = static_cast<float>(height);
  const float w = std::min(fw - 32 * u, 820 * u);
  const float x0 = std::floor((fw - w) / 2), x1 = x0 + w;
  const float pad = 24 * u;
  const float top = std::max(16.0f, 56 * u);
  const std::vector<size_t> order = score_order(b);
  std::vector<size_t> players, watchers;
  for (size_t k : order) (b.rows[k].spectator ? watchers : players).push_back(k);
  const std::vector<TeamRow> team_list =
      b.teamplay && teams ? team_rows(b) : std::vector<TeamRow>{};

  // The heights: the header, the team tiles, the column heads, the rows (shrunk to fit), the
  // spectators' line.
  const float head_h = 64 * u, tiles_h = team_list.empty() ? 0 : 84 * u, cols_h = 30 * u;
  const float watch_h = watchers.empty() ? 0 : 36 * u, foot = 16 * u;
  const float room = fh - top - 16 * u - head_h - tiles_h - cols_h - watch_h - foot;
  const float row_h = players.empty() ? 0
                                      : std::clamp(room / static_cast<float>(players.size()),
                                                   std::min(16 * u, 16.0f), 38 * u);
  const size_t shown =
      row_h <= 0 ? 0 : std::min(players.size(), static_cast<size_t>(std::max(0.0f, room / row_h)));
  const float card_h =
      head_h + tiles_h + cols_h + row_h * static_cast<float>(shown) + watch_h + foot;
  slab(dl, {x0, top, w, card_h}, u);

  // The header.
  float y = top;
  dl.text(bold(), 26 * u, x0 + pad, y + 40 * u, "SCOREBOARD", kInk, ui::Align::left, 3 * u);
  const std::string rules = b.teamplay ? "TEAMS" : "FREE FOR ALL";
  dl.text(plain(), 13 * u, x0 + pad + 190 * u, y + 39 * u, rules, kAccent, ui::Align::left, 2 * u);
  dl.text(bold(), 22 * u, x1 - pad, y + 40 * u, clock_text(b.minutes, b.seconds), kInk,
          ui::Align::right, 1 * u);
  dl.rect({x0 + pad, y + head_h - 2 * u, w - 2 * pad, std::max(1.0f, u)}, kSlabEdge);
  y += head_h;

  // The teams' tiles.
  if (!team_list.empty()) {
    const size_t n = std::min<size_t>(team_list.size(), 4);
    const float gap = 10 * u;
    const float tw = (w - 2 * pad - gap * static_cast<float>(n - 1)) / static_cast<float>(n);
    for (size_t i = 0; i < n; i++) {
      const TeamRow& t = team_list[i];
      const float tx = x0 + pad + (tw + gap) * static_cast<float>(i), ty = y + 12 * u;
      ui::Box tile;
      tile.rect = {tx, ty, tw, 62 * u};
      tile.radius = 6 * u;
      tile.fill = ui::solid(t.self ? kSelf : kStripe);
      tile.border = t.self ? 2 * u : 0;
      tile.border_color = kAccent;
      dl.box(tile);
      std::string name = t.team;
      for (char& c : name) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
      dl.text(plain(), 13 * u, tx + 12 * u, ty + 22 * u, name, kMuted, ui::Align::left, 2 * u);
      dl.text(bold(), 30 * u, tx + tw - 12 * u, ty + 46 * u, std::to_string(t.frags), kInk,
              ui::Align::right);
      dl.text(plain(), 13 * u, tx + 12 * u, ty + 48 * u,
              std::to_string(t.players) + (t.players == 1 ? " player" : " players") + " · " +
                  std::to_string(t.mean) + " ms",
              kMuted);
    }
    y += tiles_h;
  }

  // The column heads.
  const bool team_col = b.teamplay;
  const Columns c = columns(x0, x1, pad, u, team_col);
  const float hy = y + 21 * u, hs = 12 * u, ht = 1.5f * u;
  dl.text(plain(), hs, c.rank, hy, "#", kMuted, ui::Align::right, ht);
  dl.text(plain(), hs, c.name, hy, "PLAYER", kMuted, ui::Align::left, ht);
  if (team_col) dl.text(plain(), hs, c.team, hy, "TEAM", kMuted, ui::Align::left, ht);
  dl.text(plain(), hs, c.frags, hy, "FRAGS", kMuted, ui::Align::right, ht);
  dl.text(plain(), hs, c.time, hy, "TIME", kMuted, ui::Align::right, ht);
  dl.text(plain(), hs, c.ping, hy, "PING", kMuted, ui::Align::right, ht);
  dl.text(plain(), hs, c.loss, hy, "LOSS", kMuted, ui::Align::right, ht);
  y += cols_h;

  // The rows.
  const float big = std::min(22 * u, row_h * 0.62f), small = std::min(16 * u, row_h * 0.5f);
  for (size_t i = 0; i < shown; i++) {
    const ScoreRow& r = b.rows[players[i]];
    const float ry = y + row_h * static_cast<float>(i);
    const float base = ry + row_h * 0.5f + big * 0.36f;
    if (r.self) {
      dl.rect({x0 + 4 * u, ry, w - 8 * u, row_h}, kSelf, 4 * u);
      dl.rect({x0 + 4 * u, ry + 3 * u, 3 * u, row_h - 6 * u}, kAccent, 1.5f * u);
    } else if (i % 2 == 1) {
      dl.rect({x0 + 4 * u, ry, w - 8 * u, row_h}, kStripe, 4 * u);
    }
    dl.text(plain(), small, c.rank, base, std::to_string(i + 1), kMuted, ui::Align::right);
    const float chip = std::min(20 * u, row_h - 8 * u), cy = ry + (row_h - chip) / 2;
    ui::Box chip_box;
    chip_box.rect = {c.chip, cy, chip, chip};
    chip_box.radius = 4 * u;
    chip_box.fill = ui::linear(0, 0, 0, chip,
                               std::array<ui::Stop, 4>{ui::Stop{0, player_colour(r.top)},
                                                       ui::Stop{0.5f, player_colour(r.top)},
                                                       ui::Stop{0.5f, player_colour(r.bottom)},
                                                       ui::Stop{1, player_colour(r.bottom)}}
                                   .data(),
                               4);
    dl.box(chip_box);
    dl.text(r.self ? bold() : plain(), std::min(18 * u, row_h * 0.55f), c.name, base, r.name, kInk);
    if (team_col) dl.text(plain(), small, c.team, base, r.team, kMuted);
    dl.text(bold(), big, c.frags, base, std::to_string(r.frags), r.self ? kAccent : kInk,
            ui::Align::right);
    dl.text(plain(), small, c.time, base, std::to_string(std::max(r.minutes, 0)) + "m", kMuted,
            ui::Align::right);
    const int ping = std::clamp(r.ping, 0, 999);
    dl.text(plain(), small, c.ping, base, std::to_string(ping), ping_tint(ping), ui::Align::right);
    dl.text(plain(), small, c.loss, base, std::to_string(std::clamp(r.loss, 0, 100)) + "%",
            r.loss > 25 ? kPoor : kMuted, ui::Align::right);
  }
  y += row_h * static_cast<float>(shown);
  if (shown < players.size()) {
    dl.text(plain(), 12 * u, x1 - pad, y + 2 * u,
            "+" + std::to_string(players.size() - shown) + " more", kMuted, ui::Align::right);
  }

  // The spectators.
  if (!watchers.empty()) {
    std::string names;
    for (size_t k : watchers) names += (names.empty() ? "" : ", ") + b.rows[k].name;
    const float sy = y + 24 * u;
    const float lw =
        dl.text(plain(), 12 * u, x0 + pad, sy, "SPECTATORS", kMuted, ui::Align::left, 1.5f * u);
    dl.text(plain(), 15 * u, x0 + pad + lw + 12 * u, sy, names, kInk);
  }
}

void draw_modern_level_card(ui::DrawList& dl, int width, int height, std::string_view heading,
                            std::string_view title, const LevelStats& s, bool darken) {
  const float u = unit_of(height);
  const float fw = static_cast<float>(width), fh = static_cast<float>(height);
  if (darken) dl.rect({0, 0, fw, fh}, ui::hex(0x000000, 0.55f));
  const float w = std::min(fw - 32 * u, 660 * u), h = 250 * u;
  const float x0 = std::floor((fw - w) / 2), y0 = std::floor(fh * 0.42f - h / 2);
  slab(dl, {x0, y0, w, h}, u);
  const float cx = x0 + w / 2;
  dl.text(bold(), 30 * u, cx, y0 + 50 * u, heading, kAccent, ui::Align::center, 3 * u);
  if (!title.empty() && title != heading)
    dl.text(plain(), 17 * u, cx, y0 + 78 * u, title, kMuted, ui::Align::center);
  const float pad = 24 * u, gap = 12 * u;
  const float tw = (w - 2 * pad - 2 * gap) / 3, ty = y0 + 100 * u, th = 124 * u;
  struct Tile {
    const char* label;
    std::string value;
    float fill;  // < 0: no bar
  };
  const Tile tiles[3] = {
      {"KILLS", std::to_string(s.kills) + " / " + std::to_string(s.total_kills),
       share(s.kills, s.total_kills)},
      {"SECRETS", std::to_string(s.secrets) + " / " + std::to_string(s.total_secrets),
       share(s.secrets, s.total_secrets)},
      {"TIME", clock_text(s.minutes, s.seconds), -1}};
  for (int i = 0; i < 3; i++) {
    const float tx = x0 + pad + (tw + gap) * static_cast<float>(i);
    dl.rect({tx, ty, tw, th}, kStripe, 8 * u);
    dl.text(plain(), 13 * u, tx + tw / 2, ty + 30 * u, tiles[i].label, kMuted, ui::Align::center,
            2 * u);
    dl.text(bold(), 40 * u, tx + tw / 2, ty + 80 * u, tiles[i].value, kInk, ui::Align::center);
    if (tiles[i].fill >= 0)
      bar(dl, {tx + 18 * u, ty + th - 24 * u, tw - 36 * u, 6 * u}, tiles[i].fill);
  }
}

void draw_modern_solo_strip(ui::DrawList& dl, int width, int height, std::string_view title,
                            const LevelStats& s) {
  const float u = unit_of(height);
  const float fw = static_cast<float>(width), fh = static_cast<float>(height);
  const float w = std::min(fw - 32 * u, 900 * u), h = 64 * u;
  const float x0 = std::floor((fw - w) / 2), y0 = fh - h - 24 * u;
  slab(dl, {x0, y0, w, h}, u);
  const float pad = 22 * u;
  dl.text(bold(), 19 * u, x0 + pad, y0 + 39 * u, title, kInk);
  struct Stat {
    const char* label;
    std::string value;
    float fill;
  };
  const Stat stats[3] = {
      {"KILLS", std::to_string(s.kills) + "/" + std::to_string(s.total_kills),
       share(s.kills, s.total_kills)},
      {"SECRETS", std::to_string(s.secrets) + "/" + std::to_string(s.total_secrets),
       share(s.secrets, s.total_secrets)},
      {"TIME", clock_text(s.minutes, s.seconds), -1}};
  const float sw = std::min(150 * u, (w - 2 * pad) / 5);
  for (int i = 0; i < 3; i++) {
    const float sx = x0 + w - pad - sw * static_cast<float>(3 - i);
    dl.text(plain(), 11 * u, sx, y0 + 22 * u, stats[i].label, kMuted, ui::Align::left, 1.5f * u);
    dl.text(bold(), 20 * u, sx, y0 + 46 * u, stats[i].value, kInk);
    if (stats[i].fill >= 0) bar(dl, {sx, y0 + 52 * u, sw - 24 * u, 4 * u}, stats[i].fill);
  }
}

}  // namespace shooters_edge::menu
