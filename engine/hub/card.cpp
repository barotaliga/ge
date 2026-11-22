// A match's card as a picture: the still, the band, the board in the game's charset (card.hpp).
//
// ge:summary A match card composed: the still covered, darkened, the board in the charset scaled
// ge:keywords hub card compose_card card_jpeg board charset scale team-column frag-box
// ge:skill net ui
// ge:test tests/hub_card_test.cpp
#include "hub/card.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

#include "capture/jpeg.hpp"
#include "hub/page_parts.hpp"

namespace ge::hub {

namespace gn = ge::net;

namespace {

constexpr int kQuality = 90;  // the JPEG's: the letters' edges stay clean at 4:4:4

// A pixel of `p` blended toward `rgb` by `a` (0..255).
void blend(CardPicture& p, int x, int y, uint32_t rgb, int a) {
  if (x < 0 || y < 0 || x >= p.width || y >= p.height) return;
  uint8_t* d = &p.rgba[(size_t(y) * size_t(p.width) + size_t(x)) * 4];
  const int c[3] = {int(rgb >> 16 & 255), int(rgb >> 8 & 255), int(rgb & 255)};
  for (int k = 0; k < 3; k++) d[k] = uint8_t((d[k] * (255 - a) + c[k] * a + 127) / 255);
  d[3] = 255;
}

void fill(CardPicture& p, int x0, int y0, int w, int h, uint32_t rgb, int a = 255) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) blend(p, x, y, rgb, a);
}

// The board's own pixels: drawn at the charset's size, scaled up by `scale` when laid on the card.
struct Board {
  CardPicture px;  // RGBA with alpha: 0 where nothing is drawn
  int cell = 8;
  const CardPicture* sheet = nullptr;

  void put(int x, int y, uint32_t rgb, int a) {
    if (x < 0 || y < 0 || x >= px.width || y >= px.height) return;
    uint8_t* d = &px.rgba[(size_t(y) * size_t(px.width) + size_t(x)) * 4];
    d[0] = uint8_t(rgb >> 16);
    d[1] = uint8_t(rgb >> 8);
    d[2] = uint8_t(rgb);
    d[3] = uint8_t(std::max<int>(d[3], a));
  }
  void rect(int x0, int y0, int w, int h, uint32_t rgb) {
    for (int y = y0; y < y0 + h; y++)
      for (int x = x0; x < x0 + w; x++) put(x, y, rgb, 255);
  }
  // Glyph `g` at (x, y) as the sheet paints it.
  void glyph(int x, int y, uint8_t g) {
    const int gx = (g % 16) * cell, gy = (g / 16) * cell;
    for (int j = 0; j < cell; j++) {
      for (int i = 0; i < cell; i++) {
        const uint8_t* s =
            &sheet->rgba[(size_t(gy + j) * size_t(sheet->width) + size_t(gx + i)) * 4];
        if (s[3] < 128) continue;
        put(x + i, y + j, uint32_t(s[0]) << 16 | uint32_t(s[1]) << 8 | s[2], 255);
      }
    }
  }
  // `text` from cell column `col` (in cells) on pixel row `y`; `alt` the second set.
  void text(int x, int y, std::string_view t, bool alt = false) {
    for (const char ch : t) {
      uint8_t g = uint8_t(ch);
      if (alt && g >= 32 && g < 128) g = uint8_t(g | 128);
      if (g != ' ') glyph(x, y, g);
      x += cell;
    }
  }
};

std::string clip(std::string_view s, size_t n) { return std::string(s.substr(0, n)); }

std::string right(std::string s, size_t n) {
  if (s.size() > n) return s.substr(s.size() - n);
  return std::string(n - s.size(), ' ') + s;
}

const char* phase_words(gn::StatusPhase p) {
  switch (p) {
    case gn::StatusPhase::lobby:
      return "waiting";
    case gn::StatusPhase::countdown:
      return "starting";
    case gn::StatusPhase::playing:
      return "live";
    case gn::StatusPhase::intermission:
      return "scores";
    case gn::StatusPhase::unknown:
      break;
  }
  return "";
}

// The players on the board in its order: by frags, or grouped by team (the higher total first).
std::vector<const gn::StatusPlayer*> board_rows(const HubSpec& spec, const gn::Status& s,
                                                bool teams) {
  std::vector<const gn::StatusPlayer*> v;
  for (const gn::StatusPlayer& p : s.players)
    if ((p.flags & (gn::kPlayerSpectator | gn::kPlayerViewer)) == 0) v.push_back(&p);
  std::map<std::string, int64_t> total;
  for (const gn::StatusPlayer* p : v) total[p->team] += p->score;
  for (const gn::StatusTeam& t : s.teams) total[t.name] = t.score;
  auto better = [&](int64_t a, int64_t b) { return spec.score_low_first ? a < b : a > b; };
  std::stable_sort(v.begin(), v.end(), [&](const gn::StatusPlayer* a, const gn::StatusPlayer* b) {
    if (teams && a->team != b->team) {
      if (total[a->team] != total[b->team]) return better(total[a->team], total[b->team]);
      return a->team < b->team;
    }
    return better(a->score, b->score);
  });
  return v;
}

uint32_t team_colour(const gn::Status& s, const std::string& team) {
  for (const gn::StatusTeam& t : s.teams)
    if (t.name == team) return t.colour;
  return 0;
}

}  // namespace

bool card_teams(const gn::Status& s) {
  if (s.teams.size() >= 2) return true;
  std::set<std::string> names;
  for (const gn::StatusPlayer& p : s.players)
    if (!p.team.empty() && (p.flags & (gn::kPlayerSpectator | gn::kPlayerViewer)) == 0)
      names.insert(p.team);
  if (names.size() < 2) return false;
  // A duel or a free-for-all names no teams even when the players type one.
  return s.mode != "1on1" && s.mode != "duel" && s.mode != "ffa";
}

CardPicture compose_card(const HubSpec& spec, const gn::Status& s, const CardLook& look,
                         const CardPicture& still, int width, int height) {
  CardPicture out;
  out.width = width;
  out.height = height;
  out.rgba.assign(size_t(width) * size_t(height) * 4, 255);

  // ── The ground: the still scaled to cover and darkened, or the map's hue dark ──
  if (still.width > 0 && still.height > 0) {
    const double k = std::max(double(width) / still.width, double(height) / still.height);
    const double ox = (still.width * k - width) / 2, oy = (still.height * k - height) / 2;
    for (int y = 0; y < height; y++) {
      const int sy = std::clamp(int((y + oy) / k), 0, still.height - 1);
      for (int x = 0; x < width; x++) {
        const int sx = std::clamp(int((x + ox) / k), 0, still.width - 1);
        const uint8_t* src = &still.rgba[(size_t(sy) * size_t(still.width) + size_t(sx)) * 4];
        uint8_t* d = &out.rgba[(size_t(y) * size_t(width) + size_t(x)) * 4];
        for (int c = 0; c < 3; c++) d[c] = uint8_t(src[c] * 3 / 5);  // darkened for the letters
      }
    }
  } else {
    const int hue = map_hue(s.map);
    // A dark ground tinted by the hue: three channels from the hue's place on the wheel.
    const auto ch = [&](int off) {
      const int d = std::abs(((hue + off) % 360 + 360) % 360 - 180);
      return 18 + d * 30 / 180;
    };
    for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++) {
        uint8_t* d = &out.rgba[(size_t(y) * size_t(width) + size_t(x)) * 4];
        const int shade = 100 - y * 40 / std::max(1, height);
        d[0] = uint8_t(ch(0) * shade / 100);
        d[1] = uint8_t(ch(120) * shade / 100);
        d[2] = uint8_t(ch(240) * shade / 100);
      }
  }
  if (look.charset.width < 16 || look.charset.height < look.charset.width) return out;

  // ── The board at the charset's pixels ──
  Board b;
  b.sheet = &look.charset;
  b.cell = look.charset.width / 16;
  const int cell = b.cell, lead = cell + cell / 4;
  const bool teams = card_teams(s);
  const std::vector<const gn::StatusPlayer*> rows = board_rows(spec, s, teams);
  std::vector<std::string> specs;
  for (const gn::StatusPlayer& p : s.players)
    if (p.flags & gn::kPlayerSpectator) specs.push_back(p.name);
  // The scale: the largest whole number at which every row fits the card (the top line and a gap,
  // the words and their rule, the rows, a gap and the foot), at most what makes the card 90 of the
  // charset's pixels high, as a low-resolution screen of the game would be: a duel's few rows as
  // large as a phone's card can show them.
  const int top = cell / 2 + 1;
  const int shown_rows = std::max<int>(1, int(rows.size()));
  int scale = std::max(1, height / (90 * cell / 8));
  // ... and at which the columns fit across: ping 4, frags 5 (a box), team 4 when shown, the name
  // as wide as the longest (4 to 15), a cell between each and one at each side. The rows need the
  // top line, a gap, the words, the rule, the rows and the foot: the rows and five more.
  size_t longest = 4;
  for (const gn::StatusPlayer* p : rows) longest = std::max(longest, p->name.size());
  const int name_cells = int(std::min<size_t>(longest, 15));
  const int width_cells = 4 + 1 + 5 + 1 + (teams ? 5 : 0) + name_cells;
  // The top line: the mode on the map, and the clock or the phase at the right.
  const std::string mode = s.mode.empty() ? std::string("match") : s.mode;
  std::string title = mode + " on " + s.map;
  std::string phase = phase_words(s.phase);
  if (s.time_left_s >= 0 && s.phase == gn::StatusPhase::playing) {
    char t[24];
    std::snprintf(t, sizeof t, "%d:%02d left", s.time_left_s / 60, s.time_left_s % 60);
    phase = t;
  }
  // The top line whole too where it can be: the title, the phase and three cells between.
  const int top_cells = std::min(40, int(title.size() + phase.size()) + 3);
  while (scale > 1 && (top + (shown_rows + 5) * lead > height / scale ||
                       (std::max(width_cells + 2, top_cells + 2)) * cell > width / scale))
    scale--;
  const int vw = width / scale, vh = height / scale;
  b.px.width = vw;
  b.px.height = vh;
  b.px.rgba.assign(size_t(vw) * size_t(vh) * 4, 0);
  const int cols = vw / cell;

  b.text(cell, top, clip(title, size_t(std::max(0, cols - int(phase.size()) - 3))));
  b.text((cols - 1 - int(phase.size())) * cell, top, phase, look.words_alt);

  const int x0 = (vw - width_cells * cell) / 2;
  const int xping = x0, xfrags = x0 + 5 * cell, xteam = x0 + 11 * cell;
  const int xname = x0 + (teams ? 16 : 11) * cell;
  const int board_h = (int(rows.size()) + 2) * lead;
  int y = std::clamp((vh - board_h) / 2, top + 2 * lead,
                     std::max(top + 2 * lead, vh - 2 * lead - board_h));
  // The column words, then their rule (the charset's own line glyphs: 0x1d, 0x1e .. 0x1f).
  b.text(xping, y, "ping", look.words_alt);
  b.text(xfrags, y, "frags", look.words_alt);
  if (teams) b.text(xteam, y, "team", look.words_alt);
  b.text(xname, y, "name", look.words_alt);
  y += lead;
  auto rule = [&](int x, int n) {
    std::string r(size_t(n), '\x1e');
    r.front() = '\x1d';
    r.back() = '\x1f';
    b.text(x, y, r);
  };
  rule(xping, 4);
  rule(xfrags, 5);
  if (teams) rule(xteam, 4);
  rule(xname, name_cells);
  y += lead;
  for (const gn::StatusPlayer* p : rows) {
    if (y + cell > vh - 2 * lead) break;
    const bool bot = (p->flags & gn::kPlayerBot) != 0;
    b.text(xping, y, right(bot ? std::string("bot") : std::to_string(p->ping_ms), 4));
    const std::string frags = right(score_text(spec, p->score), 3);
    if (look.score_box) {
      uint32_t c = p->colour ? p->colour : team_colour(s, p->team);
      if (!c) c = look.box_rgb;
      // The box a little taller than a glyph: the top half its colour, the bottom half darker;
      // with the player's own rows and the game's shades, the upper and lower colour he picked.
      uint32_t dark =
          ((c >> 16 & 255) * 3 / 4) << 16 | ((c >> 8 & 255) * 3 / 4) << 8 | ((c & 255) * 3 / 4);
      if (p->top < look.rows.size() && p->bottom < look.rows.size()) {
        c = look.rows[p->top];
        dark = look.rows[p->bottom];
      }
      b.rect(xfrags, y - 1, 5 * cell, (cell + 2) / 2, c);
      b.rect(xfrags, y - 1 + (cell + 2) / 2, 5 * cell, (cell + 2) - (cell + 2) / 2, dark);
      b.text(xfrags + cell, y, frags);
    } else {
      b.text(xfrags + cell, y, frags);
    }
    if (teams) b.text(xteam, y, clip(p->team, 4));
    b.text(xname, y, clip(p->name, size_t(name_cells)));
    y += lead;
  }
  if (rows.empty()) b.text(xname, y, "no players", look.words_alt);
  // The foot: who spectates.
  if (!specs.empty()) {
    std::string line;
    for (size_t i = 0; i < specs.size(); i++) line += (i ? ", " : "") + specs[i];
    if (specs.size() > 1) line += " (" + std::to_string(specs.size()) + ")";
    const int fy = vh - lead - cell / 2;
    const std::string word = "specs: ";
    const int room = cols - 2 - int(word.size());
    const std::string shown = clip(line, size_t(std::max(0, room)));
    const int fx = (vw - int(word.size() + shown.size()) * cell) / 2;
    b.text(fx, fy, word, look.words_alt);
    b.text(fx + int(word.size()) * cell, fy, shown);
  }

  // ── Laid on the card: a band behind the top line, then each board pixel as a square ──
  fill(out, 0, 0, width, (top + cell + cell / 2) * scale, 0x000000, 150);
  for (int by = 0; by < vh; by++) {
    for (int bx = 0; bx < vw; bx++) {
      const uint8_t* s4 = &b.px.rgba[(size_t(by) * size_t(vw) + size_t(bx)) * 4];
      // A one-pixel shadow down and right of every glyph pixel, as a console's text reads on a
      // bright wall.
      if (s4[3] == 0) {
        const bool lit = (bx > 0 && by > 0 &&
                          b.px.rgba[(size_t(by - 1) * size_t(vw) + size_t(bx - 1)) * 4 + 3] != 0);
        if (lit) fill(out, bx * scale, by * scale, scale, scale, 0x000000, 160);
        continue;
      }
      fill(out, bx * scale, by * scale, scale, scale,
           uint32_t(s4[0]) << 16 | uint32_t(s4[1]) << 8 | s4[2]);
    }
  }
  return out;
}

std::vector<uint8_t> card_jpeg(const CardPicture& picture) {
  return capture::encode_jpeg(picture.width, picture.height, picture.rgba, kQuality, false);
}

}  // namespace ge::hub
