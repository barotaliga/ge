// LAB `se-tv`: A MATCH SERVED TO CLASSIC RELAYS (games/shooters_edge/net/classic_qtv.hpp). Two
// clients play the test mod's arena over the simulated network while relays connect to the
// server's stream port over loopback TCP at the given seconds; each is sent the whole signon when
// it is let in (its catch-up) and then the match as it is played. It answers each relay's catch-up
// and steady bytes a second, and that the demo reader plays each one's stream to its end; the
// first relay's viewer says something and joins once all are in, and both come back in every
// stream. The bytes
// sent to each relay are charted over the match.
//
// ge:summary Lab: a match served to classic relays: catch-up, bytes a second, streams read back
// ge:keywords lab qtv stream-port relay catch-up bytes-per-second say_game qul read-back
// ge:skill net shooter labs
// ge:test tests/shooters_edge_classic_qtv_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/tcp_socket.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_server.hpp"
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

struct Relay {
  double join_s = 0;
  bool dialed = false;
  gn::TcpStream tcp;
  std::vector<uint8_t> got;
  int64_t made_at_join = -1;  // the server's bytes for it after its first frame
  double first_frame_s = -1;
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
};

// Bytes the server has made for relay `id` so far: sent, waiting and not yet flushed.
int64_t made(const cn::QtvPort& p, int id) {
  for (const auto& r : p.relays)
    if (r->id == id) return r->sent + static_cast<int64_t>(r->out.size() + r->rec.bytes.size());
  return -1;
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  const std::vector<double> joins = numbers(lab.arg("joins"));
  auto owned = std::make_unique<Match>();
  Match& m = *owned;
  std::string error;
  if (!sv::build_test_mod(m.progs, error))
    return lab.fail("the test mod did not build: " + error,
                    "next: run tests/shooters_edge_classic_qtv_test.cpp; skill: progs.");
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
  cn::QtvConfig qcfg;
  qcfg.max_streams = 0;
  if (!cn::qtv_open(m.port, gn::loopback_v4(0), qcfg, error))
    return lab.fail("the stream port did not open: " + error,
                    "next: check that loopback TCP works here (tests/net_tcp_socket_test.cpp).");

  std::vector<Relay> relays(joins.size());
  for (size_t i = 0; i < joins.size(); i++) {
    relays[i].join_s = joins[i];
    relays[i].line.label =
        "relay " + std::to_string(i + 1) + " (joins at " + fmt(joins[i], 0) + " s)";
  }
  const int ticks = static_cast<int>(seconds * 1e6 / kTickUs);
  bool said = false;
  const double say_at = joins.empty() ? 0 : *std::max_element(joins.begin(), joins.end()) + 1;
  int64_t now = 0;
  for (int t = 0; t < ticks; t++) {
    now += kTickUs;
    const double at = static_cast<double>(now) / 1e6;
    gn::sim_advance(m.net, now);
    cn::classic_frame(m.server, now);
    for (cn::ClassicClientEnd& k : m.c) cn::client_tick(k, now);
    for (Relay& r : relays) {
      if (r.dialed || at < r.join_s) continue;
      r.dialed = true;
      cn::QtvRequest q;
      q.version = 1;
      q.receive = true;
      q.userinfo = "\\name\\" + r.line.label;
      const std::string text = cn::qtv_request_text(q);
      if (!gn::tcp_connect(r.tcp, m.port.listener.local, &error)) continue;
      // Loopback: the connection is up and the request with the server before its next look,
      // so a run lets every relay in on the same frame.
      for (int i = 0; i < 100000 && r.tcp.connecting; i++) gn::tcp_pump(r.tcp);
      gn::tcp_send_bytes(r.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }
    cn::qtv_update(m.port, m.server);
    for (size_t i = 0; i < relays.size(); i++) {
      Relay& r = relays[i];
      if (!r.dialed) continue;
      gn::tcp_pump(r.tcp);
      gn::tcp_take_bytes(r.tcp, r.got);
      const int64_t b = made(m.port, static_cast<int>(i) + 1);
      if (b > 0 && r.made_at_join < 0) {
        for (const auto& pr : m.port.relays)
          if (pr->id == static_cast<int>(i) + 1 && pr->rec.frames > 0) {
            r.made_at_join = b;
            r.first_frame_s = at;
          }
      }
      if (b >= 0 && t % 8 == 0) {
        r.line.x.push_back(at);
        r.line.y.push_back(static_cast<double>(b) / 1024.0);
      }
    }
    if (!said && !relays.empty() && at >= say_at && relays[0].made_at_join > 0) {
      said = true;
      std::vector<uint8_t> up;
      cn::qtv_put_up(up, "say_game \"#1:eve: good game\"");
      cn::qtv_put_up(up, "qul 1 1 \"eve\"");
      gn::tcp_send_bytes(relays[0].tcp, up.data(), up.size());
    }
  }
  std::vector<int64_t> totals;
  for (size_t i = 0; i < relays.size(); i++)
    totals.push_back(made(m.port, static_cast<int>(i) + 1));
  cn::qtv_close(m.port, m.server);
  for (Relay& r : relays)
    for (int i = 0; i < 20000 && r.dialed && !r.tcp.closed; i++) {
      gn::tcp_pump(r.tcp);
      gn::tcp_take_bytes(r.tcp, r.got);
    }

  gl::LabTable table{
      "each relay's stream: its catch-up and its steady rate",
      {"relay", "joins at s", "catch-up KB", "steady B/s", "frames read", "chat seen", "join seen"},
      {}};
  std::vector<gl::Trace> lines;
  bool all_read = true, all_heard = true;
  double catchup_sum = 0, rate_sum = 0;
  for (size_t i = 0; i < relays.size(); i++) {
    Relay& r = relays[i];
    const std::string key = "relay" + std::to_string(i + 1);
    const std::string text(r.got.begin(), r.got.end());
    cn::QtvAnswer a;
    size_t used = 0;
    int frames = 0;
    bool chat = false, join = false;
    if (cn::qtv_parse_answer(text, a, used) == cn::QtvParse::ok &&
        a.kind == cn::QtvAnswerKind::begin) {
      cl::Demo d;
      if (cl::open_demo(
              std::vector<uint8_t>(r.got.begin() + static_cast<std::ptrdiff_t>(used), r.got.end()),
              d, error)) {
        for (;;) {
          const cl::DemoRead st = cl::next_frame(d, error);
          if (st != cl::DemoRead::frame) {
            all_read = all_read && st == cl::DemoRead::end;
            break;
          }
          frames++;
          for (const cl::DemoPrint& p : d.state.prints) {
            chat = chat || p.text.find("qtv_say_game:#1:eve: good game") != std::string::npos;
            join = join || p.text.find(": eve join") != std::string::npos;
          }
        }
      } else {
        all_read = false;
      }
    } else {
      all_read = false;
    }
    all_heard = all_heard && chat && join;
    const double catchup = static_cast<double>(r.made_at_join) / 1024.0;
    const double span = seconds - r.first_frame_s;
    const double rate = span > 0 ? static_cast<double>(totals[i] - r.made_at_join) / span : 0;
    catchup_sum += catchup;
    rate_sum += rate;
    table.rows.push_back({std::to_string(i + 1), fmt(r.join_s, 0), fmt(catchup, 1), fmt(rate, 0),
                          std::to_string(frames), chat ? "yes" : "no", join ? "yes" : "no"});
    lab.metric(key + ".catchup-kb", catchup, "KB");
    lab.metric(key + ".bytes-per-s", rate, "bytes/s");
    lab.metric(key + ".frames", frames, "frames");
    lines.push_back(r.line);
  }
  lab.table(table);
  if (!relays.empty()) {
    const double n = static_cast<double>(relays.size());
    lab.note("a relay's catch-up was " + fmt(catchup_sum / n, 1) + " KB and its stream " +
             fmt(rate_sum / n, 0) + " bytes a second, for " + std::to_string(kClients) +
             " players on the arena");
  }
  const gl::ChartFrame frame{"THE MATCH SENT TO EACH RELAY",
                             std::to_string(kClients) + " players on the test arena; " +
                                 lab.arg("seconds") + " s; relays join at " + lab.arg("joins") +
                                 " s",
                             "match time (s)", "bytes made for the relay (KB)"};
  lab.shot({"bytes",
            frame.title,
            "The bytes the server made for each relay since it was let in: a step for the catch-up "
            "(the whole signon and first frame), then the match's steady rate.",
            "each line rising from its join with a step, then parallel to the others",
            {{"joins", lab.arg("joins")}, {"seconds", lab.arg("seconds")}}},
           gl::to_picture(gl::line_chart({frame, lines, {}, {}, {}, false, false, false}).plot));
  if (!all_read)
    lab.fail("a relay's stream did not play to its end",
             "next: run tests/shooters_edge_classic_qtv_test.cpp; skill: net.");
  if (!all_heard)
    lab.fail("the relay's chat or its viewer's join did not come back in every stream",
             "next: read up_command in games/shooters_edge/net/classic_qtv.cpp; skill: net.");
}

const gl::LabSpec kSpec{
    "se-tv",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_qtv: the server's stream port, relays let in, the match "
    "streamed to each, chat and viewers sent up",
    "net",
    "Served to classic relays that join at different times, what does each one's catch-up cost, "
    "how many bytes a second is the match, and does every stream play to its end with the chat?",
    {
        {"seconds", "12", "the match's length"},
        {"joins", "0,4,8", "the seconds the relays connect at, comma-separated"},
    },
    "--seconds=6 --joins=0,3",
    run,
};
GE_LAB(kSpec);

}  // namespace
