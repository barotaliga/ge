// The HUD kit defined: the frame's scale and safe area, the anchors and the stack, each widget's
// measure and draw, the blink and the layout check.
//
// ge:summary The HUD kit defined: frame, anchors, stack, widget measures and draws, the check
// ge:keywords hud frame_for place stack measure draw readout big meter pips banner blink
// ge:keywords layout_problems overlap
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#include "hud/hud.hpp"

#include <algorithm>
#include <cmath>

namespace ge::hud {

namespace {

float x_for(ui::Rect at, ui::Align align) {
  return align == ui::Align::left     ? at.x
         : align == ui::Align::center ? at.x + at.w / 2
                                      : at.right();
}

float line_h(const Face& face, float size) { return ascent(face, size) + descent(face, size); }

}  // namespace

Frame frame_for(int width, int height, const Style& style, Insets insets) {
  Frame f;
  f.width = width;
  f.height = height;
  f.portrait = height > width;
  const float short_side = static_cast<float>(std::min(width, height));
  f.scale = std::clamp(short_side / style.reference, style.min_scale, style.max_scale);
  const float m = style.margin * f.scale;
  f.safe = {insets.left + m, insets.top + m,
            static_cast<float>(width) - insets.left - insets.right - 2 * m,
            static_cast<float>(height) - insets.top - insets.bottom - 2 * m};
  return f;
}

ui::Rect place(const Frame& frame, Anchor anchor, float w, float h, float dx, float dy) {
  const ui::Rect& s = frame.safe;
  const int col = static_cast<int>(anchor) % 3, row = static_cast<int>(anchor) / 3;
  const float x = col == 0 ? s.x : col == 1 ? s.x + (s.w - w) / 2 : s.right() - w;
  const float y = row == 0 ? s.y : row == 1 ? s.y + (s.h - h) / 2 : s.bottom() - h;
  return {x + dx, y + dy, w, h};
}

ui::Align align_of(Anchor anchor) {
  const int col = static_cast<int>(anchor) % 3;
  return col == 0 ? ui::Align::left : col == 1 ? ui::Align::center : ui::Align::right;
}

Stack::Stack(const Frame& frame, Anchor anchor, float gap, float dx, float dy)
    : frame_(&frame), anchor_(anchor), gap_(gap), dx_(dx), dy_(dy) {}

ui::Rect Stack::next(float w, float h) {
  const bool up = static_cast<int>(anchor_) / 3 == 2;
  const float at = used_ > 0 ? used_ + gap_ : 0;
  const ui::Rect r = place(*frame_, anchor_, w, h, dx_, dy_ + (up ? -at : at));
  used_ = at + h;
  return r;
}

Size measure(const Frame& f, const Style& st, const Readout& r) {
  const float vs = f.px(st.value_size), cs = f.px(st.caption_size);
  const float w = std::max(text_width(st.value_face, vs, r.value),
                           text_width(st.caption_face, cs, r.caption, f.px(st.caption_tracking)));
  const float h = line_h(st.value_face, vs) + (r.caption.empty() ? 0 : line_h(st.caption_face, cs));
  return {w, h};
}

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Readout& r, ui::Rect at,
          ui::Align align) {
  const float vs = f.px(st.value_size), cs = f.px(st.caption_size);
  const float x = x_for(at, align), drop = f.px(st.shadow_drop);
  const float vb = at.y + ascent(st.value_face, vs);
  draw_shadowed(dl, st.value_face, vs, x, vb, r.value, r.color, st.shadow, drop, align);
  if (r.caption.empty()) return;
  const float cb = at.y + line_h(st.value_face, vs) + ascent(st.caption_face, cs);
  draw_shadowed(dl, st.caption_face, cs, x, cb, r.caption, st.caption, st.shadow, drop * 0.75f,
                align, f.px(st.caption_tracking));
}

namespace {

float unit_gap(const Frame& f, const Style& st) { return f.px(st.unit_size) * 0.25f; }

}  // namespace

Size measure(const Frame& f, const Style& st, const Big& b) {
  const float bs = f.px(st.big_size), us = f.px(st.unit_size), cs = f.px(st.caption_size);
  float w = text_width(st.big_face, bs, b.value);
  if (!b.unit.empty()) w += unit_gap(f, st) + text_width(st.caption_face, us, b.unit);
  const float tracking = f.px(st.caption_tracking);
  if (!b.caption.empty()) w = std::max(w, text_width(st.caption_face, cs, b.caption, tracking));
  const float h = line_h(st.big_face, bs) + (b.caption.empty() ? 0 : line_h(st.caption_face, cs));
  return {w, h};
}

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Big& b, ui::Rect at,
          ui::Align align) {
  const float bs = f.px(st.big_size), us = f.px(st.unit_size), cs = f.px(st.caption_size);
  const float drop = f.px(st.shadow_drop) * 1.5f;
  const float vw = text_width(st.big_face, bs, b.value);
  const float uw = b.unit.empty() ? 0 : unit_gap(f, st) + text_width(st.caption_face, us, b.unit);
  // The number and its unit are one run: aligned as a whole.
  float x = x_for(at, align);
  if (align == ui::Align::center) x -= (vw + uw) / 2;
  if (align == ui::Align::right) x -= vw + uw;
  const float base = at.y + ascent(st.big_face, bs);
  draw_shadowed(dl, st.big_face, bs, x, base, b.value, b.color, st.shadow, drop);
  if (!b.unit.empty())
    draw_shadowed(dl, st.caption_face, us, x + vw + unit_gap(f, st), base, b.unit, st.caption,
                  st.shadow, drop * 0.6f);
  if (b.caption.empty()) return;
  const float cb = at.y + line_h(st.big_face, bs) + ascent(st.caption_face, cs);
  draw_shadowed(dl, st.caption_face, cs, x_for(at, align), cb, b.caption, st.caption, st.shadow,
                drop * 0.5f, align, f.px(st.caption_tracking));
}

Size measure(const Frame& f, const Meter& m) { return {f.px(m.w), f.px(m.h)}; }

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Meter& m, ui::Rect at) {
  const float r = at.h / 2;
  // The track sits on a shadow of its own, so a white bar reads on white snow.
  ui::Box shadow;
  shadow.rect = ui::inset(at, f.px(1.5f));
  shadow.radius = r + f.px(1.5f);
  shadow.fill = ui::solid(ui::with_alpha(st.shadow, st.shadow.a * 0.7f));
  shadow.feather = f.px(2);
  dl.box(shadow);
  dl.rect(at, st.track, r);
  const float v = std::clamp(m.value, m.centred ? -1.0f : 0.0f, 1.0f);
  if (m.centred) {
    const float mid = at.x + at.w / 2, len = std::fabs(v) * at.w / 2;
    if (len > 0.5f) dl.rect({v < 0 ? mid - len : mid, at.y, len, at.h}, m.fill, r);
    dl.rect({mid - f.px(1), at.y - f.px(2), f.px(2), at.h + f.px(4)}, st.ink);
  } else if (v > 0) {
    dl.rect({at.x, at.y, std::max(at.w * v, at.h), at.h}, m.fill, r);
  }
  for (int i = 1; i < m.ticks; i++) {
    const float x = at.x + at.w * static_cast<float>(i) / static_cast<float>(m.ticks);
    dl.rect({x - f.px(0.75f), at.y, f.px(1.5f), at.h}, st.shadow);
  }
}

Size measure(const Frame& f, const Pips& p) {
  const float cell = f.px(p.size) / 2, gap = f.px(p.spacing);
  return {static_cast<float>(p.count) * (cell + gap) - gap, f.px(p.size)};
}

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Pips& p, ui::Rect at) {
  const float cell = f.px(p.size) / 2, gap = f.px(p.spacing), drop = f.px(st.shadow_drop);
  for (int i = 0; i < p.count; i++) {
    const ui::Rect r{at.x + static_cast<float>(i) * (cell + gap), at.y, cell, f.px(p.size)};
    const bool lit = i < p.lit;
    const bool shown = lit && !p.blink_off;
    if (shown) draw_symbol(dl, p.font, p.symbol, {r.x, r.y + drop, r.w, r.h}, st.shadow);
    draw_symbol(dl, p.font, p.symbol, r, shown ? p.color : st.dim);
  }
}

namespace {

float banner_pad(const Frame& f, const Style& st) { return f.px(st.banner_size) * 0.45f; }

}  // namespace

Size measure(const Frame& f, const Style& st, const Banner& b) {
  const float ts = f.px(st.banner_size), ls = f.px(st.value_size) * 0.7f;
  const float tracking = f.px(st.caption_tracking);
  const float w = std::max(text_width(st.value_face, ts, b.title, tracking),
                           text_width(st.caption_face, ls, b.line));
  const float h = (b.title.empty() ? 0 : line_h(st.value_face, ts)) +
                  (b.line.empty() ? 0 : line_h(st.caption_face, ls));
  const float pad = banner_pad(f, st);
  return {w + 2 * pad, h + pad};
}

void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Banner& b, ui::Rect at) {
  const float ts = f.px(st.banner_size), ls = f.px(st.value_size) * 0.7f;
  const float pad = banner_pad(f, st), drop = f.px(st.shadow_drop);
  ui::Box plate;
  plate.rect = at;
  plate.radius = pad * 0.6f;
  plate.fill = ui::solid(st.plate);
  plate.feather = f.px(10);
  dl.box(plate);
  const float cx = at.x + at.w / 2;
  float y = at.y + pad / 2;
  if (!b.title.empty()) {
    draw_shadowed(dl, st.value_face, ts, cx, y + ascent(st.value_face, ts), b.title, b.color,
                  st.shadow, drop, ui::Align::center, f.px(st.caption_tracking));
    y += line_h(st.value_face, ts);
  }
  if (!b.line.empty())
    draw_shadowed(dl, st.caption_face, ls, cx, y + ascent(st.caption_face, ls), b.line, st.ink,
                  st.shadow, drop * 0.75f, ui::Align::center);
}

bool blink(double t, double hz) {
  const double phase = t * hz;
  return phase - std::floor(phase) < 0.5;
}

std::vector<std::string> layout_problems(const Frame& f, const std::vector<Placed>& placed) {
  std::vector<std::string> out;
  const ui::Rect screen{0, 0, static_cast<float>(f.width), static_cast<float>(f.height)};
  for (const Placed& p : placed) {
    if (p.rect.x < screen.x - 0.5f || p.rect.y < screen.y - 0.5f ||
        p.rect.right() > screen.right() + 0.5f || p.rect.bottom() > screen.bottom() + 0.5f)
      out.push_back(p.name + " leaves the screen");
  }
  for (size_t i = 0; i < placed.size(); i++) {
    for (size_t j = i + 1; j < placed.size(); j++) {
      const ui::Rect o = ui::intersect(placed[i].rect, placed[j].rect);
      if (o.w > 1 && o.h > 1) out.push_back(placed[i].name + " overlaps " + placed[j].name);
    }
  }
  return out;
}

}  // namespace ge::hud
