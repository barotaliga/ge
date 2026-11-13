// games/shooters_edge/net/classic_tv_udp.hpp and classic_tv_recording.hpp: our TV's face for
// classic clients on the simulated network. Our classic server runs the test mod with three
// players; one records its client recording from the connect. The recording is played into the
// face, and a viewer (our scripted classic client asking for no extension, as the oldest client
// would) joins the face as a server: it walks the signon, is seated in the recorder's view with no
// speed of its own, gets the recorder's own lists, sees the players move frame by frame in whole
// entity lists, and stays on through the loop, which signs it on again with a new count. A second
// viewer joins late, and a wrong password is refused in a player's words.
//
// ge:summary Test: the face for classic clients, viewers watching a client recording in plain 28
// ge:keywords test classic-tv udp-face viewer client-recording plain-protocol loop
// ge:keywords late-joiner password
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_tv_recording.hpp"
#include "shooters_edge/net/classic_tv_udp.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

constexpr int kPlayers = 3;
constexpr int kTicks = 700;  // 13 ms each: about 9 s
constexpr uint16_t kPortTv = 27900;

struct Recorded {
  std::vector<uint8_t> recording;
  std::vector<std::string> models;
  int recorder = -1;
};

// A match on our server, client 0 recording from its connect.
Recorded record_match() {
  Recorded out;
  std::string error;
  ge::progs::Progs progs;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  CHECK(qc != nullptr);
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  CHECK_MSG(sv::load_map(*qc, map, error), error);
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport st, ct[kPlayers];
  gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
  cn::ServerConfig cfg;
  sv::RulesRunner rules = sv::qc_runner_over(*qc);
  auto server = std::make_unique<cn::ClassicServer>();
  CHECK_MSG(cn::classic_start(*server, rules, st, cfg, error), error);
  std::vector<std::unique_ptr<cn::ClassicClientEnd>> c;
  for (int i = 0; i < kPlayers; i++) {
    gn::open_sim_transport(ct[i], net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = "p" + std::to_string(i);
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    c.push_back(std::make_unique<cn::ClassicClientEnd>());
    cn::client_start(*c.back(), ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  cn::ClientRecorder rec;
  cn::record_start(rec, false);
  c[0]->record = &rec;
  int64_t now = 0;
  for (int f = 0; f < kTicks; f++) {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(*server, now);
    for (auto& k : c) cn::client_tick(*k, now);
  }
  cn::record_stop(rec);
  out.recording = rec.bytes;
  out.models = c[0]->models;
  out.recorder = c[0]->playernum;
  return out;
}

const Recorded& recorded() {
  static const Recorded r = record_match();
  return r;
}

struct Watch {
  gn::SimNetwork net;
  gn::Transport face_t, viewer_t[2];
  cn::ClassicTvUdp face;
  cn::UdpTvRecording demo;
  std::unique_ptr<cn::ClassicClientEnd> viewer[2];
  int64_t now = 0;

  Watch(const std::string& password, bool loop) {
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(2, lan);
    gn::open_sim_transport(face_t, net, gn::sim_address(0, kPortTv));
    cn::UdpTvConfig fc;
    fc.password = password;
    cn::udp_tv_start(face, face_t, fc);
    std::string error;
    CHECK_MSG(cn::udp_tv_recording_open(demo, recorded().recording, loop, error), error);
  }

  void join(int i, const std::string& password) {
    gn::open_sim_transport(viewer_t[i], net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = "viewer" + std::to_string(i);
    cc.qport = static_cast<uint16_t>(300 + i);
    cc.ask = cn::Ext{};  // the oldest client: no extension
    cc.password = password;
    cc.fetch_looks = false;
    viewer[i] = std::make_unique<cn::ClassicClientEnd>();
    cn::client_start(*viewer[i], viewer_t[i], gn::sim_address(0, kPortTv), cc,
                     static_cast<double>(now) / 1e6);
  }

  void run(int ticks) {
    for (int f = 0; f < ticks; f++) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::udp_tv_recording_update(demo, face, now);
      cn::udp_tv_frame(face, now);
      for (auto& v : viewer)
        if (v) cn::client_tick(*v, now);
    }
  }
};

}  // namespace

TEST(the_recording_holds_its_signon_and_reads) {
  const Recorded& r = recorded();
  CHECK(r.recording.size() > 1000);
  CHECK(r.recorder >= 0);
  CHECK(r.models.size() > 2);
}

TEST(a_viewer_with_no_extension_watches_seated_in_the_recorders_view) {
  Watch w("", false);
  w.join(0, "");
  w.run(400);
  const cn::ClassicClientEnd& v = *w.viewer[0];
  CHECK_MSG(v.stage == cn::ClientStage::active, "the viewer never got into the game: " + v.refused);
  CHECK(v.playernum == recorded().recorder);  // seated in his view, not a spectator
  CHECK(v.models == recorded().models);
  CHECK(v.movevars[2] == 0.0f);  // no speed of its own
  CHECK(v.player_seen[recorded().recorder]);
  CHECK(w.face.left_out >= 0);
  CHECK_MSG(w.face.error.empty(), w.face.error);
  // Frames came, each with every entity the recorder saw.
  int valid = 0;
  for (const cn::ClientFrame& fr : v.frames) valid += fr.valid ? 1 : 0;
  CHECK(valid > 10);
  // The players move.
  const ge::shooter::Vec3f a = v.players[recorded().recorder].origin;
  w.run(100);
  const ge::shooter::Vec3f b = w.viewer[0]->players[recorded().recorder].origin;
  CHECK(a.x != b.x || a.y != b.y || a.z != b.z);
}

TEST(the_loop_signs_a_viewer_on_again_with_a_new_count) {
  Watch w("", true);
  w.join(0, "");
  w.run(300);
  CHECK(w.viewer[0]->stage == cn::ClientStage::active);
  const int first = w.viewer[0]->spawncount;
  w.run(kTicks + 300);
  CHECK(w.demo.rounds >= 1);
  CHECK(w.viewer[0]->stage == cn::ClientStage::active);
  CHECK(w.viewer[0]->spawncount != first);
  CHECK(cn::udp_tv_viewers(w.face) == 1);
}

TEST(a_late_viewer_starts_at_once_from_what_the_face_keeps) {
  Watch w("", false);
  w.join(0, "");
  w.run(350);
  w.join(1, "");
  w.run(150);
  CHECK(w.viewer[1]->stage == cn::ClientStage::active);
  CHECK(w.viewer[1]->models == recorded().models);
  CHECK(w.viewer[1]->player_seen[recorded().recorder]);
}

TEST(a_wrong_password_is_refused_in_a_players_words) {
  Watch w("secret", false);
  w.join(0, "wrong");
  w.run(100);
  CHECK(w.viewer[0]->stage != cn::ClientStage::active);
  CHECK(w.viewer[0]->refused.find("password") != std::string::npos);
  CHECK(w.viewer[0]->refused.find("next:") == std::string::npos);
  CHECK(w.face.refused >= 1);
}
