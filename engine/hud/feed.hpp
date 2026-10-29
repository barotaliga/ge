// THE FEED: a column of short lines that arrive, hold and fade, the newest nearest its corner. A
// racing game's news (a gate's split, a tree brushed, a landing's air), a shooter's kill feed and
// a chat's last lines are all one of these. The game pushes a line when its event happens and the
// HUD draws the feed every frame at the frame's time; the feed forgets a line once it has faded,
// and the oldest when it is full.
//
// Time is the caller's (the run's clock, in s), so a recorded run's feed draws the same frames.
// A line holds `hold_s` at full strength, then fades over `fade_s`; it slides in over its first
// `rise_s`, so the column moves rather than jumps when a line arrives.
//
// ge:summary The HUD's feed: lines that arrive, hold and fade, newest nearest the corner
// ge:keywords hud feed news kill-feed chat log toast notification line hold fade push
// ge:keywords draw_feed column
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hud/hud.hpp"

namespace ge::hud {

struct FeedLine {
  std::string text;
  ui::Color color{1, 1, 1, 1};
  double born = 0;  // s, the caller's clock when it was pushed
};

class Feed {
 public:
  explicit Feed(size_t capacity = 5, double hold_s = 3.5, double fade_s = 0.8, double rise_s = 0.2);

  void push(std::string_view text, ui::Color color, double t);
  // Forgets every line faded out by `t`.
  void expire(double t);
  void clear() { lines_.clear(); }

  // 0..1: how strongly a line pushed at `born` shows at `t`.
  double strength(double born, double t) const;
  // 0..1: how far into its place a line pushed at `born` has slid at `t`.
  double risen(double born, double t) const;

  const std::vector<FeedLine>& lines() const { return lines_; }  // oldest first
  size_t capacity() const { return capacity_; }

 private:
  size_t capacity_;
  double hold_s_, fade_s_, rise_s_;
  std::vector<FeedLine> lines_;
};

// The feed drawn at `anchor` (a bottom anchor stacks the newest lowest, a top one highest), each
// line in the style's value face at `size` reference px. Returns the box the whole column may
// take at full strength (capacity lines of the widest the frame allows: `max_w` reference px),
// which the layout check holds clear of the HUD's other boxes.
ui::Rect draw_feed(ui::DrawList& dl, const Frame& f, const Style& st, const Feed& feed, double t,
                   Anchor anchor, float size, float max_w, float dx = 0, float dy = 0);

}  // namespace ge::hud
