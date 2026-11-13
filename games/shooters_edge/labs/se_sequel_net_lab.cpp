// LAB `se-sequel-net`: THE SEQUEL'S LIVE PROTOCOL (games/shooters_edge/net/sequel_*.hpp). Without
// `--server` our scripted client joins our sequel net server on the simulated network
// (deterministic: the smoke run) and a second player walks; with `--server=<host:port>` the client
// joins a live server of the sequel's protocol over UDP by the wall clock. It answers whether the
// client got through the signon, how many frames came and how many were deltas, whether its
// moves moved it (`--walk`), what the link cost, and whether the recording it kept walks whole.
//
// ge:summary Lab: our sequel client on our sequel server, or on a live one, through the signon
// ge:keywords lab se-sequel-net sequel protocol-34 live client server signon frames deltas moves
// ge:keywords checksum recording live-server
// ge:skill net shooter labs
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

// A hall of 2048 units with two starts.
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  for (const char* at : {"-400 -400 24", "400 400 24"}) {
    br::Entity e;
    e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", at}, {"angle", "45"}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Seen {
  double x0 = 0, y0 = 0, z0 = 0;
  bool placed = false;
};

void report(gl::Lab& lab, const cn::SequelClientEnd& c, const Seen& seen, const std::string& where,
            const cn::SequelNetClient* ours, gl::Det det) {
  cl::Demo d;
  std::string error;
  int read = 0;
  bool whole = cl::open_demo(c.demo.bytes, d, error);
  if (whole) {
    cl::DemoRead got;
    while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) read++;
    whole = got == cl::DemoRead::end && read == c.frames;
  }
  const double dx = c.demo.state.eye.x - seen.x0, dy = c.demo.state.eye.y - seen.y0;
  const double moved = seen.placed ? std::sqrt(dx * dx + dy * dy) : 0;
  gl::LabTable t{"the client on the sequel's server", {"what", "value"}, {}};
  t.rows.push_back({"server", where});
  t.rows.push_back({"the client's last stage", cn::client_stage_name(c.stage)});
  t.rows.push_back({"refused or dropped", c.refused.empty() ? "-" : c.refused});
  t.rows.push_back({"map", c.demo.state.map.empty() ? "-" : c.demo.state.map});
  t.rows.push_back({"signon steps answered", std::to_string(c.signon_steps)});
  t.rows.push_back({"frames read", std::to_string(c.frames)});
  t.rows.push_back({"moves sent", std::to_string(c.moves)});
  t.rows.push_back({"units its view moved", ge::core::num::to_fixed(moved, 1)});
  t.rows.push_back({"bytes in / out", std::to_string(c.bytes_in) + " / " +
                                          std::to_string(c.bytes_out)});
  t.rows.push_back({"recording bytes, walks whole",
                    std::to_string(c.demo.bytes.size()) + ", " + (whole ? "yes" : "no")});
  if (ours != nullptr) {
    t.rows.push_back({"server: moves taken / bad checksums", std::to_string(ours->moves) + " / " +
                                                               std::to_string(ours->bad_checksums)});
    t.rows.push_back({"server: frames / whole frames", std::to_string(ours->frames) + " / " +
                                                         std::to_string(ours->full_frames)});
  }
  for (size_t i = 0; i < c.prints.size() && i < 6; i++) t.rows.push_back({"print", c.prints[i]});
  lab.table(t, det);
  lab.metric("sequel.active", c.frames > 0 ? 1 : 0, "bool", det);
  lab.metric("sequel.frames", static_cast<double>(c.frames), "frames", det);
  lab.metric("sequel.moved", moved, "units", det);
  lab.metric("sequel.recording_whole", whole ? 1 : 0, "bool", det);
  if (ours != nullptr) lab.metric("sequel.full_frames", double(ours->full_frames), "frames", det);
  if (c.frames == 0)
    lab.fail("the client did not get through the signon: " + c.refused,
             "next: run tests/shooters_edge_sequel_net_test.cpp; skill: net.");
  else if (!whole)
    lab.fail("the client's recording does not walk whole: " + error,
             "next: run tests/shooters_edge_sequel_net_test.cpp; skill: replay.");
}

sh::MoveCmd walk(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles.y = static_cast<float>((tick / 100) % 4) * 90.0f;
  return m;
}

void simulated(gl::Lab& lab) {
  struct Rig {
    sv::RulesRunner rules;
    gn::SimNetwork net;
    gn::Transport st, at, bt;
    cn::SequelNetServer server;
    cn::SequelClientEnd a, b;
  };
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  auto map = hall(error);
  r.rules = sv::sequel_runner(sv::make_sequel_server({}));
  r.rules.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  if (map == nullptr || !r.rules.start(spec, error))
    return lab.fail("the sequel's match did not start: " + error,
                    "next: run tests/shooters_edge_sequel_net_test.cpp; skill: net.");
  gn::LinkConditions link;
  gn::conditions_preset(lab.arg("conditions"), link);
  r.net = gn::make_sim_network(1, link);
  const gn::Address at = gn::sim_address(0, cn::kPortSequel);
  gn::open_sim_transport(r.st, r.net, at);
  gn::open_sim_transport(r.at, r.net, gn::sim_address(1, 27901));
  gn::open_sim_transport(r.bt, r.net, gn::sim_address(2, 27901));
  if (!cn::sequel_net_start(r.server, r.rules, r.st, cn::SequelNetConfig{}, error))
    return lab.fail(error, "next: skill: net.");
  cn::SequelClientConfig ac;
  ac.name = "eve";
  ac.qport = 11;
  if (lab.arg_num("walk") != 0) ac.brain = walk;
  cn::sequel_client_start(r.a, r.at, at, ac, 0);
  cn::SequelClientConfig bc;
  bc.name = "bob";
  bc.qport = 12;
  bc.brain = walk;
  cn::sequel_client_start(r.b, r.bt, at, bc, 0);
  const int ticks = static_cast<int>(lab.arg_num("seconds") * 1000.0 / 13.0);
  Seen seen;
  int64_t now = 0;
  for (int i = 0; i < ticks; i++) {
    now += 13000;
    gn::sim_advance(r.net, now);
    cn::sequel_client_tick(r.a, now);
    cn::sequel_client_tick(r.b, now);
    cn::sequel_net_frame(r.server, now);
    if (!seen.placed && r.a.frames > 0)
      seen = {r.a.demo.state.eye.x, r.a.demo.state.eye.y, r.a.demo.state.eye.z, true};
  }
  cn::sequel_client_stop(r.a);
  report(lab, r.a, seen, "ours, simulated network (" + lab.arg("conditions") + ")",
         &r.server.clients[0], gl::Det::deterministic);
}

void live(gl::Lab& lab, const std::string& server) {
  std::string text = server;
  if (text.find(':') == std::string::npos) text += ":27910";
  gn::Address to;
  if (!gn::parse_address(text, to))
    return lab.fail("'" + server + "' is not host:port", "next: --server=127.0.0.1:27910");
  gn::Transport t;
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error))
    return lab.fail("the UDP socket did not open: " + error, "next: check the container's network");
  cn::SequelClientConfig cfg;
  cfg.name = lab.arg("name");
  cfg.spectator = lab.arg_num("spectator") != 0;
  cfg.qport = 2026;
  if (lab.arg_num("walk") != 0) cfg.brain = walk;
  auto c = std::make_unique<cn::SequelClientEnd>();
  cn::sequel_client_start(*c, t, to, cfg, 0);
  const auto start = std::chrono::steady_clock::now();
  const auto seconds = static_cast<int64_t>(lab.arg_num("seconds") * 1e6);
  Seen seen;
  int64_t now_us = 0;
  while (now_us < seconds && c->stage != cn::ClientStage::dropped) {
    std::this_thread::sleep_for(std::chrono::milliseconds(13));
    now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    cn::sequel_client_tick(*c, now_us);
    if (!seen.placed && c->frames > 0)
      seen = {c->demo.state.eye.x, c->demo.state.eye.y, c->demo.state.eye.z, true};
  }
  cn::sequel_client_stop(*c);
  report(lab, *c, seen, text, nullptr, gl::Det::measured);
}

void run(gl::Lab& lab) {
  const std::string& server = lab.arg("server");
  if (server.empty())
    simulated(lab);
  else
    live(lab, server);
}

const gl::LabSpec kSpec{
    "se-sequel-net",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/sequel_*: the sequel's live protocol, client and server",
    "net",
    "Our sequel client through a server's signon, on our server simulated or a live one: does it "
    "get frames as deltas, do its moves move it, and does its recording walk whole?",
    {
        {"server", "", "a live sequel server, as host:port (empty: ours, simulated)"},
        {"seconds", "10", "how long the client plays"},
        {"conditions", "lan", "the simulated link's preset (lan, dsl, bad ...)"},
        {"walk", "1", "1: the client walks a square; 0: it stands"},
        {"spectator", "0", "1: join as a spectator"},
        {"name", "edge", "the client's name"},
    },
    "--seconds=6",
    run,
};
GE_LAB(kSpec);

}  // namespace
