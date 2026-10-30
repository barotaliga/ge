// The HUD's picture widgets defined: each box in picture px times the unit, sampled nearest at a
// whole unit; the digit fields right-aligned; the strip's cells, plates, frames and counts; the
// ring's track and its one or two laps of fill.
//
// ge:summary The HUD's picture widgets defined: pictures, digits, strips and rings drawn
// ge:keywords hud picture digits strip cell ring arc measure draw nearest whole-unit
// ge:skill ui
// ge:test tests/hud_pictures_test.cpp
#include "hud/pictures.hpp"

#include <algorithm>

namespace ge::hud {

namespace {

constexpr float kDegToRad = 0.017453292519943295f;

float fw(const ui::Image* img) { return img != nullptr ? static_cast<float>(img->width) : 0; }
float fh(const ui::Image* img) { return img != nullptr ? static_cast<float>(img->height) : 0; }

// `u` here is screen px per PICTURE pixel (the art's unit over its density).
void picture_box(ui::DrawList& dl, const ui::Image* img, ui::Rect r, ui::Color tint, float u,
                 float opacity = 1) {
  if (img == nullptr || img->empty() || opacity <= 0) return;
  ui::Box b;
  b.rect = r;
  b.image = img;
  b.uv = {0, 0, fw(img), fh(img)};
  b.fill = ui::solid(tint);
  b.nearest = whole_unit(u);
  b.feather = 0.5f;
  b.opacity = opacity;
  dl.box(b);
}

// The cell's size, art px.
Size cell_size(const StripShape& s, const Cell& c, float density) {
  return {s.cell_w > 0 ? s.cell_w : fw(c.image) / density,
          s.cell_h > 0 ? s.cell_h : fh(c.image) / density};
}

}  // namespace

Size measure(const Picture& p, float u) {
  return {fw(p.image) / p.density * u, fh(p.image) / p.density * u};
}

void draw(ui::DrawList& dl, const Picture& p, ui::Rect at, float u) {
  picture_box(dl, p.image, at, p.tint, u / p.density);
}

Size DigitSheet::cell() const { return {fw(glyph[0]) / density, fh(glyph[0]) / density}; }

Size measure(const Digits& d, float u) {
  if (d.sheet == nullptr) return {0, 0};
  const Size c = d.sheet->cell();
  const int n = d.cells > 0 ? d.cells : static_cast<int>(d.value.size());
  return {c.w * static_cast<float>(n) * u, c.h * u};
}

void draw(ui::DrawList& dl, const Digits& d, ui::Rect at, float u) {
  if (d.sheet == nullptr) return;
  const Size c = d.sheet->cell();
  const int n = d.cells > 0 ? d.cells : static_cast<int>(d.value.size());
  const int len = static_cast<int>(d.value.size());
  // Right-aligned: the value's last character in the field's last cell; a value longer than the
  // field shows its last `n` characters.
  for (int k = 0; k < std::min(n, len); ++k) {
    const char ch = d.value[static_cast<size_t>(len - 1 - k)];
    const int g = ch >= '0' && ch <= '9' ? ch - '0' : ch == '-' ? 10 : -1;
    if (g < 0) continue;
    const auto gi = static_cast<size_t>(g);
    const ui::Image* img =
        d.alt && d.sheet->alt[gi] != nullptr ? d.sheet->alt[gi] : d.sheet->glyph[gi];
    const float x = at.x + c.w * static_cast<float>(n - 1 - k) * u;
    picture_box(dl, img, {x, at.y, c.w * u, c.h * u}, d.tint, u / d.sheet->density);
  }
}

Size measure(const StripShape& s, const Strip& strip, float u) {
  if (strip.cells == nullptr || strip.cells->empty()) return {0, 0};
  float along = 0, across = 0;
  for (const Cell& c : *strip.cells) {
    const Size z = cell_size(s, c, strip.density);
    along += (s.vertical ? z.h : z.w) + s.gap;
    across = std::max(across, s.vertical ? z.w : z.h);
  }
  along -= s.gap;
  return s.vertical ? Size{across * u, along * u} : Size{along * u, across * u};
}

void draw(ui::DrawList& dl, const StripShape& s, const Strip& strip, ui::Rect at, float u) {
  if (strip.cells == nullptr) return;
  float pos = 0;
  for (const Cell& c : *strip.cells) {
    const Size z = cell_size(s, c, strip.density);
    const ui::Rect r = s.vertical ? ui::Rect{at.x, at.y + pos * u, z.w * u, z.h * u}
                                  : ui::Rect{at.x + pos * u, at.y, z.w * u, z.h * u};
    pos += (s.vertical ? z.h : z.w) + s.gap;
    if (!c.show || c.alpha <= 0) continue;
    if (c.plate.a > 0) {
      const float ph = (s.plate_h > 0 ? s.plate_h : z.h) * u;
      const float top = ph * s.plate_split;
      dl.rect({r.x, r.y, r.w, top}, ui::with_alpha(c.plate, c.plate.a * c.alpha));
      dl.rect({r.x, r.y + top, r.w, ph - top},
              ui::with_alpha(c.plate_low, c.plate_low.a * c.alpha));
    }
    picture_box(dl, c.image, r, c.tint, u / strip.density, c.alpha);
    if (c.framed) {
      ui::Box b;
      b.rect = r;
      b.fill = ui::solid({0, 0, 0, 0});
      b.border = s.frame_w * u;
      b.border_color = s.frame;
      b.feather = 0.5f;
      b.opacity = c.alpha;
      dl.box(b);
    }
    if (!c.text.empty()) {
      const float size = s.text_size * u;
      const float w = text_width(strip.face, size, c.text);
      const float h = ascent(strip.face, size) + descent(strip.face, size);
      const int col = static_cast<int>(s.text_at) % 3, row = static_cast<int>(s.text_at) / 3;
      const float x = (col == 0   ? r.x
                       : col == 1 ? r.x + (r.w - w) / 2
                                  : r.right() - w) +
                      s.text_dx * u;
      const float y = (row == 0   ? r.y
                       : row == 1 ? r.y + (r.h - h) / 2
                                  : r.bottom() - h) +
                      s.text_dy * u;
      const float base = y + ascent(strip.face, size);
      const ui::Color ink = ui::with_alpha(c.text_color, c.text_color.a * c.alpha);
      if (strip.shadow.a > 0)
        draw_text(dl, strip.face, size, x + u, base + u, c.text,
                  ui::with_alpha(strip.shadow, strip.shadow.a * c.alpha));
      draw_text(dl, strip.face, size, x, base, c.text, ink);
    }
  }
}

Size measure(const Frame& f, const RingShape& s) {
  const float d = 2 * f.px(s.radius + s.width / 2);
  return {d, d};
}

void draw(ui::DrawList& dl, const Frame& f, const RingShape& s, const Ring& r, ui::Rect at) {
  const ui::Vec2 c{at.x + at.w / 2, at.y + at.h / 2};
  const float rad = f.px(s.radius), w = f.px(s.width);
  const float a0 = s.from_deg * kDegToRad, a1 = s.to_deg * kDegToRad;
  dl.arc(c, rad, w, a0, a1, ui::solid(r.track), s.round);
  const auto lap = [&](float share, ui::Color col) {
    if (share <= 0) return;
    const float sweep = (a1 - a0) * share;
    if (s.reverse)
      dl.arc(c, rad, w, a1 - sweep, a1, ui::solid(col), s.round);
    else
      dl.arc(c, rad, w, a0, a0 + sweep, ui::solid(col), s.round);
  };
  lap(std::clamp(r.level, 0.0f, 1.0f), r.fill);
  lap(std::clamp(r.level - 1, 0.0f, 1.0f), r.over);
}

}  // namespace ge::hud
