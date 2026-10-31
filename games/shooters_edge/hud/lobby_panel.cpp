// The lobby over the game: lobby_panel.hpp's header says what is drawn where.
//
// ge:summary The lobby over the game: the warmup banner, the countdown, the two teams' roster
// ge:keywords lobby panel banner countdown roster draw_lobby
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/hud/lobby_panel.hpp"

#include <algorithm>
#include <cmath>

#include "ui/font.hpp"

namespace shooters_edge::hud {

namespace {

namespace ui = ge::ui;

constexpr ui::Color kInk = ui::hex(0xf2ece0);
constexpr ui::Color kDim = ui::hex(0xf2ece0, 0.55f);
constexpr ui::Color kPanel = ui::hex(0x0c0d10, 0.72f);
constexpr ui::Color kReady = ui::hex(0x7ee07a);
constexpr ui::Color kWarm = ui::hex(0xffc35a);
constexpr ui::Color kRed = ui::hex(0xe0473c);
constexpr ui::Color kBlue = ui::hex(0x3f7fe8);

ui::Color team_ink(int team) { return team == 1 ? kRed : team == 2 ? kBlue : kDim; }

// One team's block from (x, y), `bw` wide: its header and its players; the y after it.
float team_block(ui::DrawList& dl, const LobbyView& v, int team, float x, float y, float bw,
                 float s) {
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  const float row = 26 * s, pad = 10 * s;
  int n = 0, ready = 0;
  for (const LobbyRow& r : v.rows)
    if (r.team == team) {
      n++;
      ready += r.ready ? 1 : 0;
    }
  const float h = 34 * s + row * static_cast<float>(std::max(n, 1)) + pad;
  dl.rect({x, y, bw, h}, kPanel, 6 * s);
  dl.rect({x, y, bw, 4 * s}, team_ink(team), 2 * s);
  dl.text(bold, 20 * s, x + pad, y + 26 * s, team == 1 ? "RED" : "BLUE", team_ink(team));
  dl.text(body, 16 * s, x + bw - pad, y + 26 * s,
          std::to_string(ready) + " / " + std::to_string(n) + " READY", kDim, ui::Align::right);
  float ry = y + 34 * s;
  if (n == 0) dl.text(body, 16 * s, x + pad, ry + 18 * s, "nobody yet", kDim);
  for (const LobbyRow& r : v.rows) {
    if (r.team != team) continue;
    if (r.me)
      dl.rect({x + 4 * s, ry + 1 * s, bw - 8 * s, row - 2 * s}, ui::hex(0xffffff, 0.08f), 4 * s);
    float nx = x + pad;
    if (r.leader) nx += dl.text(bold, 13 * s, nx, ry + 18 * s, "HOST ", kWarm);
    nx += dl.text(r.me ? bold : body, 18 * s, nx, ry + 19 * s, r.name, kInk);
    if (r.bot) dl.text(body, 13 * s, nx + 6 * s, ry + 18 * s, "BOT", kDim);
    // The mark: a lit tick box for ready, a dash for not.
    const float bx = x + bw - pad - 64 * s;
    if (r.ready) {
      dl.rect({bx, ry + 4 * s, 64 * s, row - 8 * s}, ui::hex(0x7ee07a, 0.18f), 3 * s);
      dl.text(bold, 14 * s, bx + 32 * s, ry + 18 * s, "READY", kReady, ui::Align::center);
    } else {
      dl.text(body, 14 * s, bx + 32 * s, ry + 18 * s, "NOT READY", kDim, ui::Align::center);
    }
    ry += row;
  }
  return y + h;
}

}  // namespace

void draw_lobby(ui::DrawList& dl, const LobbyView& v, int w, int h) {
  if (v.phase == LobbyShown::match) return;
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  const float s = std::clamp(std::min(fh / 720.f, fw / 420.f), 0.8f, 2.0f);
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  int seated = 0, ready = 0;
  for (const LobbyRow& r : v.rows) {
    seated++;
    ready += r.ready ? 1 : 0;
  }
  bool me_ready = false;
  for (const LobbyRow& r : v.rows) me_ready = me_ready || (r.me && r.ready);

  // The banner.
  const bool counting = v.phase == LobbyShown::countdown;
  const std::string title = counting ? "COUNTDOWN" : "WARMUP";
  const std::string state = std::to_string(ready) + " OF " + std::to_string(seated) + " READY";
  const std::string keys =
      std::string(me_ready ? "F1 BREAK" : "F1 READY") + "   F2 TEAM   F3 ROSTER";
  const float bw = std::min(fw - 32, 560 * s), bh = 62 * s;
  const float bx = (fw - bw) / 2, by = 16 * s;
  dl.rect({bx, by, bw, bh}, kPanel, 8 * s);
  dl.rect({bx, by + bh - 3 * s,
           bw * (seated ? static_cast<float>(ready) / static_cast<float>(seated) : 0.f), 3 * s},
          kReady, 1.5f * s);
  dl.text(bold, 26 * s, bx + 14 * s, by + 30 * s, title, counting ? kWarm : kInk);
  dl.text(bold, 18 * s, bx + bw - 14 * s, by + 28 * s, state, me_ready ? kReady : kInk,
          ui::Align::right);
  dl.text(body, 15 * s, fw / 2, by + 52 * s, keys, kDim, ui::Align::center);

  // The count.
  if (counting) {
    const float pulse =
        1 + 0.18f * static_cast<float>(std::pow(1 - std::clamp(v.fraction, 0.0, 1.0), 3.0));
    dl.text(bold, 22 * s, fw / 2, fh * 0.30f, "MATCH STARTS IN", kInk, ui::Align::center);
    dl.text(bold, 96 * s * pulse, fw / 2, fh * 0.30f + 92 * s, std::to_string(v.seconds_left),
            kWarm, ui::Align::center);
  }

  // The roster: beside each other on the right, or stacked under the banner on a narrow screen.
  if (!v.roster) return;
  const bool narrow = fw < 700;
  if (narrow) {
    const float tw = fw - 32;
    float y = by + bh + 12 * s;
    if (counting) y = fh * 0.30f + 120 * s;
    y = team_block(dl, v, 1, 16, y, tw, s) + 10 * s;
    team_block(dl, v, 2, 16, y, tw, s);
  } else {
    const float tw = std::min(330 * s, (fw - 48) / 2);
    const float y = by + bh + 16 * s;
    float x = fw - 16 - tw;
    const float yr = team_block(dl, v, 1, x, y, tw, s) + 10 * s;
    team_block(dl, v, 2, x, yr, tw, s);
  }
}

}  // namespace shooters_edge::hud
