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
#include "shooters_edge/menu/plate_letters.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr int kScreenW = 320, kScreenH = 200;
constexpr float kRowTop = 32;
constexpr float kLabelEnd = 192, kCursorX = 200, kValueX = 220;
constexpr size_t kLabelChars = 22, kValueChars = 12;
constexpr size_t kSlotChars = 39;  // a save slot's comment line, the whole width from x 16

// The glyphs a row's label holds on a layout: right-aligned to kLabelEnd, clear of the plaque.
size_t label_chars(const ClassicLayout& l) {
  if (l.label_left <= 0) return kLabelChars;
  const auto room = static_cast<size_t>(std::max(0.0f, (kLabelEnd - l.label_left) / 8));
  return std::min(kLabelChars, room);
}
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

// The rows and names the strips have no words for, set in their carved letters by code
// (carved_letters.hpp). The games' names are spelt letter by letter: the tree names neither game.
constexpr char kQ1Name[] = {'Q', 'u', 'a', 'k', 'e'};
constexpr char kQ2Name[] = {'Q', 'u', 'a', 'k', 'e', ' ', 'i', 'i'};
struct LetterWord {
  std::string_view row, text;
};
constexpr LetterWord kLetterWords[] = {{"demos", "Demos"},
                                       {"games", "Select Game"},
                                       {"ranked", "Ranked Game"},
                                       {"game-q1", std::string_view(kQ1Name, std::size(kQ1Name))},
                                       {"game-q2", std::string_view(kQ2Name, std::size(kQ2Name))}};
// The same rows on Quake II's tags (plate_letters.hpp), in its capitals as its own tags have
// them.
constexpr char kQ1Tag[] = {'Q', 'U', 'A', 'K', 'E'};
constexpr char kQ2Tag[] = {'Q', 'U', 'A', 'K', 'E', ' ', 'I', 'I'};
constexpr LetterWord kTagWords[] = {{"demos", "DEMOS"},
                                    {"games", "SELECT GAME"},
                                    {"help", "HELP"},
                                    {"ranked", "RANKED GAME"},
                                    {"game-q1", std::string_view(kQ1Tag, std::size(kQ1Tag))},
                                    {"game-q2", std::string_view(kQ2Tag, std::size(kQ2Tag))}};

// Row `id`'s word in the strips' cut letters, or empty.
std::string_view letter_word(const ClassicArt& art, std::string_view id) {
  if (art.letters.empty() || art.plates()) return {};
  for (const LetterWord& w : kLetterWords)
    if (w.row == id && carved_text_width(art, w.text) > 0) return w.text;
  return {};
}

// Row `id`'s word on Quake II's tags, or empty.
std::string_view tag_word(const ClassicArt& art, std::string_view id) {
  if (art.letters.empty() || !art.plates()) return {};
  for (const LetterWord& w : kTagWords)
    if (w.row == id && plate_layout(art, w.text, nullptr) > 0) return w.text;
  return {};
}

// `text` in the art's cut letters, its line's top left at (x, y), classic px.
void set_letters(const ClassicPen& pen, std::string_view text, float x, float y,
                 Tone tone = Tone::full) {
  const ClassicArt& art = pen.art;
  const std::map<char, ui::Image>& set = toned(art.letters, tone);
  std::vector<float> xs;
  carved_layout(art, text, &xs);
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == ' ') continue;
    const ui::Image& img = set.at(text[i]);
    ui::Box b;
    b.rect = pen.at(x + xs[i], y, art.width_of(img), kCarvedLine);
    b.image = &img;
    b.uv = {0, 0, static_cast<float>(img.width), static_cast<float>(img.height)};
    b.nearest = art.density <= 1;
    b.feather = 0;
    pen.dl.box(b);
  }
}

// Tag or banner pieces (plate_letters.hpp) from (x, y), classic px.
void draw_pieces(const ClassicPen& pen, const std::vector<TagPiece>& pieces, float x, float y) {
  const ClassicArt& art = pen.art;
  for (const TagPiece& p : pieces) {
    const float d = static_cast<float>(p.image->height) / art.height_of(*p.image);
    ui::Box b;
    if (p.shade != 1) b.fill = ui::solid({p.shade, p.shade, p.shade, 1});
    b.rect = pen.at(x + p.x, y, p.w, kPlateLine);
    b.image = p.image;
    b.uv = {p.from * d, 0, p.w * d, static_cast<float>(p.image->height)};
    b.nearest = art.density <= 1;
    b.feather = 0;
    pen.dl.box(b);
  }
}

// `text` on a tag, its top left at (x, y), classic px; from the lit tags' letters when `lit` and
// they have them.
void set_tag(const ClassicPen& pen, std::string_view text, float x, float y, bool lit,
             Tone tone = Tone::full) {
  draw_pieces(pen, tag_pieces(pen.art, text, lit, nullptr, tone), x, y);
}

// Row `id`'s word at (x, y), classic px: in the strips' cut letters, or on Quake II's tags; false
// when the art has neither for it (the caller draws the glyphs instead). `lit`: the tag lit.
bool carve(const ClassicPen& pen, std::string_view id, float x, float y, bool lit = false,
           Tone tone = Tone::full) {
  if (const std::string_view text = letter_word(pen.art, id); !text.empty()) {
    set_letters(pen, text, x, y, tone);
    return true;
  }
  if (const std::string_view text = tag_word(pen.art, id); !text.empty()) {
    set_tag(pen, text, x, y, lit, tone);
    return true;
  }
  return false;
}

}  // namespace

bool carve_word(const ClassicPen& pen, std::string_view id, float x, float y, bool lit, Tone tone) {
  return carve(pen, id, x, y, lit, tone);
}

float carved_width(const ClassicArt& art, std::string_view id) {
  if (const std::string_view text = letter_word(art, id); !text.empty())
    return carved_text_width(art, text);
  if (const std::string_view text = tag_word(art, id); !text.empty())
    return plate_layout(art, text, nullptr);
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

// Quake II's root: a plate a row (classic_view.hpp).
constexpr float kPlateX = 84, kPlateCursorX = 58, kPlaqueX = 16, kPlateTall = 32;

// Quake II's banner over each page that has one.
constexpr std::pair<std::string_view, std::string_view> kBanners[] = {
    {"single", "m_banner_game"},        {"multi", "m_banner_multiplayer"},
    {"host", "m_banner_start_server"},  {"setup", "m_banner_plauer_setup"},
    {"options", "m_banner_options"},    {"video", "m_banner_video"},
    {"load", "m_banner_load_game"},     {"save", "m_banner_save_game"},
    {"controls", "m_banner_customize"}, {"servers", "m_banner_join_server"},
};

// A root row's own plate, or empty: the rows Quake II's main menu has words for.
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

// Quake II's root drawn: the plaque and the logo, a plate a row, the lit plate and the cursor on
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

ClassicLayout classic_screen(int width, int height, float tall, float want, float share) {
  const float fit = std::max(1.0f, std::floor(std::min(static_cast<float>(width) / kScreenW,
                                                       static_cast<float>(height) / tall)));
  ClassicLayout l;
  l.scale = want > 0 ? std::max(1.0f, want) : std::max(1.0f, std::round(fit * share));
  l.x = std::floor((static_cast<float>(width) - kScreenW * l.scale) / 2);
  l.y = std::floor((static_cast<float>(height) - tall * l.scale) / 2) +
        (tall - kScreenH) / 2 * l.scale;
  return l;
}

ClassicLayout classic_screen(int width, int height) {
  return classic_screen(width, height, kScreenH, 0, 1);
}

ClassicLayout classic_screen(const ClassicArt& art, int width, int height) {
  return classic_screen(width, height, art.screen_height(), art.scale, 1);
}

bool banner_title(const ClassicPen& pen, std::string_view title, float y) {
  const ClassicArt& art = pen.art;
  if (!art.plates() || art.letters.empty()) return false;
  // The title in capitals, then each word's first letter a capital and the rest small.
  std::string caps, mixed;
  bool start = true;
  for (char c : title) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c != ' ' && art.letters.count(c) == 0) continue;
    if (c == ' ' && (caps.empty() || caps.back() == ' ')) continue;
    caps += c;
    mixed += start || c == ' ' ? c : static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    start = c == ' ';
  }
  while (!caps.empty() && caps.back() == ' ') {
    caps.pop_back();
    mixed.pop_back();
  }
  if (caps.empty()) return false;
  for (const std::string& text : {caps, mixed}) {
    float wide = 0;
    const std::vector<TagPiece> pieces = banner_pieces(art, text, &wide);
    if (wide <= 0 || wide > kScreenW) continue;
    draw_pieces(pen, pieces, std::floor((kScreenW - wide) / 2), y);
    return true;
  }
  return false;
}

std::string_view banner_of(std::string_view page) {
  for (const auto& [id, banner] : kBanners)
    if (id == page) return banner;
  return {};
}

namespace {

// A page longer than the screen's rows shows the ones round the focus, moved up so the last
// fits; the rows above and below are not drawn (and not pointed at).
void scroll_rows(const ui::Page& page, ClassicLayout& l, float top) {
  const auto fit = static_cast<int>((kScreenH - kRowTop) / 8);
  std::vector<size_t> shown;
  int focus_row = 0;
  for (size_t i = 0; i < page.items.size(); i++) {
    if (l.items[i].w <= 0) continue;
    if (static_cast<int>(i) == page.focus) focus_row = static_cast<int>(shown.size());
    shown.push_back(i);
  }
  const int n = static_cast<int>(shown.size());
  if (n <= fit) return;
  const int first = std::clamp(focus_row - fit / 2, 0, n - fit);
  for (int k = 0; k < n; k++) {
    ui::Rect& r = l.items[shown[static_cast<size_t>(k)]];
    if (k < first || k >= first + fit)
      r = {};
    else
      r.y = top + static_cast<float>((k - first) * 8) * l.scale;
  }
}

}  // namespace

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
  const bool quit = page.id == "quit" || page.id == "newgame";  // a question in its box
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
  if (!l.strip && !quit) scroll_rows(page, l, kRowTop * l.scale + l.y);
  const bool slots = page.id == "load" || page.id == "save";
  if (const ui::Image* plaque = art.find("qplaque"); plaque != nullptr && !slots && !l.strip)
    l.label_left = 16 + art.width_of(*plaque);  // the plaque at x 16, the labels from its edge
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
    const bool slot = (page.id == "load" || page.id == "save") && it.kind != ui::ItemKind::back;
    const size_t cap = it.kind == ui::ItemKind::label ? 40
                       : slot                         ? kSlotChars
                                                      : label_chars(layout);
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
  // The save slots fill the width from the left, as the original's: no plaque beside them.
  const bool slots = page.id == "load" || page.id == "save";
  if (!slots) pen.picture(art.find("qplaque"), 16, 4);
  const PageArt* pa = art_of(page.id);
  const ui::Image* title = pa != nullptr ? art.find(pa->title) : nullptr;
  const ui::Image* banner = art.plates() ? art.find(banner_of(page.id)) : nullptr;
  if (page.id == "quit" || page.id == "newgame") {
    // The question in its box, no title.
  } else if (banner != nullptr) {
    pen.picture(banner, std::floor((kScreenW - art.width_of(*banner)) / 2), -16);
  } else if (art.plates() && !page.title.empty()) {
    // The page's title written on a banner of its own, else in the glyphs.
    if (!banner_title(pen, page.title, -16)) {
      const std::string s = sentence(page.title);
      pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), 0, true);
    }
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

  // Quake II asks whether to quit on a tag in its quit picture (the rest of the picture its
  // credits): the tag alone cut from it, over the answers, in place of the box and the question
  // written in the glyphs (whose sheet has no box pieces).
  const ui::Image* asked = page.id == "quit" && art.plates() ? art.find("quit") : nullptr;
  if (asked != nullptr) {
    constexpr ui::Rect kQuitTag{86, 50, 144, 30};  // classic px of the 320 x 240 picture
    float first = -1;
    for (size_t i = 0; i < page.items.size(); i++)
      if (l.items[i].w > 0 && page.items[i].kind != ui::ItemKind::label) {
        first = local_y(l.items[i]);
        break;
      }
    const float py = first >= 0 ? std::max(0.0f, first - kQuitTag.h - 6) : 90;
    const float d = static_cast<float>(asked->width) / art.width_of(*asked);  // px a classic px
    ui::Box b;
    b.rect = pen.at(std::floor((kScreenW - kQuitTag.w) / 2), py, kQuitTag.w, kQuitTag.h);
    b.image = asked;
    b.uv = {kQuitTag.x * d, kQuitTag.y * d, kQuitTag.w * d, kQuitTag.h * d};
    b.nearest = art.density <= 1;
    b.feather = 0;
    pen.dl.box(b);
  } else if (page.id == "quit" || page.id == "newgame") {
    pen.box(56, 68, 24, 5);
  }
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    const ui::Rect& r = l.items[i];
    if (r.w <= 0) continue;
    const float y = local_y(r);
    const float alpha = it.disabled ? 0.3f : 1.0f;
    if (it.kind == ui::ItemKind::label) {
      if (asked != nullptr) continue;  // the picture asks it
      const std::string s = sentence(it.text);
      pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), y, false);
      continue;
    }
    std::string label = sentence(it.text);
    if (slots && it.kind != ui::ItemKind::back) {  // a slot's whole comment line, then its cursor
      label = it.text;                             // the comment as the save wrote it
      if (label.size() > kSlotChars) label.resize(kSlotChars);
      pen.text(label, 16, y, false, alpha);
      if (static_cast<int>(i) == page.focus)
        pen.glyph(static_cast<uint8_t>(12 + (static_cast<int>(std::floor(t * 4)) & 1)), 8, y);
      continue;
    }
    if (const size_t most = label_chars(l); label.size() > most) label.resize(most);
    if (page.id == "quit" || page.id == "newgame") {
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
