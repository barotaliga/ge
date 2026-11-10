// THE FRAME RATE ON SCREEN (`scr_showfps`, platform/pacing.hpp): the rate in the top-right corner
// and, at mode 2, the last frames' intervals as a bar graph under it, one bar a frame, newest on
// the right. A dashed line marks the display's refresh interval: on a paced, vsynced run every bar
// stands at it; a bar that reaches twice its height is a refresh missed (a repeated picture), and
// bars that alternate are judder. Bars over 1.5 times the refresh interval are drawn in the
// style's `bad` colour.
//
// ge:summary The frame rate and the frame-time graph drawn in a corner (scr_showfps)
// ge:keywords hud fps showfps scr_showfps frame-time graph frametime-graph pacing judder hitch
// ge:keywords draw_frame_rate
// ge:skill ui perf
// ge:test tests/hud_frame_graph_test.cpp
#pragma once

#include "hud/hud.hpp"
#include "loop/pacing.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

inline constexpr int kGraphFrames = 120;  // the graph's bars
inline constexpr float kGraphMs = 33.4f;  // the graph's height, ms

// Mode 1: the rate; mode 2: with the graph; 0 draws nothing. `refresh_hz` 0 when unknown (no
// line). Returns the rectangle it took (empty for mode 0).
ui::Rect draw_frame_rate(ui::DrawList& dl, const Frame& frame, const Style& style,
                         const loop::FrameTimes& times, double refresh_hz, int mode);

}  // namespace ge::hud
