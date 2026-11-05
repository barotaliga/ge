// The lobby over the game: lobby_panel.hpp's header says what is drawn where.
//
// ge:summary The lobby over the game: the warmup banner and the countdown
// ge:keywords lobby panel banner countdown draw_lobby
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
      std::string(me_ready ? "F1 BREAK" : "F1 READY") + "   F4 BACK TO THE LOBBY";
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
}

}  // namespace shooters_edge::hud
