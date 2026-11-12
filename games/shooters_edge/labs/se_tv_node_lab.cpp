// LAB `se-tv-node`: A MATCH RELAYED BY OUR TV NODE (games/shooters_edge/net/classic_tv.hpp). Two
// clients play the test mod's arena over the simulated network; our TV node pulls the match from
// the server's stream port over loopback TCP, holds it for the delay and serves it to viewers who
// join at the given seconds, each caught up from the match as the node keeps it. It answers each
// viewer's catch-up, the frames it read, whether its stream plays to the same end as the first
// viewer's (every player, their frags and name, every entity), and whether a viewer's say reached
// every viewer and its say_game the players. The bytes each viewer was sent are charted over the
// match. With `--upstream=<host:port>` the node relays a live server's stream port instead (a
// server built beside the checkout, never in the tree): on wall-clock time, its viewers' frames,
// the node's name in the server info and the chat sent up are reported; no digest is kept.
//
// ge:summary Lab: a match relayed by our TV node: delay, late viewers' catch-up, same end, chat
// ge:keywords lab tv-node relay delay catch-up late-join viewers say say_game same-end
// ge:skill net shooter labs
// ge:test tests/shooters_edge_classic_tv_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/tcp_socket.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_tv.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace gl = ge::lab;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

constexpr int kClients = 2;
constexpr int kTickUs = 13000;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<double> numbers(const std::string& s) {
  std::vector<double> out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t comma = s.find(',', at);
    out.push_back(std::atof(s.substr(at, comma == std::string::npos ? comma : comma - at).c_str()));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

struct Viewer {
  double join_s = 0;
  bool dialed = false;
  gn::TcpStream tcp;
  std::vector<uint8_t> got;
  gl::Trace line;
};

struct Match {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct[kClients];
  cn::ClassicServer server;
  cn::ClassicClientEnd c[kClients];
  cn::QtvPort port;
  cn::TvNode node;
};

// Loopback is not instant everywhere (macOS queues it): before a side reads, every byte the other
// side has sent is in, so a run reads the same bytes on the same step on every machine.
void settle(Match& m, std::vector<Viewer>& viewers) {
  std::vector<gn::TcpStream*> all{&m.node.up};
  for (const auto& r : m.port.relays) all.push_back(&r->tcp);
  for (const auto& v : m.node.viewers) all.push_back(&v->tcp);
  for (Viewer& v : viewers) all.push_back(&v.tcp);
  gn::tcp_settle_pairs(all);
}

// Until `l` holds the connection just made, so it is accepted on the next look on every machine.
void until_waiting(const gn::TcpListener& l, gn::TcpStream& dialed) {
  for (int i = 0; i < 1000000 && !gn::tcp_waiting(l) && !dialed.closed; i++) gn::tcp_pump(dialed);
}

void take(Viewer& v) {
  gn::tcp_pump(v.tcp);
  gn::tcp_take_bytes(v.tcp, v.got);
}

// The end a viewer's stream plays to, as text; `frames` read, `said` and `game` whether the
// chat came through.
std::string end_of(const Viewer& v, int& frames, bool& said, bool& game) {
  const std::string text(v.got.begin(), v.got.end());
  cn::QtvAnswer a;
  size_t used = 0;
  if (cn::qtv_parse_answer(text, a, used) != cn::QtvParse::ok || a.kind != cn::QtvAnswerKind::begin)
    return "not let in";
  cl::Demo d;
  std::string error;
  if (!cl::open_demo(
          std::vector<uint8_t>(v.got.begin() + static_cast<std::ptrdiff_t>(used), v.got.end()), d,
          error))
    return "unread";
  for (;;) {
    const cl::DemoRead st = cl::next_frame(d, error);
    if (st == cl::DemoRead::error) return "broke: " + error;
    if (st == cl::DemoRead::end) break;
    frames++;
    for (const cl::DemoPrint& p : d.state.prints)
      game = game || p.text.find("qtv_say_game:#1:eve: good game") != std::string::npos;
  }
  said = text.find("#0:qtv_say:#1:eve: hello") != std::string::npos;
  std::string t;
  const auto f = [](float x) { return std::to_string(static_cast<int>(x * 8)); };
  for (size_t k = 0; k < d.state.players.size(); k++) {
    const cl::DemoEntity& p = d.state.players[k];
    if (p.present)
      t += std::to_string(k) + ":" + f(p.origin.x) + "," + f(p.origin.y) + "," + f(p.origin.z) +
           "/" + std::to_string(d.state.frags[k]) + d.state.names[k] + ";";
  }
  for (size_t e = 0; e < d.state.entities.size(); e++)
    if (d.state.entities[e].present)
      t += "e" + std::to_string(e) + ":" + f(d.state.entities[e].origin.x) + ";";
  return t;
}

bool has_text(const std::vector<uint8_t>& bytes, const std::string& text) {
  return std::search(bytes.begin(), bytes.end(), text.begin(), text.end()) != bytes.end();
}

// The node in front of a live server's stream port: the viewers join at their seconds of wall
// time, the first says hello (say_game, up to the players) once all are in, and each one's frames
// and the names it was shown are reported.
void run_live(gl::Lab& lab, const std::string& upstream) {
  const double seconds = lab.arg_num("seconds");
  const double delay = lab.arg_num("delay");
  const std::vector<double> joins = numbers(lab.arg("joins"));
  gn::Address server;
  if (!gn::parse_address(upstream, server))
    return lab.fail("--upstream=" + upstream + " is not an address",
                    "next: pass the live server's stream port as <host>:<port>.");
  std::string error;
  cn::TvNode node;
  cn::TvConfig tc;
  tc.delay_s = delay;
  tc.hostname = "ge node";
  tc.server_password = lab.arg("upstream-password");
  if (!cn::tv_open(node, gn::loopback_v4(0), server, tc, error))
    return lab.fail("the node did not open: " + error,
                    "next: check that loopback TCP works here (tests/net_tcp_socket_test.cpp).");
  std::vector<Viewer> viewers(joins.size());
  for (size_t i = 0; i < joins.size(); i++) viewers[i].join_s = joins[i];
  // A player on the live server (`--player`), so the chat is seen both ways: the player's say in
  // the viewers' streams, the first viewer's say_game in the player's prints.
  gn::Transport udp;
  cn::ClassicClientEnd player;
  const std::string player_at = lab.arg("player");
  const bool playing = !player_at.empty();
  bool player_said = false;
  if (playing) {
    gn::Address to;
    if (!gn::parse_address(player_at, to) ||
        !gn::open_udp_transport(udp, gn::ipv4(0, 0, 0, 0, 0), &error))
      return lab.fail("the player could not set out for " + player_at + ": " + error,
                      "next: pass the live server's game port as <host>:<port>.");
    cn::ClientConfig cc;
    cc.name = "pat";
    const std::string basedir = lab.arg("basedir");
    // The map's checksums the server asks for, from the game's own files.
    auto files = std::make_shared<cl::SearchPath>();
    auto mounted = std::make_shared<std::string>();
    if (!basedir.empty())
      // NOLINTNEXTLINE(bugprone-exception-escape)
      cc.read_file = [basedir, files, mounted](const std::string& gamedir,
                                               const std::string& path) {
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
    cn::client_start(player, udp, to, cc, 0);
  }
  const double say_at = joins.empty() ? 0 : *std::max_element(joins.begin(), joins.end()) + 1;
  bool said = false;
  const auto start = std::chrono::steady_clock::now();
  for (;;) {
    const double at =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (at > seconds + delay + 1) break;
    cn::tv_update(node, at);
    if (playing) {
      cn::client_tick(player, static_cast<int64_t>(at * 1e6));
      if (!player_said && player.stage == cn::ClientStage::active && at >= say_at - 0.5) {
        player_said = true;
        cn::client_command(player, "say hello from a ge player");
      }
    }
    for (Viewer& v : viewers) {
      if (!v.dialed && at >= v.join_s) {
        cn::QtvRequest q;
        q.version = 1;
        q.receive = true;
        q.userinfo = "\\name\\eve";
        const std::string text = cn::qtv_request_text(q);
        v.dialed = gn::tcp_connect(v.tcp, node.listener.local, &error);
        for (int i = 0; i < 100000 && v.tcp.connecting; i++) gn::tcp_pump(v.tcp);
        gn::tcp_send_bytes(v.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
      }
      if (v.dialed) take(v);
    }
    if (!said && at >= say_at && !viewers.empty() && viewers[0].dialed) {
      said = true;
      std::vector<uint8_t> up;
      cn::qtv_put_up(up, "say_game \"hello from a ge viewer\"");
      gn::tcp_send_bytes(viewers[0].tcp, up.data(), up.size());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  cn::tv_close(node);
  for (Viewer& v : viewers)
    for (int i = 0; i < 2000 && v.dialed && !v.tcp.closed; i++) take(v);
  gl::LabTable table{"each viewer through the node, from " + upstream,
                     {"viewer", "joins at s", "KB", "frames read", "node's name shown", "end"},
                     {}};
  bool all = !viewers.empty();
  for (size_t i = 0; i < viewers.size(); i++) {
    int frames = 0;
    bool say = false, game = false;
    const std::string end = end_of(viewers[i], frames, say, game);
    const std::string text(viewers[i].got.begin(), viewers[i].got.end());
    const size_t at = text.find("ge node (live: ");
    const std::string shown =
        at == std::string::npos ? "no" : text.substr(at, text.find('\\', at) - at);
    all = all && frames > 0 && at != std::string::npos;
    table.rows.push_back({std::to_string(i + 1), fmt(viewers[i].join_s, 0),
                          fmt(static_cast<double>(viewers[i].got.size()) / 1024.0, 1),
                          std::to_string(frames), shown, end.substr(0, 60)});
  }
  lab.table(table);
  if (playing) {
    bool down = !viewers.empty(), up = false;
    for (const Viewer& v : viewers) down = down && has_text(v.got, "hello from a ge player");
    for (const std::string& line : player.prints)
      up = up || line.find("hello from a ge viewer") != std::string::npos;
    lab.table({"the chat both ways",
               {"what", "seen"},
               {{"the player's say in every viewer's stream", down ? "yes" : "no"},
                {"the first viewer's say_game in the player's prints", up ? "yes" : "no"},
                {"the player's state", cn::client_stage_name(player.stage)},
                {"refused or dropped", player.refused.empty() ? "-" : player.refused},
                {"read error", player.error.empty() ? "-" : player.error}}});
    for (const std::string& line : player.prints)
      if (!line.empty()) lab.note("player saw: " + line);
    if (!down || !up)
      lab.fail("the chat did not cross the node both ways",
               "next: read viewer_command and the up messages in "
               "games/shooters_edge/net/classic_tv.cpp; skill: net.");
  }
  lab.note("the node: " + std::to_string(node.counts.released) + " records released; " +
           (node.last_error.empty() ? "no error" : "last error: " + node.last_error));
  for (const std::string& line : node.log) lab.note("node: " + line);
  if (!all)
    lab.fail("a viewer of the live server through our node read no frames or no node name",
             "next: read the node's log above and tv_update in "
             "games/shooters_edge/net/classic_tv.cpp; skill: net.");
}

void run(gl::Lab& lab) {
  if (!lab.arg("upstream").empty()) return run_live(lab, lab.arg("upstream"));
  const double seconds = lab.arg_num("seconds");
  const double delay = lab.arg_num("delay");
  const std::vector<double> joins = numbers(lab.arg("joins"));
  auto owned = std::make_unique<Match>();
  Match& m = *owned;
  std::string error;
  if (!sv::build_test_mod(m.progs, error))
    return lab.fail("the test mod did not build: " + error,
                    "next: run tests/shooters_edge_classic_tv_test.cpp; skill: progs.");
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  m.qc = sv::make_qc_server(m.progs, so, error);
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!m.qc || !sv::load_map(*m.qc, map, error))
    return lab.fail("the arena did not load: " + error, "next: run the se-mod lab; skill: progs.");
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  m.net = gn::make_sim_network(1, lan);
  gn::open_sim_transport(m.st, m.net, gn::sim_address(0, cn::kPortServer));
  m.rules = sv::qc_runner_over(*m.qc);
  if (!cn::classic_start(m.server, m.rules, m.st, cn::ServerConfig{}, error))
    return lab.fail("the server did not start: " + error,
                    "next: run the se-dedicated lab; skill: net.");
  for (int i = 0; i < kClients; i++) {
    gn::open_sim_transport(m.ct[i], m.net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = "p" + std::to_string(i);
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    cn::client_start(m.c[i], m.ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  if (!cn::qtv_open(m.port, gn::loopback_v4(0), cn::QtvConfig{}, error))
    return lab.fail("the stream port did not open: " + error,
                    "next: check that loopback TCP works here (tests/net_tcp_socket_test.cpp).");
  cn::TvConfig tc;
  tc.delay_s = delay;
  if (!cn::tv_open(m.node, gn::loopback_v4(0), m.port.listener.local, tc, error))
    return lab.fail("the node did not open: " + error,
                    "next: check that loopback TCP works here (tests/net_tcp_socket_test.cpp).");

  std::vector<Viewer> viewers(joins.size());
  for (size_t i = 0; i < joins.size(); i++) {
    viewers[i].join_s = joins[i];
    viewers[i].line.label =
        "viewer " + std::to_string(i + 1) + " (joins at " + fmt(joins[i], 0) + " s)";
  }
  const double say_at = joins.empty() ? 0 : *std::max_element(joins.begin(), joins.end()) + 1;
  bool said = false;
  int64_t now = 0;
  const int ticks = static_cast<int>(seconds * 1e6 / kTickUs);
  const auto step = [&](bool playing) {
    now += kTickUs;
    const double at = static_cast<double>(now) / 1e6;
    if (playing) {
      gn::sim_advance(m.net, now);
      cn::classic_frame(m.server, now);
      for (cn::ClassicClientEnd& k : m.c) cn::client_tick(k, now);
      settle(m, viewers);
      cn::qtv_update(m.port, m.server);
    }
    settle(m, viewers);
    const bool dialling = m.node.state == cn::TvUpState::idle;
    cn::tv_update(m.node, at);
    // Loopback: the node's connection up and its request with the server before its next look.
    if (dialling) {
      for (int i = 0; i < 100000 && m.node.up.connecting; i++) gn::tcp_pump(m.node.up);
      until_waiting(m.port.listener, m.node.up);
    }
    gn::tcp_pump(m.node.up);
    settle(m, viewers);
    for (Viewer& v : viewers) {
      if (!v.dialed && at >= v.join_s && playing) {
        cn::QtvRequest q;
        q.version = 1;
        q.receive = true;
        q.userinfo = "\\name\\eve";
        const std::string text = cn::qtv_request_text(q);
        v.dialed = gn::tcp_connect(v.tcp, m.node.listener.local, &error);
        for (int i = 0; i < 100000 && v.tcp.connecting; i++) gn::tcp_pump(v.tcp);
        until_waiting(m.node.listener, v.tcp);
        gn::tcp_send_bytes(v.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
      }
      if (!v.dialed) continue;
      take(v);
      if (playing && static_cast<int>(now / kTickUs) % 8 == 0) {
        v.line.x.push_back(at);
        v.line.y.push_back(static_cast<double>(v.got.size()) / 1024.0);
      }
    }
    if (!said && playing && at >= say_at && !viewers.empty() && viewers[0].dialed) {
      said = true;
      std::vector<uint8_t> up;
      cn::qtv_put_up(up, "say \"hello\"");
      cn::qtv_put_up(up, "say_game \"good game\"");
      gn::tcp_send_bytes(viewers[0].tcp, up.data(), up.size());
    }
  };
  for (int t = 0; t < ticks; t++) step(true);
  // The match ends: its last records wait out the delay, then go to every viewer.
  settle(m, viewers);
  cn::qtv_close(m.port, m.server);
  // The server's end is gone: the node holds all it sent, and the close, before its next look.
  for (int i = 0; i < 1000000 && m.node.up.handle != -1 && !m.node.up.closed; i++)
    gn::tcp_pump(m.node.up);
  const int tail = static_cast<int>((delay + 1) * 1e6 / kTickUs);
  for (int t = 0; t < tail; t++) step(false);
  const std::vector<int64_t> catch_up = [&] {
    std::vector<int64_t> c;
    for (size_t i = 0; i < viewers.size(); i++) {
      int64_t b = -1;
      for (const auto& v : m.node.viewers)
        if (v->id == static_cast<int>(i) + 1) b = v->catch_up;
      c.push_back(b);
    }
    return c;
  }();
  cn::tv_close(m.node);
  for (Viewer& v : viewers)
    for (int i = 0; i < 2000 && v.dialed && !v.tcp.closed; i++) take(v);

  gl::LabTable table{"each viewer through the node",
                     {"viewer", "joins at s", "catch-up KB", "frames read", "same end as viewer 1",
                      "say seen", "say_game seen"},
                     {}};
  std::vector<gl::Trace> lines;
  std::string first;
  bool same = true, heard = true;
  for (size_t i = 0; i < viewers.size(); i++) {
    int frames = 0;
    bool say = false, game = false;
    const std::string end = end_of(viewers[i], frames, say, game);
    if (i == 0) first = end;
    const bool alike = end == first && !end.empty() && end.find(':') != std::string::npos;
    same = same && alike;
    heard = heard && say && game;
    const double kb = static_cast<double>(catch_up[i]) / 1024.0;
    table.rows.push_back({std::to_string(i + 1), fmt(viewers[i].join_s, 0), fmt(kb, 1),
                          std::to_string(frames), alike ? "yes" : "no", say ? "yes" : "no",
                          game ? "yes" : "no"});
    const std::string key = "viewer" + std::to_string(i + 1);
    lab.metric(key + ".catchup-kb", kb, "KB");
    lab.metric(key + ".frames", frames, "frames");
    lines.push_back(viewers[i].line);
  }
  lab.metric("records", static_cast<double>(m.node.counts.released), "records");
  lab.table(table);
  lab.note("the node held the match " + fmt(delay, 1) + " s and released " +
           std::to_string(m.node.counts.released) + " records to " +
           std::to_string(viewers.size()) +
           " viewers; each late one was caught up from the match " + "as the node kept it");
  const gl::ChartFrame frame{"THE MATCH THROUGH OUR TV NODE",
                             std::to_string(kClients) + " players on the test arena; " +
                                 lab.arg("seconds") + " s; delay " + lab.arg("delay") +
                                 " s; viewers join at " + lab.arg("joins") + " s",
                             "match time (s)", "bytes the viewer has (KB)"};
  lab.shot({"bytes",
            frame.title,
            "The bytes each viewer had from the node: nothing until the delay has passed, a step "
            "for a late viewer's catch-up, then the match's steady rate.",
            "the first line rising after the delay; each later one a step at its join, then "
            "parallel",
            {{"joins", lab.arg("joins")}, {"delay", lab.arg("delay")}}},
           gl::to_picture(gl::line_chart({frame, lines, {}, {}, {}, false, false, false}).plot));
  if (!same)
    lab.fail("a late viewer's stream did not play to the first viewer's end",
             "next: run tests/shooters_edge_classic_tv_test.cpp; read mv_catch_up in "
             "games/shooters_edge/net/classic_mv_state.cpp; skill: net.");
  if (!heard)
    lab.fail("a viewer's say or say_game did not reach every viewer",
             "next: read viewer_command in games/shooters_edge/net/classic_tv.cpp; skill: net.");
}

const gl::LabSpec kSpec{
    "se-tv-node",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_tv: our TV node pulling a match from a stream port, delaying "
    "it and serving it to viewers who join late",
    "net",
    "Relayed by our TV node to viewers who join at different times, what does each late viewer's "
    "catch-up cost, and does every stream play to the same end with the chat?",
    {
        {"seconds", "12", "the match's length"},
        {"delay", "2", "the node's delay, s"},
        {"joins", "0,4,8", "the seconds the viewers connect at, comma-separated"},
        {"upstream", "", "a live server's stream port, <host>:<port> (empty: our own match)"},
        {"upstream-password", "", "the live server's stream password (empty: none)"},
        {"player", "", "with --upstream: a player joins this game port to chat (empty: none)"},
        {"basedir", "", "the live server's game files (its id1 and mod folders): the player's map"},
    },
    "--seconds=6 --joins=0,3",
    run,
};
GE_LAB(kSpec);

}  // namespace
