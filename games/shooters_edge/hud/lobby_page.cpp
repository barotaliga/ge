// The lobby's screen: lobby_page.hpp's header says what is drawn where.
//
// ge:summary The lobby's screen: the head, the two teams, the notes and the keys
// ge:keywords lobby lobby-page full-screen teams roster frags draw_lobby_page
// ge:skill ui
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/hud/lobby_page.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "ui/font.hpp"

namespace shooters_edge::hud {

namespace {

namespace ui = ge::ui;

constexpr ui::Color kInk = ui::hex(0xf2ece0);
constexpr ui::Color kDim = ui::hex(0xf2ece0, 0.55f);
constexpr ui::Color kFaint = ui::hex(0xf2ece0, 0.32f);
constexpr ui::Color kScrim = ui::hex(0x050608, 0.42f);
constexpr ui::Color kPanel = ui::hex(0x0c0d10, 0.78f);
constexpr ui::Color kKey = ui::hex(0x23252b, 0.92f);
constexpr ui::Color kReady = ui::hex(0x7ee07a);
constexpr ui::Color kWarm = ui::hex(0xffc35a);
constexpr ui::Color kRed = ui::hex(0xe0473c);
constexpr ui::Color kBlue = ui::hex(0x3f7fe8);

ui::Color team_ink(int team) { return team == 1 ? kRed : team == 2 ? kBlue : kDim; }

// One team's panel from (x, y), `bw` wide and at least `rows` rows tall: its header and players.
void team_panel(ui::DrawList& dl, const LobbyView& v, int team, float x, float y, float bw,
                int rows, float s) {
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  const float row = 34 * s, pad = 14 * s, head = 52 * s;
  int n = 0, ready = 0, frags = 0;
  for (const LobbyRow& r : v.rows)
    if (r.team == team) {
      n++;
      ready += r.ready ? 1 : 0;
      frags += r.frags;
    }
  const float h = head + row * static_cast<float>(std::max(rows, 1)) + pad;
  dl.rect({x, y, bw, h}, kPanel, 8 * s);
  dl.rect({x, y, bw, 5 * s}, team_ink(team), 2 * s);
  dl.text(bold, 26 * s, x + pad, y + 36 * s, team == 1 ? "RED" : "BLUE", team_ink(team));
  dl.text(body, 15 * s, x + bw - pad, y + 34 * s,
          std::to_string(ready) + " / " + std::to_string(n) + " READY   " + std::to_string(frags) +
              " FRAGS",
          kDim, ui::Align::right);
  // The columns: the name, where he is, the frags, ready.
  const float c_ready = x + bw - pad - 70 * s, c_frags = c_ready - 22 * s,
              c_where = c_frags - 128 * s;
  float ry = y + head;
  dl.rect({x + pad, ry - 6 * s, bw - 2 * pad, 1 * s}, kFaint);
  if (n == 0) dl.text(body, 17 * s, x + pad, ry + 23 * s, "nobody yet", kDim);
  for (const LobbyRow& r : v.rows) {
    if (r.team != team) continue;
    if (r.me)
      dl.rect({x + 6 * s, ry + 2 * s, bw - 12 * s, row - 4 * s}, ui::hex(0xffffff, 0.09f), 5 * s);
    float nx = x + pad;
    if (r.leader) nx += dl.text(bold, 13 * s, nx, ry + 22 * s, "HOST  ", kWarm);
    nx += dl.text(r.me ? bold : body, 20 * s, nx, ry + 24 * s, r.name, kInk);
    if (r.bot) dl.text(body, 13 * s, nx + 8 * s, ry + 22 * s, "BOT", kDim);
    // Where he is: a lit chip in the warmup, a dim one on this screen.
    const float cw = 112 * s;
    dl.rect({c_where, ry + 7 * s, cw, row - 14 * s}, r.warm ? ui::hex(0xffc35a, 0.16f) : kKey,
            4 * s);
    dl.text(bold, 13 * s, c_where + cw / 2, ry + 22 * s, r.warm ? "IN WARMUP" : "IN LOBBY",
            r.warm ? kWarm : kDim, ui::Align::center);
    dl.text(bold, 20 * s, c_frags, ry + 24 * s, std::to_string(r.frags), r.warm ? kInk : kDim,
            ui::Align::right);
    if (r.ready) {
      dl.rect({c_ready, ry + 7 * s, 70 * s, row - 14 * s}, ui::hex(0x7ee07a, 0.18f), 4 * s);
      dl.text(bold, 14 * s, c_ready + 35 * s, ry + 22 * s, "READY", kReady, ui::Align::center);
    } else {
      dl.text(body, 18 * s, c_ready + 35 * s, ry + 23 * s, "-", kDim, ui::Align::center);
    }
    ry += row;
  }
}

// A key's cap and what it does, from x on the baseline y; the x after it.
float key(ui::DrawList& dl, float x, float y, const std::string& cap, const std::string& what,
          bool lit, float s) {
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  const float cw = std::max(34.f * s, bold.advance(cap, 14 * s) + 16 * s);
  dl.rect({x, y - 19 * s, cw, 26 * s}, lit ? ui::hex(0xffc35a, 0.9f) : kKey, 4 * s);
  dl.text(bold, 14 * s, x + cw / 2, y - 1 * s, cap, lit ? ui::hex(0x15130f) : kInk,
          ui::Align::center);
  const float tw = dl.text(body, 16 * s, x + cw + 8 * s, y, what, lit ? kInk : kDim);
  return x + cw + 8 * s + tw + 26 * s;
}

}  // namespace

void draw_lobby_page(ui::DrawList& dl, const LobbyView& v, int w, int h) {
  if (v.phase == LobbyShown::match) return;
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  const bool narrow = fw < 760;
  const float s = std::clamp(narrow ? fw / 460.f : std::min(fh / 720.f, fw / 1280.f), 0.7f, 2.0f);
  const ui::Font& bold = ui::ui_font(ui::Weight::semibold);
  const ui::Font& body = ui::ui_font(ui::Weight::medium);
  dl.rect({0, 0, fw, fh}, kScrim);

  int seated = 0, ready = 0, red = 0, blue = 0;
  bool me_ready = false, me_leader = false;
  for (const LobbyRow& r : v.rows) {
    seated++;
    ready += r.ready ? 1 : 0;
    red += r.team == 1 ? 1 : 0;
    blue += r.team == 2 ? 1 : 0;
    if (r.me) {
      me_ready = r.ready;
      me_leader = r.leader;
    }
  }
  const bool counting = v.phase == LobbyShown::countdown;

  // The head.
  const float m = narrow ? 16 : 56 * s;
  float y = (narrow ? 44 : 70) * s;
  dl.text(bold, 15 * s, m, y - 30 * s, "SHOOTER'S EDGE", kDim, ui::Align::left, 2 * s);
  dl.text(bold, (narrow ? 30 : 44) * s, m, y + 10 * s, "MULTIPLAYER LOBBY", kInk);
  dl.text(body, 18 * s, m, y + 40 * s,
          (v.map.empty() ? std::string("deathmatch") : "map " + v.map) + "   " +
              std::to_string(seated) + (seated == 1 ? " player" : " players"),
          kDim);
  // The phase, on the right (under the head on a narrow screen).
  const float px = narrow ? m : fw - m;
  const float py = narrow ? y + 84 * s : y + 4 * s;
  const ui::Align pa = narrow ? ui::Align::left : ui::Align::right;
  if (counting) {
    const float pulse =
        1 + 0.15f * static_cast<float>(std::pow(1 - std::clamp(v.fraction, 0.0, 1.0), 3.0));
    dl.text(bold, 18 * s, px, py - 20 * s, "MATCH STARTS IN", kInk, pa);
    dl.text(bold, 64 * s * pulse, px, py + 46 * s, std::to_string(v.seconds_left), kWarm, pa);
  } else {
    dl.text(bold, 30 * s, px, py, "WARMUP", kWarm, pa);
    dl.text(body, 18 * s, px, py + 28 * s,
            std::to_string(ready) + " of " + std::to_string(seated) + " ready", kInk, pa);
  }

  // The teams.
  const int rows = std::max({red, blue, 1});
  const float top = narrow ? py + (counting ? 70 : 52) * s : y + 92 * s;
  float foot_y;
  if (narrow) {
    const float tw = fw - 2 * m;
    int rn = std::max(red, 1), bn = std::max(blue, 1);
    team_panel(dl, v, 1, m, top, tw, rn, s);
    const float next = top + (52 + 34 * static_cast<float>(rn) + 14) * s + 12 * s;
    team_panel(dl, v, 2, m, next, tw, bn, s);
    foot_y = next + (52 + 34 * static_cast<float>(bn) + 14) * s + 28 * s;
  } else {
    const float gap = 24 * s, tw = (fw - 2 * m - gap) / 2;
    team_panel(dl, v, 1, m, top, tw, rows, s);
    team_panel(dl, v, 2, m + tw + gap, top, tw, rows, s);
    foot_y = top + (52 + 34 * static_cast<float>(rows) + 14) * s + 34 * s;
  }

  // The notes.
  float ny = std::min(foot_y, fh - 96 * s);
  if (seated < v.min_players)
    dl.text(bold, 17 * s, m, ny,
            "The match needs " + std::to_string(v.min_players) +
                " players: " + std::to_string(v.min_players - seated) + " more (a bot counts: F5)",
            kWarm);
  else
    dl.text(body, 17 * s, m, ny, "Join the warmup to fight the others while you wait.", kDim);
  dl.text(body, 15 * s, m, ny + 24 * s, "Frags in the warmup are reset when the match starts.",
          kFaint);

  // The keys.
  const float ky = fh - 30 * s;
  dl.rect({0, ky - 34 * s, fw, 56 * s}, ui::hex(0x050608, 0.6f));
  float kx = m;
  const auto next_line = [&](float want) {
    if (narrow && kx + want > fw - m) {
      kx = m;
      return true;
    }
    return false;
  };
  float line_y = ky;
  const struct {
    const char* cap;
    std::string what;
    bool lit;
  } keys[] = {
      {"F4", "join the warmup", true},
      {"F1", me_ready ? "break" : "ready", false},
      {"F2", "switch team", false},
      {"F5", "add a bot", false},
      {"F6", "remove a bot", false},
      {"ENTER", me_leader ? "start the match" : "start (the host)", false},
  };
  if (narrow) line_y = ky - 30 * s;  // two lines of keys on a narrow screen
  for (const auto& k : keys) {
    const float want = bold.advance(k.cap, 14 * s) + body.advance(k.what, 16 * s) + 66 * s;
    if (next_line(want)) line_y += 30 * s;
    kx = key(dl, kx, line_y, k.cap, k.what, k.lit, s);
  }
}

}  // namespace shooters_edge::hud
