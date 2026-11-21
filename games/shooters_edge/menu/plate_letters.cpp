// Quake II's plate letters cut out as see-through ink from its main menu's tags
// (plate_letters.hpp).
//
// ge:summary Cuts Quake II's tag letters out by whose ink each pixel is; pieces, shrinks, tags
// ge:keywords menu q2 plates tag carved-letters ink alpha ownership rim pieced-letters
// ge:keywords small-capitals shading dirty-metal banner
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/plate_letters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr int kLine = static_cast<int>(kPlateLine), kPad = static_cast<int>(kInkPad);

// The five tags: each one's cells' left edges, its right end's last (the cells run to `end`).
struct PlateCells {
  std::string_view name;
  std::vector<int> starts;
  int end = 0;
};
const std::array<PlateCells, 5>& plate_cells() {
  static const std::array<PlateCells, 5> cells = {{
      {"m_main_game", {19, 36, 53, 69, 86}, 103},
      {"m_main_quit", {19, 36, 53, 69, 86}, 103},
      {"m_main_video", {19, 36, 53, 69, 86, 103}, 120},
      {"m_main_options", {19, 36, 53, 69, 86, 103, 120, 137}, 154},
      {"m_main_multiplayer", {19, 36, 53, 69, 86, 103, 120, 137, 154, 171, 187, 204}, 221},
  }};
  return cells;
}
enum Plate : uint8_t { kGame, kQuit, kVideo, kOptions, kMulti };

// Each capital a tag holds: its tag and cell, and the rows its letter sits lower than the others'
// (GAME's).
struct Source {
  char c;
  Plate plate;
  int cell;
  int lift;
};
constexpr std::array<Source, 17> kSources = {{
    {'A', kMulti, 7, 0},
    {'D', kVideo, 2, 0},
    {'E', kMulti, 9, 0},
    {'G', kGame, 0, 1},
    {'I', kMulti, 4, 0},
    {'L', kMulti, 6, 0},
    {'M', kMulti, 0, 0},
    {'N', kOptions, 5, 0},
    {'O', kOptions, 0, 0},
    {'P', kOptions, 1, 0},
    {'Q', kQuit, 0, 0},
    {'R', kMulti, 10, 0},
    {'S', kOptions, 6, 0},
    {'T', kMulti, 3, 0},
    {'U', kMulti, 1, 0},
    {'V', kVideo, 0, 0},
    {'Y', kMulti, 8, 0},
}};
const Source* source_of(char c) {
  for (const Source& s : kSources)
    if (s.c == c) return &s;
  return nullptr;
}
int cell_x(const Source& s) { return plate_cells()[s.plate].starts[static_cast<size_t>(s.cell)]; }
int cell_w(const Source& s) {
  const PlateCells& p = plate_cells()[s.plate];
  const auto k = static_cast<size_t>(s.cell);
  return (k + 1 < p.starts.size() ? p.starts[k + 1] : p.end) - p.starts[k];
}

// The index of (x, y) in a row-major grid `w` wide.
size_t idx(int x, int y, int w) {
  return static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
}

size_t offset(const ui::Image& img, int x, int y) {
  return (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 4;
}

// The brightness (0.3 R + 0.6 G + 0.1 B, 0..255) of classic px (x, y): its block's mean.
float lum(const ui::Image& img, int x, int y, int d) {
  float sum = 0;
  for (int b = 0; b < d; b++)
    for (int a = 0; a < d; a++) {
      const size_t at = offset(img, x * d + a, y * d + b);
      sum += 0.3f * img.rgba[at] + 0.6f * img.rgba[at + 1] + 0.1f * img.rgba[at + 2];
    }
  return sum / static_cast<float>(d * d);
}

// --- Whose ink each pixel is ---

// A tag's pixel is a letter's ink (its cell's index), nobody's (-1), or a cut-off shadow (-2: ink,
// but no letter's).
constexpr int kNobody = -1, kStray = -2;
// Bright: a face or a highlight; dark: a shadow; a face's dimmer pixel beside a bright one is
// bright too. Only the letters' rows are looked at.
constexpr float kBright = 52, kDark = 28, kFace = 46;
constexpr int kRowTop = 4, kRowEnd = 26;

struct Owners {
  int w = 0;
  std::vector<int> own;  // w x kLine
  int at(int x, int y) const {
    return x >= 0 && y >= 0 && x < w && y < kLine ? own[idx(x, y, w)] : kNobody;
  }
};

// The 8-connected parts of `mask` (w x kLine), each its pixels' indices.
std::vector<std::vector<int>> parts(const std::vector<uint8_t>& mask, int w) {
  std::vector<uint8_t> seen(mask.size(), 0);
  std::vector<std::vector<int>> out;
  for (int i = 0; i < static_cast<int>(mask.size()); i++) {
    if (mask[static_cast<size_t>(i)] == 0 || seen[static_cast<size_t>(i)] != 0) continue;
    std::vector<int> stack = {i}, part;
    seen[static_cast<size_t>(i)] = 1;
    while (!stack.empty()) {
      const int p = stack.back();
      stack.pop_back();
      part.push_back(p);
      const int x = p % w, y = p / w;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const int a = x + dx, b = y + dy;
          if (a < 0 || b < 0 || a >= w || b >= kLine) continue;
          const auto q = idx(a, b, w);
          if (mask[q] != 0 && seen[q] == 0) {
            seen[q] = 1;
            stack.push_back(b * w + a);
          }
        }
    }
    out.push_back(std::move(part));
  }
  return out;
}

// Bright parts go to the cell holding their middle (a part over two cells with 15 px or more in
// each is split at the cells' edge); every shadow to the letter of the nearest bright pixel within
// 3 px; a letter's shadows cut off from its bright pixels are nobody's letter.
Owners owners(const ui::Image& img, const PlateCells& cells, int d) {
  Owners o;
  o.w = img.width / d;
  const int w = o.w;
  const auto n = static_cast<size_t>(w) * kLine;
  std::vector<uint8_t> bright(n, 0), ink(n, 0);
  std::vector<float> l(n, 0);
  for (int y = kRowTop; y < kRowEnd; y++)
    for (int x = 0; x < w; x++) {
      const auto i = idx(x, y, w);
      l[i] = lum(img, x, y, d);
      bright[i] = l[i] >= kBright ? 1 : 0;
      ink[i] = l[i] >= kBright || l[i] <= kDark ? 1 : 0;
    }
  for (int pass = 0; pass < 2; pass++) {
    std::vector<size_t> grown;
    for (int y = kRowTop; y < kRowEnd; y++)
      for (int x = 0; x < w; x++) {
        const auto i = idx(x, y, w);
        if (bright[i] != 0 || l[i] < kFace) continue;
        // Grown only from a bright pixel beside or above: the plate above a top serif is as bright
        // as a face, and growing up into it puts a strip over the letter.
        bool near = false;
        for (int b = -1; b <= 0 && !near; b++)
          for (int a = -1; a <= 1 && !near; a++)
            near = x + a >= 0 && x + a < w && bright[idx(x + a, y + b, w)] != 0;
        if (near) grown.push_back(i);
      }
    for (const size_t i : grown) bright[i] = ink[i] = 1;
  }
  const auto cell = [&cells](int x) {
    for (size_t k = 0; k < cells.starts.size(); k++) {
      const int to = k + 1 < cells.starts.size() ? cells.starts[k + 1] : cells.end;
      if (x >= cells.starts[k] && x < to) return static_cast<int>(k);
    }
    return kNobody;
  };
  o.own.assign(n, kNobody);
  std::vector<std::vector<int>> small;
  for (std::vector<int>& part : parts(bright, w)) {
    if (part.size() < 3) continue;
    if (part.size() < 15) {
      small.push_back(std::move(part));
      continue;
    }
    std::map<int, int> count;
    double mid = 0;
    for (const int p : part) {
      count[cell(p % w)]++;
      mid += p % w;
    }
    int big = 0;
    for (const auto& [k, c] : count) big += k >= 0 && c >= 15 ? 1 : 0;
    const int whole = cell(static_cast<int>(mid / static_cast<double>(part.size())));
    for (const int p : part) o.own[static_cast<size_t>(p)] = big >= 2 ? cell(p % w) : whole;
  }
  // A small bright part (a serif's tip past its cell) is the letter of the nearest big one within
  // 5 px.
  const std::vector<int> big = o.own;
  for (const std::vector<int>& part : small) {
    int best = kNobody;
    for (int r = 1; r <= 5 && best == kNobody; r++)
      for (size_t k = 0; k < part.size() && best == kNobody; k++) {
        const int x = part[k] % w, y = part[k] / w;
        for (int dy = -r; dy <= r && best == kNobody; dy++)
          for (int dx = -r; dx <= r && best == kNobody; dx++) {
            const int a = x + dx, b = y + dy;
            if (std::max(std::abs(dx), std::abs(dy)) == r && a >= 0 && b >= 0 && a < w &&
                b < kLine && big[idx(a, b, w)] >= 0)
              best = big[idx(a, b, w)];
          }
      }
    for (const int p : part) o.own[static_cast<size_t>(p)] = best;
  }
  // A tip a cell's edge split off a letter (under 10 px) touching another letter's is that one's.
  for (int k = 0; k < static_cast<int>(cells.starts.size()); k++) {
    std::vector<uint8_t> mine(n, 0);
    for (size_t i = 0; i < n; i++) mine[i] = o.own[i] == k ? 1 : 0;
    for (const std::vector<int>& part : parts(mine, w)) {
      if (part.size() >= 10) continue;
      int other = kNobody;
      for (const int p : part)
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const int a = p % w + dx, b = p / w + dy;
            if (a >= 0 && b >= 0 && a < w && b < kLine && o.own[idx(a, b, w)] >= 0 &&
                o.own[idx(a, b, w)] != k)
              other = o.own[idx(a, b, w)];
          }
      if (other != kNobody)
        for (const int p : part) o.own[static_cast<size_t>(p)] = other;
    }
  }
  std::vector<int> res = o.own;
  for (int y = 0; y < kLine; y++)
    for (int x = 0; x < w; x++) {
      const auto i = idx(x, y, w);
      if (ink[i] == 0 || o.own[i] != kNobody) continue;
      int best = kNobody;
      for (int r = 1; r <= 3 && best == kNobody; r++)
        for (int dy = -r; dy <= r && best == kNobody; dy++)
          for (int dx = -r; dx <= r && best == kNobody; dx++)
            if (std::max(std::abs(dx), std::abs(dy)) == r && o.at(x + dx, y + dy) >= 0)
              best = o.at(x + dx, y + dy);
      res[i] = best;
    }
  for (int k = 0; k < static_cast<int>(cells.starts.size()); k++) {
    std::vector<uint8_t> mine(n, 0);
    for (size_t i = 0; i < n; i++) mine[i] = res[i] == k ? 1 : 0;
    for (const std::vector<int>& part : parts(mine, w)) {
      const auto lit = std::count_if(part.begin(), part.end(), [&bright](int p) {
        return bright[static_cast<size_t>(p)] != 0;
      });
      if (lit < 3)
        for (const int p : part) res[static_cast<size_t>(p)] = kStray;
    }
  }
  o.own = std::move(res);
  return o;
}

// --- The letters as pieces of cells ---

// The w x h classic px at (x, y) of `c`'s cell (x from the cell's left, y from its letter's top),
// set at (dx, dy) of the new cell; upside down when `flip`; `c` 0 clears them instead.
struct Bit {
  char c = 0;
  int x = 0, y = 0, w = 0, h = 0, dx = 0, dy = 0;
  bool flip = false;
};
// A cell `w` px wide: nothing, the bits laid over it in order.
struct Recipe {
  char c;
  int w;
  std::vector<Bit> bits;
};

// Columns x0..x1 and rows y0..y1 of `c`'s cell, moved by (dx, dy).
Bit part(char c, int x0, int y0, int x1, int y1, int dx = 0, int dy = 0) {
  return {c, x0, y0, x1 - x0 + 1, y1 - y0 + 1, x0 + dx, y0 + dy, false};
}
Bit clear(int x, int y, int w, int h) { return {0, x, y, w, h, x, y, false}; }

// Every capital: the seventeen cut whole (with their pads, so a serif past the cell comes along),
// the nine pieced (the header says from what).
std::vector<Recipe> recipes() {
  std::vector<Recipe> out;
  const auto full = [](char c) {
    const Source& s = *source_of(c);
    return part(c, -kPad, 0, cell_w(s) + kPad - 1, kLine - 1);
  };
  out.reserve(kSources.size() + 9);
  for (const Source& s : kSources) out.push_back({s.c, cell_w(s), {full(s.c)}});
  out.push_back({'B', 17, {part('P', -4, 0, 20, 15), part('D', -4, 16, 19, 31, 1)}});
  out.push_back({'C', 17, {full('O'), clear(11, 10, 10, 8)}});
  out.push_back({'F', 16, {part('E', -4, 0, 19, 16), part('E', -4, 17, 6, 31)}});
  out.push_back({'H',
                 17,
                 {full('N'), clear(4, 8, 8, 12), clear(7, 20, 5, 3), part('E', 3, 13, 8, 15, 1),
                  part('E', 3, 13, 8, 15, 3)}});
  out.push_back({'J', 15, {part('U', 8, 0, 20, 31, -2), part('U', 0, 18, 7, 31, -2)}});
  out.push_back(
      {'K', 17, {part('R', -4, 0, 6, 31), part('R', 7, 17, 20, 31), part('Y', 7, 6, 20, 16, -3)}});
  // W: two V's, the second's left serif on the first's right one.
  out.push_back({'W', 28, {full('V'), part('V', -4, 0, 20, 31, 11)}});
  Bit under = part('Y', -4, 7, 20, 14, 0, 8);
  under.flip = true;
  out.push_back(
      {'X', 17, {part('Y', -4, 0, 20, 14), under, part('A', -4, 19, 20, 31), clear(5, 19, 5, 2)}});
  // Z: T's bar without its stem or its right tip, Y's right arm twice, one below and left of the
  // other, down to L's foot without its stem.
  out.push_back({'Z',
                 17,
                 {part('T', -4, 0, 20, 9), clear(-4, 9, 30, 5), clear(14, 0, 10, 9),
                  part('L', -4, 19, 20, 31), clear(-4, 19, 9, 1), part('Y', 8, 8, 17, 14, 0, 1),
                  part('Y', 8, 8, 17, 14, -6, 7)}});
  return out;
}

// --- Painting a letter ---

// One set of tags (the plain or the lit): the pictures, each tag's shade to GAME's surface and the
// recess's colour.
struct Sheet {
  int d = 1;
  std::array<const ui::Image*, 5> plate{};
  std::array<float, 5> shade{};
  std::array<float, 3> recess{};
  bool ok = false;
};

// The mean brightness of a picture's surface: its rows 2 to 5, x `x0` to `x1` (classic px); 0 when
// it is missing.
float surface(const ui::Image* img, int d, int x0 = 20, int x1 = 80) {
  if (img == nullptr) return 0;
  double sum = 0;
  int count = 0;
  for (int y = 2; y < 6 && y < img->height / d; y++)
    for (int x = x0; x < x1 && x < img->width / d; x++) {
      sum += lum(*img, x, y, d);
      count++;
    }
  return count > 0 ? static_cast<float>(sum / count) : 0;
}

Sheet sheet_of(const ClassicArt& art, std::string_view suffix, const std::array<Owners, 5>& own) {
  Sheet s;
  s.d = std::max(1, static_cast<int>(art.density));
  std::array<float, 5> f{};
  for (size_t k = 0; k < 5; k++) {
    s.plate[k] = art.find(std::string(plate_cells()[k].name) + std::string(suffix));
    f[k] = surface(s.plate[k], s.d);
    if (f[k] <= 0 || s.plate[k]->height < kLine * s.d || s.plate[k]->width / s.d != own[k].w)
      return s;
  }
  for (size_t k = 0; k < 5; k++) s.shade[k] = f[kGame] / f[k];
  // The recess: MULTIPLAYER's pixels between its letters that are nobody's ink.
  const ui::Image& m = *s.plate[kMulti];
  std::array<double, 3> sum{};
  int count = 0;
  for (int y = 6; y < 24; y++)
    for (int x = 19; x < own[kMulti].w - 14; x++) {
      const float l = lum(m, x, y, s.d);
      if (l <= kDark || l >= kBright || own[kMulti].at(x, y) != kNobody) continue;
      for (int b = 0; b < s.d; b++)
        for (int a = 0; a < s.d; a++)
          for (size_t c = 0; c < 3; c++) sum[c] += m.rgba[offset(m, x * s.d + a, y * s.d + b) + c];
      count += s.d * s.d;
    }
  for (size_t c = 0; c < 3; c++)
    s.recess[c] = count > 0 ? static_cast<float>(sum[c] / count) * s.shade[kMulti] : 40;
  s.ok = true;
  return s;
}

// A cell's classic px: where its colour comes from and what it is there.
enum Kind : uint8_t { kEmpty, kCore, kGround, kForeign };
struct Px {
  uint8_t plate = 0;  // a Plate; meaningful only when `kind` is not kEmpty
  int16_t x = 0, y = 0;
  Kind kind = kEmpty;
};
struct Canvas {
  int w = 0;  // classic px: the cell and its pads
  std::vector<Px> px;
  Px& at(int x, int y) { return px[idx(x, y, w)]; }
  const Px& at(int x, int y) const { return px[idx(x, y, w)]; }
};

// The cell of a recipe: its bits laid in order, a letter's ink never covered by another bit's
// ground.
Canvas lay(const Recipe& r, const std::array<Owners, 5>& own) {
  Canvas cv;
  cv.w = r.w + 2 * kPad;
  cv.px.assign(static_cast<size_t>(cv.w) * kLine, Px{});
  for (const Bit& b : r.bits) {
    const Source* s = b.c != 0 ? source_of(b.c) : nullptr;
    for (int j = 0; j < b.h; j++)
      for (int i = 0; i < b.w; i++) {
        const int tx = b.dx + i + kPad, ty = b.dy + j;
        if (tx < 0 || ty < 0 || tx >= cv.w || ty >= kLine) continue;
        if (s == nullptr) {
          cv.at(tx, ty) = Px{};
          continue;
        }
        const Owners& o = own[s->plate];
        const int sx = cell_x(*s) + b.x + i, sy = (b.flip ? b.y + b.h - 1 - j : b.y + j) + s->lift;
        if (sx < 0 || sy < 0 || sx >= o.w || sy >= kLine) continue;
        const int who = o.at(sx, sy);
        const Kind kind = who == s->cell ? kCore : who == kNobody ? kGround : kForeign;
        if (kind != kCore && cv.at(tx, ty).kind == kCore) continue;
        cv.at(tx, ty) = {static_cast<uint8_t>(s->plate), static_cast<int16_t>(sx),
                         static_cast<int16_t>(sy), kind};
      }
  }
  // A crumb of ink a cut left apart from the letter (under 6 px) is dropped.
  std::vector<uint8_t> core(cv.px.size(), 0);
  for (size_t i = 0; i < core.size(); i++) core[i] = cv.px[i].kind == kCore ? 1 : 0;
  for (const std::vector<int>& bit : parts(core, cv.w))
    if (bit.size() < 6)
      for (const int p : bit) cv.px[static_cast<size_t>(p)].kind = kForeign;
  return cv;
}

// The rim: whole for two px around the ink, then fading.
constexpr std::array<float, 4> kRim = {1.0f, 1.0f, 0.6f, 0.25f};

// The cell's picture: its ink and the rim around it coloured from `s`'s tags (the recess's colour
// where a pixel is another letter's), see-through elsewhere.
ui::Image paint(const Canvas& cv, const Sheet& s) {
  const int d = s.d;
  ui::Image out;
  out.width = cv.w * d;
  out.height = kLine * d;
  out.rgba.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
  for (int y = 0; y < kLine; y++)
    for (int x = 0; x < cv.w; x++) {
      int dist = 99;
      for (int b = -3; b <= 3; b++)
        for (int a = -3; a <= 3; a++)
          if (x + a >= 0 && y + b >= 0 && x + a < cv.w && y + b < kLine &&
              cv.at(x + a, y + b).kind == kCore)
            dist = std::min(dist, std::max(std::abs(a), std::abs(b)));
      if (dist >= static_cast<int>(kRim.size())) continue;
      const Px& p = cv.at(x, y);
      const bool own = p.kind == kCore || p.kind == kGround;
      const auto alpha = static_cast<uint8_t>(255 * kRim[static_cast<size_t>(dist)]);
      for (int b = 0; b < d; b++)
        for (int a = 0; a < d; a++) {
          const size_t to = offset(out, x * d + a, y * d + b);
          for (size_t c = 0; c < 3; c++) {
            float v = s.recess[c];
            if (own) {
              const ui::Image& src = *s.plate[p.plate];
              v = static_cast<float>(src.rgba[offset(src, p.x * d + a, p.y * d + b) + c]) *
                  s.shade[p.plate];
            }
            out.rgba[to + c] = static_cast<uint8_t>(std::min(255.0f, v + 0.5f));
          }
          out.rgba[to + 3] = alpha;
        }
    }
  return out;
}

// --- The small letters ---

// The rows and columns a capital drops to become its small letter.
struct Shrink {
  std::vector<int> rows, cols;
};

// The letter's band (rows 6 to 22) loses the four of rows 9 to 18 most like the row under them, and
// the three columns of its cell most like the column after them, a stem's (a run of 7 or more
// bright ink pixels) last, never one beside a dropped one; a tie goes to the first.
constexpr int kBandTop = 6, kBandEnd = 23, kDropRows = 4, kDropCols = 3;
Shrink shrink_lines(const Canvas& cv, const Sheet& s) {
  const auto ink = [&](int x, int y) {
    const Px& p = cv.at(x, y);
    return p.kind == kCore ? lum(*s.plate[p.plate], p.x, p.y, s.d) : 0.0f;
  };
  std::vector<bool> stem(static_cast<size_t>(cv.w), false);
  for (int x = 0; x < cv.w; x++) {
    int run = 0, best = 0;
    for (int y = 0; y < kLine; y++) {
      run = ink(x, y) >= 50 ? run + 1 : 0;
      best = std::max(best, run);
    }
    stem[static_cast<size_t>(x)] = best >= 7;
  }
  std::vector<std::pair<float, int>> rows, cols;
  for (int y = 9; y <= 18; y++) {
    float cost = 0;
    for (int x = 0; x < cv.w; x++) cost += std::abs(ink(x, y) - ink(x, y + 1));
    rows.emplace_back(cost, y);
  }
  // A stem's column only after every other (a letter too narrow for three without one).
  float most = 0;
  for (int x = kPad + 2; x < cv.w - kPad - 3; x++) {
    float cost = 0;
    for (int y = kBandTop; y < kBandEnd; y++) cost += std::abs(ink(x, y) - ink(x + 1, y));
    most = std::max(most, cost);
    cols.emplace_back(cost, x);
  }
  for (auto& [cost, x] : cols)
    if (stem[static_cast<size_t>(x)] || stem[static_cast<size_t>(x) + 1]) cost += most + 1;
  std::sort(rows.begin(), rows.end());
  std::sort(cols.begin(), cols.end());
  Shrink out;
  for (int k = 0; k < kDropRows && k < static_cast<int>(rows.size()); k++)
    out.rows.push_back(rows[static_cast<size_t>(k)].second);
  for (const auto& [cost, x] : cols) {
    if (static_cast<int>(out.cols.size()) == kDropCols) break;
    if (std::none_of(out.cols.begin(), out.cols.end(),
                     [col = x](int c) { return std::abs(c - col) <= 1; }))
      out.cols.push_back(x);
  }
  return out;
}

// The small letter's cell: the capital's kept columns, its rows above and below the band as they
// are, its band's kept rows set down onto the foot and nothing above them.
Canvas shrink(const Canvas& cap, const Shrink& sh) {
  const auto has = [](const std::vector<int>& v, int i) {
    return std::find(v.begin(), v.end(), i) != v.end();
  };
  std::vector<int> xs, ys;
  for (int x = 0; x < cap.w; x++)
    if (!has(sh.cols, x)) xs.push_back(x);
  for (int y = kBandTop; y < kBandEnd; y++)
    if (!has(sh.rows, y)) ys.push_back(y);
  const int top = kBandEnd - static_cast<int>(ys.size());
  Canvas out;
  out.w = static_cast<int>(xs.size());
  out.px.assign(static_cast<size_t>(out.w) * kLine, Px{});
  for (int y = 0; y < kLine; y++) {
    int from = y;
    if (y >= kBandTop && y < kBandEnd) {
      if (y < top) continue;
      from = ys[static_cast<size_t>(y - top)];
    }
    for (int i = 0; i < out.w; i++) out.at(i, y) = cap.at(xs[static_cast<size_t>(i)], from);
  }
  return out;
}

// --- The tag's ends and its metal ---

// Columns x0 .. x0 + w of `src` (classic px), mirrored when `mirror`, every pixel times `k`.
ui::Image columns(const ui::Image& src, int x0, int w, float k, int d, bool mirror) {
  ui::Image out;
  out.width = w * d;
  out.height = kLine * d;
  out.rgba.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
  for (int y = 0; y < out.height; y++)
    for (int x = 0; x < out.width; x++) {
      const int sx = mirror ? (x0 + w) * d - 1 - x : x0 * d + x;
      const size_t from = offset(src, sx, y), to = offset(out, x, y);
      for (size_t c = 0; c < 3; c++)
        out.rgba[to + c] = static_cast<uint8_t>(
            std::min(255.0f, static_cast<float>(src.rgba[from + c]) * k + 0.5f));
      out.rgba[to + 3] = src.rgba[from + 3];
    }
  return out;
}

// The metal: the banner's 60 px after its logo, then the same mirrored, so the strip tiles.
constexpr int kMetalFrom = static_cast<int>(kBannerLeft), kMetalHalf = 60;
ui::Image metal(const ui::Image& banner, float k, int d) {
  ui::Image out = columns(banner, kMetalFrom, 2 * kMetalHalf, k, d, false);
  const ui::Image back = columns(banner, kMetalFrom, kMetalHalf, k, d, true);
  for (int y = 0; y < out.height; y++)
    std::copy_n(back.rgba.begin() + static_cast<std::ptrdiff_t>(offset(back, 0, y)), back.width * 4,
                out.rgba.begin() + static_cast<std::ptrdiff_t>(offset(out, kMetalHalf * d, y)));
  return out;
}

// Pieces of the metal strip covering `from` .. `to` (classic px).
void metal_run(const ui::Image* strip, float from, float to, std::vector<TagPiece>* out) {
  const int strips = static_cast<int>(std::ceil((to - from) / kMetalStrip));
  for (int k = 0; k < strips; k++) {
    const float x = from + kMetalStrip * static_cast<float>(k);
    out->push_back({strip, x, 0, std::min(kMetalStrip, to - x), 1});
  }
}

}  // namespace

void cut_plate_letters(ClassicArt* art) {
  art->letters.clear();
  art->letters_lit.clear();
  if (art->strip_print != kPlatePrint) return;
  const ui::Image* banner = art->find(kBannerFrame);
  const int d = std::max(1, static_cast<int>(art->density));
  if (banner == nullptr || banner->width < (kMetalFrom + 2 * kMetalHalf) * d ||
      banner->height < kLine * d)
    return;
  std::array<Owners, 5> own;
  for (size_t k = 0; k < 5; k++) {
    const ui::Image* img = art->find(plate_cells()[k].name);
    if (img == nullptr || img->height < kLine * d) return;
    own[k] = owners(*img, plate_cells()[k], d);
  }
  const Sheet plain = sheet_of(*art, "", own), lit = sheet_of(*art, "_sel", own);
  if (!plain.ok) return;
  const float steel = surface(banner, d, kMetalFrom, kMetalFrom + kMetalHalf);
  const std::vector<Recipe> all = recipes();
  for (const Sheet* s : {&plain, &lit}) {
    if (!s->ok) continue;
    auto& set = s == &plain ? art->letters : art->letters_lit;
    const ui::Image& game = *s->plate[kGame];
    if (game.width < 99 * d) return;
    constexpr int kLeft = static_cast<int>(kTagLeft), kRight = static_cast<int>(kTagRight);
    set.emplace('[', columns(game, 0, kLeft, 1.0f, d, false));
    set.emplace(']', columns(game, 86, kRight, 1.0f, d, false));
    set.emplace('_', metal(*banner, steel > 0 ? surface(&game, d) / steel : 1, d));
    for (const Recipe& r : all) {
      const Canvas cap = lay(r, own);
      set.emplace(r.c, paint(cap, *s));
      set.emplace(static_cast<char>(r.c - 'A' + 'a'),
                  paint(shrink(cap, shrink_lines(cap, plain)), *s));
    }
  }
}

namespace {

// A letter's cell width (classic px): its picture without its pads; a space's; -1 when not cut.
float advance(const ClassicArt& art, char c) {
  if (c == ' ') return kTagSpace;
  if (c == '[' || c == ']' || c == '_') return -1;
  const auto it = art.letters.find(c);
  return it == art.letters.end() ? -1 : art.width_of(it->second) - 2 * kInkPad;
}

// The letters of `text` over the metal, cell after cell from `x`, from `set`.
void letter_pieces(const ClassicArt& art, const std::map<char, ui::Image>& set,
                   std::string_view text, float x, std::vector<TagPiece>* out) {
  for (const char c : text) {
    if (c != ' ') {
      const auto it = set.find(c);
      const ui::Image& img = it != set.end() ? it->second : art.letters.at(c);
      out->push_back({&img, x - kInkPad, 0, art.width_of(img), 1});
    }
    x += advance(art, c);
  }
}

}  // namespace

float plate_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs) {
  if (xs != nullptr) xs->assign(text.size() + 2, 0);
  if (art.letters.count('[') == 0) return 0;
  float x = kTagLeft;
  for (size_t i = 0; i < text.size(); i++) {
    const float a = advance(art, text[i]);
    if (a < 0) return 0;
    if (xs != nullptr) (*xs)[i + 1] = x;
    x += a;
  }
  if (xs != nullptr) xs->back() = x;
  return x + kTagRight;
}

std::vector<TagPiece> tag_pieces(const ClassicArt& art, std::string_view text, bool lit,
                                 float* width, Tone tone) {
  const float wide = plate_layout(art, text, nullptr);
  if (width != nullptr) *width = wide;
  if (wide <= 0) return {};
  const auto& set =
      toned(lit && art.letters_lit.count('[') != 0 ? art.letters_lit : art.letters, tone);
  std::vector<TagPiece> out;
  metal_run(&set.at('_'), kTagLeft, wide - kTagRight, &out);
  out.push_back({&set.at('['), 0, 0, kTagLeft, 1});
  out.push_back({&set.at(']'), wide - kTagRight, 0, kTagRight, 1});
  letter_pieces(art, set, text, kTagLeft, &out);
  return out;
}

float banner_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs) {
  if (xs != nullptr) xs->assign(text.size(), 0);
  const ui::Image* frame = art.find(kBannerFrame);
  if (frame == nullptr || art.width_of(*frame) <= kBannerRight || art.letters.count('_') == 0)
    return 0;
  float word = 0;
  for (size_t i = 0; i < text.size(); i++) {
    const float a = advance(art, text[i]);
    if (a < 0) return 0;
    if (xs != nullptr) (*xs)[i] = word;
    word += a;
  }
  const float plain = std::max(kBannerRight - kBannerLeft, word + 8);
  const float start = kBannerLeft + static_cast<float>(static_cast<int>((plain - word) / 2));
  if (xs != nullptr)
    for (float& x : *xs) x += start;
  return kBannerLeft + plain + art.width_of(*frame) - kBannerRight;
}

float banner_shade(const ClassicArt& art) {
  const ui::Image* frame = art.find(kBannerFrame);
  const auto it = art.letters.find('_');
  if (frame == nullptr || it == art.letters.end()) return 1;
  const int d = std::max(1, static_cast<int>(art.density));
  const float f = surface(frame, d, kMetalFrom, kMetalFrom + kMetalHalf);
  const float m = surface(&it->second, d, 0, kMetalHalf);
  return f > 0 && m > 0 ? m / f : 1;
}

std::vector<TagPiece> banner_pieces(const ClassicArt& art, std::string_view text, float* width) {
  std::vector<float> xs;
  const float wide = banner_layout(art, text, &xs);
  if (width != nullptr) *width = wide;
  if (wide <= 0) return {};
  const ui::Image* frame = art.find(kBannerFrame);
  const float right = art.width_of(*frame) - kBannerRight, shade = banner_shade(art);
  std::vector<TagPiece> out;
  out.push_back({frame, 0, 0, kBannerLeft, shade});
  metal_run(&art.letters.at('_'), kBannerLeft, wide - right, &out);
  out.push_back({frame, wide - right, kBannerRight, right, shade});
  letter_pieces(art, art.letters, text, xs.empty() ? 0 : xs[0], &out);
  return out;
}

}  // namespace shooters_edge::menu
