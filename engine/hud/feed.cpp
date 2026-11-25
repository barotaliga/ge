// The HUD's feed defined: the lines' life (hold, fade, slide) and their column.
//
// ge:summary The feed defined: a line's strength and slide over time, the column drawn
// ge:keywords hud feed strength risen expire draw_feed wrap long-line
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#include "hud/feed.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace ge::hud {
namespace {

// `text` in rows no wider than `w` px at `size`: broken at its spaces, a word wider than a row cut
// where the row ends (on a code point). One row when it fits.
std::vector<std::string> wrapped(const Face& face, float size, std::string_view text, float w) {
  std::vector<std::string> rows;
  if (w <= 0 || text_width(face, size, text) <= w + 0.5f) {
    rows.emplace_back(text);
    return rows;
  }
  std::string row;
  const auto fits = [&](std::string_view t) { return text_width(face, size, t) <= w + 0.5f; };
  size_t i = 0;
  while (i < text.size()) {
    const size_t space = text.find(' ', i);
    const std::string_view word =
        text.substr(i, space == std::string_view::npos ? std::string_view::npos : space - i);
    i = space == std::string_view::npos ? text.size() : space + 1;
    const std::string joined = row.empty() ? std::string(word) : row + " " + std::string(word);
    if (fits(joined)) {
      row = joined;
      continue;
    }
    if (!row.empty()) rows.push_back(std::move(row));
    row.clear();
    // The word alone; too wide for a row, it is cut a code point at a time.
    for (size_t k = 0; k < word.size();) {
      size_t n = 1;
      while (k + n < word.size() && (static_cast<unsigned char>(word[k + n]) & 0xC0) == 0x80) ++n;
      const std::string next = row + std::string(word.substr(k, n));
      if (!row.empty() && !fits(next)) {
        rows.push_back(std::move(row));
        row = std::string(word.substr(k, n));
      } else {
        row = next;
      }
      k += n;
    }
  }
  if (!row.empty() || rows.empty()) rows.push_back(std::move(row));
  return rows;
}

}  // namespace

Feed::Feed(size_t capacity, double hold_s, double fade_s, double rise_s)
    : capacity_(std::max<size_t>(capacity, 1)), hold_s_(hold_s), fade_s_(fade_s), rise_s_(rise_s) {
  lines_.reserve(capacity_);
}

void Feed::push(std::string_view text, ui::Color color, double t) {
  if (lines_.size() == capacity_) lines_.erase(lines_.begin());
  lines_.push_back({std::string(text), color, t});
}

void Feed::configure(size_t capacity, double hold_s) {
  capacity_ = std::max<size_t>(capacity, 1);
  hold_s_ = hold_s;
  if (lines_.size() > capacity_)
    lines_.erase(lines_.begin(), lines_.end() - static_cast<std::ptrdiff_t>(capacity_));
}

void Feed::expire(double t) {
  std::erase_if(lines_, [&](const FeedLine& l) { return strength(l.born, t) <= 0; });
}

double Feed::strength(double born, double t) const {
  const double age = t - born;
  if (age < 0) return 0;
  if (hold_s_ < 0 || age <= hold_s_) return 1;
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
    // A line wider than the column wraps onto the rows under it, read top to bottom.
    const std::vector<std::string> rows = wrapped(st.value_face, s, l.text, w);
    const float tall = lh * static_cast<float>(rows.size());
    const float slot = offset + (1 - rise) * -tall;
    const float top = up ? box.bottom() - tall - slot : box.y + slot;
    const auto alpha = static_cast<float>(a) * rise;
    for (size_t r = 0; r < rows.size(); r++) {
      const float base = top + lh * static_cast<float>(r) + ascent(st.value_face, s);
      draw_shadowed(dl, st.value_face, s, x, base, rows[r],
                    ui::with_alpha(l.color, l.color.a * alpha),
                    ui::with_alpha(st.shadow, st.shadow.a * alpha), f.px(st.shadow_drop), align);
    }
    offset += tall * rise;
  }
  dl.pop_clip();
  return box;
}

}  // namespace ge::hud
