// THE HOPS, HEADLESS: `shooters_edge_hops <kind> ...` runs one of our hops between a player's
// client and a classic server as a process of its own, so any classic client (ours or an existing
// one) goes through it: a forward (engine/forward with the classic face, classic_forward.hpp: the
// route in the connect's `prx`), the near or far proxy (engine/proxy) or both with a lossy link
// between them, or a TV node (classic_tv.hpp) relaying a server's stream port to classic viewers.
// It runs on the wall clock, a frame every millisecond, until stopped or `--seconds`, and writes a
// status line every `--status` seconds.
//
//   shooters_edge_hops forward --port=30000 --allow=<host[:port],...> | --open
//   shooters_edge_hops forward --port=<p> --server=<host:port>   (a fixed route: fixed.hpp)
//   shooters_edge_hops proxy --port=<near> --server=<host:port> [--link-loss=0.1] [--copies=3]
//   shooters_edge_hops proxy-near --port=<p> --far=<host:port> --server=<host:port> [--copies=3]
//   shooters_edge_hops proxy-far --port=<p> --allow=<host[:port],...> | --open [--copies=3]
//   shooters_edge_hops tv --port=27900 --server=<host:stream port> [--server-password=<pw>]
//                         [--password=<pw>] [--delay=7] [--name=tv]
//   shooters_edge_hops sequel-tv --port=27911 --server=<sequel host:port> [--server-password=<pw>]
//                         [--password=<pw>] [--delay=7] [--name=tv]   (sequel_tv_relay.hpp)
//
// voice: agent (a command-line tool's main: its errors are for whoever runs it;
// docs/architecture.md § Player text).
//
// ge:summary The hops headless: our forward, proxies or TV node as a process for any classic client
// ge:keywords shooters_edge_hops hops-cli forward proxy tv-node relay headless live-server
// ge:keywords prx link-loss stream-port
// ge:skill net
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "forward/fixed.hpp"
#include "forward/forward.hpp"
#include "net/address.hpp"
#include "net/conditions.hpp"
#include "net/transport.hpp"
#include "proxy/proxy.hpp"
#include "shooters_edge/net/classic_forward.hpp"
#include "shooters_edge/net/classic_tv.hpp"
#include "shooters_edge/net/sequel_tv_relay.hpp"

namespace sn = shooters_edge::net;
namespace gn = ge::net;
namespace fw = ge::forward;
namespace px = ge::proxy;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

bool flag(const std::string& a, const char* name, std::string& out) {
  const std::string p = std::string("--") + name + "=";
  if (a.rfind(p, 0) != 0) return false;
  out = a.substr(p.size());
  return true;
}

void usage() {
  std::printf(
      "shooters_edge_hops <kind> [flags]: one of our hops for any classic client\n"
      "  forward     --port=30000 --allow=<host[:port],...> | --open   (the route in prx)\n"
      "  forward     --port=<p> --server=<host:port>: every client to that server, for a\n"
      "              client that cannot name a route (the sequel's, any game's)\n"
      "  proxy       --port=<near> --server=<host:port> [--link-loss=0..1]: near on --port,\n"
      "              far on the next, a link between them losing --link-loss each way\n"
      "  proxy-near  --port=<p> --far=<host:port> --server=<host:port>\n"
      "  proxy-far   --port=<p> --allow=<host[:port],...> | --open\n"
      "  tv          --port=27900 --server=<host:stream port> [--server-password=<pw>]\n"
      "              [--password=<pw>] [--delay=7] [--name=tv]\n"
      "  sequel-tv   --port=27911 --server=<host:port> [--server-password=<pw>]\n"
      "              [--password=<pw>] [--delay=7] [--name=tv]: a watcher on a server of the\n"
      "              sequel, its match served to the sequel's own clients\n"
      "Every kind: [--bind=<ip>] (default every interface; the proxy's pair: 127.0.0.1)\n"
      "  [--copies=3] [--name=<name>] [--seconds=<n>, 0 until stopped] [--status=10]\n");
}

bool addresses(const std::string& list, std::vector<gn::Address>& out) {
  size_t at = 0;
  while (at <= list.size()) {
    const size_t end = std::min(list.find(',', at), list.size());
    if (end > at) {
      gn::Address a;
      if (!sn::classic_destination(list.substr(at, end - at), a)) return false;
      out.push_back(a);
    }
    at = end + 1;
  }
  return !out.empty();
}

bool open_upstream(gn::Transport& out) {
  return gn::open_udp_transport(out, gn::ipv4(0, 0, 0, 0, 0), nullptr);
}

struct Options {
  std::string kind, name, bind, server_password, password;
  int port = 0, copies = 3;
  double link_loss = 0, delay = 7, seconds = 0, status = 10;
  bool open = false;
  std::vector<gn::Address> allow;
  gn::Address server{}, far{};
  bool has_server = false, has_far = false;
};

int fail(const std::string& what, const std::string& next) {
  std::fprintf(stderr, "shooters_edge_hops: %s; next: %s\n", what.c_str(), next.c_str());
  return 2;
}

gn::Address at(const std::string& bind, int port) {
  gn::Address a = gn::ipv4(0, 0, 0, 0, static_cast<uint16_t>(port));
  if (!bind.empty()) gn::parse_address(bind + ":" + std::to_string(port), a);
  return a;
}

// The hops, all of one process's kinds (only one is opened).
// The server's packets as one end of the proxies passes them on, counted by the classic channel's
// sequence (its low 31 bits; out-of-band packets are not counted): gaps at the far end were lost
// before our link, gaps at the near end were lost before or on it.
struct SeqCount {
  uint32_t last = 0;
  uint64_t seen = 0, gaps = 0;
  void take(const px::Passed& p) {
    if (p.to_server || p.datagram.size() < 4) return;
    const uint32_t seq =
        (static_cast<uint32_t>(p.datagram[0]) | static_cast<uint32_t>(p.datagram[1]) << 8 |
         static_cast<uint32_t>(p.datagram[2]) << 16 | static_cast<uint32_t>(p.datagram[3]) << 24) &
        0x7fffffffu;
    if (seq == 0x7fffffffu) return;
    seen++;
    if (last != 0 && seq > last + 1) gaps += seq - last - 1;
    if (seq > last) last = seq;
  }
};

struct Hops {
  SeqCount near_seq, far_seq;
  gn::Transport fwd_port, near_client, near_link, far_link;
  gn::FakeLink near_fake, far_fake;
  fw::Forward fwd;
  sn::ClassicFace face;
  fw::FixedFace fixed;
  bool fixed_route = false;
  px::ProxyNear near;
  px::ProxyFar far;
  sn::TvNode tv;
  gn::Transport sq_watcher, sq_face;
  sn::SequelTvRelay sq;
  bool has_sq = false;
  bool forward = false, has_near = false, has_far = false, has_tv = false;
};

std::string status_line(const Hops& h) {
  std::string s;
  if (h.forward && h.fixed_route)
    s += "forward: clients " + std::to_string(h.fwd.clients.size()) + ", taken " +
         std::to_string(h.fixed.clients) + ", proven " + std::to_string(h.fixed.proven) +
         ", answers held back " + std::to_string(h.fixed.held_back) + ". ";
  else if (h.forward)
    s += "forward: clients " + std::to_string(h.fwd.clients.size()) + ", accepted " +
         std::to_string(h.face.counts.accepted) + ", refused " +
         std::to_string(h.face.counts.refused) + ". ";
  if (h.has_near) {
    uint64_t up = 0, game = 0;
    for (const auto& n : h.near.clients) {
      up += n->link.stats.bytes_out;
      game += n->link.stats.payload_out;
    }
    s += "near: clients " + std::to_string(h.near.clients.size()) + ", link bytes up " +
         std::to_string(up) + " for the game's " + std::to_string(game) + ". ";
  }
  if (h.has_far) {
    uint64_t down = 0, game = 0;
    for (const auto& f : h.far.sessions) {
      down += f->link.stats.bytes_out;
      game += f->link.stats.payload_out;
    }
    s += "far: sessions " + std::to_string(h.far.sessions.size()) + ", link bytes down " +
         std::to_string(down) + " for the game's " + std::to_string(game) + ". ";
  }
  if (h.has_near && h.has_far && h.near_seq.seen > 0) {
    const uint64_t lost = h.near_seq.gaps > h.far_seq.gaps ? h.near_seq.gaps - h.far_seq.gaps : 0;
    s += "server packets lost on the link " + std::to_string(lost) + " of " +
         std::to_string(h.near_seq.seen + h.near_seq.gaps) + " (" +
         ge::core::num::to_fixed(100.0 * double(lost) / double(h.near_seq.seen + h.near_seq.gaps),
                                 1) +
         "%). ";
  }
  if (h.near_fake.lost + h.far_fake.lost > 0)
    s += "link frames lost " + std::to_string(h.near_fake.lost) + " up, " +
         std::to_string(h.far_fake.lost) + " down. ";
  if (h.has_tv)
    s += "tv: viewers " + std::to_string(sn::tv_watching(h.tv)) + ", records " +
         std::to_string(h.tv.counts.records) + ", released " +
         std::to_string(h.tv.counts.released) + ". ";
  if (h.has_sq)
    s += "sequel tv: watcher " + std::string(sn::client_stage_name(h.sq.watcher.stage)) +
         ", frames " + std::to_string(h.sq.watcher.frames) + ", served blocks " +
         std::to_string(h.sq.face.blocks) + ", whole frames " +
         std::to_string(h.sq.face.whole_frames) + ", viewers " +
         std::to_string(sn::sequel_tv_viewers(h.sq.face)) + ". ";
  return s;
}

int run(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    usage();
    return argc < 2 ? 2 : 0;
  }
  Options o;
  o.kind = argv[1];
  for (int i = 2; i < argc; i++) {
    const std::string a = argv[i];
    std::string v;
    if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else if (flag(a, "port", v)) {
      o.port = std::atoi(v.c_str());
    } else if (flag(a, "bind", v)) {
      o.bind = v;
    } else if (flag(a, "allow", v)) {
      if (!addresses(v, o.allow))
        return fail("--allow=" + v + " holds no address", "host:port,...");
    } else if (a == "--open") {
      o.open = true;
    } else if (flag(a, "server", v)) {
      if (!sn::classic_destination(v, o.server))
        return fail("--server=" + v + " is no address", "host:port");
      o.has_server = true;
    } else if (flag(a, "far", v)) {
      if (!sn::classic_destination(v, o.far))
        return fail("--far=" + v + " is no address", "host:port");
      o.has_far = true;
    } else if (flag(a, "copies", v)) {
      o.copies = std::atoi(v.c_str());
    } else if (flag(a, "link-loss", v)) {
      o.link_loss = std::atof(v.c_str());
    } else if (flag(a, "server-password", v)) {
      o.server_password = v;
    } else if (flag(a, "password", v)) {
      o.password = v;
    } else if (flag(a, "delay", v)) {
      o.delay = std::atof(v.c_str());
    } else if (flag(a, "name", v)) {
      o.name = v;
    } else if (flag(a, "seconds", v)) {
      o.seconds = std::atof(v.c_str());
    } else if (flag(a, "status", v)) {
      o.status = std::atof(v.c_str());
    } else {
      return fail("unknown argument '" + a + "'", "shooters_edge_hops --help");
    }
  }
  auto owned = std::make_unique<Hops>();
  Hops& h = *owned;
  std::string error;
  if (o.kind == "forward") {
    if (o.allow.empty() && !o.open && !o.has_server)
      return fail("a forward reaches only what it allows", "--allow=<host:port,...> or --open");
    const gn::Address bind = at(o.bind, o.port > 0 ? o.port : sn::kForwardPort);
    if (!gn::open_udp_transport(h.fwd_port, bind, &error))
      return fail("the forward's socket did not open on " + gn::to_string(bind) + ": " + error,
                  "pick a free --port");
    fw::ForwardConfig fc;
    fc.name = o.name.empty() ? "forward" : o.name;
    fc.allow = o.allow;
    if (o.has_server) fc.allow.push_back(o.server);
    fc.open = o.open;
    fc.seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    fw::forward_init(h.fwd, fc, h.fwd_port, open_upstream);
    h.forward = true;
    if (o.has_server) {
      h.fixed.to = o.server;
      fw::fixed_route_attach(h.fwd, h.fixed);
      h.fixed_route = true;
      std::printf("forward on %s, every client to %s\n", gn::to_string(bind).c_str(),
                  gn::to_string(o.server).c_str());
    } else {
      sn::classic_forward_attach(h.fwd, h.face);
      std::printf("forward on %s\n", gn::to_string(bind).c_str());
    }
  } else if (o.kind == "proxy" || o.kind == "proxy-near" || o.kind == "proxy-far") {
    const bool near = o.kind != "proxy-far", far = o.kind != "proxy-near";
    if (near && !o.has_server)
      return fail("the near proxy leads to one server", "--server=<host:port>");
    if (o.kind == "proxy-near" && !o.has_far)
      return fail("the near proxy needs its far one", "--far=<host:port>");
    if (o.kind == "proxy-far" && o.allow.empty() && !o.open)
      return fail("the far proxy reaches only what it allows", "--allow=<host:port,...> or --open");
    if (o.port <= 0) return fail("a proxy needs a port", "--port=<p>");
    px::ProxyConfig pc;
    pc.link.repeat = o.copies;
    pc.link.pack = true;
    pc.allow = o.kind == "proxy" ? std::vector<gn::Address>{o.server} : o.allow;
    pc.open = o.open;
    const auto seed =
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string pair_bind = o.bind.empty() && o.kind == "proxy" ? "127.0.0.1" : o.bind;
    gn::Address far_at = o.far;
    if (far) {
      far_at = at(pair_bind, o.kind == "proxy" ? o.port + 1 : o.port);
      if (!gn::open_udp_transport(h.far_link, far_at, &error))
        return fail(
            "the far proxy's socket did not open on " + gn::to_string(far_at) + ": " + error,
            "pick a free --port");
      pc.name = "far";
      pc.seed = seed + 1;
      pc.tap = [&h](const px::Passed& p) { h.far_seq.take(p); };
      px::proxy_far_init(h.far, pc, h.far_link, open_upstream);
      h.has_far = true;
      std::printf("far proxy on %s\n", gn::to_string(far_at).c_str());
    }
    if (near) {
      const gn::Address near_at = at(o.bind, o.port);
      if (!gn::open_udp_transport(h.near_client, near_at, &error) ||
          !gn::open_udp_transport(h.near_link, at(pair_bind, 0), &error))
        return fail(
            "the near proxy's sockets did not open on " + gn::to_string(near_at) + ": " + error,
            "pick a free --port");
      pc.name = "near";
      pc.seed = seed;
      pc.tap = [&h](const px::Passed& p) { h.near_seq.take(p); };
      px::proxy_near_init(h.near, pc, h.near_client, h.near_link, far_at, o.server);
      h.has_near = true;
      std::printf("near proxy on %s, to %s by way of %s\n", gn::to_string(near_at).c_str(),
                  gn::to_string(o.server).c_str(), gn::to_string(far_at).c_str());
    }
    if (o.link_loss > 0) {
      gn::LinkConditions bad;
      bad.loss = o.link_loss;
      if (h.has_near) transport_fake(h.near_link, &h.near_fake, bad, seed + 2);
      if (h.has_far) transport_fake(h.far_link, &h.far_fake, bad, seed + 3);
      std::printf("the link loses %s of its frames each way (a test)\n",
                  ge::core::num::to_fixed(o.link_loss, 2).c_str());
    }
  } else if (o.kind == "tv") {
    if (!o.has_server)
      return fail("a TV node relays one server's stream port", "--server=<host:port>");
    sn::TvConfig tc;
    tc.name = o.name.empty() ? "tv" : o.name;
    tc.server_password = o.server_password;
    tc.password = o.password;
    tc.delay_s = o.delay;
    const gn::Address bind = at(o.bind, o.port > 0 ? o.port : 27900);
    if (!sn::tv_open(h.tv, bind, o.server, tc, error))
      return fail("the TV node did not open on " + gn::to_string(bind) + ": " + error,
                  "pick a free --port");
    h.has_tv = true;
    std::printf("tv on %s, relaying %s\n", gn::to_string(h.tv.listener.local).c_str(),
                gn::to_string(o.server).c_str());
  } else if (o.kind == "sequel-tv") {
    if (!o.has_server) return fail("a sequel TV watches one server", "--server=<host:port>");
    const gn::Address bind = at(o.bind, o.port > 0 ? o.port : 27911);
    if (!gn::open_udp_transport(h.sq_face, bind, &error))
      return fail("the TV's socket did not open on " + gn::to_string(bind) + ": " + error,
                  "pick a free --port");
    if (!open_upstream(h.sq_watcher))
      return fail("the watcher's socket did not open", "check the machine's sockets");
    sn::SequelTvRelayConfig rc;
    rc.face.name = o.name.empty() ? "Shooter's Edge TV" : o.name;
    rc.face.password = o.password;
    rc.server_password = o.server_password;
    rc.delay_s = o.delay;
    sn::sequel_tv_relay_open(h.sq, h.sq_watcher, h.sq_face, o.server, rc, 0);
    h.has_sq = true;
    std::printf("sequel tv on %s, watching %s, %.0f s behind\n", gn::to_string(bind).c_str(),
                gn::to_string(o.server).c_str(), o.delay);
  } else {
    return fail("no kind '" + o.kind + "'",
                "forward, proxy, proxy-near, proxy-far, tv or sequel-tv");
  }
  std::fflush(stdout);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  const auto start = std::chrono::steady_clock::now();
  double next_status = o.status;
  size_t tv_log = 0, sq_log = 0, face_log = 0;
  while (g_stop == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    const double now = static_cast<double>(now_us) / 1e6;
    if (h.forward) fw::forward_update(h.fwd, now_us);
    if (h.has_near) px::proxy_near_update(h.near, now_us);
    if (h.has_far) px::proxy_far_update(h.far, now_us);
    if (h.has_tv) {
      sn::tv_update(h.tv, now);
      for (; tv_log < h.tv.log.size(); tv_log++) std::printf("tv: %s\n", h.tv.log[tv_log].c_str());
    }
    if (h.has_sq) {
      sn::sequel_tv_relay_update(h.sq, now_us);
      for (; sq_log < h.sq.log.size(); sq_log++) std::printf("tv: %s\n", h.sq.log[sq_log].c_str());
      for (; face_log < h.sq.face.log.size(); face_log++)
        std::printf("tv: %s\n", h.sq.face.log[face_log].c_str());
    }
    if (o.status > 0 && now >= next_status) {
      next_status += o.status;
      std::printf("%.0f s: %s\n", now, status_line(h).c_str());
      std::fflush(stdout);
    }
    if (o.seconds > 0 && now >= o.seconds) break;
  }
  std::printf("stopped: %s\n", status_line(h).c_str());
  if (h.forward) fw::forward_close(h.fwd);
  if (h.has_far) px::proxy_far_close(h.far);
  if (h.has_tv) sn::tv_close(h.tv);
  if (h.has_sq) sn::sequel_tv_relay_close(h.sq);
  return 0;
}

}  // namespace

int main(int argc, char** argv) { return run(argc, argv); }
