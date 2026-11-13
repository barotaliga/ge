// The menu's carved letters cut one by one from the pack's strips (carved_letters.hpp).
//
// ge:summary Cuts the carved letters out of the strips by their own pixels, and measures words
// ge:keywords menu classic carved-letters letter-pictures strips speck connected-parts
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/carved_letters.hpp"

#include <algorithm>
#include <cstdint>
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
// from it is set a row lower than its place in the band.
struct Piece {
  std::string_view picture;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int dx = 0, top = 0;
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

constexpr Letter kLetters[] = {{'Q', kQ}, {'u', kU},  {'i', kI}, {'a', kA}, {'e', kE},
                               {'k', kK}, {'S', kS},  {'l', kL}, {'c', kC}, {'t', kT},
                               {'o', kO}, {'s', kSs}, {'G', kG}, {'m', kM}, {'D', kD}};

// Copies the parts of `src`'s box that are no speck onto `dst` at (x, y), px.
void put_piece(const ui::Image& src, int bx, int by, int bw, int bh, int d, ui::Image& dst, int x,
               int y) {
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
    put_piece(*src, p.x0 * d, p.y0 * d, (p.x1 - p.x0 + 1) * d, (p.y1 - p.y0 + 1) * d, d, canvas,
              p.dx * d, p.top * d);
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

}  // namespace

void cut_letters(ClassicArt* art) {
  art->letters.clear();
  if (art->strip_print != kStripPrint) return;
  for (const Letter& l : kLetters)
    if (ui::Image img = cut(*art, l); !img.empty()) art->letters.emplace(l.c, std::move(img));
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
