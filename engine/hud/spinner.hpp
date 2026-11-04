// THE LOADING SPINNER: what any game shows in a corner while it works in the background (a map
// read, its textures decoded, its pipelines compiled), so a load is seen to be moving and the
// window never looks frozen. A faint full ring and a bright arc chasing round it, the arc's length
// breathing so the motion reads even at a glance, and beside it a caption (LOADING DM6) and the
// seconds it has taken so far. It is a function of the clock alone: the caller draws it every
// frame the work is in flight and stops drawing it when the work is done.
//
// ge:summary The loading spinner: a ring and a chasing arc in a corner, a caption, the seconds
// ge:keywords hud spinner loading busy progress throbber ring arc corner caption background-load
// ge:keywords draw_spinner
// ge:skill ui
// ge:test tests/hud_spinner_test.cpp
#pragma once

#include <string_view>

#include "hud/hud.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

struct Spinner {
  Anchor corner = Anchor::bottom_right;
  float radius = 18;      // px at the style's reference size, to the middle of the ring
  float width = 4;        // px across the ring
  double period_s = 1.1;  // one turn of the arc
  ui::Color ring = ui::hex(0xffffff, 0.18f);
  ui::Color arc = ui::hex(0xffffff, 0.92f);
};

// The spinner at time `t_s` in `frame`'s corner, `caption` beside it (empty: none) and, when
// `elapsed_s` is not negative, the seconds so far under it. Returns the rectangle it took.
ui::Rect draw_spinner(ui::DrawList& dl, const Frame& frame, const Style& style, double t_s,
                      std::string_view caption, double elapsed_s = -1, const Spinner& s = {});

}  // namespace ge::hud
