// games/shooters_edge/net/classic_game: Shooter's Edge's playable dedicated server on the engine's
// server core. Offline and on the network with nobody connected it is one digest; a classic client
// over the simulated network connects to the core's server, follows the signon and plays, and the
// core counts it seated; the settings reach the endpoint (the name in its status, the password at
// the connect); bots are refused with the next step; a whole session replays to the bit.
//
// ge:summary Test: Shooter's Edge's dedicated server on the core: one digest, a client plays on it
// ge:keywords test classic-game dedicated-server classic-client simulated-network serve net_frame
// ge:skill net shooter testing
#include <memory>
#include <string>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_server.hpp"

using namespace ge::server;
namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

constexpr uint16_t kPort = 27500, kClientPort = 27001;

ServerConfig arena(uint32_t seed = 1) {
  ServerConfig c;
  c.hostname = "Arena Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = seed;
  return c;
}

struct Session {
  gn::SimNetwork net;
  gn::Transport server_port, client_port;
  Server s;
  se::ClassicClientEnd client;
  std::string error;
  bool opened = false;
  uint64_t digest = 1469598103934665603ull;
};

// The core's server on the simulated network, one classic client joining at once, `frames`
// frames; the digest folds every frame's hash and the client's end.
void run_session(Session& r, const ServerConfig& c, int frames, const std::string& password = "") {
  gn::LinkConditions lan;
  r.net = gn::make_sim_network(c.seed, lan);
  gn::open_sim_transport(r.server_port, r.net, gn::sim_address(0, kPort));
  gn::open_sim_transport(r.client_port, r.net, gn::sim_address(1, kClientPort));
  r.opened = server_open(r.s, se::classic_game(), c, &r.server_port, nullptr, r.error);
  if (!r.opened) return;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  cc.password = password;
  se::client_start(r.client, r.client_port, gn::sim_address(0, kPort), cc, 0);
  for (int f = 0; f < frames; f++) {
    const int64_t now = r.s.now_us;
    gn::sim_advance(r.net, now);
    se::client_tick(r.client, now);
    server_frame(r.s);
    r.digest = (r.digest ^ server_digest(r.s)) * 1099511628211ull;
  }
  r.digest = (r.digest ^ static_cast<uint64_t>(r.client.stage)) * 1099511628211ull;
}

}  // namespace

TEST(classic_game_offline_and_networked_alone_are_one_digest) {
  Server off;
  std::string error;
  CHECK(server_open(off, se::classic_game(), arena(), nullptr, nullptr, error));
  CHECK(error.empty());
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  Server on;
  CHECK(server_open(on, se::classic_game(), arena(), &port, nullptr, error));
  CHECK(on.own_protocol);
  CHECK(off.frame_s > 0.012 && off.frame_s < 0.014);  // the mod's 13 ms
  for (int f = 0; f < 600; f++) {
    gn::sim_advance(net, on.now_us);
    server_frame(off);
    server_frame(on);
    if (server_digest(off) != server_digest(on)) {
      CHECK(false);
      break;
    }
  }
  CHECK(server_banner(on).find("the game's own protocol") != std::string::npos);
}

TEST(classic_game_a_client_connects_and_plays) {
  Session r;
  run_session(r, arena(), 600);  // 7.8 s
  CHECK(r.opened);
  CHECK(r.error.empty());
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK(server_status(r.s).find("seated=1") != std::string::npos);
  CHECK(server_status(r.s).find("connects=1") != std::string::npos);
  const auto* game_state = r.s.state;
  CHECK(game_state != nullptr);
}

TEST(classic_game_the_settings_reach_the_endpoint) {
  {
    ServerConfig c = arena();
    c.password = "secret";
    Session wrong;
    run_session(wrong, c, 300, "guess");
    CHECK(wrong.client.stage != se::ClientStage::active);
    Session right;
    run_session(right, c, 600, "secret");
    CHECK(right.client.stage == se::ClientStage::active);
  }
  {
    ServerConfig c = arena();
    c.bots = 2;
    Server s;
    std::string error;
    CHECK(!server_open(s, se::classic_game(), c, nullptr, nullptr, error));
    CHECK(error.find("next: --bots=0") != std::string::npos);
  }
  {
    ServerConfig c = arena();
    c.map = "nowhere";
    c.root = "no-such-folder";
    Server s;
    std::string error;
    CHECK(!server_open(s, se::classic_game(), c, nullptr, nullptr, error));
    CHECK(error.find("next:") != std::string::npos);
  }
}

TEST(classic_game_a_session_replays_to_the_bit) {
  Session a, b;
  run_session(a, arena(7), 500);
  run_session(b, arena(7), 500);
  CHECK(a.client.stage == se::ClientStage::active);
  CHECK(a.digest == b.digest);
}
