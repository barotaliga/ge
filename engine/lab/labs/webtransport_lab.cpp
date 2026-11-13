// LAB `webtransport`: A BROWSER'S SESSION TO THE FORWARDER (engine/webtransport). For each network
// preset a client opens a WebTransport session to the server over the simulated network (QUIC,
// TLS 1.3, the HTTP/3 CONNECT a browser sends) and then sends a game's datagrams, one every 20 ms,
// the server echoing each; the same datagrams also go over plain UDP on the same path. It answers
// how long the session takes to open, and what the session costs a game's round trip and its
// losses against plain UDP: nothing a player would feel is the goal. It also checks the
// certificate a browser trusts by its hash (its life fits the two weeks a browser allows) and that
// a client given another hash refuses the server.
//
// QUIC's connection ids, keys and timers are its own random draws, so every time and loss here is
// `measured`; the digest holds only what must always hold (each session opened, the refusal). The
// network drops packets by its own stream of draws, and QUIC's acknowledgements are packets too,
// so the two carriers meet different draws: a few percent of difference in loss is the draw, not
// the session (both run near the preset's own loss over a long run).
//
// ge:summary Lab: a WebTransport session's setup, round trip and loss against plain UDP, per preset
// ge:keywords lab webtransport quic session datagram round-trip loss handshake browser forward
// ge:keywords certificate-hash
// ge:skill net labs
// ge:test tests/webtransport_endpoint_test.cpp
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "webtransport/endpoint.hpp"

namespace {

namespace gl = ge::lab;
namespace net = ge::net;
namespace wt = ge::webtransport;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

struct Run {
  int reached = 0;  // datagrams the server got
  bool opened = false;
  double open_ms = 0;
  std::vector<double> rtt_ms;
  int sent = 0;
  std::string why;
};

constexpr int64_t kStepUs = 1000;
constexpr int64_t kEveryUs = 20000;
constexpr int64_t kStart = 1000000;  // the clock starts past zero, as a real one would

// A session over `c` for `seconds` (or plain UDP when `plain`), the server echoing.
Run run_path(const net::LinkConditions& c, bool plain, int seconds, uint32_t seed,
             const wt::Certificate& cert, const std::array<uint8_t, 32>& hash) {
  net::SimNetwork sim = net::make_sim_network(seed, c);
  const net::Address server_at = net::sim_address(1, 4433), client_at = net::sim_address(0, 5000);
  net::Transport st, ct;
  net::open_sim_transport(st, sim, server_at);
  net::open_sim_transport(ct, sim, client_at);
  int64_t now = kStart;
  wt::Server server;
  wt::Client client;
  Run run;
  if (!plain) {
    server.set_local(server_at);
    server.start(
        cert, {},
        [&](const net::Address& to, const uint8_t* d, size_t n) {
          net::transport_send(st, to, d, n, now);
        },
        nullptr);
    client.connect(
        client_at, server_at, "forward", "/play", hash,
        [&](const net::Address& to, const uint8_t* d, size_t n) {
          net::transport_send(ct, to, d, n, now);
        },
        static_cast<uint64_t>(now) * 1000, nullptr);
  }
  std::vector<int64_t> sent_at;
  std::vector<bool> back;  // each datagram counted once (the network may double one)
  int64_t open_at = plain ? now : -1, end = plain ? now + int64_t{seconds} * 1000000 : -1;
  int64_t next_send = now;
  uint32_t session = 0;
  uint8_t buf[net::kMaxDatagram];
  const int64_t give_up = now + 15000000;
  while ((end < 0 && now < give_up) || (end >= 0 && now < end + 2000000)) {
    const auto ns = static_cast<uint64_t>(now) * 1000;
    net::Address from;
    int n = 0;
    while ((n = net::transport_receive(st, buf, sizeof buf, from, now)) >= 0) {
      if (plain) {
        if (n == 16) ++run.reached;
        net::transport_send(st, from, buf, static_cast<size_t>(n), now);
      } else {
        server.receive(from, buf, static_cast<size_t>(n), ns);
      }
    }
    std::vector<std::vector<uint8_t>> echoed;
    if (!plain) {
      server.update(ns);
      for (wt::Event& e : server.take_events()) {
        if (e.kind == wt::Event::Kind::opened) session = e.session;
        if (e.kind == wt::Event::Kind::datagram) ++run.reached;
        if (e.kind == wt::Event::Kind::datagram)
          server.send(e.session, e.data.data(), e.data.size());
      }
      server.update(ns);
    }
    while ((n = net::transport_receive(ct, buf, sizeof buf, from, now)) >= 0) {
      if (plain) {
        echoed.emplace_back(buf, buf + n);
      } else {
        client.receive(from, buf, static_cast<size_t>(n), ns);
      }
    }
    if (!plain) {
      client.update(ns);
      echoed = client.take_datagrams();
      if (open_at < 0 && client.state() == wt::Client::State::open && session != 0) {
        open_at = now;
        run.opened = true;
        run.open_ms = static_cast<double>(now - kStart) / 1000.0;
        end = now + int64_t{seconds} * 1000000;
        next_send = now;
      }
      if (client.state() == wt::Client::State::closed) {
        run.why = client.why();
        break;
      }
    }
    for (const auto& e : echoed) {
      if (e.size() != 16) continue;
      uint32_t seq = 0;
      for (int b = 0; b < 4; b++) seq |= static_cast<uint32_t>(e[b]) << (8 * b);
      if (seq < sent_at.size() && !back[seq]) {
        back[seq] = true;
        run.rtt_ms.push_back(static_cast<double>(now - sent_at[seq]) / 1000.0);
      }
    }
    if (open_at >= 0 && now >= next_send && now < end) {
      uint8_t m[16] = {};
      const auto seq = static_cast<uint32_t>(sent_at.size());
      for (int b = 0; b < 4; b++) m[b] = static_cast<uint8_t>(seq >> (8 * b));
      sent_at.push_back(now);
      back.push_back(false);
      if (plain) {
        net::transport_send(ct, server_at, m, sizeof m, now);
      } else {
        client.send(m, sizeof m);
        client.update(ns);
      }
      next_send += kEveryUs;
    }
    now += kStepUs;
    net::sim_advance(sim, now);
  }
  if (plain) run.opened = true;
  if (!run.opened && run.why.empty()) run.why = "the session did not open in 15 s";
  run.sent = static_cast<int>(sent_at.size());
  net::close_transport(st);
  net::close_transport(ct);
  return run;
}

// A client given another certificate's hash: it must refuse the server.
bool refuses_another_hash(const wt::Certificate& cert, uint32_t seed, std::string& why) {
  std::array<uint8_t, 32> wrong = cert.hash;
  wrong[31] ^= 0x80;
  net::LinkConditions lan;
  net::conditions_preset("lan", lan);
  const Run r = run_path(lan, false, 1, seed, cert, wrong);
  why = r.why;
  return !r.opened && r.why.find("hash") != std::string::npos;
}

void run(gl::Lab& lab) {
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  wt::Certificate cert;
  std::string error;
  if (!wt::make_certificate(cert, 1800000000, "forward", &error)) {
    lab.fail("the certificate could not be made: " + error,
             "next: run tests/webtransport_certificate_test.cpp; skill: net.");
    return;
  }
  const double days = static_cast<double>(cert.not_after - cert.not_before) / 86400.0;
  lab.metric("certificate.days", days, "days");
  lab.note("the certificate: " + std::to_string(cert.der.size()) + " bytes, valid " + fmt(days, 0) +
           " days (a browser allows 14); its hash " + wt::hash_hex(cert));

  gl::LabTable table{"a session over each preset against plain UDP on the same path",
                     {"preset", "carried by", "opens in ms", "rtt p50 ms", "rtt p95 ms",
                      "lost out %", "lost both ways %"},
                     {}};
  std::vector<gl::Sample> samples;
  int opened = 0, presets = 0;
  for (const std::string& name : split(lab.arg("presets"))) {
    net::LinkConditions c;
    if (!net::conditions_preset(name, c)) {
      lab.fail("unknown preset '" + name + "'",
               "next: pick from lan, broadband, wifi, mobile, bad, awful "
               "(engine/net/conditions.cpp); skill: net.");
      return;
    }
    ++presets;
    for (int plain = 1; plain >= 0; --plain) {
      const Run r = run_path(c, plain != 0, seconds, seed, cert, cert.hash);
      if (!plain) {
        opened += r.opened;
        if (!r.opened)
          lab.fail("the session over '" + name + "' did not open: " + r.why,
                   "next: run tests/webtransport_endpoint_test.cpp, then this lab with "
                   "--presets=" +
                       name + "; skill: net.");
      }
      const gl::Summary s = gl::summarize(r.rtt_ms);
      const double lost =
          r.sent > 0 ? 100.0 * (1.0 - static_cast<double>(r.rtt_ms.size()) / r.sent) : 100.0;
      const char* by = plain ? "plain UDP" : "WebTransport";
      const double lost_out =
          r.sent > 0 ? 100.0 * (1.0 - static_cast<double>(r.reached) / r.sent) : 100.0;
      table.rows.push_back({name, by, plain ? "-" : fmt(r.open_ms, 0), fmt(s.p50, 0), fmt(s.p95, 0),
                            fmt(lost_out, 1), fmt(lost, 1)});
      const std::string key = name + (plain ? ".udp" : ".webtransport");
      if (!plain) lab.metric(key + ".open-ms", r.open_ms, "ms", gl::Det::measured);
      lab.metric(key + ".rtt-p50-ms", s.p50, "ms", gl::Det::measured);
      lab.metric(key + ".lost-pct", lost, "%", gl::Det::measured);
      samples.push_back({name + (plain ? " UDP" : " WT"), r.rtt_ms});
    }
  }
  lab.table(table, gl::Det::measured);
  lab.metric("sessions-opened", opened, "count");
  lab.metric("presets", presets, "count");

  std::string why;
  const bool refused = refuses_another_hash(cert, seed, why);
  lab.metric("refuses-another-hash", refused ? 1 : 0, "count");
  lab.note("a client given another certificate's hash: " + why);
  if (!refused)
    lab.fail("a client given another certificate's hash did not refuse the server",
             "next: read verify_certificate in engine/webtransport/endpoint.cpp; skill: net.");

  const gl::ChartFrame frame{"ROUND TRIP: PLAIN UDP AGAINST A WEBTRANSPORT SESSION",
                             "a 16-byte datagram every 20 ms for " + lab.arg("seconds") +
                                 " s, echoed; seed " + lab.arg("seed"),
                             "", "round trip (ms)"};
  lab.shot({"rtt",
            frame.title,
            "Each preset's path carries the same game datagrams twice: as plain UDP, and inside a "
            "WebTransport session (QUIC datagrams) as a browser page sends them.",
            "each WebTransport box level with its UDP box: the session adds no round trip",
            {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(gl::box_chart(frame, samples).plot), gl::Det::measured);
}

const gl::LabSpec kSpec{
    "webtransport",
    gl::LabKind::sim,
    "engine",
    "engine/webtransport (the session a browser opens, its datagrams)",
    "net",
    "How long does a browser's WebTransport session take to open under each preset, and what does "
    "it cost a game's round trip and losses against plain UDP on the same path?",
    {
        {"presets", "lan,broadband,mobile,bad", "the paths' presets, comma-separated"},
        {"seconds", "30", "how long each path is measured"},
        {"seed", "7", "the network's stream"},
    },
    "--presets=broadband --seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
