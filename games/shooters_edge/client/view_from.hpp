// A PICTURE OF ONE SPOT: the window's tools for drawing the world from a place a player names.
//
// - `viewfrom` prints the place the world was last drawn from as the line that draws it again,
//   the game and the map after it as a comment: `r_viewfrom "5.12 -0.45 23.60 -210.4 30.5" //
//   q1 start` (metres, y up, and degrees), so a player can paste where a fault shows.
// - `r_viewfrom` (never saved) draws the world from that place instead of the player's eye; the
//   game goes on as it was. With `-hidden` and `screenshot` a script takes the picture without a
//   window on the screen; the server's `setpos` (a cheat) stands the player there too.
//
// ge:summary The window's view tools: viewfrom prints where you look from, r_viewfrom draws it
// ge:keywords viewfrom r_viewfrom view-from camera picture screenshot spot repro debug-view
// ge:skill shooter render
// ge:test tests/shooters_edge_view_from_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "render/view.hpp"

namespace shooters_edge::client {

struct ViewFrom {
  std::string asked;           // r_viewfrom's value ("" the player's eye)
  ge::render::FrameView last;  // the view the world was last drawn from
  std::string place;           // the game and the map it was drawn on ("q1 start")
};

// `r_viewfrom` and `viewfrom` on `con`, over `v` (which must outlive `con`).
void add_view_from(ge::console::Console& con, ViewFrom& v);

// The view about to be drawn on `place`: r_viewfrom's place put in it when one is set, then kept as
// the one `viewfrom` prints.
void apply_view_from(ViewFrom& v, ge::render::FrameView& view, std::string place);

// The line `viewfrom` prints for `view` on `place`.
std::string view_from_line(const ge::render::FrameView& view, const std::string& place);

}  // namespace shooters_edge::client
