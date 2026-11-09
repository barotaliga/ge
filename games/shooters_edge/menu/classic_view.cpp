// The main menu laid out and drawn in the classic look (classic_view.hpp).
//
// ge:summary Lays out and draws the main menu on the classic 320x200 screen in the pack's pictures
// ge:keywords menu classic layout draw strip glyphs cursor slider text-box help dim carved-letters
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/classic_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr int kScreenW = 320, kScreenH = 200;
constexpr float kRowTop = 32;
constexpr float kLabelEnd = 192, kCursorX = 200, kValueX = 220;
constexpr size_t kLabelChars = 22, kValueChars = 12;
constexpr int kSliderCells = 10;

constexpr float kBand = 20;  // a strip's row, classic px

// The rows each strip's picture has words for, top to bottom.
constexpr std::string_view kMainRows[] = {"single", "multi", "options", "help", "quit"};
constexpr std::string_view kSingleRows[] = {"new", "load", "save"};
constexpr std::string_view kMultiRows[] = {"join", "host", "setup"};

struct PageArt {
  std::string_view page, title, strip;
  std::span<const std::string_view> rows = {};
};
constexpr PageArt kPageArt[] = {
    {"main", "ttl_main", "mainmenu", kMainRows},
    {"single", "ttl_sgl", "sp_menu", kSingleRows},
    {"multi", "p_multi", "mp_menu", kMultiRows},
    {"host", "p_multi", ""},
    {"setup", "p_multi", ""},
    {"options", "p_option", ""},
    {"video", "p_option", ""},
};

// The band of `pa`'s strip that has the words of row `id`, or -1.
int band_of(const PageArt& pa, std::string_view id) {
  for (size_t k = 0; k < pa.rows.size(); k++)
    if (pa.rows[k] == id) return static_cast<int>(k);
  return -1;
}

const PageArt* art_of(std::string_view page) {
  for (const PageArt& a : kPageArt)
    if (a.page == page) return &a;
  return nullptr;
}

// A strip row the pack's pictures have no words for, cut letter by letter out of the words they do
// have (the menu's carved capitals and small capitals), measured on the strips whose print
// (`ClassicArt::strip_print`) the word names. A piece: the `w` x `h` classic px at (x, y) of
// `picture`, drawn at (dx, dy) from the row's top left. Every piece takes a letter's whole outline
// and nothing of its neighbours', on the strips' common baseline.
struct CarvedPiece {
  std::string_view picture;
  int x = 0, y = 0, w = 0, h = 0;
  int dx = 0, dy = 0;
};
struct CarvedWord {
  std::string_view row;
  uint64_t print = 0;
  std::span<const CarvedPiece> pieces;
};
// DEMOS: the capital D a capital L's stem and the right half of a capital O (the way the small d
// is a stem and a bowl), then the small capitals e, m, o and s.
constexpr CarvedPiece kDemosPieces[] = {
    {"sp_menu", 2, 20, 8, 20, 0, 0},       // the stem of Load's L
    {"mainmenu", 11, 40, 11, 20, 8, 0},    // the right half of Options' O ...
    {"mainmenu", 22, 47, 1, 4, 19, 7},     // ... and its outline's widest column, beside the P
    {"mainmenu", 153, 60, 17, 20, 20, 0},  // Ordering's e
    {"sp_menu", 107, 0, 21, 20, 38, 1},    // Game's m (its strip sits a pixel higher)
    {"mainmenu", 67, 40, 21, 20, 60, 0},   // Options' o
    {"mainmenu", 106, 40, 17, 20, 82, 0},  // Options' s
};
constexpr CarvedWord kCarvedWords[] = {{"demos", 0xb378b8eec94a7f36ULL, kDemosPieces}};

// Row `id`'s word cut from the strips at (x, y), classic px; false when these strips are not the
// ones it was measured on (the caller draws the glyphs instead).
bool carve(const ClassicPen& pen, std::string_view id, float x, float y) {
  const ClassicArt& art = pen.art;
  for (const CarvedWord& w : kCarvedWords) {
    if (w.row != id || w.print != art.strip_print || art.strip_print == 0) continue;
    for (const CarvedPiece& p : w.pieces) {
      const ui::Image* img = art.find(p.picture);
      if (img == nullptr) return false;
      const float d = static_cast<float>(img->height) / art.height_of(*img);  // px a classic px
      ui::Box b;
      b.rect = pen.at(x + static_cast<float>(p.dx), y + static_cast<float>(p.dy),
                      static_cast<float>(p.w), static_cast<float>(p.h));
      b.image = img;
      b.uv = {static_cast<float>(p.x) * d, static_cast<float>(p.y) * d, static_cast<float>(p.w) * d,
              static_cast<float>(p.h) * d};
      b.nearest = art.density <= 1;
      b.feather = 0;
      pen.dl.box(b);
    }
    return true;
  }
  return false;
}

bool drawn(const ui::Item& it) { return !(it.kind == ui::ItemKind::back && it.disabled); }

// "MOUSE SPEED" as the classic menu wrote it: "Mouse speed".
std::string sentence(std::string_view s) {
  std::string out(s);
  bool first = true;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z' && !first) c = static_cast<char>(c - 'A' + 'a');
    if (c != ' ') first = false;
  }
  return out;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

std::string value_of(const ui::Item& it) {
  switch (it.kind) {
    case ui::ItemKind::toggle:
      return it.on ? "on" : "off";
    case ui::ItemKind::choice:
      return it.choices.empty() ? std::string() : lower(it.choices[static_cast<size_t>(it.choice)]);
    default:
      return {};
  }
}

}  // namespace

int help_pages(const ClassicArt& art) {
  int n = 0;
  while (art.find("help" + std::to_string(n)) != nullptr) n++;
  return n;
}

ClassicLayout classic_screen(int width, int height) {
  ClassicLayout l;
  l.scale = std::max(1.0f, std::floor(std::min(static_cast<float>(width) / kScreenW,
                                               static_cast<float>(height) / kScreenH)));
  l.x = std::floor((static_cast<float>(width) - kScreenW * l.scale) / 2);
  l.y = std::floor((static_cast<float>(height) - kScreenH * l.scale) / 2);
  return l;
}

ClassicLayout layout_classic(const ui::Menu& menu, const ClassicArt& art, int width, int height) {
  ClassicLayout l = classic_screen(width, height);
  const ui::Page& page = menu.top();
  const PageArt* pa = art_of(page.id);
  const ui::Image* strip = pa != nullptr && !pa->strip.empty() ? art.find(pa->strip) : nullptr;
  l.strip = strip != nullptr;
  const auto rect = [&](float x, float y, float w, float h) {
    return ui::Rect{l.x + x * l.scale, l.y + y * l.scale, w * l.scale, h * l.scale};
  };
  l.items.assign(page.items.size(), {});
  l.bands.assign(page.items.size(), -1);
  const bool help = page.id == "help" && help_pages(art) > 0;
  const bool quit = page.id == "quit";
  float row = quit ? 84 : kRowTop;
  int strip_row = 0;
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    if (!drawn(it) || help) continue;
    if (l.strip) {
      if (it.kind == ui::ItemKind::back) continue;
      l.bands[i] = band_of(*pa, it.id);
      l.items[i] =
          rect(72, kRowTop + kBand * static_cast<float>(strip_row++), art.width_of(*strip), kBand);
      continue;
    }
    l.items[i] = rect(16, row, kScreenW - 32, 8);
    row += 8;
  }
  return l;
}

std::vector<std::string> classic_problems(const ui::Menu& menu, const ClassicLayout& layout,
                                          int width, int height) {
  std::vector<std::string> out;
  const ui::Page& page = menu.top();
  const ui::Rect screen{0, 0, static_cast<float>(width), static_cast<float>(height)};
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    const ui::Rect& r = layout.items[i];
    if (r.w <= 0) continue;
    if (r.x < screen.x || r.y < screen.y || r.right() > screen.right() ||
        r.bottom() > screen.bottom())
      out.push_back("page `" + page.id + "`, row `" + it.id + "` is off the screen");
    if (layout.strip) continue;
    const size_t cap = it.kind == ui::ItemKind::label ? 40 : kLabelChars;
    if (it.text.size() > cap)
      out.push_back("page `" + page.id + "`, row `" + it.id + "`: the label \"" + it.text +
                    "\" is wider than its " + std::to_string(cap) + " glyphs; shorten it");
    if (value_of(it).size() > kValueChars)
      out.push_back("page `" + page.id + "`, row `" + it.id + "`: the value \"" + value_of(it) +
                    "\" is wider than its 12 glyphs; shorten its choice");
  }
  return out;
}

void draw_classic(ui::DrawList& dl, const ui::Menu& menu, const ClassicArt& art,
                  const ClassicLayout& l, double t, int help_page) {
  const ClassicPen pen{dl, art, l};
  dl.rect({0, 0, static_cast<float>(dl.width()), static_cast<float>(dl.height())},
          ui::hex(0x000000, 0.6f));
  const ui::Page& page = menu.top();
  const int helps = help_pages(art);
  if (page.id == "help" && helps > 0) {
    const int n = ((help_page % helps) + helps) % helps;
    pen.picture(art.find("help" + std::to_string(n)), 0, 0);
    return;
  }
  pen.picture(art.find("qplaque"), 16, 4);
  const PageArt* pa = art_of(page.id);
  const ui::Image* title = pa != nullptr ? art.find(pa->title) : nullptr;
  if (page.id == "quit") {
    // The question in its box, no title.
  } else if (title != nullptr) {
    pen.picture(title, std::floor((kScreenW - art.width_of(*title)) / 2), 4);
  } else if (!page.title.empty()) {
    const std::string s = sentence(page.title);
    pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), 12, true);
  }
  const auto local_y = [&](const ui::Rect& r) { return (r.y - l.y) / l.scale; };

  if (l.strip) {
    // Each row its own band of the picture, so a row the picture has no words for (cut from its
    // letters, or in big glyphs) sits between them in the game's order.
    const ui::Image* strip = art.find(pa->strip);
    const float d = static_cast<float>(strip->height) / art.height_of(*strip);  // px a classic px
    for (size_t i = 0; i < page.items.size(); i++) {
      const ui::Rect& r = l.items[i];
      if (r.w <= 0) continue;
      const int band = l.bands[i];
      if (band >= 0) {
        // The last band runs to the picture's foot (its shadow).
        const bool last = band + 1 == static_cast<int>(pa->rows.size());
        const float from = kBand * static_cast<float>(band);
        const float tall = last ? std::max(kBand, art.height_of(*strip) - from) : kBand;
        ui::Box b;
        b.rect = {r.x, r.y, r.w, tall * l.scale};
        b.image = strip;
        b.uv = {0, from * d, static_cast<float>(strip->width), tall * d};
        b.nearest = art.density <= 1;
        b.feather = 0;
        dl.box(b);
      } else if (!carve(pen, page.items[i].id, 74, local_y(r))) {
        pen.big_text(page.items[i].text, 74, local_y(r) + 2);
      }
      if (page.items[i].disabled) {
        ui::Box b;
        b.rect = r;
        b.fill = ui::solid(ui::hex(0x000000, 0.68f));
        b.feather = 0;
        dl.box(b);
      }
      if (static_cast<int>(i) == page.focus) {
        const int frame = static_cast<int>(std::floor(t * 10)) % 6;
        pen.picture(art.find("menudot" + std::to_string(frame + 1)), 54, local_y(r));
      }
    }
    return;
  }

  if (page.id == "quit") pen.box(56, 68, 24, 5);
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    const ui::Rect& r = l.items[i];
    if (r.w <= 0) continue;
    const float y = local_y(r);
    const float alpha = it.disabled ? 0.3f : 1.0f;
    if (it.kind == ui::ItemKind::label) {
      const std::string s = sentence(it.text);
      pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), y, false);
      continue;
    }
    std::string label = sentence(it.text);
    if (label.size() > kLabelChars) label.resize(kLabelChars);
    if (page.id == "quit") {
      // The answers centred in the box, the cursor before the focused one.
      const float x = std::floor((kScreenW - 8 * static_cast<float>(label.size())) / 2);
      pen.text(label, x, y, true, alpha);
      if (static_cast<int>(i) == page.focus)
        pen.glyph(static_cast<uint8_t>(12 + (static_cast<int>(std::floor(t * 4)) & 1)), x - 16, y);
      continue;
    }
    pen.text(label, kLabelEnd - 8 * static_cast<float>(label.size()), y, true, alpha);
    if (it.kind == ui::ItemKind::slider) {
      const double span = it.max - it.min;
      const double f = span > 0 ? std::clamp((it.value - it.min) / span, 0.0, 1.0) : 0;
      pen.glyph(128, kValueX, y, alpha);
      for (int c = 0; c < kSliderCells; c++)
        pen.glyph(129, kValueX + 8 * static_cast<float>(c + 1), y, alpha);
      pen.glyph(130, kValueX + 8 * (kSliderCells + 1), y, alpha);
      pen.glyph(131, kValueX + 8 + static_cast<float>(std::lround(f * (kSliderCells - 1) * 8)), y,
                alpha);
    } else {
      std::string v = value_of(it);
      if (v.size() > kValueChars) v.resize(kValueChars);
      pen.text(v, kValueX, y, false, alpha);
    }
    if (static_cast<int>(i) == page.focus)
      pen.glyph(static_cast<uint8_t>(12 + (static_cast<int>(std::floor(t * 4)) & 1)), kCursorX, y);
  }
}

}  // namespace shooters_edge::menu
