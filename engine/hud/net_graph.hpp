// THE NET GRAPH (`r_netgraph`): how the packets from a server arrive, drawn in the top-right
// corner under the other readouts. One bar a packet, newest on the right: its height is the round
// trip the client measured when it came (the newest move it acknowledged), green when it came
// whole, yellow when it named a frame the client no longer held (the picture waited for a full
// one), and a red bar to the top for each packet that never came. Above the bars, the ping (the
// mean round trip of the packets that came) and the loss (the share that never came), over the
// last `kNetPackets`.
//
// THE LAG GRAPH (a smaller square of the same history, as Quake II's `scr_lag_draw` draws it): the
// last `kLagColumns` packets one column each, newest on the right, a column's height the round
// trip between the look's `min_ms` and `max_ms`; a packet lost a whole column in the critical
// colour, a stale one in the warning colour; on a dark box when the look asks. The game places and
// scales it (in its 2D screen's pixels) and sets it.
//
// ANY GAME'S CLIENT records into a `NetHistory` as it reads its packets (`net_arrived`,
// `net_lost`) and the kit draws it; the setting is registered with the corner readouts
// (corner_readouts.hpp's `add_readout_cvars`).
//
// ge:summary The net graph (r_netgraph): each packet's round trip, the lost ones, ping and loss
// ge:keywords hud net-graph netgraph r_netgraph ping packet-loss lost-packets round-trip
// ge:keywords net-history net_arrived net_lost net_ping_ms net_loss_percent draw_net_graph
// ge:keywords lag-graph draw_lag_graph scr_lag_draw
// ge:skill ui net
// ge:test tests/hud_net_graph_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "hud/hud.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

inline constexpr int kNetPackets = 64;     // the packets the graph keeps
inline constexpr float kNetGraphMs = 300;  // the graph's height, ms of round trip

enum class PacketMark : uint8_t {
  none,     // no packet recorded in this slot yet
  arrived,  // came whole
  stale,    // came, but named a frame the client no longer held
  lost,     // never came
};

// The last packets, oldest overwritten. Allocation-free.
struct NetHistory {
  float ms[kNetPackets] = {};
  PacketMark mark[kNetPackets] = {};
  uint32_t count = 0;  // packets recorded in all
};

// A packet came, `round_trip_ms` the client's newest round trip (negative: none measured yet, as
// while it signs on: a short grey bar, left out of the ping), `stale` when it could not be applied
// to a frame the client held.
void net_arrived(NetHistory& h, double round_trip_ms, bool stale = false);
// `n` packets never came (a gap in the sequence before the one that did).
void net_lost(NetHistory& h, uint32_t n);
// The `i`th newest packet (0 the newest); `none` past what was recorded.
PacketMark net_mark(const NetHistory& h, int i);
float net_round_trip(const NetHistory& h, int i);
// The mean round trip of the kept packets that came (0 when none did), and the kept packets'
// share that never came, in whole percent.
double net_ping_ms(const NetHistory& h);
int net_loss_percent(const NetHistory& h);
// "PING 45  LOSS 2%" ("PING --" before any round trip is measured).
std::string net_words(const NetHistory& h);

// Draws the graph in the top-right corner `top` px below the safe area's top when `mode` is 1;
// returns its box's height in px (0 when off).
float draw_net_graph(ui::DrawList& dl, const Frame& frame, const Style& style, const NetHistory& h,
                     int mode, float top = 0);

inline constexpr int kLagColumns = 48;  // the lag graph's columns (and its height, in its pixels)

struct LagGraphLook {
  float min_ms = 0, max_ms = 200;  // the round trips at the bottom and the top
  bool box = false;                // on a dark box
  float alpha = 1;                 // every colour's opacity times this
  ui::Color ok = ui::hex(0xe0e0e0), warn = ui::hex(0xf2c94c), lost = ui::hex(0xeb5757);
  ui::Color back = {0, 0, 0, 0.4f};
};

// Draws the lag graph of `h` in `at` (its columns `at.w / kLagColumns` wide). Nothing before a
// packet is recorded but the box.
void draw_lag_graph(ui::DrawList& dl, const NetHistory& h, ui::Rect at, const LagGraphLook& look);

}  // namespace ge::hud
