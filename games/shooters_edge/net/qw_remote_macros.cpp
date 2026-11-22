// The joined server's macros (qw_remote_macros.hpp).
//
// ge:summary The connection's macros and the rates stepped while joined
// ge:keywords cl_ping cl_lag cl_pps cl_mps net_uprate net_dnrate remote_rates_step serverip
// ge:keywords connectiontype latency ping
// ge:skill net console
// ge:test tests/shooters_edge_qw_remote_macros_test.cpp
#include "shooters_edge/net/qw_remote_macros.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "core/num_text.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

std::string lag_text(const ge::hud::NetHistory& h) {
  int lost = 0, came = 0;
  for (int i = 0; i < ge::hud::kNetPackets; i++) {
    const ge::hud::PacketMark m = ge::hud::net_mark(h, i);
    if (m == ge::hud::PacketMark::lost)
      lost++;
    else if (m == ge::hud::PacketMark::arrived || m == ge::hud::PacketMark::stale)
      came++;
  }
  const double share = came > 0 ? 100.0 * lost / came : 0;
  return ge::core::num::to_fixed(share, 2) + "%";
}

void remote_rates_step(Remote& r, double wall_s) {
  if (r.transport == nullptr) return;
  const int moves = r.q2 ? r.q2->tick : r.end.tick;
  ge::net::rates_step(r.rates, wall_s, *r.transport, static_cast<uint64_t>(moves < 0 ? 0 : moves));
}

void add_remote_macros(ge::console::Console& con, Remote& r) {
  con.add_macro("cl_server", [&r] { return r.active ? r.server_name : std::string(); });
  // The older clients' names: $serverip (the server joined), $connectiontype (disconnected,
  // spectator or player), $latency and $ping (the round trip, ms).
  con.add_macro("serverip", [&r] { return r.active ? r.server_name : std::string(); });
  con.add_macro("connectiontype", [&r] {
    if (!r.active) return std::string("disconnected");
    const client::DemoState& s = r.demo.state;
    const bool watching = r.watch != nullptr ||
                          (s.player >= 0 && static_cast<size_t>(s.player) < s.spectator.size() &&
                           s.spectator[static_cast<size_t>(s.player)]);
    return std::string(watching ? "spectator" : "player");
  });
  con.add_macro("cl_ping", [&r] {
    const ge::hud::NetHistory* h = remote_net(r);
    return std::to_string(h != nullptr ? std::lround(ge::hud::net_ping_ms(*h)) : 0);
  });
  for (const char* name : {"latency", "ping"})
    con.add_macro(name, [&r] {
      const ge::hud::NetHistory* h = remote_net(r);
      return std::to_string(h != nullptr ? std::lround(ge::hud::net_ping_ms(*h)) : 0);
    });
  con.add_macro("cl_lag", [&r] {
    const ge::hud::NetHistory* h = remote_net(r);
    return h != nullptr ? lag_text(*h) : std::string("0.00%");
  });
  // A rate a second, whole; 0 while nothing is joined.
  const auto rate = [&r](double ge::net::Rates::*field) {
    return [&r, field] {
      const bool joined = r.active && !r.watch;
      return std::to_string(joined ? std::lround(r.rates.*field) : 0);
    };
  };
  // $cl_fps: the client's frames a second; ours builds one move a frame, so the moves' rate.
  con.add_macro("cl_fps", rate(&ge::net::Rates::counted));
  // $cl_numentities: the entities in the newest frame read; $cl_weaponmodel: the held weapon's
  // model as the server names it ("models/weapons/v_rail/tris.md2"; "" when none or not joined).
  con.add_macro("cl_numentities", [&r] {
    int n = 0;
    if (r.active && !r.watch)
      for (const client::DemoEntity& e : r.demo.state.entities) n += e.present ? 1 : 0;
    return std::to_string(n);
  });
  con.add_macro("cl_weaponmodel", [&r] {
    if (!r.active || r.watch || !r.q2 || r.demo.last_frame < 0) return std::string();
    const int gun = r.demo.views[static_cast<size_t>(r.demo.last_frame & 15)].gun_model;
    const std::vector<std::string>& models = r.demo.state.models;
    return gun > 0 && static_cast<size_t>(gun) < models.size() ? models[static_cast<size_t>(gun)]
                                                               : std::string();
  });
  con.add_macro("cl_pps", rate(&ge::net::Rates::sent_packets));
  con.add_macro("cl_mps", rate(&ge::net::Rates::counted));
  con.add_macro("net_uprate", rate(&ge::net::Rates::sent_bytes));
  con.add_macro("net_dnrate", rate(&ge::net::Rates::received_bytes));
}

}  // namespace shooters_edge::net
