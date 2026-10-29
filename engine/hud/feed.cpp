// The HUD's feed defined: the lines' life (hold, fade, slide) and their column.
//
// ge:summary The feed defined: a line's strength and slide over time, the column drawn
// ge:keywords hud feed strength risen expire draw_feed
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#include "hud/feed.hpp"

#include <algorithm>

namespace ge::hud {

Feed::Feed(size_t capacity, double hold_s, double fade_s, double rise_s)
    : capacity_(std::max<size_t>(capacity, 1)), hold_s_(hold_s), fade_s_(fade_s), rise_s_(rise_s) {
  lines_.reserve(capacity_);
}

void Feed::push(std::string_view text, ui::Color color, double t) {
  if (lines_.size() == capacity_) lines_.erase(lines_.begin());
  lines_.push_back({std::string(text), color, t});
}

void Feed::expire(double t) {
  std::erase_if(lines_, [&](const FeedLine& l) { return strength(l.born, t) <= 0; });
}

double Feed::strength(double born, double t) const {
  const double age = t - born;
  if (age < 0) return 0;
  if (age <= hold_s_) return 1;
  return fade_s_ > 0 ? std::clamp(1 - (age - hold_s_) / fade_s_, 0.0, 1.0) : 0;
}

double Feed::risen(double born, double t) const {
  if (rise_s_ <= 0) return 1;
  const double k = std::clamp((t - born) / rise_s_, 0.0, 1.0);
  return 1 - (1 - k) * (1 - k);  // eased out: quick to start, settling into place
}

ui::Rect draw_feed(ui::DrawList& dl, const Frame& f, const Style& st, const Feed& feed, double t,
                   Anchor anchor, float size, float max_w, float dx, float dy) {
  const float s = f.px(size), lh = (ascent(st.value_face, s) + descent(st.value_face, s)) * 1.12f;
  const float w = f.px(max_w);
  const ui::Rect box = place(f, anchor, w, lh * static_cast<float>(feed.capacity()), dx, dy);
  const bool up = static_cast<int>(anchor) / 3 == 2;
  const ui::Align align = align_of(anchor);
  const float x = align == ui::Align::left    ? box.x
                  : align == ui::Align::right ? box.right()
                                              : box.x + box.w / 2;
  // Newest nearest the corner: at a bottom anchor the last line sits lowest.
  const auto& lines = feed.lines();
  float offset = 0;
  dl.push_clip(box);
  for (size_t k = 0; k < lines.size(); k++) {
    const FeedLine& l = lines[lines.size() - 1 - k];
    const double a = feed.strength(l.born, t);
    if (a <= 0) continue;
    const float rise = static_cast<float>(feed.risen(l.born, t));
    const float slot = offset + (1 - rise) * -lh;
    const float top = up ? box.bottom() - lh - slot : box.y + slot;
    const float base = top + ascent(st.value_face, s);
    const auto alpha = static_cast<float>(a) * rise;
    draw_shadowed(dl, st.value_face, s, x, base, l.text, ui::with_alpha(l.color, l.color.a * alpha),
                  ui::with_alpha(st.shadow, st.shadow.a * alpha), f.px(st.shadow_drop), align);
    offset += lh * rise;
  }
  dl.pop_clip();
  return box;
}

}  // namespace ge::hud
