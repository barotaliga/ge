// games/shooters_edge/net/sequel_net*.hpp, sequel_client.hpp, sequel_net_server.hpp: the sequel's
// live protocol. A move and its checksum come back as written; the scripted client joins the
// sequel's net server on the simulated network through the whole signon, its frames are deltas,
// its recording walks whole, and a client that walks forward is seen to move; refusals read as a
// player's words; a lossy link still plays.
//
// ge:summary Test: the sequel's live protocol, our client joined to our server on a simulated net
// ge:keywords test sequel protocol-34 live client server signon move checksum delta refusal loss
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "forward/forward.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace fw = ge::forward;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// Our server on the simulated network, and as many clients as asked.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::SequelNetServer server;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::SequelNetConfig& cfg, const char* conditions = "lan") {
  auto r = std::make_unique<Rig>();
  r->rules = sv::sequel_runner(sv::make_sequel_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset(conditions, link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortSequel));
  CHECK_MSG(cn::sequel_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

cn::SequelClientEnd& join(Rig& r, cn::SequelClientConfig cfg) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::SequelClientEnd>());
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::sequel_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortSequel), cfg,
                          static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::sequel_client_tick(*c, r.now);
    cn::sequel_net_frame(r.server, r.now);
  }
}

sh::MoveCmd forward(int /*tick*/) {
  sh::MoveCmd m;
  m.forward = 300;
  return m;
}

}  // namespace

TEST(a_move_and_its_checksum_come_back_as_written) {
  cn::SequelCmd cmds[3];
  cmds[1].forward = 400;
  cmds[1].angles[1] = 8192;
  cmds[2] = cmds[1];
  cmds[2].buttons = 1;
  cmds[2].msec = 16;
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_sequel_move(w, 42, cmds, 1234);
  CHECK(!w.overflow);
  CHECK(buf[0] == cn::kClcMove);
  gn::Reader r = gn::make_reader(buf + 1, w.size - 1);
  cn::SequelMove m;
  CHECK(cn::read_sequel_move(r, 1234, m));
  CHECK(m.checksum_ok);
  CHECK(m.frame == 42);
  for (int k = 0; k < 3; k++) CHECK(m.cmds[k] == cmds[k]);
  gn::Reader again = gn::make_reader(buf + 1, w.size - 1);
  CHECK(cn::read_sequel_move(again, 1235, m));
  CHECK(!m.checksum_ok);  // another packet's number: another byte
}

TEST(the_checksum_is_the_byte_a_live_server_expected) {
  // Moves a live server of the sequel refused, with the byte it said it expected (the body after
  // the checksum byte, the packet's sequence): what the measured key table must reproduce.
  struct Vector {
    const char* body;
    uint32_t sequence;
    int expected;
  };
  const Vector vectors[] = {
      {"ab020000082c010d00000d00000d00", 37, 58},
      {"b80200000a00802c010d00000d00000d00", 612, 19},
      {"c10200000a00402c010d00000d00000d00", 959, 7},
      {"cc0200000a00c02c010d00000d00000d00", 1500, 96},
  };
  for (const Vector& v : vectors) {
    std::string body;
    CHECK(cn::from_hex(v.body, body));
    const uint8_t got = cn::sequel_move_checksum(reinterpret_cast<const uint8_t*>(body.data()),
                                                 body.size(), v.sequence);
    CHECK_MSG(got == v.expected, "sequence " + std::to_string(v.sequence) + ": " +
                                     std::to_string(got) + ", the server expected " +
                                     std::to_string(v.expected));
  }
  uint64_t sum = 0, weighted = 0;
  for (int i = 0; i < cn::kSequelChecksumTableSize; i++) {
    sum += cn::kSequelChecksumTable[i];
    weighted += uint64_t(i + 1) * cn::kSequelChecksumTable[i];
  }
  CHECK(sum == 124780);
  CHECK(weighted == 59989697);
  for (int i = 960; i < cn::kSequelChecksumTableSize; i++) CHECK(cn::kSequelChecksumTable[i] == 0);
}

TEST(out_of_band_words_keep_a_quoted_word_whole) {
  const auto w = cn::sequel_words("connect 34 7 99 \"\\name\\a b\\skin\\male/grunt\"\n");
  CHECK(w.size() == 5);
  CHECK(w[4] == "\\name\\a b\\skin\\male/grunt");
}

TEST(a_client_joins_through_the_signon_and_plays_on_deltas) {
  auto r = rig({});
  cn::SequelClientConfig cfg;
  cfg.name = "eve";
  cfg.brain = forward;
  cn::SequelClientEnd& c = join(*r, cfg);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.signon_steps >= 3);  // configstrings, baselines, begin at least
  CHECK(c.levels == 1);
  CHECK(c.demo.state.map == "hall");
  const float x0 = c.demo.state.eye.x, y0 = c.demo.state.eye.y;
  run(*r, 200);
  CHECK(c.frames >= 20);
  const cn::SequelNetClient& sc = r->server.clients[0];
  CHECK(sc.moves > 100);
  CHECK(sc.bad_checksums == 0);
  CHECK_MSG(sc.full_frames <= 2, std::to_string(sc.full_frames) + " whole frames");
  const float dx = c.demo.state.eye.x - x0, dy = c.demo.state.eye.y - y0;
  CHECK_MSG(dx * dx + dy * dy > 100.0f * 100.0f,
            "the player walked " + std::to_string(dx) + "," + std::to_string(dy));
  cn::sequel_client_stop(c);
  run(*r, 10);
  CHECK(cn::sequel_net_clients(r->server) == 0);
  // The recording it kept walks whole.
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(c.demo.bytes, d, error), error);
  int frames = 0;
  cl::DemoRead got;
  while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) frames++;
  CHECK_MSG(got == cl::DemoRead::end, error);
  CHECK(frames == c.frames);
}

TEST(refusals_read_as_a_players_words) {
  cn::SequelNetConfig cfg;
  cfg.password = "secret";
  cfg.max_clients = 1;
  auto r = rig(cfg);
  cn::SequelClientConfig wrong;
  wrong.name = "mallory";
  cn::SequelClientEnd& a = join(*r, wrong);
  run(*r, 40);
  CHECK(a.stage == cn::ClientStage::dropped);
  CHECK(a.refused == "Password required or incorrect.\nConnection refused.\n");
  cn::SequelClientConfig right;
  right.password = "secret";
  cn::SequelClientEnd& b = join(*r, right);
  run(*r, 80);
  CHECK_MSG(b.stage == cn::ClientStage::active, b.refused);
  cn::SequelClientEnd& full = join(*r, right);
  run(*r, 40);
  CHECK(full.stage == cn::ClientStage::dropped);
  CHECK(full.refused == "Server is full.\n");
}

TEST(a_lossy_link_still_plays) {
  auto r = rig({}, "bad");
  cn::SequelClientConfig cfg;
  cfg.brain = forward;
  cn::SequelClientEnd& c = join(*r, cfg);
  run(*r, 800);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.frames > 30);
  CHECK(r->server.clients[0].moves > 300);
}

TEST(a_client_plays_through_two_forwarders) {
  auto r = rig({});
  // Forwarders on nodes 20 and 21 (their upstream sockets from port 40000), the server on node 0.
  const gn::Address a = gn::sim_address(20, 30000), b = gn::sim_address(21, 30000);
  gn::Transport a_port, b_port;
  gn::open_sim_transport(a_port, r->net, a);
  gn::open_sim_transport(b_port, r->net, b);
  uint16_t next_up = 40000;
  auto opener = [&r, &next_up](uint32_t node) {
    return [&r, &next_up, node](gn::Transport& t) {
      return gn::open_sim_transport(t, r->net, gn::sim_address(node, next_up++));
    };
  };
  fw::Forward fa, fb;
  fw::ForwardConfig fc;
  fc.name = "first";
  fc.allow = {b};
  fc.seed = 1;
  fw::forward_init(fa, fc, a_port, opener(20));
  fc.name = "second";
  fc.allow = {gn::sim_address(0, cn::kPortSequel)};
  fc.seed = 2;
  fw::forward_init(fb, fc, b_port, opener(21));
  cn::SequelClientConfig cfg;
  cfg.brain = forward;
  cfg.via = {a, b};
  cn::SequelClientEnd& c = join(*r, cfg);
  for (int i = 0; i < 300; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    fw::forward_update(fa, r->now);
    fw::forward_update(fb, r->now);
    cn::sequel_client_tick(c, r->now);
    cn::sequel_net_frame(r->server, r->now);
  }
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(c.dial.state == fw::DialState::routed);
  CHECK(c.frames > 20);
  CHECK(r->server.clients[0].moves > 200);
  CHECK(r->server.clients[0].bad_checksums == 0);
  // The server sees the second forwarder, not the client.
  CHECK(gn::sim_node(r->server.clients[0].address) == 21);
  fw::forward_close(fa);
  fw::forward_close(fb);
}
