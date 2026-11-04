// Tests: the window joined to a server (games/shooters_edge/net/classic_remote.hpp) over
// engine/net's simulated network: the classic server on the test arena, the remote client driven by
// "hands" instead of a script. It signs on, its recording read back names the map and puts the eye
// on the player, the server's turn (setangle) is taken by the view, walking forward moves the eye
// the way the view faces, an impulse is sent once, and leaving frees the server's slot.
//
// ge:summary Tests: the remote client signs on, draws the server's state, walks, and leaves
// ge:keywords test remote-client connect disconnect classic-server live-demo setangle walk
// ge:skill net shooter testing
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

// The test arena served on a LAN of the simulated network, and one remote client on it.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::ClassicServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0;

  bool start() {
    std::string error;
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
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    gn::open_sim_transport(ct, net, gn::sim_address(1, cn::kPortClient));
    if (!cn::classic_start(server, *qc, st, cn::ServerConfig{}, error)) return false;
    cn::ClientConfig cc;
    cc.name = "window";
    cn::remote_start(remote, ct, gn::sim_address(0, cn::kPortServer), cc);
    return true;
  }
  // One server frame and one window frame of the same length; the last step's result.
  cn::RemoteStep frame() {
    now += cn::kTickUs;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    return cn::remote_step(remote, cn::kTickUs * 1e-6, said);
  }
  // Frames until the client plays and has drawn a level; false if it never does.
  bool join() {
    bool level = false;
    for (int f = 0; f < 800 && !(level && remote.end.stage == cn::ClientStage::active); f++)
      if (frame() == cn::RemoteStep::new_level) level = true;
    return level && remote.end.stage == cn::ClientStage::active;
  }
};

bool said_has(const std::vector<std::string>& said, const std::string& what) {
  for (const std::string& s : said)
    if (s.find(what) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_remote_client_signs_on_and_draws_the_server_s_player_as_a_demo) {
  Rig rig;
  CHECK(rig.start());
  CHECK_MSG(rig.join(),
            "the client's stage: " + std::string(cn::client_stage_name(rig.remote.end.stage)) +
                " " + rig.remote.end.refused);
  CHECK(said_has(rig.said, "playing on "));
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  CHECK_MSG(ds.map == "arena", "the map read: '" + ds.map + "'");
  CHECK(ds.player == rig.remote.end.playernum);
  // The eye rides the player's own origin as the server sent it.
  for (int f = 0; f < 20; f++) rig.frame();
  const int slot = rig.remote.end.playernum;
  CHECK(slot >= 0);
  const sh::Vec3f at = rig.remote.end.players[slot].origin;
  CHECK_MSG(std::fabs(ds.eye.x - at.x) < 1 && std::fabs(ds.eye.y - at.y) < 1,
            "the eye is not over the player");
  CHECK(cn::remote_status(rig.remote) == "playing on sim:0:27500");
}

TEST(the_view_takes_the_server_s_turn_and_walking_moves_the_eye_the_way_it_faces) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  // The server turns the player (as a teleport does): the view takes its angles.
  cn::SvcMsg turn;
  turn.id = cn::svc::setangle;
  turn.ints = {-1};
  turn.floats = {0, 135, 0};
  cn::reliable_svc(rig.server, rig.remote.end.playernum, turn);
  for (int f = 0; f < 40; f++) rig.frame();
  CHECK_MSG(rig.remote.setangles > 0 && std::fabs(rig.remote.view.y - 135) < 1,
            "the server's turn was not taken: yaw " + std::to_string(rig.remote.view.y));
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  const sh::Vec3f from = ds.eye;
  // Facing +x (yaw 0, level), forward for a second.
  rig.remote.view = {0, 0, 0};
  rig.remote.hands.forward = 400;
  for (int f = 0; f < 77; f++) rig.frame();
  rig.remote.hands.forward = 0;
  for (int f = 0; f < 20; f++) rig.frame();
  const float dx = ds.eye.x - from.x, dy = ds.eye.y - from.y;
  CHECK_MSG(dx > 100 && std::fabs(dy) < dx * 0.25f,
            "walked by " + std::to_string(dx) + ", " + std::to_string(dy));
  // The drawn view is the hands' own, whatever the recording's last command said.
  rig.remote.view = {10, 90, 0};
  rig.frame();
  CHECK(ds.angles.x == 10 && ds.angles.y == 90);
}

TEST(an_impulse_is_sent_once_and_leaving_frees_the_server_s_slot) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  CHECK(cn::classic_count(rig.server, cn::ConnState::spawned) == 1);
  rig.remote.hands.impulse = 7;
  rig.frame();
  CHECK(rig.remote.hands.impulse == 0);
  cn::remote_disconnect(rig.remote);
  CHECK(!rig.remote.active);
  for (int f = 0; f < 20; f++) rig.frame();
  CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == 0);
  // Not connected: a step draws nothing.
  CHECK(rig.frame() == cn::RemoteStep::waiting);
  CHECK(cn::remote_status(rig.remote) == "not connected");
}
