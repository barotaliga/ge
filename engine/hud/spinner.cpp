// The loading spinner (spinner.hpp).
//
// ge:summary Draws the loading spinner: the ring, the chasing arc, the caption and its seconds
// ge:keywords hud spinner loading draw_spinner ring arc caption
// ge:skill ui
// ge:test tests/hud_spinner_test.cpp
#include "hud/spinner.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/num_text.hpp"
#include "hud/face.hpp"

namespace ge::hud {

namespace {

constexpr float kTau = 6.28318530717958647692f;

// t's place in a cycle of `period`, 0..1.
float phase(double t, double period) {
  const double p = period > 0 ? t / period : 0;
  return static_cast<float>(p - std::floor(p));
}

bool on_left(Anchor a) {
  return a == Anchor::top_left || a == Anchor::left || a == Anchor::bottom_left;
}

}  // namespace

ui::Rect draw_spinner(ui::DrawList& dl, const Frame& frame, const Style& style, double t_s,
                      std::string_view caption, double elapsed_s, const Spinner& s) {
  const float r = frame.px(s.radius), half = frame.px(s.width) / 2;
  const float size = frame.px(style.caption_size);
  const float gap = frame.px(style.gap);
  std::string seconds;
  if (elapsed_s >= 0) seconds = core::num::to_fixed(elapsed_s, 1) + "S";
  const float cap_w = caption.empty() ? 0 : text_width(style.caption_face, size, caption, 1);
  const float sec_w = seconds.empty() ? 0 : text_width(style.caption_face, size, seconds, 1);
  const float words = std::max(cap_w, sec_w);
  const float ring = 2 * (r + half);
  const float w = ring + (words > 0 ? gap + words : 0);
  const float h = std::max(ring, seconds.empty() || caption.empty() ? size : 2.4f * size);
  const ui::Rect box = place(frame, s.corner, w, h);
  const bool left = on_left(s.corner);
  const ui::Vec2 centre{left ? box.x + r + half : box.right() - r - half, box.y + h / 2};

  dl.arc(centre, r, 2 * half, 0, kTau, ui::solid(s.ring));
  // The arc's head turns once a period; its tail follows at a length that breathes between a
  // tenth and two thirds of the ring, out and back every two turns (a triangle wave: no sine).
  const float head = phase(t_s, s.period_s) * kTau;
  const float breath = std::fabs(phase(t_s, 2 * s.period_s) * 2 - 1);
  const float length = (0.1f + 0.57f * breath) * kTau;
  dl.arc(centre, r, 2 * half, head - length, head, ui::solid(s.arc), true);

  if (words > 0) {
    const float x = left ? box.x + ring + gap : box.right() - ring - gap;
    const ui::Align align = left ? ui::Align::left : ui::Align::right;
    float base = centre.y + 0.35f * size;
    if (!caption.empty() && !seconds.empty()) base = centre.y - 0.15f * size;
    if (!caption.empty())
      draw_shadowed(dl, style.caption_face, size, x, base, caption, style.ink, style.shadow,
                    frame.px(style.shadow_drop) * 0.5f, align, 1);
    if (!seconds.empty()) {
      const float under = caption.empty() ? base : base + 1.2f * size;
      draw_shadowed(dl, style.caption_face, size, x, under, seconds, style.caption, style.shadow,
                    frame.px(style.shadow_drop) * 0.5f, align, 1);
    }
  }
  return box;
}

}  // namespace ge::hud
