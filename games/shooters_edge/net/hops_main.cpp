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
//   shooters_edge_hops tv --port=27900 --demo=<client recording> [--password=<pw>] [--name=tv]
//                         a client recording played round and round to any classic client, the
//                         oldest too, over plain protocol 28   (classic_tv_udp.hpp,
//                         classic_tv_recording.hpp)
//   shooters_edge_hops sequel-tv --port=27911 --server=<sequel host:port> [--server-password=<pw>]
//                         [--password=<pw>] [--delay=7] [--name=tv]   (sequel_tv_relay.hpp)
//   shooters_edge_hops sequel-tv --port=27911 --demo=<file.dm2> [--password=<pw>] [--name=tv]
//                         a recording played round and round instead   (sequel_tv_demo.hpp)
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
#include <ctime>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "forward/fixed.hpp"
#include "forward/forward.hpp"
#include "net/address.hpp"
#include "net/conditions.hpp"
#include "net/master.hpp"
#include "net/packet.hpp"
#include "net/transport.hpp"
#include "proxy/proxy.hpp"
#include "shooters_edge/net/classic_forward.hpp"
#include "shooters_edge/net/classic_tv.hpp"
#include "shooters_edge/net/classic_tv_recording.hpp"
#include "shooters_edge/net/sequel_tv_demo.hpp"
#include "shooters_edge/net/sequel_tv_relay.hpp"
#include "webtransport/forward.hpp"

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
      "  forward     ... --webtransport=<port>: pages in a browser too, on that UDP port\n"
      "              (https://host:port/play?to=<server>; the certificate's hash is printed)\n"
      "  forward     ... --master=<host[:port],...>: listed there, so a hub offers its pages\n"
      "  proxy       --port=<near> --server=<host:port> [--link-loss=0..1]: near on --port,\n"
      "              far on the next, a link between them losing --link-loss each way\n"
      "  proxy-near  --port=<p> --far=<host:port> --server=<host:port>\n"
      "  proxy-far   --port=<p> --allow=<host[:port],...> | --open\n"
      "  tv          --port=27900 --server=<host:stream port> [--server-password=<pw>]\n"
      "              [--password=<pw>] [--delay=7] [--name=tv]\n"
      "  sequel-tv   --port=27911 --server=<host:port> [--server-password=<pw>]\n"
      "              [--password=<pw>] [--delay=7] [--name=tv]: a watcher on a server of the\n"
      "              sequel, its match served to the sequel's own clients\n"
      "              or --demo=<file.dm2> in place of --server: a recording, played round and\n"
      "              round at the game's pace\n"
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
  std::string kind, name, bind, server_password, password, demo;
  int port = 0, copies = 3, webtransport = 0;
  double link_loss = 0, delay = 7, seconds = 0, status = 10;
  bool open = false;
  std::vector<gn::Address> allow, masters;
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
  ge::webtransport::WebForward web;  // pages' sessions, when --webtransport
  gn::MasterBeat beat;               // the forward listed at --master's masters
  sn::ClassicFace face;
  fw::FixedFace fixed;
  px::ProxyNear near;
  px::ProxyFar far;
  sn::TvNode tv;
  gn::Transport sq_watcher, sq_face;
  sn::SequelTvRelay sq;
  sn::SequelTvFace sq_demo_face;  // --demo: the face fed from a recording, no watcher
  sn::SequelTvDemo sq_demo;
  gn::Transport qw_face;  // tv --demo: the face for classic clients fed from a recording
  sn::ClassicTvUdp cl_demo_face;
  sn::UdpTvRecording cl_demo;
  bool forward = false, has_near = false, has_far = false, has_tv = false;
  bool has_web = false, beating = false, fixed_route = false;
  bool has_sq = false, has_sq_demo = false, has_cl_demo = false;
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
  if (h.has_web) {
    uint64_t up = 0, down = 0;
    for (const auto& c : h.web.fwd.clients) {
      up += c->packets_up;
      down += c->packets_down;
    }
    s += "pages: " + std::to_string(h.web.sessions.size()) + " on, " +
         std::to_string(h.web.stats.opened) + " came, " + std::to_string(h.web.stats.refused) +
         " refused, packets " + std::to_string(up) + " up and " + std::to_string(down) + " down. ";
  }
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
  if (h.has_cl_demo)
    s += "tv: recording packets " + std::to_string(h.cl_demo.packets) + ", rounds " +
         std::to_string(h.cl_demo.rounds) + (h.cl_demo.ended ? " (ended)" : "") + ", viewers " +
         std::to_string(sn::udp_tv_viewers(h.cl_demo_face)) + ". ";
  if (h.has_sq_demo)
    s += "sequel tv: recording frames " + std::to_string(h.sq_demo.frames) + ", rounds " +
         std::to_string(h.sq_demo.rounds) + (h.sq_demo.ended ? " (ended)" : "") + ", viewers " +
         std::to_string(sn::sequel_tv_viewers(h.sq_demo_face)) + ". ";
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
    } else if (flag(a, "webtransport", v)) {
      o.webtransport = std::atoi(v.c_str());
      if (o.webtransport <= 0 || o.webtransport > 65535)
        return fail("--webtransport=" + v + " is no port", "--webtransport=<udp port>");
    } else if (flag(a, "bind", v)) {
      o.bind = v;
    } else if (flag(a, "allow", v)) {
      if (!addresses(v, o.allow))
        return fail("--allow=" + v + " holds no address", "host:port,...");
    } else if (flag(a, "master", v)) {
      if (!addresses(v, o.masters))
        return fail("--master=" + v + " holds no address", "host[:port],...");
      for (gn::Address& m : o.masters)
        if (m.port == 0) m.port = gn::kMasterPort;
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
    } else if (flag(a, "demo", v)) {
      o.demo = v;
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
    if (o.webtransport > 0) {
      ge::webtransport::Certificate cert;
      const auto unix_now = static_cast<int64_t>(std::time(nullptr));
      if (!ge::webtransport::make_certificate(cert, unix_now, fc.name, &error))
        return fail("the forward's certificate could not be made: " + error, "run it again");
      const gn::Address web_bind = at(o.bind, o.webtransport);
      if (!ge::webtransport::web_forward_start(h.web, cert, web_bind, fc, open_upstream, &error))
        return fail(
            "the WebTransport socket did not open on " + gn::to_string(web_bind) + ": " + error,
            "pick a free --webtransport port");
      h.has_web = true;
      // The forward's record says where pages connect and how they trust it (a hub reads it).
      h.fwd.config.keys.emplace_back(fw::kWebTransportKey, std::to_string(o.webtransport));
      fw::status_put_cert(h.fwd.config.keys, ge::webtransport::hash_hex(cert));
      std::printf("webtransport on %s, certificate %s\n", gn::to_string(web_bind).c_str(),
                  ge::webtransport::hash_hex(cert).c_str());
    }
    if (!o.masters.empty()) {
      gn::MasterBeatConfig bc;
      bc.masters = o.masters;
      bc.protocol = gn::protocol_id("shooters_edge", 1);  // listed among the game's nodes
      bc.kind = gn::NodeKind::forward;
      bc.version = fc.version;
      bc.seed = fc.seed;
      gn::master_beat_init(h.beat, bc);
      h.fwd_port.on_public = [](void* ctx, const uint8_t* d, size_t n, const gn::Address& from,
                                int64_t now_us) {
        return gn::master_beat_read(*static_cast<gn::MasterBeat*>(ctx), d, n, from, now_us);
      };
      h.fwd_port.public_ctx = &h.beat;
      h.beating = true;
      std::printf("listed at %zu master(s)\n", o.masters.size());
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
    pc.link.code = true;
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
  } else if (o.kind == "tv" && !o.demo.empty()) {
    std::ifstream in(o.demo, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (bytes.empty())
      return fail("the recording " + o.demo + " did not open",
                  "name a client recording that exists");
    if (!sn::udp_tv_recording_open(h.cl_demo, std::move(bytes), true, error))
      return fail(o.demo + ": " + error, "name a multiplayer client's recording");
    const gn::Address bind = at(o.bind, o.port > 0 ? o.port : 27900);
    if (!gn::open_udp_transport(h.qw_face, bind, &error))
      return fail("the TV's socket did not open on " + gn::to_string(bind) + ": " + error,
                  "pick a free --port");
    sn::UdpTvConfig fc;
    fc.name = o.name.empty() ? "Shooter's Edge TV" : o.name;
    fc.password = o.password;
    sn::udp_tv_start(h.cl_demo_face, h.qw_face, fc);
    h.has_cl_demo = true;
    std::printf("tv on %s, playing %s round and round to classic clients\n",
                gn::to_string(bind).c_str(), o.demo.c_str());
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
  } else if (o.kind == "sequel-tv" && !o.demo.empty()) {
    std::ifstream in(o.demo, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (!in.good() && bytes.empty())
      return fail("the recording " + o.demo + " did not open", "name a .dm2 file that exists");
    if (!sn::sequel_tv_demo_open(h.sq_demo, std::move(bytes), true, error))
      return fail(o.demo + ": " + error, "name a recording of the sequel (.dm2)");
    const gn::Address bind = at(o.bind, o.port > 0 ? o.port : 27911);
    if (!gn::open_udp_transport(h.sq_face, bind, &error))
      return fail("the TV's socket did not open on " + gn::to_string(bind) + ": " + error,
                  "pick a free --port");
    sn::SequelTvConfig fc;
    fc.name = o.name.empty() ? "Shooter's Edge TV" : o.name;
    fc.password = o.password;
    sn::sequel_tv_start(h.sq_demo_face, h.sq_face, fc);
    h.has_sq_demo = true;
    std::printf("sequel tv on %s, playing %s round and round\n", gn::to_string(bind).c_str(),
                o.demo.c_str());
  } else if (o.kind == "sequel-tv") {
    if (!o.has_server)
      return fail("a sequel TV watches one server or plays one recording",
                  "--server=<host:port> or --demo=<file.dm2>");
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
  size_t tv_log = 0, sq_log = 0, face_log = 0, web_log = 0;
  while (g_stop == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    const double now = static_cast<double>(now_us) / 1e6;
    if (h.beating) gn::master_beat_update(h.beat, h.fwd_port, now_us);
    if (h.forward) fw::forward_update(h.fwd, now_us);
    if (h.has_web) {
      ge::webtransport::web_forward_update(h.web, now_us);
      for (; web_log < h.web.log.size(); web_log++)
        std::printf("webtransport: %s\n", h.web.log[web_log].c_str());
    }
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
    if (h.has_cl_demo) {
      sn::udp_tv_recording_update(h.cl_demo, h.cl_demo_face, now_us);
      sn::udp_tv_frame(h.cl_demo_face, now_us);
      for (; face_log < h.cl_demo_face.log.size(); face_log++)
        std::printf("tv: %s\n", h.cl_demo_face.log[face_log].c_str());
    }
    if (h.has_sq_demo) {
      sn::sequel_tv_demo_update(h.sq_demo, h.sq_demo_face, now_us);
      sn::sequel_tv_frame(h.sq_demo_face, now_us);
      for (; face_log < h.sq_demo_face.log.size(); face_log++)
        std::printf("tv: %s\n", h.sq_demo_face.log[face_log].c_str());
    }
    if (o.status > 0 && now >= next_status) {
      next_status += o.status;
      std::printf("%.0f s: %s\n", now, status_line(h).c_str());
      std::fflush(stdout);
    }
    if (o.seconds > 0 && now >= o.seconds) break;
  }
  std::printf("stopped: %s\n", status_line(h).c_str());
  if (h.beating) {
    const int64_t end_us = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    gn::master_beat_stop(h.beat, h.fwd_port, end_us);
  }
  if (h.forward) fw::forward_close(h.fwd);
  if (h.has_web) ge::webtransport::web_forward_close(h.web);
  if (h.has_far) px::proxy_far_close(h.far);
  if (h.has_tv) sn::tv_close(h.tv);
  if (h.has_sq) sn::sequel_tv_relay_close(h.sq);
  if (h.has_sq_demo) sn::sequel_tv_stop(h.sq_demo_face);
  if (h.has_cl_demo) sn::udp_tv_stop(h.cl_demo_face);
  return 0;
}

}  // namespace

int main(int argc, char** argv) { return run(argc, argv); }
