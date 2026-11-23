// THE 2D SWITCH AND THE GRAPH ALONG THE BOTTOM, EVERY CLASSIC GAME'S: what is drawn over the view
// (`scr_draw2d`: 0 nothing, for screenshots; 1 all but the status bar; 2 everything; 3 all but the
// scoreboard and the help computer) and the frame-time graph (`timegraph`: each frame's time a
// column along the bottom of the screen, a unit tall a millisecond times `graphscale` plus
// `graphshift`, held to `graphheight`; `debuggraph` the graph drawn though nothing feeds it).
// Registered once in the classic layer (client/cvar_layers.cpp), so a value set in one game holds
// in the next; Quake's `r_timegraph` is a synonym of `timegraph`, QuakeWorld's `r_drawhud` of
// `scr_draw2d` with its values mapped (0 to 0, 1 to 2). Quake II's bar
// (client/q2_bar.cpp) and every other game's HUD (`draw_hud_2d`) honour the same switch.
//
// ge:summary The 2D switch and the frame-time graph every classic game shares, their cvars and draw
// ge:keywords scr_draw2d r_drawhud timegraph r_timegraph debuggraph
// ge:keywords graphheight graphscale graphshift
// ge:keywords bottom-graph hud-switch screenshot frame-time
// ge:skill ui console
// ge:test tests/shooters_edge_screen_2d_test.cpp
#pragma once

#include <memory>
#include <vector>

#include "console/cvar.hpp"
#include "hud/bottom_graph.hpp"
#include "hud/layout.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

struct Screen2D {
  int draw2d = 2;  // scr_draw2d: 0 nothing, 1 all but the status bar, 2 all, 3 no scoreboard
  // timegraph each frame's time in ms; debuggraph the graph drawn though nothing feeds it;
  // graphheight, graphscale, graphshift its height, units a millisecond and lift.
  bool time_graph = false, debug_graph = false;
  int graph_height = 32;
  double graph_scale = 1, graph_shift = 0;
};

// scr_draw2d, timegraph, debuggraph, graphheight, graphscale and graphshift bound to `s` (which
// must outlive `v`), with Quake's spelling r_timegraph and QuakeWorld's r_drawhud.
void add_screen_2d_cvars(ge::console::Cvars& v, Screen2D& s);

// The graph's samples and the time the last one was taken at (-1: none yet).
struct ScreenGraph {
  std::unique_ptr<ge::hud::BottomGraph> graph = std::make_unique<ge::hud::BottomGraph>();
  double at = -1;
};

// The graph along the bottom of a `width` x `height` screen drawn `scale` times over, fed this
// frame's time from the real time `now` (s) while timegraph is on; drawn while timegraph or
// debuggraph is and scr_draw2d is not 0. Returns whether it drew. `s` null: the defaults (off).
bool draw_screen_graph(ge::ui::DrawList& dl, ScreenGraph& g, const Screen2D* s, double now,
                       int width, int height, int scale, float alpha = 1);

// The HUD's layout drawn as scr_draw2d asks: 0 none of it, 1 the crosshair alone, 2 and 3 all
// of it (3 leaves out the scoreboard, which `shows_scores` answers). Returns what was placed.
std::vector<ge::hud::Placed> draw_hud_2d(ge::ui::DrawList& dl, ge::hud::Layout& layout,
                                         const ge::hud::Frame& f, const ge::hud::Style& st,
                                         const Screen2D& s);
// Whether the scoreboard may be drawn: scr_draw2d 1 and 2.
inline bool shows_scores(const Screen2D& s) { return s.draw2d == 1 || s.draw2d == 2; }

}  // namespace shooters_edge::client
