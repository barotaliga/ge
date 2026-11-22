// engine/hud/net_graph: the history keeps the last packets in order, the lost ones marked; the
// ping is the mean round trip of the packets that came and the loss the share that did not;
// r_netgraph takes the game's default; the graph is drawn only when turned on.
//
// ge:summary Tests the net graph: the packet history, ping and loss, r_netgraph, the drawing
// ge:keywords test net-graph r_netgraph ping packet-loss net-history
// ge:skill ui testing
#include "check.hpp"
#include "console/console.hpp"
#include "hud/corner_readouts.hpp"
#include "hud/net_graph.hpp"

namespace gh = ge::hud;

TEST(the_history_keeps_the_last_packets_newest_first) {
  gh::NetHistory h;
  CHECK(gh::net_mark(h, 0) == gh::PacketMark::none);
  CHECK(gh::net_ping_ms(h) == 0 && gh::net_loss_percent(h) == 0);
  CHECK(gh::net_words(h) == "PING --  LOSS 0%");
  gh::net_arrived(h, 40);
  gh::net_lost(h, 2);
  gh::net_arrived(h, 60, true);
  CHECK(gh::net_mark(h, 0) == gh::PacketMark::stale && gh::net_round_trip(h, 0) == 60);
  CHECK(gh::net_mark(h, 1) == gh::PacketMark::lost && gh::net_mark(h, 2) == gh::PacketMark::lost);
  CHECK(gh::net_mark(h, 3) == gh::PacketMark::arrived && gh::net_round_trip(h, 3) == 40);
  CHECK(gh::net_mark(h, 4) == gh::PacketMark::none);
  CHECK(gh::net_ping_ms(h) == 50);
  CHECK(gh::net_loss_percent(h) == 50);
  CHECK(gh::net_words(h) == "PING 50  LOSS 50%");
  gh::net_arrived(h, -1);  // no round trip measured yet: not in the ping
  CHECK(gh::net_mark(h, 0) == gh::PacketMark::arrived && gh::net_ping_ms(h) == 50);
}

TEST(the_oldest_packets_are_overwritten) {
  gh::NetHistory h;
  gh::net_lost(h, 1000);  // a long gap counts no more than the graph holds
  CHECK(h.count == gh::kNetPackets && gh::net_loss_percent(h) == 100);
  for (int i = 0; i < gh::kNetPackets; i++) gh::net_arrived(h, 20);
  CHECK(gh::net_loss_percent(h) == 0 && gh::net_ping_ms(h) == 20);
  gh::net_lost(h, 16);
  CHECK(gh::net_loss_percent(h) == 25);
  CHECK(gh::net_mark(h, gh::kNetPackets) == gh::PacketMark::none);
}

TEST(r_netgraph_takes_the_games_default_and_draws_only_when_on) {
  ge::console::Console c;
  gh::ReadoutSettings s;
  s.net = 1;  // a game that shows the graph unless the player hides it
  gh::add_readout_cvars(c.cvars(), s);
  const ge::console::Cvar* v = c.cvars().find("r_netgraph");
  CHECK(v != nullptr && v->fallback == "1" && v->is_default());
  c.execute("r_netgraph 0");
  CHECK(s.net == 0);
  gh::NetHistory h;
  gh::net_arrived(h, 80);
  gh::net_lost(h, 1);
  const gh::Style style;
  const gh::Frame frame = gh::frame_for(1280, 720, style);
  ge::ui::DrawList off;
  off.begin(1280, 720);
  CHECK(gh::draw_net_graph(off, frame, style, h, s.net) == 0);
  c.execute("r_netgraph 1");
  ge::ui::DrawList on;
  on.begin(1280, 720);
  CHECK(gh::draw_net_graph(on, frame, style, h, s.net, 40) > 0);
}

TEST(the_lag_graph_draws_a_column_a_packet_on_its_box) {
  gh::NetHistory h;
  ge::ui::DrawList none;
  none.begin(320, 240);
  gh::draw_lag_graph(none, h, {200, 100, 48, 48}, {});
  CHECK(none.size() == 0);  // nothing recorded, no box asked
  gh::net_arrived(h, 100);  // half way up 0..200 ms
  gh::net_lost(h, 1);       // a whole column
  gh::net_arrived(h, 500);  // held to the top
  ge::ui::DrawList dl;
  dl.begin(320, 240);
  gh::draw_lag_graph(dl, h, {200, 100, 48, 48}, {.box = true});
  CHECK(dl.size() == 4);  // the box and three columns
  const auto& c = dl.cmds();
  CHECK(c[1].box.rect.x == 247 && c[1].box.rect.h == 48);  // the newest on the right, at the top
  CHECK(c[2].box.rect.h == 48);                            // the lost one, all the way
  CHECK(c[3].box.rect.x == 245 && c[3].box.rect.h == 24);  // 100 of 200 ms: half
}
