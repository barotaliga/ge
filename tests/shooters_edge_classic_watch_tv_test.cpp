// Tests: setv --watch (games/shooters_edge/net/classic_watch_tv.hpp) on the simulated network. Our
// classic server plays a match with a player running round; the node's watcher joins it as a
// spectator and a game window's end (tv/net_link.hpp's TvClient) joins the node: it is sent the
// match, which reads as a client recording. The node's record names the server it watches under
// the game's protocol, so a hub finds it; a watcher the server drops is followed by a new
// broadcast after the wait.
//
// ge:summary Tests: a server's spectator view broadcast live, a viewer joined, the record's source
// ge:keywords test watch-tv setv live broadcast tv-client source retry
// ge:skill net replay testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/packet.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_watch_tv.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "tv/net_link.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace tv = ge::tv;

namespace {

// Held on the heap: a whole server is too big for Windows' main-thread stack.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, pt, vt;
  cn::ClassicServer server;
  cn::ClassicClientEnd player;
  cn::WatchTv node;
  tv::TvClient viewer;
  gn::Address server_at, node_at;
  int64_t now = 0;

  bool start(std::string& error) {
    if (!sv::build_test_mod(progs, error)) return false;
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    qc = sv::make_qc_server(progs, so, error);
    if (!qc) return false;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    if (!sv::load_map(*qc, map, error)) return false;
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(1, lan);
    server_at = gn::sim_address(0, cn::kPortServer);
    node_at = gn::sim_address(2, 27900);
    gn::open_sim_transport(st, net, server_at);
    gn::open_sim_transport(pt, net, gn::sim_address(1, cn::kPortClient));
    gn::open_sim_transport(node.up, net, gn::sim_address(2, cn::kPortClient));
    gn::open_sim_transport(node.down, net, node_at);
    gn::open_sim_transport(vt, net, gn::sim_address(3, 40000));
    rules = sv::qc_runner_over(*qc);
    if (!cn::classic_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    cn::ClientConfig pc;
    pc.name = "runner";
    pc.qport = 4242;
    pc.seed = 3;
    cn::client_start(player, pt, server_at, pc, 0);
    cn::WatchTvConfig c;
    c.name = "test tv";
    c.delay_s = 1;
    c.seed = 7;
    cn::watch_tv_start(node, server_at, c, 0);
    return true;
  }
  void tick() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    cn::client_tick(player, now);
    cn::watch_tv_update(node, now);
    if (viewer.net.transport != nullptr) tv::tv_client_update(viewer, now);
  }
};

}  // namespace

TEST(a_window_joined_to_the_node_is_sent_the_servers_match) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  tv::tv_client_init(r.viewer, r.vt, r.node_at, "window", false, 9, 0);
  for (int i = 0; i < 900; i++) r.tick();
  CHECK_MSG(r.node.watcher && r.node.watcher->client.stage == cn::ClientStage::active,
            "the watcher did not get into the match");
  CHECK(r.viewer.greeted);
  CHECK(r.viewer.watch.have_header);
  CHECK_MSG(r.viewer.watch.blocks >= 5, std::to_string(r.viewer.watch.blocks) + " blocks");
  // What the window was sent unpacks to a client recording that reads.
  cl::DemoUnpacker u;
  std::vector<uint8_t> recording;
  CHECK_MSG(cl::unpack_more(u, r.viewer.watch.stream, recording, error), error);
  cl::Demo demo;
  CHECK_MSG(cl::open_demo(std::move(recording), demo, error), error);
  CHECK(cn::watch_tv_status(r.node).find("viewers 1") != std::string::npos);
}

TEST(the_nodes_record_names_its_server_under_the_games_protocol) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  const gn::Status s = tv::tv_host_status(r.node.host);
  CHECK(s.kind == gn::NodeKind::tv);
  CHECK(s.protocol == gn::protocol_id("shooters_edge", 1));
  std::string source;
  for (const auto& [k, v] : s.keys)
    if (k == tv::kSourceKey) source = v;
  CHECK_MSG(source == gn::to_string(r.server_at), source);
}

TEST(a_dropped_watcher_is_followed_by_a_new_broadcast) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  for (int i = 0; i < 300; i++) r.tick();
  CHECK(r.node.broadcasts == 1);
  cn::watcher_stop(*r.node.watcher);  // as when the server lets it go
  r.tick();
  CHECK(r.node.retry_at_us > 0);
  CHECK(r.node.host.tv.finished);
  const int steps = static_cast<int>(r.node.config.retry_s * 1e6 / 13000) + 2;
  for (int i = 0; i < steps; i++) r.tick();
  CHECK(r.node.broadcasts == 2);
  CHECK(r.node.retry_at_us < 0);
  for (int i = 0; i < 300; i++) r.tick();
  CHECK(r.node.watcher->client.stage == cn::ClientStage::active);
}
