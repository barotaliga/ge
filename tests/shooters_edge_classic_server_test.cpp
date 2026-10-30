// Tests: the classic server endpoint (games/shooters_edge/net/classic_server.hpp) over engine/net's
// simulated network: a full signon of scripted clients (each way of asking for extensions) under
// every condition preset, ending in play with the skins they lacked downloaded and every move's
// checksum verified; the out-of-band answers (ping, status, a challenge) and the rejects (no
// challenge, a wrong password, a full server); a move with a wrong checksum dropped and counted;
// lag compensation rewinding the players for a client's hitscan; and the session replaying to the
// bit. tests/golden/shooters_edge_classic_server.txt holds each preset's digest, written by this
// test under GE_GOLDEN_WRITE=1 (a deliberate change to the server, the client or the loop rewrites
// it; the PR says why).
//
// Golden rows: <preset> <digest hex>.
//
// ge:summary Tests: classic signon under every preset, out-of-band answers, rejects, checksums
// ge:keywords test classic-protocol classic-server signon preset getchallenge connect reject
// ge:keywords status ping download move-checksum antilag digest golden
// ge:skill net shooter testing
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_trial.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_classic_server.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

// A served arena on a clean simulated network, and a raw socket beside it.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  gn::SimNetwork net;
  gn::Transport st, raw[2];
  cn::ClassicServer server;
  int64_t now = 0;

  bool start(const cn::ServerConfig& cfg) {
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
    gn::open_sim_transport(raw[0], net, gn::sim_address(9, cn::kPortClient));
    gn::open_sim_transport(raw[1], net, gn::sim_address(10, cn::kPortClient));
    return cn::classic_start(server, *qc, st, cfg, error);
  }
  void frame() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
  }
  void oob(const std::string& text, int from = 0) {
    std::string d = "\xff\xff\xff\xff" + text;
    gn::transport_send(raw[from], gn::sim_address(0, cn::kPortServer),
                       reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
  }
  // Every answer a raw socket got, the out-of-band marker stripped (the ping's has none).
  std::vector<std::string> answers(int at = 0) {
    std::vector<std::string> out;
    for (int f = 0; f < 20; f++) frame();
    uint8_t buf[2048];
    gn::Address from;
    int n;
    while ((n = gn::transport_receive(raw[at], buf, sizeof buf, from, now)) >= 0) {
      const auto size = static_cast<size_t>(n);
      const size_t skip = cn::is_out_of_band(buf, size) ? 4 : 0;
      out.emplace_back(reinterpret_cast<const char*>(buf) + skip, size - skip);
    }
    return out;
  }
};

bool any_has(const std::vector<std::string>& v, const std::string& what) {
  for (const std::string& s : v)
    if (s.find(what) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(scripted_clients_sign_on_and_play_under_every_preset) {
  std::vector<std::vector<uint64_t>> got;
  const std::vector<std::string>& presets = gn::preset_names();
  for (size_t p = 0; p < presets.size(); p++) {
    cn::TrialOptions o;
    o.preset = presets[p];
    o.clients = 3;
    o.seconds = 20;
    const cn::TrialReport r = cn::run_classic_trial(o);
    CHECK_MSG(r.ok, r.error);
    for (const cn::TrialClient& c : r.clients) {
      CHECK_MSG(c.stage == "active", o.preset + ": " + c.name + " (" + c.mode + ") ended at " +
                                         c.stage + (c.refused.empty() ? "" : ": " + c.refused) +
                                         "; next: make lab LAB=se-net ARGS=--preset=" + o.preset);
      CHECK_MSG(c.players_seen == 3, o.preset + ": " + c.name + " did not see every player");
      CHECK_MSG(c.full_frames + c.delta_frames > 0, o.preset + ": " + c.name + " got no frame");
    }
    // Every client fetched the skins it lacked of the players it knew at its skins step (lines and
    // pext agree on chunked downloads, the base-protocol client takes the classic transfer),
    // each one whole; the blue and red clients lack at least their own.
    for (const cn::TrialClient& c : r.clients)
      CHECK_MSG(c.downloads == c.files, o.preset + ": " + c.name + " fetched a damaged file");
    if (r.clients.size() == 3)
      CHECK_MSG(r.clients[1].downloads >= 1 && r.clients[2].downloads >= 1,
                o.preset + ": a skin was not downloaded");
    CHECK_MSG(r.moves > 0 && r.moves_unchecked == 0 && r.moves_bad_checksum == 0 &&
                  r.server.rejects == 0 && r.server.mod_bytes_dropped == 0,
              o.preset +
                  ": every move must be checked and right, no client refused, and every "
                  "byte the mod wrote must read as whole messages");
    got.push_back({p, r.digest});
    if (p == 0) {
      const cn::TrialReport again = cn::run_classic_trial(o);
      CHECK_MSG(again.digest == r.digest, "a classic session did not replay to the bit");
    }
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/net/classic_trial.hpp's digest by preset (index into "
           "preset_names): 3 clients, 20 s, seed 1 (tests/shooters_edge_classic_server_test.cpp)\n";
    for (const auto& row : got) {
      char line[64];
      std::snprintf(line, sizeof line, "%llx %016llx\n", static_cast<unsigned long long>(row[0]),
                    static_cast<unsigned long long>(row[1]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "the classic session moved off its digest under " + presets[i] +
                                     ": if meant, rewrite with GE_GOLDEN_WRITE=1 and say why; "
                                     "else compare make lab LAB=se-net runs before and after");
}

TEST(out_of_band_answers_and_rejects) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.password = "secret";
  CHECK(rig.start(cfg));
  rig.oob("ping\n");
  rig.oob("status 23\n");
  rig.oob("getchallenge\n");
  std::vector<std::string> a = rig.answers();
  CHECK(std::find(a.begin(), a.end(), "l") != a.end());  // the ping's answer
  CHECK(any_has(a, "\\hostname\\Shooter's Edge"));       // the status
  std::string challenge;
  for (const std::string& s : a)
    if (!s.empty() && s[0] == 'c') challenge = std::string(s.c_str() + 1);
  CHECK(!challenge.empty());
  // A connect with no challenge for the address, then a wrong password.
  rig.oob("connect 28 1 12345 \"\\name\\x\"\n");
  rig.oob("connect 28 1 " + challenge + " \"\\name\\x\\password\\wrong\"\n");
  a = rig.answers();
  CHECK(any_has(a, "Bad challenge") || any_has(a, "No challenge"));
  CHECK(any_has(a, "password"));
  CHECK(rig.server.counts.rejects == 2);
  // The right password connects.
  rig.oob("connect 28 1 " + challenge + " \"\\name\\x\\password\\secret\"\n");
  a = rig.answers();
  CHECK(any_has(a, "j"));
  CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == 1);
}

TEST(a_full_server_refuses_a_client) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  CHECK(rig.start(cfg));
  rig.qc->sv.set.maxclients = 1;
  for (int i = 0; i < 2; i++) {
    rig.oob("getchallenge\n", i);
    std::string challenge;
    for (const std::string& s : rig.answers(i))
      if (!s.empty() && s[0] == 'c') challenge = std::string(s.c_str() + 1);
    rig.oob("connect 28 " + std::to_string(10 + i) + " " + challenge + " \"\\name\\p" +
                std::to_string(i) + "\"\n",
            i);
    const std::vector<std::string> a = rig.answers(i);
    if (i == 1) CHECK(any_has(a, "server is full"));
  }
}

namespace {

// Brings `n` scripted clients into play on the rig (false when one does not get there).
bool play(Rig& rig, gn::Transport* ct, cn::ClassicClientEnd* c, int n) {
  for (int i = 0; i < n; i++) {
    gn::open_sim_transport(ct[i], rig.net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = "p" + std::to_string(i);
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  for (int f = 0; f < 600; f++) {
    rig.frame();
    for (int i = 0; i < n; i++) cn::client_tick(c[i], rig.now);
  }
  for (int i = 0; i < n; i++)
    if (c[i].stage != cn::ClientStage::active) return false;
  return true;
}

}  // namespace

TEST(a_move_with_a_wrong_checksum_is_dropped_and_counted) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  gn::Transport ct[1];
  cn::ClassicClientEnd c[1];
  CHECK(play(rig, ct, c, 1));
  const cn::ClassicClient& sc = rig.server.clients[c[0].playernum];
  CHECK(sc.moves_bad_checksum == 0 && sc.moves > 0 && sc.moves_unchecked == 0);
  // A move whose checksum byte is off by one.
  uint8_t body[64];
  gn::Writer w = gn::make_writer(body, sizeof body);
  cn::write_move(w, c[0].cmds, 0, c[0].ch.outgoing_sequence);
  body[1] = static_cast<uint8_t>(body[1] + 1);
  uint8_t pkt[cn::kMaxPacket];
  const size_t n =
      cn::channel_transmit(c[0].ch, body, w.size, static_cast<double>(rig.now) / 1e6, pkt);
  gn::transport_send(ct[0], gn::sim_address(0, cn::kPortServer), pkt, n, rig.now);
  rig.frame();
  rig.frame();
  CHECK(sc.moves_bad_checksum == 1);
}

TEST(lag_compensation_moves_the_others_back_for_a_shot_and_restores_them) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.antilag = true;
  CHECK(rig.start(cfg));
  gn::Transport ct[2];
  cn::ClassicClientEnd c[2];
  CHECK(play(rig, ct, c, 2));
  CHECK(rig.qc->lag_rewind != nullptr);
  const int shooter = c[0].playernum + 1, other = c[1].playernum + 1;
  const sh::Vec3f now = rig.qc->sv.edicts[static_cast<size_t>(other)].origin;
  rig.qc->lag_rewind(rig.qc->lag_ctx, shooter, true);
  const sh::Vec3f then = rig.qc->sv.edicts[static_cast<size_t>(other)].origin;
  rig.qc->lag_rewind(rig.qc->lag_ctx, shooter, false);
  const sh::Vec3f back = rig.qc->sv.edicts[static_cast<size_t>(other)].origin;
  CHECK(rig.server.lag_rewinds == 1);
  CHECK_MSG(then.x != now.x || then.y != now.y, "the other player was not moved back");
  CHECK(back.x == now.x && back.y == now.y && back.z == now.z);
  // Without antilag the hook is not set.
  auto owned_plain = std::make_unique<Rig>();
  Rig& plain = *owned_plain;
  CHECK(plain.start(cn::ServerConfig{}));
  CHECK(plain.qc->lag_rewind == nullptr);
}
