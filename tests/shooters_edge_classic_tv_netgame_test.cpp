// games/shooters_edge/net/classic_tv_netgame.hpp: a recording served as a netgame on the
// single-player protocol, on the simulated network. A scripted client written from the transport's
// facts (porting/shooters-edge/netgame-transport.md, not from our face) connects, reassembles the
// reliable messages, acknowledges every fragment, answers each `signonnum` with the stage's
// commands and then takes frame datagrams: the server's time rising, entity updates, the recorded
// view as a `setangle`. At the end of the recording it is sent "reconnect" and the level again.
// Over a lossy link the same client still gets in, by resends.
//
// ge:summary Test: the netgame face, a scripted single-player client watching a recording
// ge:keywords test netgame single-player protocol-15 signon reliable resend loop reconnect
// ge:skill net shooter testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/classic_tv_netgame.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

namespace {

constexpr uint16_t kPortTv = 26000, kPortClient = 27001;

void be32(std::vector<uint8_t>& o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>(v >> s));
}
uint32_t rd32(const uint8_t* p) {
  return uint32_t{p[0]} << 24 | uint32_t{p[1]} << 16 | uint32_t{p[2]} << 8 | p[3];
}

// The client side of the transport and the signon, enough to watch.
struct Client {
  gn::Transport* t = nullptr;
  gn::Address server;
  bool accepted = false;
  uint32_t recv_seq = 0, send_seq = 0, unrel_out = 0;
  std::vector<uint8_t> partial;
  int signon = 0;
  int reliable_messages = 0, frames = 0, entity_updates = 0, setangles = 0, reconnects = 0;
  float last_time = -1;
  bool time_fell = false;
  bool pending = false;  // a reliable command sent and not yet acknowledged
  std::vector<uint8_t> in_flight;
  int64_t flight_us = 0;
  std::vector<std::string> outbox;
  cl::Demo view;  // what the client knows, read by the game's own reader of the protocol
  std::string read_error;

  void read(const std::vector<uint8_t>& m) {
    cl::DemoMsg dm;
    dm.b = m;
    if (!cl::read_single_messages(view, dm, read_error)) read_error += " (a message did not read)";
  }

  void send_raw(const std::vector<uint8_t>& d, int64_t now) {
    gn::transport_send(*t, server, d.data(), d.size(), now);
  }
  void connect(int64_t now) {
    std::vector<uint8_t> d;
    const char game[] = {'Q', 'U', 'A', 'K', 'E', 0};  // the game's name, a wire constant
    be32(d, 0x80000000u | static_cast<uint32_t>(4 + 1 + sizeof(game) + 1));
    d.push_back(0x01);
    d.insert(d.end(), game, game + sizeof(game));
    d.push_back(3);
    send_raw(d, now);
  }
  void command(const std::string& c) { outbox.push_back(c); }
  void flush(int64_t now) {
    if (pending && now - flight_us >= 1000000) {  // resent after a second, as the client does
      send_raw(in_flight, now);
      flight_us = now;
    }
    if (pending || outbox.empty()) return;
    std::vector<uint8_t> body;
    for (const std::string& c : outbox) {
      body.push_back(4);
      body.insert(body.end(), c.begin(), c.end());
      body.push_back(0);
    }
    outbox.clear();
    std::vector<uint8_t> d;
    be32(d, 0x00090000u | static_cast<uint32_t>(8 + body.size()));
    be32(d, send_seq);
    d.insert(d.end(), body.begin(), body.end());
    send_raw(d, now);
    in_flight = d;
    flight_us = now;
    pending = true;
  }
  // The server's messages: the ones this client acts on read, the rest stepped over only where it
  // must be (a frame's messages are counted by their first byte).
  void messages(const std::vector<uint8_t>& m, bool reliable) {
    if (m.size() == 1 && m[0] == 1) return;  // a keepalive
    read(m);
    if (!reliable) {
      frames++;
      if (m[0] == 7) {
        float at = 0;
        std::memcpy(&at, m.data() + 1, 4);
        if (at < last_time) time_fell = true;
        last_time = at;
      }
      for (const cl::DemoEntity& e : view.state.entities) entity_updates += e.present ? 1 : 0;
      if (m.size() >= 4 && m[m.size() - 4] == 10) setangles++;
      return;
    }
    reliable_messages++;
    // Find the signon numbers and stuffed text by their ids where they stand alone at the end.
    for (size_t i = 0; i + 1 < m.size(); i++) {
      if (m[i] == 25 && i + 2 == m.size()) {
        const int n = m[i + 1];
        if (n == signon + 1) {
          signon = n;
          if (n == 1) command("prespawn");
          if (n == 2) command("name \"viewer\"\n"), command("color 0 0\n"), command("spawn ");
          if (n == 3) command("begin");
        }
      }
    }
    static const std::string kRe = "reconnect\n";
    for (size_t i = 0; i + kRe.size() + 1 < m.size() + 1; i++)
      if (m[i] == 9 && std::memcmp(m.data() + i + 1, kRe.data(), kRe.size()) == 0) {
        reconnects++;
        signon = 0;
      }
  }
  void tick(int64_t now) {
    uint8_t buf[2048];
    gn::Address from;
    for (;;) {
      const int n = gn::transport_receive(*t, buf, sizeof buf, from, now);
      if (n < 0) break;
      const uint32_t w = rd32(buf);
      if ((w & 0x80000000u) != 0) {
        if (buf[4] == 0x81) {
          accepted = true;
          server.port = static_cast<uint16_t>(buf[5] | buf[6] << 8);
        }
        continue;
      }
      if (from != server) continue;
      const uint32_t seq = rd32(buf + 4);
      const std::vector<uint8_t> payload(buf + 8, buf + (w & 0xFFFF));
      if ((w & 0x00020000u) != 0) {
        if (pending && seq == send_seq) pending = false, send_seq++;
      } else if ((w & 0x00010000u) != 0) {
        std::vector<uint8_t> ack;
        be32(ack, 0x00020008u);
        be32(ack, seq);
        send_raw(ack, now);
        if (seq != recv_seq) continue;
        recv_seq++;
        partial.insert(partial.end(), payload.begin(), payload.end());
        if ((w & 0x00080000u) != 0) {
          const std::vector<uint8_t> msg = std::move(partial);
          partial.clear();
          messages(msg, true);
        }
      } else if ((w & 0x00100000u) != 0) {
        messages(payload, false);
      }
    }
    flush(now);
    if (signon == 3 && frames > 0) {  // a move a frame once watching, as a client does
      std::vector<uint8_t> d;
      be32(d, 0x00100000u | 24u);
      be32(d, unrel_out++);
      d.push_back(3);
      d.resize(24, 0);
      send_raw(d, now);
    }
  }
};

struct Run {
  gn::SimNetwork net;
  gn::Transport face_t, client_t;
  cn::NetgameTv tv;
  Client c;
  int64_t now = 0;
  explicit Run(const char* conditions, int frames) {
    gn::LinkConditions lc;
    gn::conditions_preset(conditions, lc);
    net = gn::make_sim_network(2, lc);
    gn::open_sim_transport(face_t, net, gn::sim_address(0, kPortTv));
    gn::open_sim_transport(client_t, net, gn::sim_address(1, kPortClient));
    std::string error;
    const cl::DemoScript s = cl::loop_script("halls", {128, 192, 24}, 64, frames);
    CHECK_MSG(cn::udp_tv_netgame_open(tv, cl::write_single_demo(s), true, error), error);
    cn::udp_tv_netgame_start(tv, face_t, {});
    c.t = &client_t;
    c.server = gn::sim_address(0, kPortTv);
  }
  void run_to(double s) {
    while (now < static_cast<int64_t>(s * 1e6)) {
      now += 13000;
      gn::sim_advance(net, now);
      if (!c.accepted && now % 500000 < 13000) c.connect(now);
      cn::udp_tv_netgame_frame(tv, now);
      c.tick(now);
    }
  }
};

}  // namespace

TEST(a_single_player_client_signs_on_and_watches_the_recording) {
  Run r("lan", 400);
  CHECK(r.tv.levels.size() == 1);
  r.run_to(3);
  CHECK(r.c.accepted);
  CHECK(r.c.server.port == kPortTv);  // the accept names the same port
  CHECK_MSG(r.c.signon == 3, "the three signon stages, one after each answer");
  CHECK(cn::udp_tv_netgame_viewers(r.tv) == 1);
  CHECK(r.tv.viewers[0].stage == cn::NetgameStage::playing);
  CHECK_MSG(r.c.frames > 50, "frame datagrams once seated");
  CHECK(r.c.entity_updates > 0);
  CHECK(r.c.setangles > 50);  // the recorded view, a frame at a time
  CHECK(!r.c.time_fell);
  CHECK_MSG(r.c.read_error.empty(), r.c.read_error);
  CHECK(r.c.view.state.map == "halls");
}

TEST(the_end_of_the_recording_reconnects_the_viewer_to_the_start) {
  Run r("lan", 200);
  r.run_to(12);
  CHECK(r.tv.rounds >= 1);
  CHECK(r.c.reconnects >= 1);
  CHECK_MSG(r.c.signon == 3, "signed on again after the loop");
  CHECK(r.tv.viewers[0].stage == cn::NetgameStage::playing);
}

TEST(a_lossy_link_still_signs_on_by_resends) {
  Run r("awful", 600);
  r.run_to(20);
  CHECK(r.c.accepted);
  CHECK_MSG(r.c.signon == 3, "every fragment lost is sent again");
  CHECK(r.c.frames > 20);
}

TEST(anything_but_a_recording_is_refused_with_a_reason) {
  cn::NetgameTv tv;
  std::string error;
  CHECK(!cn::udp_tv_netgame_open(tv, std::vector<uint8_t>{1, 2, 3}, true, error));
  CHECK(!error.empty());
}
