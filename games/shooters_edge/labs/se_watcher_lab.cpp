// LAB `se-watcher`: A MATCH ON A SERVER WITH NO BROADCASTER, WATCHED THROUGH OUR TV NODE BY WAY OF
// A WATCHER (games/shooters_edge/net/classic_watcher.hpp). A spectator joins the server, its view
// is broadcast as a replay file while it grows, and a TV node held 1 s sends it to a viewer there
// from the start and one who joins half way. Without `--server` the match is our classic server on
// the simulated network with a player running round (deterministic: the smoke run); with
// `--server=<host:port>` the watcher joins a live classic server over UDP (one whose stream port is
// off, or any other: the watcher needs none). It answers whether the watcher got in, how many
// frames and bytes it saw, what the broadcast cost, and whether both viewers got the whole view.
//
// ge:summary Lab: a spectator's view of a server with no broadcaster, watched through a TV node
// ge:keywords lab se-watcher watcher spectator tv broadcast no-broadcaster late-viewer live-server
// ge:skill net replay labs
// ge:test tests/shooters_edge_classic_watcher_test.cpp
#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/recording_tv.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_watcher.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "tv/node.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace tv = ge::tv;

// The TV node and its two viewers, fed the watcher's broadcast.
struct Viewers {
  tv::Node node;
  tv::Watch early, late;
  int64_t late_catch_up = -1;
  bool fed = true;

  void feed(cn::ClassicWatcher& w, int64_t now_ms) {
    const std::span<const uint8_t> fresh = cl::recording_broadcast_fresh(w.cast);
    if (!fresh.empty()) fed = tv::tv_feed(node, fresh, now_ms) && fed;
    tv::Watch* watch[2] = {&early, &late};
    tv::tv_update(node, now_ms, [&](int link, std::span<const uint8_t> msg) {
      return tv::watch_receive(*watch[link], msg);
    });
  }
  void join(int link) {
    if (link == 1) late_catch_up = tv::tv_catch_up_blocks(node);
    tv::tv_join(node, link);
    tv::tv_receive(node, link, tv::tv_hello(link == 0 ? "early" : "late", false));
  }
};

void report(gl::Lab& lab, cn::ClassicWatcher& w, Viewers& v, const std::string& where) {
  int whole = 0, frames = 0;
  std::string error;
  for (const tv::Watch* x : {&v.early, &v.late}) {
    std::vector<uint8_t> back;
    if (!x->ended || !cl::unpack_demo(x->stream, back, error) || back != w.recording.bytes)
      continue;
    whole++;
    cl::Demo d;
    if (!cl::open_demo(back, d, error)) continue;
    int n = 0;
    while (cl::next_frame(d, error) == cl::DemoRead::frame) n++;
    frames = n;
  }
  const cn::ClassicClientEnd& c = w.client;
  gl::LabTable t{"the watcher and its viewers", {"what", "value"}, {}};
  t.rows.push_back({"server", where});
  t.rows.push_back({"the watcher's last stage", cn::client_stage_name(c.stage)});
  t.rows.push_back({"refused or dropped", c.refused.empty() ? "-" : c.refused});
  t.rows.push_back({"level", c.level.empty() ? "-" : c.level});
  t.rows.push_back({"frames seen", std::to_string(c.full_frames + c.delta_frames)});
  t.rows.push_back(
      {"recording bytes / broadcast bytes",
       std::to_string(w.recording.bytes.size()) + " / " + std::to_string(w.cast.out.size())});
  t.rows.push_back({"broadcast blocks / TV node fed",
                    std::to_string(w.cast.blocks) + " / " + (v.fed ? "yes" : "no")});
  t.rows.push_back({"the late viewer's catch-up (blocks)", std::to_string(v.late_catch_up)});
  t.rows.push_back({"viewers given the whole view (of 2)", std::to_string(whole)});
  t.rows.push_back({"frames a viewer reads to the end", std::to_string(frames)});
  lab.table(t);
  lab.metric("watcher.active", c.full_frames + c.delta_frames > 0 ? 1 : 0, "bool");
  lab.metric("watcher.frames", static_cast<double>(c.full_frames + c.delta_frames), "frames");
  lab.metric("watcher.recording_bytes", static_cast<double>(w.recording.bytes.size()), "bytes");
  lab.metric("watcher.broadcast_bytes", static_cast<double>(w.cast.out.size()), "bytes");
  lab.metric("watcher.viewers_whole", whole, "viewers");
  if (whole != 2)
    lab.fail("a viewer did not get the watcher's whole view through the TV node",
             "next: run tests/shooters_edge_classic_watcher_test.cpp; skill: replay.");
}

// Our classic server on the simulated network, a player running round, the watcher joining.
void simulated(gl::Lab& lab) {
  struct Rig {
    ge::progs::Progs progs;
    std::unique_ptr<sv::QcServer> qc;
    sv::RulesRunner rules;
    gn::SimNetwork net;
    gn::Transport st, pt, wt;
    cn::ClassicServer server;
    cn::ClassicClientEnd player;
    cn::ClassicWatcher watcher;
    Viewers viewers;
  };
  auto owned = std::make_unique<Rig>();  // a whole server: too big for a thread's stack
  Rig& r = *owned;
  std::string error;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  if (sv::build_test_mod(r.progs, error)) r.qc = sv::make_qc_server(r.progs, so, error);
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!r.qc || !sv::load_map(*r.qc, map, error))
    return lab.fail("the test match did not start: " + error,
                    "next: run tests/shooters_edge_classic_watcher_test.cpp; skill: net.");
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  r.net = gn::make_sim_network(1, lan);
  const gn::Address server_at = gn::sim_address(0, cn::kPortServer);
  gn::open_sim_transport(r.st, r.net, server_at);
  gn::open_sim_transport(r.pt, r.net, gn::sim_address(1, cn::kPortClient));
  gn::open_sim_transport(r.wt, r.net, gn::sim_address(2, cn::kPortClient));
  r.rules = sv::qc_runner_over(*r.qc);
  if (!cn::classic_start(r.server, r.rules, r.st, cn::ServerConfig{}, error))
    return lab.fail("the classic server did not start: " + error,
                    "next: run tests/shooters_edge_classic_server_test.cpp; skill: net.");
  cn::ClientConfig pc;
  pc.name = "runner";
  pc.qport = 4242;
  pc.seed = 3;
  cn::client_start(r.player, r.pt, server_at, pc, 0);
  cn::ClientConfig wc;
  wc.name = "watcher";
  wc.qport = 4343;
  wc.seed = 4;
  cn::watcher_start(r.watcher, r.wt, server_at, wc, {}, 0);
  r.viewers.node.config.delay_ms = 1000;
  r.viewers.join(0);
  const int ticks = static_cast<int>(lab.arg_num("seconds") * 1000.0 / 13.0);
  int64_t now = 0;
  for (int i = 0; i < ticks; i++) {
    now += 13000;
    gn::sim_advance(r.net, now);
    cn::classic_frame(r.server, now);
    cn::client_tick(r.player, now);
    cn::watcher_tick(r.watcher, now);
    r.viewers.feed(r.watcher, now / 1000);
    if (i == ticks / 2) r.viewers.join(1);
  }
  cn::watcher_stop(r.watcher);
  r.viewers.feed(r.watcher, now / 1000);  // the trailer, before the end
  tv::tv_end(r.viewers.node);
  for (int i = 0; i < 400; i++) {
    now += 13000;
    r.viewers.feed(r.watcher, now / 1000);
  }
  report(lab, r.watcher, r.viewers, "our classic server, simulated network");
}

// A live classic server over UDP, by the wall clock.
void live(gl::Lab& lab, const std::string& server) {
  std::string text = server;
  if (text.find(':') == std::string::npos) text += ":27500";
  gn::Address to;
  if (!gn::parse_address(text, to))
    return lab.fail("'" + server + "' is not host:port", "next: --server=127.0.0.1:27500");
  gn::Transport t;
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error))
    return lab.fail("the UDP socket did not open: " + error, "next: check the container's network");
  cn::ClientConfig cfg;
  cfg.name = lab.arg("name");
  const std::string& basedir = lab.arg("basedir");
  auto files = std::make_shared<cl::SearchPath>();
  auto mounted = std::make_shared<std::string>();
  if (!basedir.empty())
    // NOLINTNEXTLINE(bugprone-exception-escape)
    cfg.read_file = [basedir, files, mounted](const std::string& gamedir, const std::string& path) {
      std::vector<uint8_t> store;
      if (*mounted != gamedir) {
        std::string why;
        *files = {};
        if (!cl::open_game_dir(basedir, gamedir, *files, why)) return store;
        *mounted = gamedir;
      }
      const std::span<const uint8_t> got = cl::find_file(*files, path, store);
      return std::vector<uint8_t>(got.begin(), got.end());
    };
  auto w = std::make_unique<cn::ClassicWatcher>();
  auto v = std::make_unique<Viewers>();
  cn::watcher_start(*w, t, to, cfg, {}, 0);
  v->node.config.delay_ms = 1000;
  v->join(0);
  const auto start = std::chrono::steady_clock::now();
  const auto seconds = static_cast<int64_t>(lab.arg_num("seconds") * 1e6);
  bool joined = false;
  int64_t now_us = 0;
  while (now_us < seconds && !w->ended) {
    std::this_thread::sleep_for(std::chrono::milliseconds(13));
    now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    cn::watcher_tick(*w, now_us);
    v->feed(*w, now_us / 1000);
    if (!joined && now_us >= seconds / 2) {
      v->join(1);
      joined = true;
    }
  }
  cn::watcher_stop(*w);
  v->feed(*w, now_us / 1000);  // the trailer, before the end
  tv::tv_end(v->node);
  for (int i = 0; i < 400; i++) {
    now_us += 13000;
    v->feed(*w, now_us / 1000);
  }
  report(lab, *w, *v, text);
}

void run(gl::Lab& lab) {
  const std::string& server = lab.arg("server");
  if (server.empty())
    simulated(lab);
  else
    live(lab, server);
}

const gl::LabSpec kSpec{
    "se-watcher",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_watcher: a server with no broadcaster watched through TV",
    "replay",
    "A spectator's view of a server with no broadcaster, broadcast as it is recorded through a TV "
    "node to a viewer from the start and one half way: does each get the whole view?",
    {
        {"server", "", "a live classic server, as host:port (empty: ours, simulated)"},
        {"seconds", "20", "how long the watcher watches"},
        {"name", "watcher", "the spectator's name"},
        {"basedir", "", "the game's base folder (its id1 and mod folders): the map's checksum"},
    },
    "--seconds=8",
    run,
};
GE_LAB(kSpec);

}  // namespace
