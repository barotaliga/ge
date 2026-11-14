// The sequel's plate letters cut one cell each from its main menu's tags (plate_letters.hpp).
//
// ge:summary Cuts, pieces and shrinks the sequel's tag letters to one surface, and lays words
// ge:keywords menu sequel plates tag carved-letters cells pieced-letters small-capitals shading
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/plate_letters.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr std::string_view kGame = "m_main_game", kQuit = "m_main_quit", kVideo = "m_main_video",
                           kOptions = "m_main_options", kMulti = "m_main_multiplayer";
constexpr std::array<std::string_view, 5> kPlates = {kGame, kQuit, kVideo, kOptions, kMulti};
constexpr int kLine = static_cast<int>(kPlateLine);

// The w x h classic px at (x, y) of `plate`, set at (dx, dy) of the cell.
struct Bit {
  std::string_view plate;
  int x = 0, y = 0, w = 0, h = 0;
  int dx = 0, dy = 0;
};
Bit cell(std::string_view plate, int x, int w) { return {plate, x, 0, w, kLine, 0, 0}; }

// A cell `w` px wide: plain tag, the bits laid over it in order.
struct Recipe {
  char c;
  int w;
  std::vector<Bit> bits;
};

// Every capital, both ends and the space (the header says where each comes from).
std::vector<Recipe> recipes() {
  const Bit a = cell(kMulti, 137, 17), d = cell(kVideo, 53, 16), e = cell(kMulti, 171, 16),
            i = cell(kMulti, 86, 17), l = cell(kMulti, 120, 17), m = cell(kMulti, 19, 17),
            n = cell(kOptions, 103, 17), o = cell(kOptions, 19, 17), p = cell(kOptions, 36, 17),
            q = cell(kQuit, 19, 17), r = cell(kMulti, 187, 17), s = cell(kOptions, 120, 17),
            t = cell(kMulti, 69, 17), u = cell(kMulti, 36, 17), v = cell(kVideo, 19, 17),
            y = cell(kMulti, 154, 17);
  // The rows a..b (inclusive) of a cell, set on the same rows.
  const auto rows = [](Bit b, int from, int to) {
    b.h = to - from + 1;
    b.y = from;
    b.dy = from;
    return b;
  };
  std::vector<Recipe> out = {
      {'[', 19, {{kGame, 0, 0, 19, kLine, 0, 0}}},  // the hole
      {']', 13, {{kGame, 86, 0, 13, kLine, 0, 0}}},
      {' ', 6, {}},
      {'A', 17, {a}},
      {'D', 16, {d}},
      {'E', 16, {e}},
      // GAME's letters stand a row lower than every other plate's: its G lifted a row.
      {'G',
       17,
       {{kGame, 19, 0, 17, 3, 0, 0}, {kGame, 19, 4, 17, 25, 0, 3}, {kGame, 19, 28, 17, 4, 0, 28}}},
      {'I', 17, {i}},
      {'L', 17, {l}},
      {'M', 17, {m}},
      {'N', 17, {n}},
      {'O', 17, {o}},
      {'P', 17, {p}},
      {'Q', 17, {q}},
      {'R', 17, {r}},
      {'S', 17, {s}},
      {'T', 17, {t}},
      {'U', 17, {u}},
      {'V', 17, {v}},
      {'Y', 17, {y}},
      // B: P's stem and bowl over D's foot and lower curve.
      {'B', 17, {rows(p, 0, 14), rows(d, 15, kLine - 1)}},
      // C: an O with its right side hidden under two slips of the recess's plain ground.
      {'C', 17, {o, {kOptions, 24, 10, 6, 9, 11, 11}, {kOptions, 24, 10, 6, 9, 15, 11}}},
      // F: E's bars over P's bare stem.
      {'F', 17, {rows(e, 0, 15), rows(p, 16, kLine - 1)}},
      // H: an N with its diagonal hidden and a slip of T's bar across.
      {'H',
       17,
       {n,
        {kOptions, 24, 10, 6, 12, 4, 8},
        {kOptions, 24, 10, 4, 12, 7, 9},
        {kMulti, 75, 7, 8, 2, 4, 13}}},
      // J: U's top, then its right stem and curve slid left over plain tag (no left stem).
      {'J', 17, {rows(u, 0, 5), {kMulti, 36 + 7, 6, 10, kLine - 6, 7, 6}}},
      // K: an R with Y's right arm over its bowl.
      {'K', 17, {r, {kMulti, 160, 4, 12, 11, 5, 4}}},
      // X: Y's arms over A's legs.
      {'X', 17, {rows(y, 0, 17), rows(a, 18, kLine - 1)}},
  };
  // W: two V's narrowed to every other column, side by side, the second's first column dropped
  // where they meet.
  Recipe w{'W', 17, {}};
  for (int k = 0; k < 9; k++) w.bits.push_back({kVideo, 19 + 2 * k, 5, 1, 19, k, 5});
  for (int k = 1; k < 9; k++) w.bits.push_back({kVideo, 19 + 2 * k, 5, 1, 19, 8 + k, 5});
  out.push_back(std::move(w));
  // Z: T's bar, L's foot, and between them A's left leg leaned the other way a row at a time.
  Recipe z{'Z', 17, {rows(t, 0, 8), rows(l, 19, kLine - 1)}};
  constexpr int kLegRow[] = {9, 10, 11, 11, 12, 13, 13, 14, 15, 15};
  constexpr int kLegShift[] = {0, -1, -1, -2, -3, -3, -4, -5, -5, -6};
  for (int k = 0; k < 10; k++)
    z.bits.push_back({kMulti, 137, kLegRow[k], 8, 1, 6 + kLegShift[k], 9 + k});
  out.push_back(std::move(z));
  return out;
}

// One set of tags (the plain or the lit), each plate's surface shaded to the darkest one's.
struct Sheet {
  const ClassicArt* art = nullptr;
  std::string_view suffix;  // "" or "_sel"
  int d = 1;
  std::map<std::string_view, float> shade;

  const ui::Image* plate(std::string_view name) const {
    return art->find(std::string(name) + std::string(suffix));
  }
};

// The mean brightness of a plate's surface: its rows 2 to 5 above the letters' recess, x 20 to 80
// (classic px); 0 when the plate is missing.
float surface(const ui::Image* img, int d) {
  if (img == nullptr) return 0;
  const int x1 = std::min(img->width, 80 * d), y1 = std::min(img->height, 6 * d);
  double sum = 0;
  int count = 0;
  for (int y = 2 * d; y < y1; y++)
    for (int x = 20 * d; x < x1; x++) {
      const size_t at =
          (static_cast<size_t>(y) * static_cast<size_t>(img->width) + static_cast<size_t>(x)) * 4;
      sum += 0.3 * img->rgba[at] + 0.6 * img->rgba[at + 1] + 0.1 * img->rgba[at + 2];
      count++;
    }
  return count > 0 ? static_cast<float>(sum / count) : 0;
}

// The sheet of `suffix`'s plates; empty shades when any plate is missing.
Sheet sheet_of(const ClassicArt& art, std::string_view suffix) {
  Sheet s;
  s.art = &art;
  s.suffix = suffix;
  s.d = std::max(1, static_cast<int>(art.density));
  float darkest = 0;
  std::map<std::string_view, float> surfaces;
  for (const std::string_view name : kPlates) {
    const float f = surface(s.plate(name), s.d);
    if (f <= 0) return s;
    surfaces[name] = f;
    if (darkest == 0 || f < darkest) darkest = f;
  }
  for (const auto& [name, f] : surfaces) s.shade[name] = darkest / f;
  return s;
}

ui::Image blank(int w, int d) {
  ui::Image out;
  out.width = w * d;
  out.height = kLine * d;
  out.rgba.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
  return out;
}

size_t offset(const ui::Image& img, int x, int y) {
  return (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 4;
}

// Classic px (x, y) of `src` to (tx, ty) of `dst`, every pixel of its d x d block, its colour
// times `k`.
void put(const ui::Image& src, int x, int y, float k, ui::Image& dst, int tx, int ty, int d) {
  for (int b = 0; b < d; b++)
    for (int a = 0; a < d; a++) {
      const size_t from = offset(src, x * d + a, y * d + b),
                   to = offset(dst, tx * d + a, ty * d + b);
      for (int c = 0; c < 3; c++)
        dst.rgba[to + static_cast<size_t>(c)] = static_cast<uint8_t>(std::min(
            255.0f, static_cast<float>(src.rgba[from + static_cast<size_t>(c)]) * k + 0.5f));
      dst.rgba[to + 3] = src.rgba[from + 3];
    }
}

// The plain tag: the bare column after OPTIONS' S, flat from rim to rim, tiled.
constexpr int kPlainX = 136;

// A cell `w` px wide of plain tag with `bits` over it; empty when a bit's plate is missing or too
// small.
ui::Image build(const Sheet& s, int w, const std::vector<Bit>& bits) {
  const int d = s.d;
  ui::Image out = blank(w, d);
  const ui::Image* plain = s.plate(kOptions);
  if (plain == nullptr || plain->width < (kPlainX + 1) * d || plain->height < kLine * d) return {};
  for (int y = 0; y < kLine; y++)
    for (int x = 0; x < w; x++) put(*plain, kPlainX, y, s.shade.at(kOptions), out, x, y, d);
  for (const Bit& b : bits) {
    const ui::Image* src = s.plate(b.plate);
    if (src == nullptr || (b.x + b.w) * d > src->width || (b.y + b.h) * d > src->height) return {};
    const float k = s.shade.at(b.plate);
    for (int j = 0; j < b.h; j++)
      for (int i = 0; i < b.w; i++) {
        const int tx = b.dx + i, ty = b.dy + j;
        if (tx >= 0 && ty >= 0 && tx < w && ty < kLine)
          put(*src, b.x + i, b.y + j, k, out, tx, ty, d);
      }
  }
  return out;
}

// --- The small letters ---

// The rows and columns a capital drops to become its small letter.
struct Shrink {
  std::vector<int> rows, cols;
};

// The capital's brightness at classic px (x, y): 3R + 6G + B summed over its block.
int bright(const ui::Image& img, int x, int y, int d) {
  int sum = 0;
  for (int b = 0; b < d; b++)
    for (int a = 0; a < d; a++) {
      const size_t at = offset(img, x * d + a, y * d + b);
      sum += 3 * img.rgba[at] + 6 * img.rgba[at + 1] + img.rgba[at + 2];
    }
  return sum;
}

// The letter's band (rows 6 to 22) loses the four of rows 9 to 18 most like the row under them, and
// the three of columns 3 to w - 5 most like the column after them, never two columns side by side;
// a tie goes to the first.
constexpr int kBandTop = 6, kBandEnd = 23, kDropRows = 4, kDropCols = 3;
Shrink shrink_lines(const ui::Image& cap, int d) {
  const int w = cap.width / d;
  std::vector<std::pair<int, int>> rows, cols;
  for (int y = 9; y <= 18; y++) {
    int cost = 0;
    for (int x = 0; x < w; x++) cost += std::abs(bright(cap, x, y, d) - bright(cap, x, y + 1, d));
    rows.emplace_back(cost, y);
  }
  for (int x = 3; x < w - 4; x++) {
    int cost = 0;
    for (int y = kBandTop; y < kBandEnd; y++)
      cost += std::abs(bright(cap, x, y, d) - bright(cap, x + 1, y, d));
    cols.emplace_back(cost, x);
  }
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

// The small letter: the capital's kept columns, its rims above and below the band as they are, its
// band's kept rows set down onto the foot and plain tag (`plain`, as wide) above them.
ui::Image shrink(const ui::Image& cap, const ui::Image& plain, const Shrink& sh, int d) {
  const auto has = [](const std::vector<int>& v, int i) {
    return std::find(v.begin(), v.end(), i) != v.end();
  };
  std::vector<int> xs, ys;
  for (int x = 0; x < cap.width / d; x++)
    if (!has(sh.cols, x)) xs.push_back(x);
  for (int y = kBandTop; y < kBandEnd; y++)
    if (!has(sh.rows, y)) ys.push_back(y);
  const int w = static_cast<int>(xs.size()), top = kBandEnd - static_cast<int>(ys.size());
  if (plain.width != w * d) return {};
  ui::Image out = plain;
  for (int y = 0; y < kLine; y++) {
    int from = y;
    if (y >= kBandTop && y < kBandEnd) {
      if (y < top) continue;
      from = ys[static_cast<size_t>(y - top)];
    }
    for (int i = 0; i < w; i++) put(cap, xs[static_cast<size_t>(i)], from, 1, out, i, y, d);
  }
  return out;
}

}  // namespace

void cut_plate_letters(ClassicArt* art) {
  art->letters.clear();
  art->letters_lit.clear();
  if (art->strip_print != kPlatePrint) return;
  const Sheet plain = sheet_of(*art, ""), lit = sheet_of(*art, "_sel");
  if (plain.shade.empty()) return;
  const int d = plain.d;
  for (const Recipe& r : recipes()) {
    ui::Image cap = build(plain, r.w, r.bits);
    if (cap.empty()) continue;
    ui::Image cap_lit = lit.shade.empty() ? ui::Image{} : build(lit, r.w, r.bits);
    if (r.c >= 'A' && r.c <= 'Z') {
      const char small = static_cast<char>(r.c - 'A' + 'a');
      const Shrink sh = shrink_lines(cap, d);
      const int w = r.w - static_cast<int>(sh.cols.size());
      if (ui::Image s = shrink(cap, build(plain, w, {}), sh, d); !s.empty()) {
        art->letters.emplace(small, std::move(s));
        if (!cap_lit.empty())
          if (ui::Image sl = shrink(cap_lit, build(lit, w, {}), sh, d); !sl.empty())
            art->letters_lit.emplace(small, std::move(sl));
      }
    }
    art->letters.emplace(r.c, std::move(cap));
    if (!cap_lit.empty()) art->letters_lit.emplace(r.c, std::move(cap_lit));
  }
}

float plate_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs) {
  if (xs != nullptr) xs->assign(text.size() + 2, 0);
  float x = 0;
  for (size_t i = 0; i < text.size() + 2; i++) {
    const char c = i == 0 ? '[' : i == text.size() + 1 ? ']' : text[i - 1];
    const auto it = art.letters.find(c);
    if (it == art.letters.end()) return 0;
    if (xs != nullptr) (*xs)[i] = x;
    x += art.width_of(it->second);
  }
  return x;
}

float banner_layout(const ClassicArt& art, std::string_view text, std::vector<float>* xs) {
  if (xs != nullptr) xs->assign(text.size(), 0);
  const ui::Image* frame = art.find(kBannerFrame);
  if (frame == nullptr || art.width_of(*frame) <= kBannerRight) return 0;
  float word = 0;
  for (size_t i = 0; i < text.size(); i++) {
    const auto it = art.letters.find(text[i]);
    if (it == art.letters.end()) return 0;
    if (xs != nullptr) (*xs)[i] = word;
    word += art.width_of(it->second);
  }
  const float plain = std::max(kBannerRight - kBannerLeft, word + 8);
  const float start = kBannerLeft + static_cast<float>(static_cast<int>((plain - word) / 2));
  if (xs != nullptr)
    for (float& x : *xs) x += start;
  return kBannerLeft + plain + art.width_of(*frame) - kBannerRight;
}

float banner_shade(const ClassicArt& art) {
  const ui::Image* frame = art.find(kBannerFrame);
  const auto it = art.letters.find('I');
  if (frame == nullptr || it == art.letters.end()) return 1;
  const int d = std::max(1, static_cast<int>(art.density));
  // The frame's plain left of the title, and the letter cell's rows above its letter.
  const auto mean = [d](const ui::Image& img, int x0, int x1) {
    double sum = 0;
    int n = 0;
    for (int y = 2 * d; y < 6 * d && y < img.height; y++)
      for (int x = x0 * d; x < x1 * d && x < img.width; x++) {
        const size_t at = offset(img, x, y);
        sum += 0.3 * img.rgba[at] + 0.6 * img.rgba[at + 1] + 0.1 * img.rgba[at + 2];
        n++;
      }
    return n > 0 ? sum / n : 0.0;
  };
  const double f = mean(*frame, static_cast<int>(kBannerLeft), 100);
  const double l = mean(it->second, 0, it->second.width / d);
  return f > 0 && l > 0 ? static_cast<float>(std::min(1.0, l / f)) : 1;
}

}  // namespace shooters_edge::menu
