// The menu's carved letters cut one by one from the pack's strips (carved_letters.hpp).
//
// ge:summary Cuts the carved letters out of the strips by their own pixels, and measures words
// ge:keywords menu classic carved-letters letter-pictures strips speck connected-parts
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/carved_letters.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <span>
#include <vector>

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

// The strips the table was measured on (`ClassicArt::strip_print`).
constexpr uint64_t kStripPrint = 0xb378b8eec94a7f36ULL;
// A part of a piece smaller than this (pixels at the pack's own size) is a speck of a neighbour.
constexpr int kSpeck = 6;

// The spacing, measured between the inks of the strips' own words: 1 px between letters (39 of
// the strips' 46 gaps; the rest 2), and a word's space 5 px before a round capital ("New Game":
// the w to the G) and 10 px before a stem ("Single Player": the e to the P).
constexpr float kLetterGap = 1, kSpaceRound = 5, kSpaceStem = 10;
bool round_left(char c) {
  return c == 'G' || c == 'O' || c == 'Q' || c == 'C' || c == 'o' || c == 'c';
}

// A piece of a letter: the box x0..x1, y0..y1 (classic px, inclusive) of `picture`, set at `dx`
// from the letter's left and with the box's top on row `top` of the line. The strips' first band
// (`mainmenu`'s and `sp_menu`'s top row of words) sits a pixel higher than the others, so a piece
// from it is set a row lower than its place in the band. A leaning piece keeps, on its box's row
// j, only the columns from `lean` * j on: the cut between two letters whose diagonals lie side by
// side (the A and the v of Save, the a and the y of Player).
struct Piece {
  std::string_view picture;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int dx = 0, top = 0;
  float lean = 0;
};
struct Letter {
  char c;
  std::span<const Piece> pieces;
};

constexpr Piece kQ[] = {{"mainmenu", 0, 80, 22, 103, 0, 0}};  // Quit's Q, its tail to row 23
constexpr Piece kU[] = {{"mainmenu", 23, 83, 41, 95, 0, 3}};  // Quit's u
constexpr Piece kI[] = {{"mainmenu", 43, 83, 50, 95, 0, 3}};  // Quit's i
// Player's a from its apex's broad row: its point would stand a pixel above every flat top.
constexpr Piece kA[] = {{"mainmenu", 151, 3, 167, 14, 0, 4}};
constexpr Piece kE[] = {{"mainmenu", 153, 63, 169, 75, 0, 3}};  // Ordering's e
// The k: Player's r stem, Player's y right arm against it (short of the e's stem beside it), the
// r's leg under the arm.
constexpr Piece kK[] = {{"mainmenu", 199, 2, 208, 14, 0, 3},
                        {"mainmenu", 176, 2, 182, 9, 9, 3},
                        {"mainmenu", 211, 9, 220, 14, 9, 10}};
constexpr Piece kS[] = {{"mainmenu", 1, 0, 20, 19, 0, 1}};    // Single's S
constexpr Piece kL[] = {{"mainmenu", 40, 60, 56, 79, 0, 0}};  // Help's l
// The c: the left half of Options' o and the ends of its right half's top and foot, open between.
constexpr Piece kC[] = {{"mainmenu", 67, 40, 77, 59, 0, 0},
                        {"mainmenu", 78, 43, 83, 45, 11, 3},
                        {"mainmenu", 78, 53, 83, 55, 11, 13}};
constexpr Piece kT[] = {{"mainmenu", 40, 40, 58, 59, 0, 0}};     // Options' t
constexpr Piece kO[] = {{"mainmenu", 67, 40, 87, 59, 0, 0}};     // Options' o
constexpr Piece kSs[] = {{"mainmenu", 106, 40, 122, 59, 0, 0}};  // Options' s
// Game's G, its tail to row 22; its last two columns only down to the jaw, where the a begins.
constexpr Piece kG[] = {{"sp_menu", 70, 0, 88, 21, 0, 1}, {"sp_menu", 89, 0, 90, 10, 19, 1}};
constexpr Piece kM[] = {{"sp_menu", 107, 0, 127, 19, 0, 1}};  // Game's m
// The capital D: Load's L stem and the right half of Options' O.
constexpr Piece kD[] = {{"sp_menu", 2, 20, 9, 39, 0, 0}, {"mainmenu", 11, 40, 21, 59, 8, 0}};

// The other capitals the strips hold.
constexpr Piece kCapM[] = {{"mainmenu", 1, 20, 22, 39, 0, 0}};    // Multiplayer's M
constexpr Piece kCapH[] = {{"mainmenu", 1, 60, 21, 79, 0, 0}};    // Help's H
constexpr Piece kCapO[] = {{"mainmenu", 91, 60, 113, 79, 0, 0}};  // Ordering's O
constexpr Piece kCapP[] = {{"mainmenu", 114, 0, 133, 19, 0, 1}};  // Player's P
constexpr Piece kCapN[] = {{"sp_menu", 1, 0, 21, 19, 0, 1}};      // New's N
constexpr Piece kCapL[] = {{"sp_menu", 2, 20, 20, 39, 0, 0}};     // Load's L
constexpr Piece kCapJ[] = {{"mp_menu", 3, 0, 12, 19, 0, 1}};      // Join's J, its foot to row 19
// And the other small capitals.
constexpr Piece kN[] = {{"mainmenu", 88, 40, 105, 59, 0, 0}};    // Options' n
constexpr Piece kGs[] = {{"mainmenu", 219, 60, 237, 79, 0, 0}};  // Ordering's g, its tail to 19
constexpr Piece kR[] = {{"mainmenu", 114, 60, 133, 79, 0, 0}};   // Ordering's r
constexpr Piece kP[] = {{"mainmenu", 86, 20, 104, 39, 0, 0}};    // Multiplayer's p
constexpr Piece kDs[] = {{"mainmenu", 134, 60, 152, 79, 0, 0}};  // Ordering's d
constexpr Piece kW[] = {{"sp_menu", 39, 0, 65, 19, 0, 1}};       // New's w
// Save's v and Player's y, each cut from the a beside it along the a's leg.
constexpr Piece kV[] = {{"sp_menu", 28, 40, 49, 59, 0, 0, 0.7F}};
constexpr Piece kY[] = {{"mainmenu", 163, 0, 181, 19, 0, 1, 0.45F}};

constexpr Letter kLetters[] = {{'Q', kQ},    {'u', kU},    {'i', kI},    {'a', kA},    {'e', kE},
                               {'k', kK},    {'S', kS},    {'l', kL},    {'c', kC},    {'t', kT},
                               {'o', kO},    {'s', kSs},   {'G', kG},    {'m', kM},    {'D', kD},
                               {'M', kCapM}, {'H', kCapH}, {'O', kCapO}, {'P', kCapP}, {'N', kCapN},
                               {'L', kCapL}, {'J', kCapJ}, {'n', kN},    {'g', kGs},   {'r', kR},
                               {'p', kP},    {'d', kDs},   {'w', kW},    {'v', kV},    {'y', kY}};

// The letters the strips hold in one size only, made from the other size: a small capital grown
// into its capital (three rows and 15 % of its columns more), a capital shrunk into its small
// capital (three rows and 13 % of its columns fewer). The rows and columns doubled or dropped are
// the ones most like their neighbour (inside a straight stem or bar), never two side by side and
// never in the three at either edge, so the rim, the bevel and every curve stay the letter's own.
struct Sized {
  char c, from;
  bool grow;
};
constexpr Sized kSized[] = {
    {'A', 'a', true}, {'C', 'c', true},  {'E', 'e', true},  {'I', 'i', true}, {'K', 'k', true},
    {'R', 'r', true}, {'T', 't', true},  {'U', 'u', true},  {'V', 'v', true}, {'W', 'w', true},
    {'Y', 'y', true}, {'h', 'H', false}, {'j', 'J', false}, {'q', 'Q', false}};

// Copies the parts of `src`'s box that are no speck onto `dst` at (x, y), px.
void put_piece(const ui::Image& src, int bx, int by, int bw, int bh, int d, float lean,
               ui::Image& dst, int x, int y) {
  std::vector<int> label(static_cast<size_t>(bw) * static_cast<size_t>(bh), 0);
  const auto solid = [&](int i, int j) {
    return src.rgba[(static_cast<size_t>(by + j) * static_cast<size_t>(src.width) +
                     static_cast<size_t>(bx + i)) *
                        4 +
                    3] > 0;
  };
  const auto at = [bw](int i, int j) {
    return static_cast<size_t>(j) * static_cast<size_t>(bw) + static_cast<size_t>(i);
  };
  std::vector<int> sizes(1, 0);
  std::vector<std::pair<int, int>> stack;
  for (int j = 0; j < bh; j++)
    for (int i = 0; i < bw; i++) {
      if (!solid(i, j) || label[at(i, j)] != 0) continue;
      const int n = static_cast<int>(sizes.size());
      sizes.push_back(0);
      stack.assign(1, {i, j});
      label[at(i, j)] = n;
      while (!stack.empty()) {
        const auto [a, b] = stack.back();
        stack.pop_back();
        sizes[static_cast<size_t>(n)]++;
        const int next[4][2] = {{a + 1, b}, {a - 1, b}, {a, b + 1}, {a, b - 1}};
        for (const auto& [e, f] : next) {
          if (e < 0 || f < 0 || e >= bw || f >= bh || !solid(e, f)) continue;
          int& l = label[at(e, f)];
          if (l != 0) continue;
          l = n;
          stack.push_back({e, f});
        }
      }
    }
  for (int j = 0; j < bh; j++)
    for (int i = 0; i < bw; i++) {
      const int l = label[at(i, j)];
      if (l == 0 || sizes[static_cast<size_t>(l)] < kSpeck * d * d) continue;
      if (const int ci = i / d, cj = j / d; static_cast<float>(ci) < lean * static_cast<float>(cj))
        continue;
      const int tx = x + i, ty = y + j;
      if (tx < 0 || ty < 0 || tx >= dst.width || ty >= dst.height) continue;
      const size_t from = (static_cast<size_t>(by + j) * static_cast<size_t>(src.width) +
                           static_cast<size_t>(bx + i)) *
                          4;
      const size_t to =
          (static_cast<size_t>(ty) * static_cast<size_t>(dst.width) + static_cast<size_t>(tx)) * 4;
      std::copy_n(src.rgba.begin() + static_cast<std::ptrdiff_t>(from), 4,
                  dst.rgba.begin() + static_cast<std::ptrdiff_t>(to));
    }
}

// The letter cut from `art`'s strips and trimmed to its own columns; empty when a piece's
// picture is missing or too small.
ui::Image cut(const ClassicArt& art, const Letter& letter) {
  const int d = std::max(1, static_cast<int>(art.density));
  int wide = 0;
  for (const Piece& p : letter.pieces) wide = std::max(wide, p.dx + p.x1 - p.x0 + 1);
  ui::Image canvas;
  canvas.width = wide * d;
  canvas.height = static_cast<int>(kCarvedLine) * d;
  canvas.rgba.assign(static_cast<size_t>(canvas.width) * static_cast<size_t>(canvas.height) * 4, 0);
  for (const Piece& p : letter.pieces) {
    const ui::Image* src = art.find(p.picture);
    if (src == nullptr || (p.x1 + 1) * d > src->width || (p.y1 + 1) * d > src->height) return {};
    put_piece(*src, p.x0 * d, p.y0 * d, (p.x1 - p.x0 + 1) * d, (p.y1 - p.y0 + 1) * d, d, p.lean,
              canvas, p.dx * d, p.top * d);
  }
  // Trimmed to the columns the letter fills, in whole classic px.
  int left = canvas.width, right = -1;
  for (int y = 0; y < canvas.height; y++)
    for (int x = 0; x < canvas.width; x++)
      if (canvas.rgba[(static_cast<size_t>(y) * static_cast<size_t>(canvas.width) +
                       static_cast<size_t>(x)) *
                          4 +
                      3] > 0) {
        left = std::min(left, x);
        right = std::max(right, x);
      }
  if (right < 0) return {};
  left = left / d * d;
  right = (right / d + 1) * d;
  ui::Image out;
  out.width = right - left;
  out.height = canvas.height;
  out.rgba.resize(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4);
  for (int y = 0; y < out.height; y++)
    std::copy_n(
        canvas.rgba.begin() + static_cast<std::ptrdiff_t>(
                                  (static_cast<size_t>(y) * static_cast<size_t>(canvas.width) +
                                   static_cast<size_t>(left)) *
                                  4),
        static_cast<size_t>(out.width) * 4,
        out.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) *
                                                       static_cast<size_t>(out.width) * 4));
  return out;
}

// --- The letters made from the other size ---

// The lines (rows, or columns when `cols`) of `ink` (w by h) to double or drop: `k` of the lines lo
// to hi - 1, each the one most like the line after it, then the nearest the middle, never two
// side by side.
std::vector<int> alike_lines(const std::vector<uint8_t>& ink, int w, int h, bool cols, int lo,
                             int hi, int k) {
  struct Line {
    int cost;
    float off;
    int at;
  };
  const int len = cols ? h : w;
  const auto ink_at = [&](int line, int t) {
    return cols
               ? ink[(static_cast<size_t>(t) * static_cast<size_t>(w) + static_cast<size_t>(line))]
               : ink[(static_cast<size_t>(line) * static_cast<size_t>(w) + static_cast<size_t>(t))];
  };
  std::vector<Line> lines;
  for (int i = lo; i < hi; i++) {
    int cost = 0;
    for (int t = 0; t < len; t++) cost += ink_at(i, t) != ink_at(i + 1, t) ? 1 : 0;
    lines.push_back({cost, std::abs(static_cast<float>(i) - static_cast<float>(lo + hi) / 2), i});
  }
  std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) {
    if (a.cost != b.cost) return a.cost < b.cost;
    if (a.off != b.off) return a.off < b.off;
    return a.at < b.at;
  });
  std::vector<int> out;
  for (const Line& l : lines) {
    if (static_cast<int>(out.size()) == k) break;
    if (std::none_of(out.begin(), out.end(), [&](int o) { return std::abs(o - l.at) <= 1; }))
      out.push_back(l.at);
  }
  return out;
}

// `img` (density `d`) as its ink at classic px, w by h.
std::vector<uint8_t> ink_of(const ui::Image& img, int d, int* w, int* h) {
  *w = img.width / d;
  *h = img.height / d;
  std::vector<uint8_t> ink(static_cast<size_t>(*w) * static_cast<size_t>(*h));
  for (int y = 0; y < *h; y++)
    for (int x = 0; x < *w; x++)
      ink[(static_cast<size_t>(y) * static_cast<size_t>(*w) + static_cast<size_t>(x))] =
          img.rgba[((static_cast<size_t>(y) * static_cast<size_t>(d)) *
                        static_cast<size_t>(img.width) +
                    (static_cast<size_t>(x) * static_cast<size_t>(d))) *
                       4 +
                   3] != 0
              ? 1
              : 0;
  return ink;
}

// `img` grown into a capital or shrunk into a small capital (`kSized`): its body's rows from its
// first, set so that its foot lands on the other size's foot line (15 to 16, or 16 to 15).
ui::Image resize_letter(const ui::Image& img, int d, bool grow) {
  int w = 0, h = 0;
  const std::vector<uint8_t> ink = ink_of(img, d, &w, &h);
  int first = h, last = -1;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if (ink[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x))] != 0) {
        first = std::min(first, y);
        last = std::max(last, y);
      }
  if (last < 0) return {};
  const int foot = grow ? 15 : 16;
  const std::vector<int> rows = alike_lines(ink, w, h, false, first + 3, foot - 3, 3);
  const int k = grow ? (w * 15 + 50) / 100 : (w * 13 + 50) / 100;
  const std::vector<int> cols = alike_lines(ink, w, h, true, 3, w - 4, k);
  const auto has = [](const std::vector<int>& v, int i) {
    return std::find(v.begin(), v.end(), i) != v.end();
  };
  std::vector<int> ys, xs;
  for (int y = first; y <= last; y++) {
    if (!grow && has(rows, y)) continue;
    ys.push_back(y);
    if (grow && has(rows, y)) ys.push_back(y);
  }
  for (int x = 0; x < w; x++) {
    if (!grow && has(cols, x)) continue;
    xs.push_back(x);
    if (grow && has(cols, x)) xs.push_back(x);
  }
  const int top = grow ? first - 2 : first + 2;
  ui::Image out;
  out.width = static_cast<int>(xs.size()) * d;
  out.height = static_cast<int>(kCarvedLine) * d;
  out.rgba.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
  for (size_t j = 0; j < ys.size(); j++) {
    const int ty = top + static_cast<int>(j);
    if (ty < 0 || ty >= static_cast<int>(kCarvedLine)) continue;
    for (size_t i = 0; i < xs.size(); i++)
      for (int b = 0; b < d; b++)
        std::copy_n(
            img.rgba.begin() + static_cast<std::ptrdiff_t>((static_cast<size_t>(ys[j] * d + b) *
                                                                static_cast<size_t>(img.width) +
                                                            static_cast<size_t>(xs[i] * d)) *
                                                           4),
            static_cast<size_t>(d) * 4,
            out.rgba.begin() +
                static_cast<std::ptrdiff_t>(
                    ((static_cast<size_t>(ty) * static_cast<size_t>(d) + static_cast<size_t>(b)) *
                         static_cast<size_t>(out.width) +
                     i * static_cast<size_t>(d)) *
                    4));
  }
  return out;
}

// The F: the E down to its middle bar (rows to 12), the I's stem and foot under it (rows 13 on).
ui::Image make_f(const ui::Image& e, const ui::Image& i, int d) {
  ui::Image out = e;
  const size_t row = static_cast<size_t>(out.width) * 4;
  for (int y = 13 * d; y < out.height; y++) {
    std::fill_n(out.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) * row), row,
                0);
    const size_t n = static_cast<size_t>(std::min(out.width, i.width)) * 4;
    std::copy_n(i.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) *
                                                             static_cast<size_t>(i.width) * 4),
                n, out.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) * row));
  }
  return out;
}

// --- Letters pieced from others, a pack px at a time ---

// A clear letter `w` px wide on the carved line.
ui::Image blank(int w, int d) {
  ui::Image out;
  out.width = w * d;
  out.height = static_cast<int>(kCarvedLine) * d;
  out.rgba.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
  return out;
}

size_t offset(const ui::Image& img, int x, int y, int d) {
  return (static_cast<size_t>(y) * static_cast<size_t>(d) * static_cast<size_t>(img.width) +
          static_cast<size_t>(x) * static_cast<size_t>(d)) *
         4;
}

// Pack px (x, y) of `src` copied to (tx, ty) of `dst`, every row of its d by d block.
void copy_px(const ui::Image& src, int x, int y, ui::Image& dst, int tx, int ty, int d) {
  for (int b = 0; b < d; b++)
    std::copy_n(
        src.rgba.begin() +
            static_cast<std::ptrdiff_t>(
                offset(src, x, y, d) + static_cast<size_t>(b) * static_cast<size_t>(src.width) * 4),
        static_cast<size_t>(d) * 4,
        dst.rgba.begin() + static_cast<std::ptrdiff_t>(offset(dst, tx, ty, d) +
                                                       static_cast<size_t>(b) *
                                                           static_cast<size_t>(dst.width) * 4));
}

// `src`'s pack px laid over `dst` at (dx, dy), where `keep` says so; where both have ink the
// brighter wins, so a face crossing a rim stays face and the crossing reads as one surface.
template <typename Shift, typename Keep>
void lay(ui::Image& dst, const ui::Image& src, Shift dx, int dy, int d, Keep keep) {
  const int w = src.width / d, h = src.height / d, dw = dst.width / d, dh = dst.height / d;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t from = offset(src, x, y, d);
      if (src.rgba[from + 3] == 0 || !keep(x, y)) continue;
      const int tx = x + dx(y), ty = y + dy;
      if (tx < 0 || ty < 0 || tx >= dw || ty >= dh) continue;
      const size_t to = offset(dst, tx, ty, d);
      const int a = src.rgba[from] + src.rgba[from + 1] + src.rgba[from + 2];
      const int b = dst.rgba[to] + dst.rgba[to + 1] + dst.rgba[to + 2];
      if (dst.rgba[to + 3] == 0 || a > b) copy_px(src, x, y, dst, tx, ty, d);
    }
}
void lay_at(ui::Image& dst, const ui::Image& src, int dx, int dy, int d) {
  lay(dst, src, [dx](int) { return dx; }, dy, d, [](int, int) { return true; });
}

// A row's shift on a slant of `total` px from row y0 to row y1, rounded half away from zero.
int slant(int total, int y, int y0, int y1) {
  const int t = std::clamp(y, y0, y1) - y0, span = y1 - y0;
  const int s = (std::abs(total) * t * 2 + span) / (2 * span);
  return total < 0 ? std::abs(total) - s : s;
}

// The pack px rows `rows` of `src` stacked from row 0.
ui::Image stack_rows(const ui::Image& src, std::initializer_list<std::pair<int, int>> spans, int x0,
                     int x1, int d) {
  ui::Image out = blank(x1 - x0 + 1, d);
  int j = 0;
  for (const auto& [a, b] : spans)
    for (int y = a; y <= b; y++, j++)
      for (int x = x0; x <= x1; x++) copy_px(src, x, y, out, x - x0, j, d);
  return out;
}

// The letter trimmed to the columns it fills.
ui::Image trim(const ui::Image& img, int d) {
  const int w = img.width / d, h = img.height / d;
  int left = w, right = -1;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if (img.rgba[offset(img, x, y, d) + 3] != 0) {
        left = std::min(left, x);
        right = std::max(right, x);
      }
  if (right < 0) return {};
  ui::Image out = blank(right - left + 1, d);
  for (int y = 0; y < h; y++)
    for (int x = left; x <= right; x++) copy_px(img, x, y, out, x - left, y, d);
  return out;
}

// The B: the I's stem, and beside it twice the right half of the O cut down to its curves (its
// top four rows and its foot five), one bowl on rows 1 to 9 and one on 8 to 16.
ui::Image make_b(const ui::Image& i, const ui::Image& o, int d) {
  const ui::Image bowl = stack_rows(o, {{1, 4}, {12, 16}}, 11, o.width / d - 1, d);
  ui::Image out = blank(i.width / d + bowl.width / d, d);
  lay_at(out, i, 0, 0, d);
  lay_at(out, bowl, 6, 1, d);
  lay_at(out, bowl, 6, 8, d);
  return trim(out, d);
}

// The X: two I's leaned 11 px each way between rows 1 and 16, crossed; their feet stay level on the
// lines.
ui::Image make_x(const ui::Image& i, int d) {
  ui::Image out = blank(i.width / d + 11, d);
  const auto all = [](int, int) { return true; };
  lay(out, i, [](int y) { return slant(11, y, 1, 16); }, 0, d, all);
  lay(out, i, [](int y) { return slant(-11, y, 1, 16); }, 0, d, all);
  return trim(out, d);
}

// The Z: the I's stem (rows 3 to 14) leaned from the top right to the foot left, under the E's top
// bar (rows 1 to 3) and its foot (rows 14 to 16).
ui::Image make_z(const ui::Image& i, const ui::Image& e, int d) {
  const int lean = e.width / d - i.width / d;
  ui::Image out = blank(e.width / d, d);
  lay(
      out, i, [lean](int y) { return slant(-lean, y, 3, 14); }, 0, d,
      [](int, int y) { return y >= 3 && y <= 14; });
  lay(out, e, [](int) { return 0; }, 0, d, [](int, int y) { return y <= 3 || y >= 14; });
  return trim(out, d);
}

}  // namespace

void cut_letters(ClassicArt* art) {
  art->letters.clear();
  if (art->strip_print != kStripPrint) return;
  for (const Letter& l : kLetters)
    if (ui::Image img = cut(*art, l); !img.empty()) art->letters.emplace(l.c, std::move(img));
  if (art->letters.empty()) return;
  const int d = std::max(1, static_cast<int>(art->density));
  const auto make = [&](char c, ui::Image img) {
    if (!img.empty() && art->letters.count(c) == 0) art->letters.emplace(c, std::move(img));
  };
  // The letters held in one size, made in the other; the F from the E and the I, the f from the F.
  for (const Sized& s : kSized)
    if (const auto it = art->letters.find(s.from); it != art->letters.end())
      make(s.c, resize_letter(it->second, d, s.grow));
  if (art->letters.count('E') != 0 && art->letters.count('I') != 0)
    make('F', make_f(art->letters.at('E'), art->letters.at('I'), d));
  if (const auto it = art->letters.find('F'); it != art->letters.end())
    make('f', resize_letter(it->second, d, false));
  // The three in neither size, pieced from others, and their small capitals.
  const auto has = [&](char c) { return art->letters.count(c) != 0; };
  if (has('I') && has('O')) make('B', make_b(art->letters.at('I'), art->letters.at('O'), d));
  if (has('I')) make('X', make_x(art->letters.at('I'), d));
  if (has('I') && has('E')) make('Z', make_z(art->letters.at('I'), art->letters.at('E'), d));
  for (const char c : {'B', 'X', 'Z'})
    if (const auto it = art->letters.find(c); it != art->letters.end())
      make(static_cast<char>(c - 'A' + 'a'), resize_letter(it->second, d, false));
}

float carved_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs) {
  if (xs != nullptr) xs->assign(text.size(), 0);
  float x = 0;
  bool space = false, first = true;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == ' ') {
      space = true;
      continue;
    }
    const auto it = art.letters.find(text[i]);
    if (it == art.letters.end()) return 0;
    if (!first) x += space ? (round_left(text[i]) ? kSpaceRound : kSpaceStem) : kLetterGap;
    if (xs != nullptr) (*xs)[i] = x;
    x += art.width_of(it->second);
    space = false;
    first = false;
  }
  return x;
}

float carved_text_width(const ClassicArt& art, std::string_view text) {
  return carved_layout(art, text, nullptr);
}

}  // namespace shooters_edge::menu
