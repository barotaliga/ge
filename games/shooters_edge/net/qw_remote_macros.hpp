// THE JOINED SERVER'S MACROS: what a bind, `say` or `draw` reads of the connection, whichever
// protocol the server speaks. `$cl_server` the server as asked ("" when none is joined); `$cl_ping`
// the mean round trip of the last packets, whole milliseconds; `$cl_lag` the packets lost over
// those that came, a percentage with two decimals ("1.25%", "0.00%" with nothing come); `$cl_pps`
// the packets sent a second and `$cl_mps` the moves; `$net_uprate` and `$net_dnrate` the bytes
// sent and received a second; `$cl_fps` the client's frames a second (one move a frame, so
// `$cl_mps`'s figure); `$cl_numentities` the entities in the newest frame and `$cl_weaponmodel` the
// held weapon's model (Quake II's servers). The rates are the last whole second's (net/rates.hpp),
// stepped by `remote_step`; nothing joined, each reads 0.
//
// ge:summary The connection's macros: $cl_ping, $cl_lag, $cl_pps, $cl_mps, $net_uprate, $net_dnrate
// ge:keywords cl_ping cl_lag cl_pps cl_mps net_uprate net_dnrate cl_server macros rates
// ge:keywords remote-macros cl_fps cl_numentities cl_weaponmodel
// ge:skill net console
// ge:test tests/shooters_edge_qw_remote_macros_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "hud/net_graph.hpp"

namespace shooters_edge::net {

struct Remote;

// The lost packets over those that came, as `$cl_lag` says it ("1.25%").
std::string lag_text(const ge::hud::NetHistory& h);
// Steps `r.rates` at `wall_s` (the window's clock, s) from the joined server's transport.
void remote_rates_step(Remote& r, double wall_s);
// The macros above on `con`, reading `r` (which outlives the console).
void add_remote_macros(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
