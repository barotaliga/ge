// THE GRAPH ALONG THE BOTTOM: a development graph across the whole width of a game's 2D screen, one
// column a sample in its own colour, newest at the right, as Quake II draws `timegraph` and
// `debuggraph`. A column stands `value x scale + shift` units of the screen tall, held to
// 0..`height`; a column of 0 is still drawn a unit tall, so the line shows where samples are.
//
// ANY GAME feeds it (`bottom_graph_add`, once a frame or once an event) and draws it with its own
// screen's scale; the game's own settings say when and how tall. Allocation-free.
//
// ge:summary The graph along the bottom of the screen: samples as columns (timegraph, debuggraph)
// ge:keywords hud bottom-graph debug-graph timegraph debuggraph graphheight graphscale graphshift
// ge:keywords bottom_graph_add draw_bottom_graph frame-time-columns
// ge:skill ui perf
// ge:test tests/hud_bottom_graph_test.cpp
#pragma once

#include <cstdint>

#include "ui/draw.hpp"

namespace ge::hud {

inline constexpr int kBottomColumns = 1024;  // the samples kept (wider screens see no more)

struct BottomGraph {
  float value[kBottomColumns] = {};
  ui::Color color[kBottomColumns] = {};
  uint32_t count = 0;  // samples added in all
};

struct BottomGraphLook {
  float height = 32;  // the graph's height, units of the screen (graphheight)
  float scale = 1;    // a sample's units a unit of value (graphscale)
  float shift = 0;    // units added to every column (graphshift)
  float alpha = 1;    // every colour's opacity times this
};

// One sample, the newest.
void bottom_graph_add(BottomGraph& g, float value, ui::Color color);
// The column a sample stands, units: `value x scale + shift` held to 1..height (0 when the graph
// has no height).
float bottom_graph_column(float value, const BottomGraphLook& look);
// Along the bottom of a `width` x `height` px screen at `unit` px a unit, a column a unit wide,
// the newest at the right; as many samples as the width holds.
void draw_bottom_graph(ui::DrawList& dl, const BottomGraph& g, float width, float height,
                       float unit, const BottomGraphLook& look);

}  // namespace ge::hud
