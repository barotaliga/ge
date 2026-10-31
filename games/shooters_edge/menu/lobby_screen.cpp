// The lobby in the classic look: lobby_screen.hpp's header says what is drawn where.
//
// ge:summary The lobby in the classic look: the banner box, the big figures, the roster box
// ge:keywords lobby classic banner countdown roster draw_lobby_classic
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
constexpr float kBannerY = 4, kCountY = 36, kRosterY = 88, kStatusY = 176;
constexpr float kCountScale = 2;  // the big figures drawn at twice their size
constexpr int kBannerCols = 26, kRosterCols = 38, kHalf = 20, kNameCells = 9;
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
    w += static_cast<float>(img->width) * kCountScale;
  }
  float x = std::floor((kScreenW - w) / 2);
  for (const ui::Image* img : imgs) {
    const float iw = static_cast<float>(img->width), ih = static_cast<float>(img->height);
    ui::Box b;
    b.rect = pen.at(x, kCountY, iw * kCountScale, ih * kCountScale);
    b.image = img;
    b.uv = {0, 0, iw, ih};
    b.nearest = true;
    b.feather = 0;
    b.opacity = opacity;
    pen.dl.box(b);
    x += iw * kCountScale;
  }
}

// A seat's name in the roster's nine cells: a bot's skill in brackets is the first to go.
std::string short_name(const std::string& name) {
  std::string n = name;
  if (n.size() > static_cast<size_t>(kNameCells))
    if (const size_t at = n.find(" ("); at != std::string::npos) n.resize(at);
  return n.substr(0, static_cast<size_t>(kNameCells));
}

// One team's half from cell column `col` of the roster box's inside.
void team_half(const ClassicPen& pen, const hd::LobbyView& v, int team, float x0, int col) {
  const float x = x0 + 8 * static_cast<float>(col);
  const float y = kRosterY + 8;
  int n = 0, ready = 0;
  for (const hd::LobbyRow& r : v.rows)
    if (r.team == team) {
      n++;
      ready += r.ready ? 1 : 0;
    }
  pen.text(team == 1 ? "RED" : "BLUE", x + 8, y, true);
  const std::string tally = std::to_string(ready) + "/" + std::to_string(n);
  pen.text(tally, x + 8 * static_cast<float>(18 - static_cast<int>(tally.size())), y, false);
  pen.dl.rect(pen.at(x + 8, y + 9, 8 * 17, 2), team == 1 ? kRedLit : kBlueLit);
  int row = 0;
  for (const hd::LobbyRow& r : v.rows) {
    if (r.team != team) continue;
    if (row >= lobby_classic_rows()) break;
    const float ry = y + 12 + 8 * static_cast<float>(row);
    if (r.me) pen.glyph(13, x, ry);  // the menu's arrow
    pen.text(short_name(r.name), x + 8, ry, false);
    if (r.leader || r.bot) pen.text(r.leader ? "H" : "B", x + 8 * 11, ry, true);
    if (r.ready)
      pen.text("READY", x + 8 * 13, ry, true);
    else
      pen.text("-", x + 8 * 15, ry, false);
    row++;
  }
  if (n == 0) pen.text("nobody", x + 8, y + 12, false);
}

}  // namespace

int lobby_classic_rows() {
  // The box's top piece and the header (12 px) above the rows, its bottom piece under them.
  return static_cast<int>((kStatusY - kRosterY - 8 - 12 - 8) / 8);
}

void draw_lobby_classic(ui::DrawList& dl, const ClassicArt& art, const hd::LobbyView& v, int width,
                        int height) {
  if (v.phase == hd::LobbyShown::match) return;
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  int seated = 0, ready = 0;
  for (const hd::LobbyRow& r : v.rows) {
    seated++;
    ready += r.ready ? 1 : 0;
  }
  const bool counting = v.phase == hd::LobbyShown::countdown;
  const float bx = std::floor((kScreenW - 8 * static_cast<float>(kBannerCols + 2)) / 2);
  pen.box(bx, kBannerY, kBannerCols, 2);
  const std::string head = std::string(counting ? "COUNTDOWN" : "WARMUP") + "  " +
                           std::to_string(ready) + " OF " + std::to_string(seated) + " READY";
  centred(pen, head, bx + 8, kBannerCols, kBannerY + 8, true);
  centred(pen, "F1 READY  F2 TEAM  F3 LIST", bx + 8, kBannerCols, kBannerY + 16, false);
  if (counting) count(pen, v.seconds_left, 0.55f + 0.45f * static_cast<float>(v.fraction));
  if (!v.roster) return;
  int rows = 0;
  for (int team = 1; team <= 2; team++) {
    int n = 0;
    for (const hd::LobbyRow& r : v.rows) n += r.team == team ? 1 : 0;
    rows = std::max(rows, n);
  }
  rows = std::clamp(rows, 1, lobby_classic_rows());
  const float rx = std::floor((kScreenW - 8 * static_cast<float>(kRosterCols + 2)) / 2);
  // The header's row and a half, then the players' rows (whole cells of the box).
  pen.box(rx, kRosterY, kRosterCols, 2 + rows);
  team_half(pen, v, 1, rx + 8, 0);
  team_half(pen, v, 2, rx + 8, kHalf);
}

}  // namespace shooters_edge::menu
