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
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "shooters_edge/menu/carved_letters.hpp"

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
    {"ranked", "p_multi", ""},
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
// THE SEQUEL'S PLATES (its main menu's words, each on a tag 32 px tall): a word a plate of its own,
// the left end (the hole) of GAME's plate, a letter a cell of the plates' even 17 px pitch (16 or
// 17 classic px, every cell the plate's whole height so its edges run on), then GAME's right end.
// A space is three slips of plain plate 2 px wide (the bare margin left of QUIT's I),
// the letters' own margins beside it. The plates' surfaces differ in brightness by a tenth (QUIT's
// is the darkest), so every piece of a word is darkened to the word's darkest surface
// (`plate_surface`) and no cell shows its edge. Letters no plate has: the C an O with its right
// side hidden under the recess's plain ground, the H an N with its diagonal hidden and a slip of
// T's bar across, the K an R with Y's right arm over its bowl.
constexpr uint64_t kPlatePrint = 0xfd50f20825e19f40ULL;
constexpr CarvedPiece kPlateGames[] = {
    {"m_main_game", 0, 0, 19, 32, 0, 0},           // the plate's left end
    {"m_main_options", 120, 0, 17, 32, 19, 0},     // S
    {"m_main_game", 69, 0, 17, 32, 36, 0},         // E
    {"m_main_multiplayer", 53, 0, 16, 32, 53, 0},  // L
    {"m_main_game", 69, 0, 17, 32, 69, 0},         // E
    {"m_main_options", 19, 0, 17, 32, 86, 0},      // C: an O ...
    {"m_main_options", 24, 10, 6, 9, 97, 11},      // ... its right side hidden
    {"m_main_options", 24, 10, 6, 9, 101, 11},
    {"m_main_multiplayer", 69, 0, 17, 32, 103, 0},  // T
    {"m_main_quit", 52, 0, 2, 32, 120, 0},          // the space: three slips of the plain plate
    {"m_main_quit", 52, 0, 2, 32, 122, 0},          // left of QUIT's I, the filter's reach
    {"m_main_quit", 52, 0, 2, 32, 124, 0},          // either side still plain
    {"m_main_game", 19, 0, 17, 32, 126, 0},         // G
    {"m_main_game", 36, 0, 17, 32, 143, 0},         // A
    {"m_main_game", 53, 0, 16, 32, 160, 0},         // M
    {"m_main_game", 69, 0, 17, 32, 176, 0},         // E
    {"m_main_game", 86, 0, 13, 32, 193, 0},         // the plate's right end
};
constexpr CarvedPiece kPlateDemos[] = {
    {"m_main_game", 0, 0, 19, 32, 0, 0},       {"m_main_video", 53, 0, 16, 32, 19, 0},  // D
    {"m_main_game", 69, 0, 17, 32, 35, 0},                                              // E
    {"m_main_game", 53, 0, 16, 32, 52, 0},                                              // M
    {"m_main_options", 19, 0, 17, 32, 68, 0},                                           // O
    {"m_main_options", 120, 0, 17, 32, 85, 0},                                          // S
    {"m_main_game", 86, 0, 13, 32, 102, 0},
};
constexpr CarvedPiece kPlateHelp[] = {
    {"m_main_game", 0, 0, 19, 32, 0, 0},
    {"m_main_options", 103, 0, 17, 32, 19, 0},  // H: an N ...
    {"m_main_options", 24, 10, 6, 12, 23, 8},   // ... its diagonal hidden ...
    {"m_main_options", 24, 10, 4, 12, 26, 9},
    {"m_main_multiplayer", 75, 7, 8, 2, 23, 13},   // ... and a slip of T's bar across
    {"m_main_game", 69, 0, 17, 32, 36, 0},         // E
    {"m_main_multiplayer", 53, 0, 16, 32, 53, 0},  // L
    {"m_main_options", 36, 0, 17, 32, 69, 0},      // P
    {"m_main_game", 86, 0, 13, 32, 86, 0},
};
constexpr CarvedPiece kPlateClassic[] = {
    {"m_main_game", 0, 0, 19, 32, 0, 0},           {"m_main_quit", 19, 0, 17, 32, 19, 0},  // Q
    {"m_main_quit", 36, 0, 17, 32, 36, 0},                                                 // U
    {"m_main_game", 36, 0, 17, 32, 53, 0},                                                 // A
    {"m_main_multiplayer", 187, 0, 17, 32, 70, 0},  // K: an R ...
    {"m_main_multiplayer", 160, 4, 12, 11, 75, 4},  // ... Y's right arm over its bowl
    {"m_main_game", 69, 0, 17, 32, 87, 0},          // E
    {"m_main_game", 86, 0, 13, 32, 104, 0},
};
constexpr CarvedPiece kPlateSequel[] = {
    {"m_main_game", 0, 0, 19, 32, 0, 0},
    {"m_main_quit", 19, 0, 17, 32, 19, 0},
    {"m_main_quit", 36, 0, 17, 32, 36, 0},
    {"m_main_game", 36, 0, 17, 32, 53, 0},
    {"m_main_multiplayer", 187, 0, 17, 32, 70, 0},
    {"m_main_multiplayer", 160, 4, 12, 11, 75, 4},
    {"m_main_game", 69, 0, 17, 32, 87, 0},
    {"m_main_quit", 52, 0, 2, 32, 104, 0},   // the space: three slips of the plain plate
    {"m_main_quit", 52, 0, 2, 32, 106, 0},   // left of QUIT's I, the filter's reach
    {"m_main_quit", 52, 0, 2, 32, 108, 0},   // either side still plain
    {"m_main_quit", 53, 0, 16, 32, 110, 0},  // I
    {"m_main_quit", 53, 0, 16, 32, 126, 0},  // I
    {"m_main_game", 86, 0, 13, 32, 142, 0},
};
constexpr CarvedWord kCarvedWords[] = {{"games", kPlatePrint, kPlateGames},
                                       {"demos", kPlatePrint, kPlateDemos},
                                       {"help", kPlatePrint, kPlateHelp},
                                       {"game-classic", kPlatePrint, kPlateClassic},
                                       {"game-sequel", kPlatePrint, kPlateSequel}};

// The rows and names the strips have no words for, set in their carved letters by code
// (carved_letters.hpp). The games' names are spelt letter by letter: the tree names neither game.
constexpr char kClassicName[] = {'Q', 'u', 'a', 'k', 'e'};
constexpr char kSequelName[] = {'Q', 'u', 'a', 'k', 'e', ' ', 'i', 'i'};
struct LetterWord {
  std::string_view row, text;
};
constexpr LetterWord kLetterWords[] = {
    {"demos", "Demos"},
    {"games", "Select Game"},
    {"ranked", "Ranked Game"},
    {"game-classic", std::string_view(kClassicName, std::size(kClassicName))},
    {"game-sequel", std::string_view(kSequelName, std::size(kSequelName))}};

// Row `id`'s word in the art's cut letters, or empty.
std::string_view letter_word(const ClassicArt& art, std::string_view id) {
  if (art.letters.empty()) return {};
  for (const LetterWord& w : kLetterWords)
    if (w.row == id && carved_text_width(art, w.text) > 0) return w.text;
  return {};
}

// `text` in the art's cut letters, its line's top left at (x, y), classic px.
void set_letters(const ClassicPen& pen, std::string_view text, float x, float y) {
  const ClassicArt& art = pen.art;
  std::vector<float> xs;
  carved_layout(art, text, &xs);
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == ' ') continue;
    const ui::Image& img = art.letters.at(text[i]);
    ui::Box b;
    b.rect = pen.at(x + xs[i], y, art.width_of(img), kCarvedLine);
    b.image = &img;
    b.uv = {0, 0, static_cast<float>(img.width), static_cast<float>(img.height)};
    b.nearest = art.density <= 1;
    b.feather = 0;
    pen.dl.box(b);
  }
}

// The mean brightness of a plate's surface: its rows 2 to 5 (classic px) above the letters' recess,
// across x 20 to 80; 0 for another picture.
float plate_surface(const ui::Image& img, float d) {
  const int y0 = static_cast<int>(2 * d), y1 = static_cast<int>(6 * d);
  const int x0 = static_cast<int>(20 * d), x1 = std::min(img.width, static_cast<int>(80 * d));
  double sum = 0;
  int n = 0;
  for (int y = y0; y < std::min(y1, img.height); y++)
    for (int x = x0; x < x1; x++) {
      const size_t i =
          (static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)) * 4;
      sum += 0.3 * img.rgba[i] + 0.6 * img.rgba[i + 1] + 0.1 * img.rgba[i + 2];
      n++;
    }
  return n > 0 ? static_cast<float>(sum / n) : 0;
}

// Row `id`'s word at (x, y), classic px: in the strips' cut letters, or cut from the sequel's
// plates; false when the art has neither for it (the caller draws the glyphs instead). `lit`: each
// plate piece from its picture's lit copy (`_sel`) where the art has one.
bool carve(const ClassicPen& pen, std::string_view id, float x, float y, bool lit = false) {
  const ClassicArt& art = pen.art;
  if (const std::string_view text = letter_word(art, id); !text.empty()) {
    set_letters(pen, text, x, y);
    return true;
  }
  for (const CarvedWord& w : kCarvedWords) {
    if (w.row != id || w.print != art.strip_print || art.strip_print == 0) continue;
    const auto source = [&](const CarvedPiece& p) {
      const ui::Image* img = lit ? art.find(std::string(p.picture) + "_sel") : nullptr;
      return img != nullptr ? img : art.find(p.picture);
    };
    float darkest = 0;
    for (const CarvedPiece& p : w.pieces) {
      const ui::Image* img = source(p);
      if (img == nullptr) return false;
      const float s = plate_surface(*img, static_cast<float>(img->height) / art.height_of(*img));
      if (s > 0 && (darkest == 0 || s < darkest)) darkest = s;
    }
    for (const CarvedPiece& p : w.pieces) {
      const ui::Image* img = source(p);
      const float d = static_cast<float>(img->height) / art.height_of(*img);  // px a classic px
      const float s = plate_surface(*img, d);
      const float shade = s > 0 && darkest > 0 ? darkest / s : 1;
      ui::Box b;
      b.fill = ui::solid({shade, shade, shade, 1});
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

}  // namespace

bool carve_word(const ClassicPen& pen, std::string_view id, float x, float y, bool lit) {
  return carve(pen, id, x, y, lit);
}

float carved_width(const ClassicArt& art, std::string_view id) {
  if (const std::string_view text = letter_word(art, id); !text.empty())
    return carved_text_width(art, text);
  for (const CarvedWord& w : kCarvedWords) {
    if (w.row != id || w.print != art.strip_print || art.strip_print == 0) continue;
    int right = 0;
    for (const CarvedPiece& p : w.pieces) right = std::max(right, p.dx + p.w);
    return static_cast<float>(right);
  }
  return 0;
}

namespace {

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

// The sequel's root: a plate a row (classic_view.hpp).
constexpr float kPlateX = 84, kPlateCursorX = 58, kPlaqueX = 16, kPlateTall = 32;

// The sequel's banner over each page that has one.
constexpr std::pair<std::string_view, std::string_view> kBanners[] = {
    {"single", "m_banner_game"},        {"multi", "m_banner_multiplayer"},
    {"host", "m_banner_start_server"},  {"setup", "m_banner_plauer_setup"},
    {"options", "m_banner_options"},    {"video", "m_banner_video"},
    {"load", "m_banner_load_game"},     {"save", "m_banner_save_game"},
    {"controls", "m_banner_customize"}, {"servers", "m_banner_join_server"},
};

// A root row's own plate, or empty: the rows the sequel's main menu has words for.
std::string_view plate_of(std::string_view id) {
  if (id == "single") return "m_main_game";
  if (id == "multi") return "m_main_multiplayer";
  if (id == "options") return "m_main_options";
  if (id == "video") return "m_main_video";
  if (id == "quit") return "m_main_quit";
  return {};
}

// The root's rows' pitch and the first one's classic y: 32 px apart (closer when they would not
// fit), centred on the 240 px screen (classic y -20 to 220).
float plate_pitch(int rows) {
  return rows <= 0 ? kPlateTall : std::min(36.0f, std::floor(224.0f / static_cast<float>(rows)));
}
float plate_top(int rows) {
  const float pitch = plate_pitch(rows);
  return -20 + std::floor((240 - (pitch * static_cast<float>(rows - 1) + kPlateTall)) / 2);
}

int plate_rows(const ui::Page& page) {
  int n = 0;
  for (const ui::Item& it : page.items)
    if (drawn(it) && it.kind != ui::ItemKind::back) n++;
  return n;
}

ClassicLayout layout_plates(const ui::Menu& menu, const ClassicArt& art, int width, int height) {
  ClassicLayout l = classic_screen(art, width, height);
  l.plates = true;
  const ui::Page& page = menu.top();
  l.items.assign(page.items.size(), {});
  l.bands.assign(page.items.size(), -1);
  const int rows = plate_rows(page);
  const float pitch = plate_pitch(rows);
  float y = plate_top(rows);
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    if (!drawn(it) || it.kind == ui::ItemKind::back) continue;
    const ui::Image* own = art.find(plate_of(it.id));
    float w = own != nullptr ? art.width_of(*own) : carved_width(art, it.id);
    if (w <= 0) w = 16 * static_cast<float>(it.text.size());
    l.items[i] =
        ui::Rect{l.x + kPlateX * l.scale, l.y + y * l.scale, w * l.scale, kPlateTall * l.scale};
    y += pitch;
  }
  return l;
}

// The sequel's root drawn: the plaque and the logo, a plate a row, the lit plate and the cursor on
// the focused one.
void draw_plates(const ClassicPen& pen, const ui::Page& page, double t) {
  const ClassicArt& art = pen.art;
  const ClassicLayout& l = pen.l;
  const float top = plate_top(plate_rows(page));
  pen.picture(art.find("m_main_plaque"), kPlaqueX, top);
  if (const ui::Image* plaque = art.find("m_main_plaque"))
    pen.picture(art.find("m_main_logo"), kPlaqueX + 1, top + art.height_of(*plaque) + 4);
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Rect& r = l.items[i];
    if (r.w <= 0) continue;
    const ui::Item& it = page.items[i];
    const float y = (r.y - l.y) / l.scale;
    const bool focus = static_cast<int>(i) == page.focus;
    const std::string_view own = plate_of(it.id);
    const ui::Image* img = focus && !own.empty() ? art.find(std::string(own) + "_sel") : nullptr;
    if (img == nullptr) img = own.empty() ? nullptr : art.find(own);
    if (img != nullptr)
      pen.picture(img, kPlateX, y);
    else if (!carve(pen, it.id, kPlateX, y, focus))
      pen.big_text(it.text, kPlateX, y + 8);
    if (it.disabled) {
      ui::Box b;
      b.rect = r;
      b.fill = ui::solid(ui::hex(0x000000, 0.68f));
      b.feather = 0;
      pen.dl.box(b);
    }
    if (focus) {
      const int frame = static_cast<int>(std::floor(t * 10)) % 15;
      pen.picture(art.find("m_cursor" + std::to_string(frame)), kPlateCursorX, y + 1);
    }
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

ClassicLayout classic_screen(const ClassicArt& art, int width, int height) {
  const float tall = art.screen_height();
  ClassicLayout l;
  l.scale = std::max(1.0f, std::floor(std::min(static_cast<float>(width) / kScreenW,
                                               static_cast<float>(height) / tall)));
  l.x = std::floor((static_cast<float>(width) - kScreenW * l.scale) / 2);
  l.y = std::floor((static_cast<float>(height) - tall * l.scale) / 2) +
        (tall - kScreenH) / 2 * l.scale;
  return l;
}

std::string_view banner_of(std::string_view page) {
  for (const auto& [id, banner] : kBanners)
    if (id == page) return banner;
  return {};
}

ClassicLayout layout_classic(const ui::Menu& menu, const ClassicArt& art, int width, int height) {
  if (art.plates() && menu.top().id == "main") return layout_plates(menu, art, width, height);
  ClassicLayout l = classic_screen(art, width, height);
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
      out.push_back("page '" + page.id + "', row '" + it.id + "' is off the screen");
    if (layout.strip || layout.plates) continue;
    const size_t cap = it.kind == ui::ItemKind::label ? 40 : kLabelChars;
    if (it.text.size() > cap)
      out.push_back("page '" + page.id + "', row '" + it.id + "': the label \"" + it.text +
                    "\" is wider than its " + std::to_string(cap) + " glyphs; shorten it");
    if (value_of(it).size() > kValueChars)
      out.push_back("page '" + page.id + "', row '" + it.id + "': the value \"" + value_of(it) +
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
  if (l.plates) {
    draw_plates(pen, page, t);
    return;
  }
  pen.picture(art.find("qplaque"), 16, 4);
  const PageArt* pa = art_of(page.id);
  const ui::Image* title = pa != nullptr ? art.find(pa->title) : nullptr;
  const ui::Image* banner = art.plates() ? art.find(banner_of(page.id)) : nullptr;
  if (page.id == "quit") {
    // The question in its box, no title.
  } else if (banner != nullptr) {
    pen.picture(banner, std::floor((kScreenW - art.width_of(*banner)) / 2), -16);
  } else if (art.plates() && !page.title.empty()) {
    const std::string s = sentence(page.title);
    pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), 0, true);
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
        // The strips' first band holds its words a pixel higher than every other band (measured:
        // its small capitals on rows 2 to 14, the others' on 3 to 15), so it is set a pixel lower
        // and every row's letters stand on the same line.
        const float drop = band == 0 ? l.scale : 0;
        ui::Box b;
        b.rect = {r.x, r.y + drop, r.w, tall * l.scale};
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
