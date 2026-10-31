// The main menu laid out and drawn in the classic look (classic_view.hpp).
//
// ge:summary Lays out and draws the main menu on the classic 320x200 screen in the pack's pictures
// ge:keywords menu classic layout draw strip glyphs cursor slider text-box help dim
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/classic_view.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;

constexpr int kScreenW = 320, kScreenH = 200;
constexpr float kRowTop = 32;
constexpr float kLabelEnd = 192, kCursorX = 200, kValueX = 220;
constexpr size_t kLabelChars = 22, kValueChars = 12;
constexpr int kSliderCells = 10;

struct PageArt {
  std::string_view page, title, strip;
};
constexpr PageArt kPageArt[] = {
    {"main", "ttl_main", "mainmenu"}, {"single", "ttl_sgl", "sp_menu"},
    {"multi", "p_multi", "mp_menu"},  {"host", "p_multi", ""},
    {"setup", "p_multi", ""},         {"options", "p_option", ""},
    {"video", "p_option", ""},
};

const PageArt* art_of(std::string_view page) {
  for (const PageArt& a : kPageArt)
    if (a.page == page) return &a;
  return nullptr;
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
  const bool help = page.id == "help" && help_pages(art) > 0;
  const bool quit = page.id == "quit";
  float row = quit ? 84 : kRowTop;
  int strip_row = 0;
  for (size_t i = 0; i < page.items.size(); i++) {
    const ui::Item& it = page.items[i];
    if (!drawn(it) || help) continue;
    if (l.strip) {
      if (it.kind == ui::ItemKind::back) continue;
      l.items[i] = rect(72, kRowTop + 20 * static_cast<float>(strip_row++),
                        static_cast<float>(strip->width), 20);
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
    pen.picture(title, std::floor((kScreenW - static_cast<float>(title->width)) / 2), 4);
  } else if (!page.title.empty()) {
    const std::string s = sentence(page.title);
    pen.text(s, std::floor((kScreenW - 8 * static_cast<float>(s.size())) / 2), 12, true);
  }
  const auto local_y = [&](const ui::Rect& r) { return (r.y - l.y) / l.scale; };

  if (l.strip) {
    const ui::Image* strip = art.find(art_of(page.id)->strip);
    pen.picture(strip, 72, kRowTop);
    for (size_t i = 0; i < page.items.size(); i++) {
      const ui::Rect& r = l.items[i];
      if (r.w <= 0) continue;
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
