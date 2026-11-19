// Tests: the classic server endpoint (games/shooters_edge/net/classic_server.hpp) over engine/net's
// simulated network: a full signon of scripted clients (each way of asking for extensions) under
// every condition preset, ending in play with the skins they lacked downloaded and every move's
// checksum verified; the out-of-band answers (ping, status, a challenge) and the rejects (no
// challenge, a wrong password, a full server); a move with a wrong checksum dropped and counted;
// lag compensation rewinding the players for a client's hitscan; a file fetched out of a pack told
// its archive and kept in a partial of it until the pack is whole; and the session replaying to the
// bit. tests/golden/shooters_edge_classic_server.txt holds each preset's digest, written by this
// test under GE_GOLDEN_WRITE=1 (a deliberate change to the server, the client or the loop rewrites
// it; the PR says why).
//
// Golden rows: <preset> <digest hex>.
//
// ge:summary Tests: classic signon under every preset, out-of-band answers, rejects, checksums
// ge:keywords test classic-protocol classic-server signon preset getchallenge connect reject
// ge:keywords status ping download move-checksum antilag digest golden download-origin partial-pack
// ge:keywords sv_iplimit
// ge:skill net shooter testing
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_trial.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "vfs/pak.hpp"
#include "vfs/partial.hpp"
#include "vfs/search.hpp"

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
  sv::RulesRunner rules;  // over `qc`
  gn::SimNetwork net;
  gn::Transport st, raw[2];
  cn::ClassicServer server;
  int64_t now = 0;
  uint32_t second_node = 10;  // raw[1]'s node (9: raw[0]'s host, another port)

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
    gn::open_sim_transport(raw[1], net,
                           gn::sim_address(second_node, cn::kPortClient + (second_node == 9)));
    rules = sv::qc_runner_over(*qc);
    return cn::classic_start(server, rules, st, cfg, error);
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

TEST(a_client_fetches_the_model_folder_it_wears_from_a_server_that_reads_it_on_demand) {
  // The first client wears `model cyborg`, `skin oni911`; the server holds that folder, the map's
  // .lit (beside the map) and .loc only on disk (read on demand) and nobody has them, so every
  // client fetches the figure, its skin, the light (as lits/arena.lit) and the place names.
  cn::TrialOptions o;
  o.clients = 2;
  o.seconds = 12;
  o.figures = true;
  const cn::TrialReport r = cn::run_classic_trial(o);
  CHECK_MSG(r.ok, r.error);
  for (const cn::TrialClient& c : r.clients) {
    CHECK_MSG(c.stage == "active", c.name + " ended at " + c.stage);
    CHECK_MSG(c.downloads == c.files, c.name + " fetched a damaged file");
    CHECK_MSG(c.downloads >= 4, c.name + " fetched " + std::to_string(c.downloads) +
                                    " files, short of the model folder, its skin and the map's "
                                    "light and place names; next: "
                                    "make lab LAB=se-net and read the downloads");
  }
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

// sv_vipslots 1 on two slots: the second client is refused unless he gives a VIP password, and a
// VIP is asked no server password.
TEST(the_last_slot_is_kept_for_a_vip) {
  for (const bool vip : {false, true}) {
    auto owned = std::make_unique<Rig>();
    Rig& rig = *owned;
    rig.second_node = 9;
    cn::ServerConfig cfg;
    cfg.vip_slots = 1;
    cfg.vip_passwords = {"gold"};
    cfg.password = "door";
    CHECK(rig.start(cfg));
    rig.qc->sv.set.maxclients = 2;
    for (int i = 0; i < 2; i++) {
      rig.oob("getchallenge\n", i);
      std::string challenge;
      for (const std::string& s : rig.answers(i))
        if (!s.empty() && s[0] == 'c') challenge = std::string(s.c_str() + 1);
      const std::string pw = i == 1 && vip ? "gold" : "door";
      std::string line = "connect 28 " + std::to_string(10 + i);
      line += " " + challenge + " \"\\name\\p" + std::to_string(i);
      line += "\\password\\" + pw + "\"\n";
      rig.oob(line, i);
      const std::vector<std::string> a = rig.answers(i);
      if (i == 1) CHECK(vip != any_has(a, "server is full"));
    }
    CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == (vip ? 2 : 1));
  }
}

// sv_locked, set by the admin while the server runs: a newcomer is refused, a client taking back
// its own seat is let in.
TEST(a_locked_server_lets_only_its_own_clients_back) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  rig.second_node = 9;
  cn::ServerConfig cfg;
  cfg.reconnect_wait_s = 0;
  CHECK(rig.start(cfg));
  auto connect = [&](int i) {
    rig.oob("getchallenge\n", i);
    std::string challenge;
    for (const std::string& s : rig.answers(i))
      if (!s.empty() && s[0] == 'c') challenge = std::string(s.c_str() + 1);
    std::string line = "connect 28 " + std::to_string(10 + i);
    line += " " + challenge + " \"\\name\\p" + std::to_string(i) + "\"\n";
    rig.oob(line, i);
    return rig.answers(i);
  };
  connect(0);
  std::string out;
  CHECK(cn::classic_admin(rig.server, "sv_locked 1", out) && rig.server.cfg.locked);
  CHECK(any_has(connect(1), "server is locked"));
  CHECK(!any_has(connect(0), "server is locked"));  // the same address and port: its seat back
  CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == 1);
  CHECK(cn::classic_admin(rig.server, "sv_locked", out) && out == "\"sv_locked\" is \"1\"\n");
  CHECK(cn::classic_admin(rig.server, "sv_locked 0", out) && !rig.server.cfg.locked);
  CHECK(!any_has(connect(1), "server is locked"));
}

// sv_iplimit: a second client from the same host is refused past the limit (server/admission).
TEST(an_address_past_sv_iplimit_is_refused) {
  for (const int limit : {1, 2}) {
    auto owned = std::make_unique<Rig>();
    Rig& rig = *owned;
    rig.second_node = 9;
    cn::ServerConfig cfg;
    cfg.ip_limit = limit;
    CHECK(rig.start(cfg));
    for (int i = 0; i < 2; i++) {
      rig.oob("getchallenge\n", i);
      std::string challenge;
      for (const std::string& s : rig.answers(i))
        if (!s.empty() && s[0] == 'c') challenge = std::string(s.c_str() + 1);
      rig.oob("connect 28 " + std::to_string(10 + i) + " " + challenge + " \"\\name\\p" +
                  std::to_string(i) + "\"\n",
              i);
      const std::vector<std::string> a = rig.answers(i);
      if (i == 1) CHECK((limit == 1) == any_has(a, "Too many connections from your IP address."));
    }
    CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == limit);
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

// sv_idletime 2: a client that presses nothing is dropped and the others told; one that keeps
// moving stays.
TEST(an_idle_client_is_dropped) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.idle_time = 2;
  CHECK(rig.start(cfg));
  gn::Transport ct[2];
  auto c = std::make_unique<cn::ClassicClientEnd[]>(2);
  for (int i = 0; i < 2; i++) {
    gn::open_sim_transport(ct[i], rig.net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = i == 0 ? "idler" : "busy";
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    if (i == 0) cc.script = [](int, int) { return sh::MoveCmd{}; };
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  for (int f = 0; f < 1200; f++) {
    rig.frame();
    for (int i = 0; i < 2; i++) cn::client_tick(c[i], rig.now);
  }
  CHECK(c[0].stage != cn::ClientStage::active);
  CHECK_MSG(c[1].stage == cn::ClientStage::active, c[1].refused);
  bool told = false;
  for (const std::string& l : c[1].prints) told = told || l == "idler was dropped for idling.\n";
  CHECK(told);
}

TEST(renames_past_the_limit_are_refused_and_the_rest_told) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.names = {2, 60, 1};  // two renames a minute, everyone told
  CHECK(rig.start(cfg));
  gn::Transport ct[2];
  auto c = std::make_unique<cn::ClassicClientEnd[]>(2);
  for (int i = 0; i < 2; i++) {
    gn::open_sim_transport(ct[i], rig.net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = i == 0 ? "renamer" : "watcher";
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  auto run = [&](int frames) {
    for (int f = 0; f < frames; f++) {
      rig.frame();
      for (int i = 0; i < 2; i++) cn::client_tick(c[i], rig.now);
    }
  };
  run(300);
  CHECK(c[0].stage == cn::ClientStage::active && c[1].stage == cn::ClientStage::active);
  for (const char* name : {"one", "two", "three"}) {
    cn::client_command(c[0], std::string("setinfo name ") + name);
    run(40);
  }
  auto heard = [&](int i, const std::string& line) {
    for (const std::string& l : c[i].prints)
      if (l == line) return true;
    return false;
  };
  CHECK(heard(1, "renamer changed name to one.\n"));
  CHECK(heard(1, "one changed name to two.\n"));
  CHECK(!heard(1, "two changed name to three.\n"));
  CHECK(heard(0, "You can't change your name too often.\n"));
  bool kept = false;
  for (const cn::ClassicClient& k : rig.server.clients) kept = kept || k.name == "two";
  CHECK(kept);
}

TEST(clients_on_one_host_each_keep_their_own_challenge) {
  // Players behind one router (or bots from one machine): each asks for a challenge before any
  // connects, and each gets in.
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  gn::Transport ct[3];
  auto c = std::make_unique<cn::ClassicClientEnd[]>(3);
  for (int i = 0; i < 3; i++) {
    gn::open_sim_transport(ct[i], rig.net,
                           gn::sim_address(1, static_cast<uint16_t>(cn::kPortClient + i)));
    cn::ClientConfig cc;
    cc.name = "p" + std::to_string(i);
    cc.qport = static_cast<uint16_t>(200 + i);
    cc.seed = i + 1;
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  for (int f = 0; f < 600; f++) {
    rig.frame();
    for (int i = 0; i < 3; i++) cn::client_tick(c[i], rig.now);
  }
  for (int i = 0; i < 3; i++)
    CHECK_MSG(c[i].stage == cn::ClientStage::active,
              "client " + std::to_string(i) + ": " + c[i].refused);
  CHECK(rig.server.counts.rejects == 0);
}

TEST(an_address_that_asks_again_gets_the_same_challenge) {
  // A client that asked twice (its first answer read late) connects with either answer.
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  rig.oob("getchallenge\n");
  const std::vector<std::string> first = rig.answers();
  rig.oob("getchallenge\n");
  const std::vector<std::string> again = rig.answers();
  CHECK(first.size() == 1 && again.size() == 1);
  const auto value = [](const std::string& a) {
    return a.substr(0, a.find_first_not_of("c-0123456789"));
  };
  CHECK(!first.empty() && !again.empty() && value(first[0]) == value(again[0]));
  rig.oob("getchallenge\n", 1);  // another host: its own
  const std::vector<std::string> other = rig.answers(1);
  CHECK(other.size() == 1 && value(other[0]) != value(first[0]));
}

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

// sv_safestrafe 3: a reversal is zeroed until three commands of none lie between; the player's own
// zero commands count; the same direction again after a short stop passes; off, nothing changes.
TEST(the_safe_strafe_keeps_reversals_apart) {
  cn::StrafeGuard g;
  CHECK(cn::safe_strafe(g, 400, 3) == 400);  // right
  CHECK(cn::safe_strafe(g, -400, 3) == 0);   // left at once: zeroed
  CHECK(cn::safe_strafe(g, -400, 3) == 0);   // two more
  CHECK(cn::safe_strafe(g, -400, 3) == 0);
  CHECK(cn::safe_strafe(g, -400, 3) == -400);  // three between: left
  CHECK(cn::safe_strafe(g, 0, 3) == 0);        // one of his own
  CHECK(cn::safe_strafe(g, 400, 3) == 0);      // right: two more to go
  CHECK(cn::safe_strafe(g, 400, 3) == 0);
  CHECK(cn::safe_strafe(g, 400, 3) == 400);
  CHECK(cn::safe_strafe(g, 0, 3) == 0);
  CHECK(cn::safe_strafe(g, 400, 3) == 400);  // the same way after a short stop
  cn::StrafeGuard off;
  CHECK(cn::safe_strafe(off, 400, 0) == 400);
  CHECK(cn::safe_strafe(off, -400, 0) == -400);
}

TEST(a_client_gives_up_a_server_that_stops_answering) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  gn::Transport ct[1];
  cn::ClassicClientEnd c[1];
  CHECK(play(rig, ct, c, 1));
  c[0].cfg.timeout = 2;
  // The server quits: only the network and the client go on.
  for (int f = 0; f < 140; f++) {  // 1.82 s: still waiting
    rig.now += 13000;
    gn::sim_advance(rig.net, rig.now);
    cn::client_tick(c[0], rig.now);
  }
  CHECK(c[0].stage == cn::ClientStage::active);
  for (int f = 0; f < 20; f++) {
    rig.now += 13000;
    gn::sim_advance(rig.net, rig.now);
    cn::client_tick(c[0], rig.now);
  }
  CHECK(c[0].stage == cn::ClientStage::dropped);
  CHECK(c[0].refused == "the server stopped answering");
}

TEST(a_bots_seat_answers_the_status_with_no_lag) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  std::string error;
  CHECK_MSG(cn::classic_add_local(rig.server, 0, "bot1", error), error);
  for (int f = 0; f < 40; f++) rig.frame();
  rig.oob("status 23\n");
  const std::vector<std::string> a = rig.answers();
  CHECK(any_has(a, "\"bot1\""));
  CHECK(!any_has(a, "9999"));  // a ping of no samples was shown as 9999 ms in the browser
}

TEST(a_missing_optional_file_is_quiet_and_a_missing_map_is_told) {
  // A client tries the server for a skin or a map's place names it lacks: when the server has
  // none either, nothing is printed. A map the server lacks is worth a line.
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  gn::Transport ct[1];
  cn::ClassicClientEnd c[1];
  CHECK(play(rig, ct, c, 1));
  const auto told = [&c](const std::string& what) {
    for (const std::string& p : c[0].prints)
      if (p.find(what) != std::string::npos) return true;
    return false;
  };
  cn::client_command(c[0], "download skins/nobody.pcx");
  cn::client_command(c[0], "download locs/nowhere.loc");
  cn::client_command(c[0], "download maps/nowhere.bsp");
  for (int f = 0; f < 60; f++) {
    rig.frame();
    cn::client_tick(c[0], rig.now);
  }
  CHECK(!told("skins/nobody.pcx"));
  CHECK(!told("locs/nowhere.loc"));
  CHECK(told("Download of maps/nowhere.bsp refused: not found."));
}

TEST(a_downloaded_file_is_told_the_archive_it_came_out_of) {
  // The server's pak1.pak holds two skins; a third skin is loose in its folder. Three clients wear
  // one each and fetch the others. A client that agreed the ge set's download origin is told each
  // packed file's archive before it comes and keeps it in pak1.partial.pak until both are held,
  // then has the server's pak1.pak byte for byte; the loose skin comes with no archive, and a
  // client that did not agree is told none.
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"skins/red.pcx", std::vector<uint8_t>(900, 1)},
                                {"skins/blue.pcx", std::vector<uint8_t>(1500, 2)}},
                               &pak, &error),
            error);
  ge::vfs::SearchPath files;
  CHECK_MSG(ge::vfs::add_archive(&files, "pak1.pak", pak, "id1", &error), error);
  cn::ServerConfig cfg;
  cfg.read_file = [&files](const std::string& path, std::vector<uint8_t>* out) {
    if (path == "skins/green.pcx") {
      out->assign(700, 3);
      return true;
    }
    const ge::vfs::FileRef* f = ge::vfs::find(files, path);
    std::string why;
    return f != nullptr && ge::vfs::read(files, *f, out, &why);
  };
  cfg.file_origin = [&files](const std::string& path, ge::vfs::ArchiveOrigin* out) {
    const ge::vfs::FileRef* f = ge::vfs::find(files, path);
    return f != nullptr && ge::vfs::archive_origin(files, *f, out);
  };
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cfg));
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "ge_se_download_origin";
  std::filesystem::remove_all(root);
  gn::Transport ct[3];
  auto c = std::make_unique<cn::ClassicClientEnd[]>(3);
  const char* skins[] = {"red", "blue", "green"};
  int told[3] = {}, untold[3] = {};
  for (int i = 0; i < 3; i++) {
    gn::open_sim_transport(ct[i], rig.net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = "p" + std::to_string(i);
    cc.skin = skins[i];
    cc.qport = static_cast<uint16_t>(300 + i);
    cc.seed = i + 1;
    if (i == 2) cc.ask[cn::ExtFamily::ge] &= ~cn::kGeDownloadOrigin;
    const std::string dir = (root / std::to_string(i) / "id1").string();
    cc.keep_file = [&, i, dir](const std::string&, const std::string& path,
                               const std::vector<uint8_t>& bytes,
                               const ge::vfs::ArchiveOrigin* origin) {
      if (origin == nullptr) {
        untold[i]++;
        return;
      }
      told[i]++;
      CHECK(origin->archive == "pak1.pak" && origin->folder == "id1" && origin->count == 2);
      ge::vfs::Kept kept;
      std::string why;
      CHECK_MSG(ge::vfs::keep_in_partial(dir, *origin, path, bytes, &kept, &why), why);
    };
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  for (int f = 0; f < 900; f++) {
    rig.frame();
    for (int i = 0; i < 3; i++) cn::client_tick(c[i], rig.now);
  }
  // Every player's skin is fetched, the client's own too: both packed skins (told) and the loose
  // one (untold) by the first two, which then hold the server's pak1.pak byte for byte.
  for (int i = 0; i < 2; i++) {
    CHECK_MSG(told[i] == 2 && untold[i] == 1, "client " + std::to_string(i) + " told " +
                                                  std::to_string(told[i]) + ", untold " +
                                                  std::to_string(untold[i]));
    const std::filesystem::path dir = root / std::to_string(i) / "id1";
    CHECK(!std::filesystem::exists(dir / "pak1.partial.pak"));
    std::ifstream in(dir / "pak1.pak", std::ios::binary);
    CHECK(std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}) == pak);
  }
  CHECK(told[2] == 0 && untold[2] == 3);
  std::filesystem::remove_all(root);
}
