// The screenshots page drawn: the grid, the viewer, the confirm and the footer (gallery_page.hpp).
//
// ge:summary The screenshots page's layout and drawing: grid, viewer, confirm, footer
// ge:keywords gallery screenshots draw layout cells thumbnail viewer confirm footer slots
// ge:skill ui replay
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/gallery_page.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace gc = ge::capture;

constexpr std::string_view kTitle = "SCREENSHOTS";
constexpr std::string_view kHelpGrid = "ENTER VIEW   DEL DELETE   ESC BACK";
constexpr std::string_view kHelpViewer = "LEFT/RIGHT PREVIOUS/NEXT   DEL DELETE   ESC BACK";
constexpr std::string_view kAsk = "DELETE THIS SCREENSHOT?";
constexpr std::string_view kHelpAsk = "ENTER DELETE   ESC KEEP";
constexpr std::string_view kEmpty = "NO SCREENSHOTS YET";
constexpr std::string_view kEmptyHow = "F12 takes one. They are kept in:";
constexpr std::string_view kUnread = "DOES NOT READ";
constexpr std::string_view kSep = "  \xc2\xb7  ";  // a middle dot between the footer's facts

std::string format_word(gc::StillFormat f) {
  std::string w(gc::still_extension(f));
  for (char& c : w) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  return w;
}

// The selected file's facts: when, the pixel size, the format and the bytes.
std::string facts(const gc::GalleryEntry& e) {
  std::string s;
  if (!e.taken.empty()) s += e.taken + std::string(kSep);
  if (e.width > 0)
    s += std::to_string(e.width) + " x " + std::to_string(e.height) + std::string(kSep);
  return s + format_word(e.format) + std::string(kSep) + gc::size_text(e.bytes);
}

// `text` cut from the left with "..." until it fits `width` (a long folder's path keeps its end).
std::string fit_left(const ui::Font& font, std::string_view text, float size, float width) {
  if (font.advance(text, size) <= width) return std::string(text);
  for (size_t cut = 1; cut < text.size(); ++cut) {
    std::string s = "..." + std::string(text.substr(cut));
    if (font.advance(s, size) <= width) return s;
  }
  return "...";
}

std::string fit_right(const ui::Font& font, std::string_view text, float size, float width) {
  const size_t n = font.fit(text, size, width);
  if (n >= text.size()) return std::string(text);
  const size_t m = font.fit(text, size, std::max(0.0f, width - font.advance("...", size)));
  return std::string(text.substr(0, m)) + "...";
}

}  // namespace

bool GalleryPage::fill(Slot& slot, const gc::GalleryPicture* pic) {
  if (pic == nullptr) return false;
  if (slot.stamp != pic->stamp) {
    slot.image.width = pic->picture.width;
    slot.image.height = pic->picture.height;
    slot.image.rgba = pic->picture.rgba;
    slot.image.revision = ++revision_;
    slot.stamp = pic->stamp;
  }
  return true;
}

int GalleryPage::hit(float x, float y) const {
  if (!on_grid_) return -1;
  for (size_t i = 0; i < cells_.size(); ++i)
    if (cells_[i].contains(x, y)) return first_ + static_cast<int>(i);
  return -1;
}

void GalleryPage::draw(ui::DrawList& list, gc::Gallery& gallery, int width, int height,
                       const ui::MenuStyle& style) {
  problems_.clear();
  cells_.clear();
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float s = std::min(w, h) / 720.0f;
  const bool portrait = h > w;
  const ui::Font& bold = style.title_font ? *style.title_font : ui::ui_font(ui::Weight::semibold);
  const ui::Font& small = style.small_font ? *style.small_font : ui::ui_font(ui::Weight::medium);
  const float margin = (portrait ? 24.0f : 48.0f) * s;
  const float ts = std::max(s, 0.75f);  // words stay legible on a phone
  const float title = style.title_size * ts, word = style.small_size * ts;
  const float line = word * 1.6f;
  const float inner = w - 2 * margin;
  const bool viewer = gallery.mode() == gc::GalleryMode::viewer ||
                      (gallery.mode() == gc::GalleryMode::confirm && gallery.picture() != nullptr);
  list.rect({0, 0, w, h}, ui::hex(0x000000, viewer ? 0.9f : 0.62f));

  // The title row: the page's name and where the selection is in the list.
  const float title_base = margin + title;
  list.text(bold, title, margin, title_base, kTitle, style.text, ui::Align::left, style.tracking);
  const int n = gallery.count();
  std::string where;
  if (n > 0 && viewer)
    where = std::to_string(gallery.selected() + 1) + " / " + std::to_string(n);
  else if (n > 0)
    where =
        std::to_string(n) + (n == 1 ? " SCREENSHOT" : " SCREENSHOTS") +
        (gallery.pages() > 1 ? std::string(kSep) + "PAGE " + std::to_string(gallery.page() + 1) +
                                   " / " + std::to_string(gallery.pages())
                             : std::string());
  if (!where.empty()) {
    const float where_w = small.advance(where, word);
    const float title_w = bold.advance(kTitle, title, style.tracking);
    if (title_w + where_w + 24 * s > inner) {  // a narrow screen: under the title
      list.text(small, word, margin, title_base + line, where, style.text_dim);
    } else {
      list.text(small, word, w - margin, title_base, where, style.text_dim, ui::Align::right);
    }
  }

  // The footer: the facts (or the delete's failure) over the keys.
  const float help_base = h - margin;
  const float facts_base = help_base - line;
  const gc::GalleryEntry* e = gallery.current();
  const std::string_view help = gallery.mode() == gc::GalleryMode::grid ? kHelpGrid : kHelpViewer;
  if (n > 0)
    list.text(small, word, margin, help_base, fit_right(small, help, word, inner), style.text_dim);
  if (!gallery.error().empty()) {
    list.text(small, word, margin, facts_base, fit_right(small, gallery.error(), word, inner),
              style.accent_hi);
  } else if (e != nullptr) {
    const std::string name = fit_right(bold, e->name, word, inner);
    const float name_w = bold.advance(name, word) + 18 * s;
    const std::string f = facts(*e);
    if (name_w + small.advance(f, word) <= inner) {
      list.text(bold, word, margin, facts_base, name, style.text);
      list.text(small, word, margin + name_w, facts_base, f, style.text_dim);
    } else {  // a narrow screen: the facts over the keys, the name above them
      list.text(bold, word, margin, facts_base - line, name, style.text);
      list.text(small, word, margin, facts_base, fit_right(small, f, word, inner), style.text_dim);
    }
  }

  const float top = title_base + (portrait ? 2.2f : 1.2f) * line;
  const float bottom = facts_base - (portrait ? 2.6f : 1.6f) * line;
  const ui::Rect area{margin, top, inner, std::max(0.0f, bottom - top)};

  if (n == 0) {
    on_grid_ = false;
    const float cy = h * 0.5f;
    list.text(bold, title * 0.8f, w * 0.5f, cy, kEmpty, style.text, ui::Align::center,
              style.tracking);
    list.text(small, word, w * 0.5f, cy + line * 1.4f, kEmptyHow, style.text_dim,
              ui::Align::center);
    list.text(small, word, w * 0.5f, cy + line * 2.4f, fit_left(small, gallery.dir(), word, inner),
              style.text_dim, ui::Align::center);
    return;
  }

  if (viewer) {
    on_grid_ = false;
    const int at = gallery.selected() - gallery.first_on_page();
    const ui::Image* shown = nullptr;
    if (fill(full_, gallery.picture()))
      shown = &full_.image;
    else if (at >= 0 && static_cast<size_t>(at) < thumbs_.size() &&
             fill(thumbs_[static_cast<size_t>(at)], gallery.thumb(gallery.selected())))
      shown = &thumbs_[static_cast<size_t>(at)].image;  // stretched until the full one is in
    if (shown != nullptr) {
      ui::Box b;
      b.rect = ui::contain_rect(*shown, area);
      b.image = shown;
      b.uv = {0, 0, static_cast<float>(shown->width), static_cast<float>(shown->height)};
      b.feather = 0.5f;
      list.box(b);
    } else if (e != nullptr && !e->error.empty()) {
      list.text(bold, title * 0.7f, w * 0.5f, h * 0.5f, kUnread, style.text_dim, ui::Align::center);
    }
  } else {
    // The grid: cells of the screen's own shape, centred in the area.
    on_grid_ = true;
    const gc::GalleryCells c =
        gc::gallery_cells(area.w, area.h, (portrait ? 150.0f : 230.0f) * s, w / h, 14 * s);
    gallery.set_grid(c.columns, c.rows);
    const float grid_w =
        c.cell_w * static_cast<float>(c.columns) + c.gap * static_cast<float>(c.columns - 1);
    const float grid_h =
        c.cell_h * static_cast<float>(c.rows) + c.gap * static_cast<float>(c.rows - 1);
    const float x0 = area.x + (area.w - grid_w) * 0.5f;
    const float y0 = area.y + std::max(0.0f, (area.h - grid_h) * 0.5f);
    first_ = gallery.first_on_page();
    const int per = gallery.per_page();
    while (thumbs_.size() < static_cast<size_t>(per)) thumbs_.emplace_back();
    for (int i = 0; i < per && first_ + i < n; ++i) {
      const int col = i % c.columns, row = i / c.columns;
      const ui::Rect cell{x0 + static_cast<float>(col) * (c.cell_w + c.gap),
                          y0 + static_cast<float>(row) * (c.cell_h + c.gap), c.cell_w, c.cell_h};
      cells_.push_back(cell);
      const bool sel = first_ + i == gallery.selected();
      list.rect(cell, style.slab, style.radius * s);
      Slot& slot = thumbs_[static_cast<size_t>(i)];
      if (fill(slot, gallery.thumb(first_ + i))) {
        ui::Box b;
        b.rect = ui::contain_rect(slot.image, ui::inset(cell, 3 * s));
        b.image = &slot.image;
        b.uv = {0, 0, static_cast<float>(slot.image.width), static_cast<float>(slot.image.height)};
        b.radius = style.radius * s * 0.6f;
        list.box(b);
      } else if (!gallery.entries()[static_cast<size_t>(first_ + i)].error.empty()) {
        list.text(small, word, cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f + word * 0.35f,
                  kUnread, style.text_disabled, ui::Align::center);
      }
      ui::Box ring;
      ring.rect = sel ? ui::inset(cell, -3 * s) : cell;
      ring.radius = style.radius * s;
      ring.fill = ui::solid({0, 0, 0, 0});
      ring.border = sel ? 4 * s : 1;
      ring.border_color = sel ? style.accent_hi : style.slab_border;
      list.box(ring);
    }
  }

  if (gallery.mode() == gc::GalleryMode::confirm) {
    list.rect({0, 0, w, h}, ui::hex(0x000000, 0.5f));
    const float bw = std::min(560 * s, inner), bh = line * 4.6f + title * 0.7f;
    const ui::Rect box{(w - bw) * 0.5f, (h - bh) * 0.5f, bw, bh};
    ui::Box b;
    b.rect = box;
    b.radius = style.radius * s;
    b.fill = ui::solid(ui::hex(0x15100c, 0.96f));
    b.border = 2 * s;
    b.border_color = style.accent;
    list.box(b);
    const float cx = box.x + bw * 0.5f;
    float y = box.y + line + title * 0.7f;
    if (bold.advance(kAsk, title * 0.7f, style.tracking) > bw - 2 * line)
      problems_.push_back("the delete's question is wider than its box at " +
                          std::to_string(width) + "x" + std::to_string(height));
    list.text(bold, title * 0.7f, cx, y, kAsk, style.text, ui::Align::center, style.tracking);
    y += line * 1.5f;
    if (e != nullptr)
      list.text(small, word, cx, y, fit_right(small, e->name, word, bw - 2 * line), style.text_dim,
                ui::Align::center);
    y += line * 1.5f;
    list.text(bold, word, cx, y, kHelpAsk, style.accent_hi, ui::Align::center, style.tracking);
  }
}

}  // namespace shooters_edge::menu
