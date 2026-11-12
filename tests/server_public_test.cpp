// engine/server/server.hpp with a game on its own protocol (`ServerGame::serve`): the core beats
// to sv_master's master from the game's socket, answers a hub's status ask with the game's record,
// and keeps both out of the game's datagrams (Transport::on_public); on close it says bye.
//
// ge:summary Test: an own-protocol server beats to the master and answers status asks
// ge:keywords test server own-protocol serve master beat status public hook on_public
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "net/master.hpp"
#include "net/packet.hpp"
#include "net/status.hpp"
#include "server/server.hpp"

using namespace ge::net;
namespace gs = ge::server;

namespace {

struct Own {
  Transport* port = nullptr;
  int datagrams = 0;  // what reached the game's own protocol
};

gs::ServerGame own_game() {
  gs::ServerGame g;
  g.binary = "own_test";
  g.protocol = protocol_id("own_test", 1);
  g.version = 4;
  g.create = [](const gs::ServerConfig&, std::string&) -> void* { return new Own; };
  g.destroy = [](void* p) { delete static_cast<Own*>(p); };
  g.frame_seconds = [](const void*) { return 0.05; };
  g.frame = [](void*) {};
  g.hash = [](const void*) -> uint64_t { return 0; };
  g.serve = [](void* p, Transport& t, std::string&) {
    static_cast<Own*>(p)->port = &t;
    return true;
  };
  g.net_frame = [](void* p, int64_t now_us) {
    Own& o = *static_cast<Own*>(p);
    uint8_t buf[kMaxDatagram];
    Address from;
    while (transport_receive(*o.port, buf, sizeof buf, from, now_us) >= 0) o.datagrams++;
  };
  g.fill_status = [](const void*, Status& st) {
    st.mode = "duel";
    StatusPlayer p;
    p.name = "ann";
    st.players.push_back(p);
  };
  return g;
}

}  // namespace

TEST(own_protocol_server_beats_and_answers) {
  SimNetwork net = make_sim_network(3, LinkConditions{});
  const Address master_at = sim_address(1, kMasterPort);
  Transport master_port, server_port, hub;
  open_sim_transport(master_port, net, master_at);
  open_sim_transport(server_port, net, sim_address(5, 27500));
  open_sim_transport(hub, net, sim_address(2, 40000));
  Master master;
  master_init(master, MasterConfig{}, master_port);

  const gs::ServerGame game = own_game();
  gs::ServerConfig c;
  c.hostname = "Own Arena";
  c.masters = "sim:1:27900";
  gs::Server s;
  std::string err;
  CHECK(gs::server_open(s, game, c, &server_port, nullptr, err));
  int64_t now = 0;
  uint64_t cookie = 0;
  Status got;
  bool have = false;
  for (int i = 0; i < 100; i++) {
    gs::server_frame(s);
    now = s.now_us;
    sim_advance(net, now);
    master_update(master, now);
    if (i % 10 == 5) {
      uint8_t ask[kStatusAskSize];
      status_ask(game.protocol, 9, cookie, ask);
      transport_send(hub, server_port.local, ask, sizeof ask, now);
    }
    uint8_t buf[kMaxDatagram];
    Address from;
    int n = 0;
    while ((n = transport_receive(hub, buf, sizeof buf, from, now)) >= 0) {
      if (status_read_reply(buf, static_cast<size_t>(n), 9, cookie, got) == StatusReply::status) {
        have = true;
      }
    }
  }
  CHECK(master_count(master, game.protocol, kAllKinds) == 1);
  CHECK(master.entries.size() == 1 && master.entries[0].version == 4);
  CHECK(have && got.name == "Own Arena" && got.mode == "duel" && got.players.size() == 1);
  CHECK(s.status_answers >= 2);
  CHECK(static_cast<Own*>(s.state)->datagrams == 0);  // acks and asks never reached the game

  gs::server_close(s);
  for (int i = 0; i < 5; i++) {
    now += 50000;
    sim_advance(net, now);
    master_update(master, now);
  }
  CHECK(master_count(master, game.protocol, kAllKinds) == 0);  // its bye
  CHECK(server_port.on_public == nullptr);
}
