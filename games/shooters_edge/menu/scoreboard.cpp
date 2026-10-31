// The multiplayer scoreboard (scoreboard.hpp).
//
// ge:summary Draws the players' and the teams' scoreboards and the scorebar on the classic screen
// ge:keywords draw_scoreboard draw_team_scoreboard draw_scorebar score_order team_rows
// ge:keywords colour_index colour-box rules header large-game
// ge:skill ui
// ge:test tests/shooters_edge_scoreboard_test.cpp
#include "shooters_edge/menu/scoreboard.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/score_modern.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr float kHeaderY = 24, kFirstRow = 40;
constexpr float kLastRow = 190;  // no row below the screen's height less 10
constexpr float kBarY = 176;
constexpr float kPlainX = 16, kTeamsX = 4, kTeamBoardX = 36;
constexpr size_t kNameChars = 15, kTeamChars = 4;

ui::Color fill_of(const ClassicArt& art, int colour) {
  if (art.glyphs.empty()) return player_colour(colour);
  return art.colour(colour_index(colour));
}

std::string clipped(std::string_view s, size_t n) { return std::string(s.substr(0, n)); }

// A rule of `n` glyphs (its two ends and the middles) from x; the x after it and its space.
float rule(const ClassicType& type, int n, float x, float y) {
  for (int i = 0; i < n; i++) {
    const uint8_t code = i == 0 ? 29 : i == n - 1 ? 31 : 30;
    type.glyph(code, x, y);
    x += 8;
  }
  return x + 8;
}

void rules(const ClassicType& type, std::initializer_list<int> widths, float x, float y) {
  for (int n : widths) x = rule(type, n, x, y);
}

void ranking(const ClassicPen& pen, const ClassicType& type) {
  if (const ui::Image* r = pen.art.find("ranking"))
    pen.picture(r, std::floor(160 - static_cast<float>(r->width) / 2), 0);
  else
    type.centred("RANKINGS", 8);
}

// The players' header, rule and rows from `y` (the header's row).
void player_rows(const ClassicPen& pen, const ClassicType& type, const Scoreboard& b,
                 BoardView& view, float y, bool teams) {
  const float x = teams ? kTeamsX : kPlainX;
  type.text(teams ? "ping pl time frags team name" : "ping pl time frags name", x, y);
  if (teams)
    rules(type, {4, 2, 4, 5, 4, 16}, x, y + 8);
  else
    rules(type, {4, 2, 4, 5, 16}, x, y + 8);
  y += 16;
  const float step = view.large ? 8 : 10;
  for (size_t k : score_order(b)) {
    if (y > kLastRow) break;
    const ScoreRow& r = b.rows[k];
    const int ping = r.ping < 0 || r.ping > 999 ? 999 : r.ping;
    type.text(padded(ping, 4), x, y);
    type.text(padded(std::clamp(r.loss, 0, 100), 3), x + 32, y, r.loss > 25);
    const float name_x = teams ? x + 192 : x + 152;
    if (r.spectator) {
      type.text("(spectator)", x + 40, y);
      type.text(clipped(r.name, kNameChars), name_x, y);
    } else {
      type.text(padded(std::max(r.minutes, 0), 4), x + 64, y);
      const float top_y = view.large ? y + 1 : y;
      pen.dl.rect(pen.at(x + 104, top_y, 40, y + 4 - top_y), fill_of(pen.art, r.top));
      pen.dl.rect(pen.at(x + 104, y + 4, 40, 4), fill_of(pen.art, r.bottom));
      type.text(padded(r.frags, 3, 3), x + 112, y);
      if (r.self) {
        type.glyph(16, x + 104, y);
        type.glyph(17, x + 136, y);
      }
      if (teams) type.text(clipped(r.team, kTeamChars), x + 152, y);
      type.text(clipped(r.name, kNameChars), name_x, y);
    }
    if (y >= kLastRow) view.large = true;
    y += step;
  }
}

std::string three(int n) { return padded(n < 0 || n > 999 ? 999 : n, 3); }

}  // namespace

int colour_index(int colour) { return std::clamp(colour, 0, 13) * 16 + 8; }

std::vector<size_t> score_order(const Scoreboard& b) {
  std::vector<size_t> out(b.rows.size());
  for (size_t i = 0; i < out.size(); i++) out[i] = i;
  // A spectator counts as -999 frags, so he sinks below every player.
  const auto key = [&b](size_t i) { return b.rows[i].spectator ? -999 : b.rows[i].frags; };
  std::stable_sort(out.begin(), out.end(), [&](size_t a, size_t c) { return key(a) > key(c); });
  return out;
}

std::vector<TeamRow> team_rows(const Scoreboard& b) {
  std::vector<TeamRow> teams;
  std::vector<int> total_ping;
  std::string own;
  for (const ScoreRow& r : b.rows)
    if (r.self && !r.spectator) own = clipped(r.team, 16);
  for (const ScoreRow& r : b.rows) {
    if (r.spectator || r.name.empty() || r.team.empty()) continue;
    const std::string name = clipped(r.team, 16);
    size_t t = 0;
    while (t < teams.size() && teams[t].team != name) t++;
    if (t == teams.size()) {
      TeamRow row;
      row.team = name;
      row.self = name == own;
      teams.push_back(row);
      total_ping.push_back(0);
    }
    TeamRow& row = teams[t];
    row.frags += r.frags;
    row.players++;
    row.low = std::min(row.low, r.ping);
    row.high = std::max(row.high, r.ping);
    total_ping[t] += r.ping;
  }
  for (size_t t = 0; t < teams.size(); t++)
    teams[t].mean = teams[t].players > 0 ? total_ping[t] / teams[t].players : 999;
  // An exchange sort: each later team with strictly more frags is swapped into the place.
  for (size_t i = 0; i < teams.size(); i++)
    for (size_t j = i + 1; j < teams.size(); j++)
      if (teams[j].frags > teams[i].frags) std::swap(teams[i], teams[j]);
  return teams;
}

void draw_scoreboard(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     const Scoreboard& b, BoardView& view) {
  if (art.glyphs.empty()) {
    draw_modern_board(dl, width, height, b, false);
    return;
  }
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  ranking(pen, type);
  player_rows(pen, type, b, view, kHeaderY, b.teamplay);
}

void draw_team_scoreboard(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                          const Scoreboard& b, BoardView& view) {
  if (art.glyphs.empty()) {
    draw_modern_board(dl, width, height, b, true);
    return;
  }
  if (!b.teamplay) {
    draw_scoreboard(dl, art, width, height, b, view);
    return;
  }
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  ranking(pen, type);
  const float x = kTeamBoardX;
  type.text("low/avg/high team total players", x, kHeaderY);
  rules(type, {12, 4, 5, 7}, x, kHeaderY + 8);
  float y = kFirstRow;
  for (const TeamRow& t : team_rows(b)) {
    if (y > kLastRow) break;
    type.text(three(t.low) + "/" + three(t.mean) + "/" + three(t.high), x, y);
    if (t.self) {
      type.glyph(16, x + 96, y);
      type.glyph(17, x + 136, y);
    }
    type.text(clipped(t.team, kTeamChars), x + 104, y);
    type.text(padded(t.frags, 5), x + 144, y);
    type.text(padded(t.players, 5), x + 192, y);
    y += 8;
  }
  player_rows(pen, type, b, view, y + 8, true);
}

void draw_scorebar(ui::DrawList& dl, const ClassicArt& art, int width, int height,
                   const Scoreboard& b) {
  if (art.glyphs.empty()) return;  // the modern board's header carries the clock
  const ClassicLayout l = classic_screen(width, height);
  const ClassicPen pen{dl, art, l};
  const ClassicType type{pen, !art.glyphs.empty()};
  if (const ui::Image* bar = art.find("scorebar"))
    pen.picture(bar, 0, kBarY);
  else
    dl.rect(pen.at(0, kBarY, 320, 24), ui::hex(0x000000, 0.75f));
  const int s = std::clamp(b.seconds, 0, 59);
  type.text("Time :" + padded(std::max(b.minutes, 0), 3) + ":" + std::to_string(s / 10) +
                std::to_string(s % 10),
            184, kBarY + 4);
}

}  // namespace shooters_edge::menu
