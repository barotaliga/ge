// THE TOP-RIGHT CORNER: the frame rate (`scr_showfps`, hud/frame_graph.hpp) and under it the
// engine's corner readouts (`show_speed`, `scr_clock`, hud/corner_readouts.hpp) and, in a game
// joined on a server, the net graph (`r_netgraph`, hud/net_graph.hpp), read from the
// player's own game: his speed from his velocity, the game's time since he entered the map, the
// time of day from the computer's clock.
//
// ge:summary Shooter's Edge's top-right corner: the frame rate, the speed and the clock
// ge:keywords show_speed scr_clock scr_showfps r_netgraph corner-readouts corner_readings
// ge:keywords draw_corners
// ge:skill ui
// ge:test tests/hud_corner_readouts_test.cpp
#pragma once

#include <ctime>

#include "hud/corner_readouts.hpp"
#include "hud/net_graph.hpp"
#include "platform/pacing.hpp"
#include "shooters_edge/client/listen.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

// What the readouts show now: from `l` when a game is hosted (its player's speed, the time on this
// map), else no speed and `fallback_s` as the game's time (a demo's clock); the time of day
// always.
ge::hud::Readings corner_readings(const Listen* l, double fallback_s = 0);

// The frame rate, then the readouts under it, then the net graph when `net` is a server's packets
// (nullptr: no server, no graph), in the top-right corner of a `w` x `h` screen.
void draw_corners(ge::ui::DrawList& dl, int w, int h, const ge::platform::Pacing& pacing,
                  const ge::hud::ReadoutSettings& s, const ge::hud::Readings& r,
                  const ge::hud::NetHistory* net = nullptr);

// The local date and time now (the wall clock).
std::tm local_time();

}  // namespace shooters_edge::client
