// Tests: talk on the classic endpoint (games/shooters_edge/net/classic_talk.hpp) over engine/net's
// simulated network, three clients on the test arena: `say_near` reaches only the players within
// sv_earshot (the talker too), a server with no earshot says so; a voice goes to the talker's
// target (`voicetarg`: everyone, one slot, the players near), a listener hears voice only after
// `unmuteall` and never one he `vignore`s; an unknown target word falls back to the team.
//
// ge:summary Tests the classic endpoint's talk: say_near by earshot, voice targets and mutes
// ge:keywords test talk say_near earshot voicetarg unmuteall muteall vignore voice-target nearby
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_talk.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

constexpr int64_t kTick = 13000;  // a server frame, us

bool always(int, int) { return true; }

// The arena with three players: 0 talks, 1 stands near him, 2 far off.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, t[3];
  cn::ClassicClientEnd c[3];
  cn::ClassicServer server;
  int64_t now = 0;

  bool start(int earshot, const char* talker_target) {
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
    lan.jitter_ms = 0;
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    rules = sv::qc_runner_over(*qc);
    cn::ServerConfig sc;
    sc.voice = true;
    sc.earshot = earshot;
    if (!cn::classic_start(server, rules, st, sc, error)) return false;
    for (int i = 0; i < 3; i++) {
      gn::open_sim_transport(t[i], net, gn::sim_address(i + 1, cn::kPortClient));
      cn::ClientConfig cc;
      cc.name = std::string("p") + std::to_string(i);
      cc.voice = true;
      if (i == 0) {
        cc.talk = always;
        cc.voice_to = talker_target;
      }
      cc.qport = static_cast<uint16_t>(10 + i);
      cn::client_start(c[i], t[i], gn::sim_address(0, cn::kPortServer), cc, 0);
    }
    return true;
  }
  // The players placed where the test wants them: 1 at 300 units from 0, 2 at 3000 (their
  // bodies moved, so the world the rules show follows).
  void place() {
    const int me = c[0].playernum;
    if (me < 0) return;
    const sh::Vec3f at = rules->sv.edicts[static_cast<size_t>(me) + 1].origin;
    for (int i = 1; i < 3; i++) {
      const int slot = c[i].playernum;
      if (slot < 0) continue;
      sh::Vec3f p = at;
      p.x += i == 1 ? 300.0f : 3000.0f;
      rules->sv.edicts[static_cast<size_t>(slot) + 1].origin = p;
      std::vector<sv::WorldEntity>& e = rules->world.entities;
      if (static_cast<size_t>(slot) + 1 < e.size()) e[static_cast<size_t>(slot) + 1].origin = p;
    }
  }
  void frame() {
    now += kTick;
    gn::sim_advance(net, now);
    place();
    cn::classic_frame(server, now);
    for (cn::ClassicClientEnd& e : c) cn::client_tick(e, now);
  }
  bool join() {
    for (int f = 0; f < 800; f++) {
      frame();
      bool all = true;
      for (const cn::ClassicClientEnd& e : c) all = all && e.stage == cn::ClientStage::active;
      if (all) return true;
    }
    return false;
  }
  void run(int frames) {
    for (int f = 0; f < frames; f++) frame();
  }
  bool read(int i, const std::string& what) const {
    for (const std::string& p : c[i].prints)
      if (p.find(what) != std::string::npos) return true;
    return false;
  }
  int64_t heard(int i) const { return c[i].voice_heard[c[0].playernum]; }
};

}  // namespace

TEST(say_near_reaches_only_the_players_within_earshot) {
  Rig rig;
  CHECK(rig.start(1000, ""));
  CHECK(rig.join());
  cn::client_command(rig.c[0], "say_near \"rope on the left\"");
  rig.run(40);
  CHECK_MSG(rig.read(0, "p0 (nearby): rope on the left"), "the talker reads his own line");
  CHECK_MSG(rig.read(1, "p0 (nearby): rope on the left"), "the near player reads it");
  CHECK_MSG(!rig.read(2, "rope on the left"), "the far player read a nearby line");
}

TEST(a_server_with_no_earshot_says_it_has_no_nearby_chat) {
  Rig rig;
  CHECK(rig.start(0, ""));
  CHECK(rig.join());
  cn::client_command(rig.c[0], "say_near \"anyone?\"");
  rig.run(40);
  CHECK(rig.read(0, "This server has no nearby chat."));
  CHECK(!rig.read(1, "anyone?") && !rig.read(2, "anyone?"));
}

TEST(a_near_voice_reaches_the_near_player_only) {
  Rig rig;
  CHECK(rig.start(1000, "near"));
  CHECK(rig.join());
  rig.run(80);
  CHECK_MSG(rig.heard(1) > 0, "the near player heard nothing");
  CHECK_MSG(rig.heard(2) == 0, "the far player heard a near voice");
  CHECK(rig.server.clients[rig.c[0].playernum].voice_to == cn::VoiceTo::near);
}

TEST(a_voice_to_one_slot_and_the_mutes) {
  Rig rig;
  CHECK(rig.start(1000, "all"));
  CHECK(rig.join());
  rig.run(40);
  CHECK(rig.heard(1) > 0 && rig.heard(2) > 0);
  // To slot of player 2 alone; player 1 then hears nothing more.
  cn::classic_voice_target(rig.c[0], std::to_string(rig.c[2].playernum));
  rig.run(20);
  const int64_t one = rig.heard(1), two = rig.heard(2);
  rig.run(40);
  CHECK_MSG(rig.heard(1) == one, "a voice to one slot reached another");
  CHECK(rig.heard(2) > two);
  // Player 2 ignores the talker: nothing more reaches him.
  cn::client_command(rig.c[2], "vignore " + std::to_string(rig.c[0].playernum) + " 1");
  rig.run(20);
  const int64_t ignored = rig.heard(2);
  rig.run(40);
  CHECK_MSG(rig.heard(2) == ignored, "an ignored talker was heard");
  // An unknown word: the team, which with no teams is everyone; muteall stops player 1.
  cn::client_command(rig.c[1], "muteall");
  cn::classic_voice_target(rig.c[0], "everyone-please");
  rig.run(20);
  CHECK(rig.server.clients[rig.c[0].playernum].voice_to == cn::VoiceTo::team);
  const int64_t muted = rig.heard(1);
  rig.run(40);
  CHECK_MSG(rig.heard(1) == muted, "a muted listener heard a voice");
}
