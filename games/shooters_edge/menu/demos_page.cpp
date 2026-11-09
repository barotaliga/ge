// The demos page: where it opens, the keys, the check, the places and the list drawn
// (demos_page.hpp).
//
// ge:summary The demos page: its opening folder, keys, check, places, list, footer, empty folder
// ge:keywords demos page browse places rows footer check draw menu_demo_dir
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/demos_page.hpp"

#include <algorithm>
#include <string_view>

#include "console/console.hpp"
#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace gv = ge::vfs;

constexpr std::string_view kTitle = "DEMOS";
constexpr std::string_view kHelp =
    "ENTER PLAY   LEFT/RIGHT PLACES   BACKSPACE UP   R RELIST   ESC BACK";

std::string fit(const ui::Font& font, std::string_view text, float size, float width) {
  const size_t n = font.fit(text, size, width);
  if (n >= text.size()) return std::string(text);
  const size_t m = font.fit(text, size, std::max(0.0f, width - font.advance("...", size)));
  return std::string(text.substr(0, m)) + "...";
}

// A path cut from the left to `width`: the folder's own name is what the player needs to see.
std::string fit_left(const ui::Font& font, std::string_view text, float size, float width) {
  if (font.advance(text, size) <= width) return std::string(text);
  const float room = std::max(0.0f, width - font.advance("...", size));
  size_t from = 0;
  while (from < text.size() && font.advance(text.substr(from), size) > room) from++;
  return "..." + std::string(text.substr(from));
}

}  // namespace

void DemosPage::add_cvars(ge::console::Console& con) {
  con_ = &con;
  con.cvars().bind(
      {.name = "menu_demo_dir",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "the folder DEMOS opens on: the one the last demo played from (\"\": the "
               "game's folder)",
       .kind = ge::console::CvarKind::text},
      &last_dir_);
}

void DemosPage::open() {
  std::vector<gv::Place> places = game_places_;
  for (gv::Place& p : player_places_.empty() ? gv::user_places() : player_places_)
    places.push_back(std::move(p));
  browser_.set_places(places);
  facts_.clear();
  refused_ = false;
  first_ = 0;
  if (!last_dir_.empty() && browser_.open(last_dir_)) return;
  for (const gv::Place& p : places)
    if (browser_.open(p.path)) return;
}

int DemosPage::hit_row(float x, float y) const {
  for (size_t i = 0; i < cells_.size(); i++)
    if (cells_[i].contains(x, y)) return first_ + static_cast<int>(i);
  return -1;
}

int DemosPage::hit_place(float x, float y) const {
  for (size_t i = 0; i < chips_.size(); i++)
    if (chips_[i].contains(x, y)) return static_cast<int>(i);
  return -1;
}

DemosAction DemosPage::input(const ui::MenuInput& in) {
  if (in.refresh) {
    browser_.rescan();
    return DemosAction::none;
  }
  if (in.click) {
    const int p = hit_place(in.pointer_x, in.pointer_y);
    if (p >= 0 && p < static_cast<int>(browser_.places().size())) {
      browser_.open(browser_.places()[static_cast<size_t>(p)].path);
      refused_ = false;
      return DemosAction::none;
    }
  }
  gv::BrowseInput bi;
  bi.up = in.up;
  bi.down = in.down;
  bi.left = in.left;
  bi.right = in.right;
  bi.accept = in.accept;
  bi.back = in.back;
  bi.parent = in.del;
  if (in.pointer_moved || in.click) bi.pointed = hit_row(in.pointer_x, in.pointer_y);
  bi.click = in.click && bi.pointed >= 0;
  const std::string before = browser_.dir();
  const gv::BrowseAction a = browser_.input(bi);
  if (a == gv::BrowseAction::leave) return DemosAction::leave;
  if (a == gv::BrowseAction::opened || a == gv::BrowseAction::moved || browser_.dir() != before) {
    if (refused_) browser_.set_note({});
    refused_ = false;
  }
  if (a != gv::BrowseAction::chosen) return DemosAction::none;
  std::string facts, why;
  if (check_ && !check_(browser_.chosen(), facts, why)) {
    browser_.set_note(why);
    refused_ = true;
    return DemosAction::none;
  }
  facts_ = facts;
  if (con_ != nullptr)
    con_->cvars().set("menu_demo_dir", browser_.dir(), ge::console::Source::code);
  return DemosAction::play;
}

void DemosPage::draw(ui::DrawList& list, int width, int height, const ui::MenuStyle& style) {
  problems_.clear();
  cells_.clear();
  chips_.clear();
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float s = std::min(w, h) / 720.0f;
  const bool portrait = h > w;
  const ui::Font& bold = style.title_font ? *style.title_font : ui::ui_font(ui::Weight::semibold);
  const ui::Font& small = style.small_font ? *style.small_font : ui::ui_font(ui::Weight::medium);
  const float margin = (portrait ? 24.0f : 48.0f) * s;
  const float ts = std::max(s, 0.75f);
  const float title = style.title_size * ts, word = style.small_size * ts;
  const float line = word * 1.6f;
  const float inner = w - 2 * margin;
  list.rect({0, 0, w, h}, ui::hex(0x000000, 0.66f));

  // The title row: the page's name, then the folder open (its tail kept when it is long).
  const float title_base = margin + title;
  list.text(bold, title, margin, title_base, kTitle, style.text, ui::Align::left, style.tracking);
  const float title_w = bold.advance(kTitle, title, style.tracking);
  const std::string& dir = browser_.dir();
  float row_y = title_base;
  if (portrait) {
    row_y += line;
    list.text(small, word, margin, row_y, fit_left(small, dir, word, inner), style.text_dim);
  } else {
    list.text(small, word, w - margin, title_base,
              fit_left(small, dir, word, inner - title_w - 32 * s), style.text_dim,
              ui::Align::right);
  }

  // The places: chips in a row (wrapped on a narrow screen), the one the folder is in lit.
  const int here = browser_.place();
  const float chip_h = std::max(word * 1.7f, 30 * s);
  const float pad = 14 * s, gap = 8 * s;
  float cx = margin, cy = row_y + line * 0.8f;
  const std::vector<gv::Place>& places = browser_.places();
  for (size_t i = 0; i < places.size(); i++) {
    const float cw = small.advance(places[i].name, word * 0.85f) + 2 * pad;
    if (cx > margin && cx + cw > margin + inner) {
      cx = margin;
      cy += chip_h + gap;
    }
    const ui::Rect chip{cx, cy, cw, chip_h};
    chips_.push_back(chip);
    const bool lit = static_cast<int>(i) == here;
    list.rect(chip, lit ? style.accent : style.slab, style.radius * s);
    list.text(small, word * 0.85f, cx + pad, cy + chip_h * 0.5f + word * 0.3f, places[i].name,
              lit ? style.text_focus : style.text);
    cx += cw + gap;
  }

  // The footer: what the check said, or the selected entry's facts, over the keys.
  const float help_base = h - margin;
  const float facts_base = help_base - line * 1.3f;
  list.text(small, word, margin, help_base, fit(small, kHelp, word, inner), style.text_dim);
  const gv::FolderEntry* sel = browser_.selection();
  if (!browser_.note().empty()) {  // two lines at most, cut at a word, over the keys
    const std::string& note = browser_.note();
    size_t cut = small.fit(note, word, inner);
    if (cut < note.size()) {
      const size_t space = note.rfind(' ', cut);
      if (space != std::string::npos && space > cut / 2) cut = space;
      list.text(small, word, margin, facts_base - line, note.substr(0, cut), style.accent_hi);
      list.text(small, word, margin, facts_base, fit(small, note.substr(cut + 1), word, inner),
                style.accent_hi);
    } else {
      list.text(small, word, margin, facts_base, note, style.accent_hi);
    }
  } else if (sel != nullptr) {
    const std::string about = sel->name == ".." ? "UP TO " + sel->path
                              : sel->folder     ? "FOLDER " + sel->path
                                                : sel->path;
    list.text(small, word, margin, facts_base, fit_left(small, about, word, inner), style.text_dim);
  }

  // The rows.
  const float top = cy + chip_h + line * 0.9f;
  const float bottom = facts_base - line * 2.2f;  // room for a two-line note
  const float row_h = std::max(word * 1.9f, 34 * s);
  const int fits = std::max(1, static_cast<int>((bottom - top) / row_h));
  browser_.set_page_rows(fits);
  const std::vector<gv::FolderEntry>& rows = browser_.entries();
  const int n = static_cast<int>(rows.size());
  const int selected = browser_.selected();
  if (selected < first_) first_ = selected;
  if (selected >= first_ + fits) first_ = selected - fits + 1;
  first_ = std::clamp(first_, 0, std::max(0, n - fits));
  if (browser_.files() == 0) {  // under the folders, when there is room
    const float y = top + static_cast<float>(std::min(n, fits)) * row_h + line * 2;
    if (y + line * 1.4f < bottom + line) {
      list.text(bold, title * 0.6f, w * 0.5f, y, "NO DEMOS HERE", style.text, ui::Align::center,
                style.tracking);
      list.text(small, word, w * 0.5f, y + line * 1.4f,
                fit(small, "Recorded games, plain or packed (.gz, .zip, .7z, .xz, .tar.gz).", word,
                    inner),
                style.text_dim, ui::Align::center);
    }
  }
  const float size_w = 110 * s, age_w = portrait ? 0.0f : 150 * s;
  for (int r = first_; r < n && r < first_ + fits; r++) {
    const gv::FolderEntry& e = rows[static_cast<size_t>(r)];
    const ui::Rect cell{margin, top + static_cast<float>(r - first_) * row_h, inner, row_h - 4 * s};
    cells_.push_back(cell);
    const bool on = r == selected;
    list.rect(cell, on ? style.accent : style.slab, style.radius * s);
    const float base = cell.y + cell.h * 0.5f + word * 0.36f;
    const ui::Color tc = on ? style.text_focus : style.text;
    const ui::Color dim = on ? style.text_focus : style.text_dim;
    const float name_w = inner - 24 * s - size_w - age_w;
    if (e.folder) {
      const std::string name = e.name == ".." ? "..  UP A FOLDER" : e.name + "/";
      list.text(bold, word, margin + 12 * s, base, fit(bold, name, word, name_w), tc);
      list.text(small, word * 0.85f, cell.right() - 12 * s, base, "FOLDER", dim, ui::Align::right);
      continue;
    }
    list.text(bold, word, margin + 12 * s, base, fit(bold, e.name, word, name_w), tc);
    if (age_w > 0)
      list.text(small, word, cell.right() - 12 * s - size_w, base, gv::age_text(e.age_s), dim,
                ui::Align::right);
    list.text(small, word, cell.right() - 12 * s, base, gv::size_text(e.bytes), dim,
              ui::Align::right);
  }
  if (n > fits) {
    const std::string more = std::to_string(first_ + 1) + "-" +
                             std::to_string(std::min(n, first_ + fits)) + " OF " +
                             std::to_string(n);
    list.text(small, word * 0.85f, w - margin, bottom + line * 0.8f, more, style.text_dim,
              ui::Align::right);
  }
  if (small.advance(kHelp, word) > inner)
    problems_.push_back("the keys are wider than the screen at " + std::to_string(width) + "x" +
                        std::to_string(height));
  if (top + row_h > bottom)
    problems_.push_back("no row fits under the places at " + std::to_string(width) + "x" +
                        std::to_string(height));
}

}  // namespace shooters_edge::menu
