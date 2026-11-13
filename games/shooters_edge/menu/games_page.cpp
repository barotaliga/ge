// The SELECT GAME page: the names' pictures, the keys, the rows in both looks (games_page.hpp).
//
// ge:summary The SELECT GAME page: name pictures decoded once, keys, modern and classic rows
// ge:keywords menu select-game games-page name-picture embedded png cards classic
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/games_page.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <span>

#include "assets/image.hpp"
#include "shooters_edge/menu/classic_pages.hpp"
#include "ui/font.hpp"

// The names' pictures, compiled in (CMakeLists.txt: ge_embed_binary).
extern const unsigned char se_game_name_classic[];
extern const std::size_t se_game_name_classic_size;
extern const unsigned char se_game_name_sequel[];
extern const std::size_t se_game_name_sequel_size;

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr std::string_view kTitle = "SELECT GAME";
constexpr std::string_view kHelp = "ENTER PICK   ESC BACK";
constexpr std::string_view kFind = "FIND ANOTHER INSTALL";

ui::Image decoded(const unsigned char* bytes, std::size_t size) {
  ui::Image img;
  const std::optional<ge::assets::Image> png =
      ge::assets::decode_png(std::span<const uint8_t>(bytes, size));
  if (!png) return img;
  img.width = png->width;
  img.height = png->height;
  img.rgba = png->rgba;
  return img;
}

// What a row says under the game's name.
std::string status_of(const GameRow& g) {
  if (g.install.empty()) return "Not found yet. Press ENTER to find its folder.";
  if (g.missing) return "Its folder is gone: " + g.install + ". Press ENTER to find it.";
  std::string s = g.edition.empty() ? std::string() : g.edition + "  ";
  return s + g.install;
}

std::string fit(const ui::Font& font, std::string_view text, float size, float width) {
  if (font.advance(text, size) <= width) return std::string(text);
  const size_t m = font.fit(text, size, std::max(0.0f, width - font.advance("...", size)));
  return std::string(text.substr(0, m)) + "...";
}

// `img` drawn `h` px tall at (x, y), its width kept in proportion; returns that width.
float picture(ui::DrawList& list, const ui::Image& img, float x, float y, float h,
              float opacity = 1) {
  if (img.empty()) return 0;
  const float w = h * static_cast<float>(img.width) / static_cast<float>(img.height);
  ui::Box b;
  b.rect = {x, y, w, h};
  b.image = &img;
  b.uv = {0, 0, static_cast<float>(img.width), static_cast<float>(img.height)};
  b.feather = 0;
  b.opacity = opacity;
  list.box(b);
  return w;
}

}  // namespace

const ui::Image& game_name_picture(std::string_view id) {
  static const std::map<std::string_view, ui::Image> kNames = {
      {"classic", decoded(se_game_name_classic, se_game_name_classic_size)},
      {"sequel", decoded(se_game_name_sequel, se_game_name_sequel_size)}};
  static const ui::Image kNone;
  const auto it = kNames.find(id);
  return it == kNames.end() ? kNone : it->second;
}

void GamesPage::open() {
  if (lister_) rows_ = lister_();
  selected_ = 0;
  for (size_t i = 0; i < rows_.size(); i++)
    if (rows_[i].playing) selected_ = static_cast<int>(i);
}

int GamesPage::hit(float x, float y) const {
  for (size_t i = 0; i < cells_.size(); i++)
    if (cells_[i].contains(x, y)) return static_cast<int>(i);
  return -1;
}

GamesAction GamesPage::input(const ui::MenuInput& in) {
  if (in.back) return GamesAction::leave;
  if (in.up) selected_ = (selected_ + count() - 1) % count();
  if (in.down) selected_ = (selected_ + 1) % count();
  bool pick = in.accept;
  if (in.pointer_moved || in.click) {
    const int at = hit(in.pointer_x, in.pointer_y);
    if (at >= 0) {
      selected_ = at;
      pick = pick || in.click;
    } else if (in.click) {
      pick = false;
    }
  }
  if (!pick) return GamesAction::none;
  note_.clear();
  if (selected_ >= static_cast<int>(rows_.size())) {
    chosen_ = {};
    return GamesAction::browse;
  }
  chosen_ = rows_[static_cast<size_t>(selected_)];
  if (chosen_.playing) return GamesAction::leave;
  return chosen_.install.empty() || chosen_.missing ? GamesAction::browse : GamesAction::open;
}

void GamesPage::draw(ui::DrawList& list, int width, int height, const ui::MenuStyle& style) {
  problems_.clear();
  cells_.clear();
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float s = std::min(w, h) / 720.0f;
  const bool portrait = h > w;
  const ui::Font& bold = style.title_font ? *style.title_font : ui::ui_font(ui::Weight::semibold);
  const ui::Font& small = style.small_font ? *style.small_font : ui::ui_font(ui::Weight::medium);
  const float margin = (portrait ? 24.0f : 64.0f) * s;
  const float ts = std::max(s, 0.75f);
  const float title = style.title_size * ts, word = style.small_size * ts;
  const float line = word * 1.6f;
  const float inner = std::min(w - 2 * margin, 900 * s);
  list.rect({0, 0, w, h}, ui::hex(0x000000, 0.6f));
  const float title_base = margin + title;
  list.text(bold, title, margin, title_base, kTitle, style.text, ui::Align::left, style.tracking);

  // A card a game: its name's picture, what is known of its install under it.
  const float card_h = std::max(118 * s, word * 5.2f);
  const float name_h = card_h * 0.42f;
  float y = title_base + line * 1.4f;
  for (int i = 0; i < count(); i++) {
    const bool game = i < static_cast<int>(rows_.size());
    const float ch = game ? card_h : std::max(word * 2.6f, 52 * s);
    const ui::Rect cell{margin, y, inner, ch};
    cells_.push_back(cell);
    const bool on = i == selected_;
    list.rect(cell, on ? style.accent : style.slab, style.radius * s);
    const float pad = 22 * s;
    if (!game) {
      list.text(bold, word * 1.1f, cell.x + pad, cell.y + ch * 0.5f + word * 0.4f, kFind,
                on ? style.text_focus : style.text, ui::Align::left, style.tracking);
      y += ch + 10 * s;
      continue;
    }
    const GameRow& g = rows_[static_cast<size_t>(i)];
    const ui::Image& name = game_name_picture(g.id);
    const float nw = picture(list, name, cell.x + pad, cell.y + pad, name_h);
    if (nw <= 0) list.text(bold, title, cell.x + pad, cell.y + pad + title, g.id, style.text);
    if (g.playing)
      list.text(bold, word, cell.right() - pad, cell.y + pad + word, "PLAYING NOW",
                on ? style.text_focus : style.accent_hi, ui::Align::right, style.tracking);
    const float sy = cell.y + pad + name_h + line * 1.1f;
    list.text(small, word, cell.x + pad, sy, fit(small, status_of(g), word, inner - 2 * pad),
              on ? style.text_focus : style.text_dim);
    y += ch + 10 * s;
  }

  // The foot: why the last folder did not load, over the keys.
  const float help_base = h - margin * 0.6f;
  list.text(small, word, margin, help_base, kHelp, style.text_dim);
  if (!note_.empty())
    list.text(small, word, margin, help_base - line * 1.3f, fit(small, note_, word, w - 2 * margin),
              style.accent_hi);
  if (y > help_base - line * 2.3f)
    problems_.push_back("the games run into the keys at " + std::to_string(width) + "x" +
                        std::to_string(height));
}

void GamesPage::draw_classic(ui::DrawList& list, const ClassicArt& art, int width, int height,
                             double t) {
  problems_.clear();
  cells_.clear();
  const ClassicLayout l = classic_screen(art, width, height);
  const ClassicPen pen{list, art, l};
  classic_list_frame(pen, "games", "", "games", "SELECT GAME");
  const float x = kListLeft + 8;
  // A game: its name's picture 20 px tall (on the sequel's plates, its own plate 32 px tall), its
  // install and edition in the glyphs under it.
  const float name_h = art.plates() ? 32 : 20;
  float y = kListTop;
  for (int i = 0; i < count(); i++) {
    const bool game = i < static_cast<int>(rows_.size());
    const float tall = game ? name_h + (art.plates() ? 22 : 24) : 12;
    cells_.push_back(pen.at(kListLeft, y, kListRight - kListLeft, tall));
    if (i == selected_) classic_cursor(pen, game ? y + name_h / 2 - 4 : y, t);
    if (!game) {
      pen.text("Find another install", x, y, true);
      break;
    }
    const GameRow& g = rows_[static_cast<size_t>(i)];
    // The name cut from the menu strips' own carved letters where they were measured, else its
    // picture.
    const ui::Image& name = game_name_picture(g.id);
    const ui::Rect r = pen.at(x, y, 0, 20);
    if (!carve_word(pen, "game-" + g.id, x + 4, y, i == selected_) &&
        picture(list, name, r.x, r.y, r.h) <= 0)
      pen.text(g.id, x, y + 6, false);
    // "playing" on the name's line: a glyph's ink ends on its cell's row 6, set on the carved
    // letters' foot (row 15); on a plate, in the middle of it.
    if (g.playing) {
      const std::string_view now = "playing";
      const float cell = art.plates() ? y + name_h / 2 - 4 : y + 15 - 6;
      pen.text(now, kListRight - 8 * static_cast<float>(now.size()), cell, false);
    }
    // The install's rows under the name, clear of the Q's tail (row 23 of the carved line).
    const float under = y + name_h + (art.plates() ? 2 : 4);
    if (g.install.empty() || g.missing) {
      pen.text(g.missing ? "Its folder is gone." : "Not found yet.", x, under, true);
      pen.text("Enter finds its folder.", x, under + 8, true);
    } else {
      pen.text(g.edition.empty() ? "Found" : glyph_safe(g.edition), x, under, false);
      pen.text(fit_chars(glyph_safe(g.install), kListChars, true), x, under + 8, true);
    }
    y += tall + 8;
  }
  if (!note_.empty()) {
    const std::string n = glyph_safe(note_);
    pen.text(fit_chars(n, kFootChars), kFootLeft, 172, false);
    if (n.size() > kFootChars)
      pen.text(fit_chars(n.substr(kFootChars - 2), kFootChars), kFootLeft, 180, false);
  }
  pen.text("enter picks  esc back", kFootLeft, 190, true);
}

}  // namespace shooters_edge::menu
