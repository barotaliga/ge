// The lobby in the classic look: lobby_screen.hpp's header says what is drawn where.
//
// ge:summary The lobby in the classic look: the banner box, the big figures, the lobby's screen
// ge:keywords lobby classic banner countdown roster draw_lobby_classic draw_lobby_page_classic
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/menu/lobby_screen.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/level_screens.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace hd = shooters_edge::hud;

constexpr float kScreenW = 320;
constexpr float kBannerY = 4, kCountY = 36;
constexpr float kCountScale = 2;  // the big figures drawn at twice their size
constexpr int kBannerCols = 26;
// The lobby's screen: the head box, the roster box, the keys' box (whole cells of 8 px).
constexpr float kHeadY = 4, kRosterY = 40;  // the keys' box follows the roster's
constexpr int kPageCols = 38, kNameCells = 12, kRosterLines = 14;
// The team strips: the palette rows the players wear (lobby.hpp), as the pack's palette draws
// them near enough.
constexpr ui::Color kRedLit = ui::hex(0xc04b2b), kBlueLit = ui::hex(0x3b5fa7);

// `s` centred in a box of `cols` cells whose left edge is at `x`.
void centred(const ClassicPen& pen, std::string_view s, float x, int cols, float y, bool alt) {
  const auto n = static_cast<int>(std::min(s.size(), static_cast<size_t>(cols)));
  pen.text(s.substr(0, static_cast<size_t>(n)),
           x + 8 * std::floor(static_cast<float>(cols - n) / 2), y, alt);
}

void count(const ClassicPen& pen, int seconds, float opacity) {
  const std::vector<std::string> names = figure_names(seconds);
  std::vector<const ui::Image*> imgs;
  float w = 0;
  for (const std::string& n : names) {
    const ui::Image* img = pen.art.find(n);
    if (img == nullptr) {  // a pack without the figures: the glyphs in the darker set
      const std::string d = std::to_string(seconds);
      pen.text(d, std::floor((kScreenW - 8 * static_cast<float>(d.size())) / 2), kCountY + 16, true,
               opacity);
      return;
    }
    imgs.push_back(img);
    w += pen.art.width_of(*img) * kCountScale;
  }
  float x = std::floor((kScreenW - w) / 2);
  for (const ui::Image* img : imgs) {
    const float iw = pen.art.width_of(*img), ih = pen.art.height_of(*img);
    ui::Box b;
    b.rect = pen.at(x, kCountY, iw * kCountScale, ih * kCountScale);
    b.image = img;
    b.uv = {0, 0, static_cast<float>(img->width), static_cast<float>(img->height)};
    b.nearest = pen.art.density <= 1;
    b.feather = 0;
    b.opacity = opacity;
    pen.dl.box(b);
    x += iw * kCountScale;
  }
}

// A seat's name in the roster's cells: a bot's skill in brackets is the first to go.
std::string short_name(const std::string& name) {
  std::string n = name;
  if (n.size() > static_cast<size_t>(kNameCells))
    if (const size_t at = n.find(" ("); at != std::string::npos) n.resize(at);
  return n.substr(0, static_cast<size_t>(kNameCells));
}

// One team's part of the roster from line `line` of the box's inside (x the inside's left): its
// header, then its rows; the line after it.
int team_part(const ClassicPen& pen, const hd::LobbyView& v, int team, float x, int line) {
  const auto at = [&](int ln) { return kRosterY + 8 + 8 * static_cast<float>(ln); };
  int n = 0, ready = 0, frags = 0;
  for (const hd::LobbyRow& r : v.rows)
    if (r.team == team) {
      n++;
      ready += r.ready ? 1 : 0;
      frags += r.frags;
    }
  const float y = at(line);
  pen.text(team == 1 ? "RED" : "BLUE", x + 8, y, true);
  const std::string tally = std::to_string(ready) + "/" + std::to_string(n) + " READY " +
                            std::to_string(frags) + " FRAGS";
  pen.text(tally, x + 8 * static_cast<float>(kPageCols - static_cast<int>(tally.size())), y, false);
  pen.dl.rect(pen.at(x + 8, y + 9, 8 * static_cast<float>(kPageCols - 1), 1),
              team == 1 ? kRedLit : kBlueLit);
  line++;
  int row = 0;
  for (const hd::LobbyRow& r : v.rows) {
    if (r.team != team) continue;
    if (row >= lobby_classic_rows()) break;
    const float ry = at(line) + 3;
    if (r.me) pen.glyph(13, x, ry);  // the menu's arrow
    pen.text(short_name(r.name), x + 8, ry, false);
    if (r.leader || r.bot) pen.text(r.leader ? "H" : "B", x + 8 * 14, ry, true);
    pen.text(r.warm ? "WARM" : "LOBBY", x + 8 * 16, ry, r.warm);
    const std::string f = std::to_string(r.frags);
    pen.text(f, x + 8 * static_cast<float>(25 - static_cast<int>(f.size())), ry, false);
    if (r.ready)
      pen.text("READY", x + 8 * 28, ry, true);
    else
      pen.text("-", x + 8 * 30, ry, false);
    row++;
    line++;
  }
  if (n == 0) {
    pen.text("nobody", x + 8, at(line) + 3, false);
    line++;
  }
  return line;
}

}  // namespace

int lobby_classic_rows() {
  // The roster box's lines: two headers and the gap between the teams, the rest shared.
  return (kRosterLines - 3) / 2;
}

void draw_lobby_classic(ui::DrawList& dl, const ClassicArt& art, const hd::LobbyView& v, int width,
                        int height) {
  if (v.phase == hd::LobbyShown::match) return;
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  int seated = 0, ready = 0;
  bool me_ready = false;
  for (const hd::LobbyRow& r : v.rows) {
    seated++;
    ready += r.ready ? 1 : 0;
    me_ready = me_ready || (r.me && r.ready);
  }
  const bool counting = v.phase == hd::LobbyShown::countdown;
  const float bx = std::floor((kScreenW - 8 * static_cast<float>(kBannerCols + 2)) / 2);
  pen.box(bx, kBannerY, kBannerCols, 2);
  const std::string head = std::string(counting ? "COUNTDOWN" : "WARMUP") + "  " +
                           std::to_string(ready) + " OF " + std::to_string(seated) + " READY";
  centred(pen, head, bx + 8, kBannerCols, kBannerY + 8, true);
  centred(pen, me_ready ? "F1 BREAK  F4 LOBBY" : "F1 READY  F4 LOBBY", bx + 8, kBannerCols,
          kBannerY + 16, false);
  if (counting) count(pen, v.seconds_left, 0.55f + 0.45f * static_cast<float>(v.fraction));
}

void draw_lobby_page_classic(ui::DrawList& dl, const ClassicArt& art, const hd::LobbyView& v,
                             int width, int height) {
  if (v.phase == hd::LobbyShown::match) return;
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  dl.rect({0, 0, static_cast<float>(width), static_cast<float>(height)}, ui::hex(0x050608, 0.4f));
  int seated = 0, ready = 0;
  bool me_ready = false;
  for (const hd::LobbyRow& r : v.rows) {
    seated++;
    ready += r.ready ? 1 : 0;
    me_ready = me_ready || (r.me && r.ready);
  }
  const bool counting = v.phase == hd::LobbyShown::countdown;
  const float x = std::floor((kScreenW - 8 * static_cast<float>(kPageCols + 2)) / 2);
  // The head.
  pen.box(x, kHeadY, kPageCols, 2);
  pen.text("MULTIPLAYER LOBBY", x + 8, kHeadY + 8, true);
  const std::string map = v.map.empty() ? std::string() : "MAP " + v.map;
  pen.text(map, x + 8 * static_cast<float>(kPageCols + 1 - static_cast<int>(map.size())),
           kHeadY + 8, true);
  centred(pen,
          counting
              ? "MATCH STARTS IN " + std::to_string(v.seconds_left)
              : "WARMUP  " + std::to_string(ready) + " OF " + std::to_string(seated) + " READY",
          x + 8, kPageCols, kHeadY + 16, counting);
  // The roster, as tall as its rows: each team its header and at least one row, a line between,
  // and a last line for the rows' drop under their headers.
  int lines = 4;
  for (int team = 1; team <= 2; team++) {
    int n = 0;
    for (const hd::LobbyRow& r : v.rows) n += r.team == team ? 1 : 0;
    lines += std::clamp(n, 1, lobby_classic_rows());
  }
  lines = std::min(lines, kRosterLines);
  pen.box(x, kRosterY, kPageCols, lines);
  const int next = team_part(pen, v, 1, x + 8, 0);
  team_part(pen, v, 2, x + 8, next + 1);
  // The keys, or what the match still needs, under it.
  const float keys_y = kRosterY + 8 * static_cast<float>(lines + 2) + 4;
  pen.box(x, keys_y, kPageCols, 2);
  centred(pen, me_ready ? "F4 WARMUP  F1 BREAK  F2 TEAM" : "F4 WARMUP  F1 READY  F2 TEAM", x + 8,
          kPageCols, keys_y + 8, true);
  centred(pen,
          seated < v.min_players
              ? "NEEDS " + std::to_string(v.min_players - seated) + " MORE  F5 ADD A BOT"
              : "F5 BOT+  F6 BOT-  ENTER START",
          x + 8, kPageCols, keys_y + 16, false);
}

}  // namespace shooters_edge::menu
