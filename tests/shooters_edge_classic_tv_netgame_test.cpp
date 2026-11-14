// games/shooters_edge/net/classic_tv_netgame.hpp: a recording served as a netgame on the
// single-player protocol, on the simulated network. A scripted client written from the transport's
// facts (net/classic_netgame_watcher.hpp, not from our face) connects, reassembles the
// reliable messages, acknowledges every fragment, answers each `signonnum` with the stage's
// commands and then takes frame datagrams: the server's time rising, entity updates, the recorded
// view as a `setangle`. At the end of the recording it is sent "reconnect" and the level again.
// Over a lossy link the same client still gets in, by resends.
//
// ge:summary Test: the netgame face, a scripted single-player client watching a recording
// ge:keywords test netgame single-player protocol-15 signon reliable resend loop reconnect
// ge:skill net shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/classic_netgame_watcher.hpp"
#include "shooters_edge/net/classic_tv_netgame.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

namespace {

constexpr uint16_t kPortTv = 26000, kPortClient = 27001;

struct Run {
  gn::SimNetwork net;
  gn::Transport face_t, client_t;
  cn::NetgameTv tv;
  cn::NetgameWatcher c;
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
    cn::netgame_watch(c, client_t, gn::sim_address(0, kPortTv));
  }
  void run_to(double s) {
    while (now < static_cast<int64_t>(s * 1e6)) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::udp_tv_netgame_frame(tv, now);
      cn::netgame_watch_tick(c, now);
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
  // Signed on at least once: over this link a loop's new sign-on can still be under way at 20 s.
  CHECK_MSG(r.c.seatings >= 1, "every fragment lost is sent again");
  CHECK(r.c.resends > 0);
  CHECK(r.c.frames > 20);
}

TEST(anything_but_a_recording_is_refused_with_a_reason) {
  cn::NetgameTv tv;
  std::string error;
  CHECK(!cn::udp_tv_netgame_open(tv, std::vector<uint8_t>{1, 2, 3}, true, error));
  CHECK(!error.empty());
}
