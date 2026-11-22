// games/shooters_edge/net/qw_remote_macros.hpp: the connection's macros read 0 (and "0.00%") with
// nothing joined, and `$cl_lag` is the packets lost over those that came, with two decimals.
//
// ge:summary Test: the connection's macros unjoined and the lag's words
// ge:keywords test cl_lag cl_ping cl_pps net_uprate macros remote-macros
// ge:skill net testing
#include "shooters_edge/net/qw_remote_macros.hpp"

#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/net_graph.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace cn = shooters_edge::net;

TEST(lag_is_lost_over_came_with_two_decimals) {
  ge::hud::NetHistory h;
  CHECK(cn::lag_text(h) == "0.00%");
  for (int i = 0; i < 40; i++) ge::hud::net_arrived(h, 30);
  CHECK(cn::lag_text(h) == "0.00%");
  ge::hud::net_lost(h, 1);
  CHECK(cn::lag_text(h) == "2.50%");  // 1 lost over 40 come
}

TEST(macros_read_nothing_with_nothing_joined) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  CHECK(con.expand_text("$cl_ping $cl_lag $cl_pps $cl_mps $net_uprate $net_dnrate") ==
        "0 0.00% 0 0 0 0");
  CHECK(con.expand_text("[$cl_server]") == "[]");
}
