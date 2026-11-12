// Tests: our client watching a classic broadcast (games/shooters_edge/net/classic_watch.hpp and
// classic_remote.hpp's `watch`). Over loopback TCP a made-up stream port answers BEGIN and hands a
// written match out a frame at a time in pieces that cut records in two; the window's remote, set
// to watch it, reports the level once, then frames, and ends where a reader of the whole match
// ends. Through our TV node with a password the client answers the challenge and watches the same
// match; with no password it is refused; an address with no port is refused.
//
// ge:summary Tests: our client watching a classic broadcast directly and through our TV node
// ge:keywords test watch viewer broadcast stream-port tv-node password whole-frames remote_watch
// ge:skill net shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_tv.hpp"
#include "shooters_edge/net/classic_watch.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

namespace {

std::string state_text(const cl::DemoState& s) {
  std::string t = s.map + "\n";
  const auto f = [](float v) { return std::to_string(static_cast<int>(v * 8)); };
  for (size_t k = 0; k < s.players.size(); k++) {
    const cl::DemoEntity& p = s.players[k];
    if (p.present)
      t += std::to_string(k) + " " + f(p.origin.x) + "," + f(p.origin.y) + "," + f(p.origin.z) +
           " " + std::to_string(s.frags[k]) + " " + s.names[k] + "\n";
  }
  for (size_t e = 0; e < s.entities.size(); e++)
    if (s.entities[e].present)
      t += "e" + std::to_string(e) + " " + f(s.entities[e].origin.x) + "\n";
  return t;
}

std::string whole_end(const std::vector<uint8_t>& match) {
  cl::Demo d;
  std::string error;
  if (!cl::open_demo(match, d, error)) return "unread";
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
  }
  return state_text(d.state);
}

// A made-up stream port: BEGIN to the first request, then `step` bytes of the match a call.
struct Port {
  gn::TcpListener listener;
  gn::TcpStream tcp;
  std::string request;
  bool begun = false;
  size_t sent = 0;
  bool closed = false;
};

void pump(Port& p, const std::vector<uint8_t>& match, size_t step) {
  if (p.tcp.handle < 0) gn::tcp_accept(p.listener, p.tcp);
  if (p.tcp.handle < 0 || p.closed) return;
  gn::tcp_pump(p.tcp);
  std::vector<uint8_t> got;
  gn::tcp_take_bytes(p.tcp, got);
  if (!p.begun) {
    p.request.append(got.begin(), got.end());
    cn::QtvRequest q;
    size_t used = 0;
    if (cn::qtv_parse_request(p.request, q, used) != cn::QtvParse::ok) return;
    p.begun = true;
    const std::string text = cn::qtv_answer_text(cn::QtvAnswer{});
    gn::tcp_send_bytes(p.tcp, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    return;
  }
  const size_t n = std::min(step, match.size() - p.sent);
  if (n > 0) gn::tcp_send_bytes(p.tcp, match.data() + p.sent, n);
  p.sent += n;
  if (p.sent == match.size() && p.tcp.out_head >= p.tcp.out.size()) {
    gn::tcp_close(p.tcp);
    p.closed = true;
  }
}

}  // namespace

TEST(the_client_watches_a_broadcast_frame_by_frame_to_its_end) {
  const std::vector<uint8_t> match = cl::write_multiview_match(cl::highlight_match());
  Port port;
  std::string error;
  CHECK(gn::tcp_listen(port.listener, gn::loopback_v4(0), &error));
  cn::Remote r;
  CHECK_MSG(cn::remote_watch(r, gn::to_string(port.listener.local), "eve", "", error), error);
  CHECK(cn::remote_status(r).starts_with("asking to watch "));
  int levels = 0, frames = 0;
  bool dropped = false;
  for (int i = 0; i < 20000 && !dropped; i++) {
    pump(port, match, 997);  // pieces that cut records in two
    std::vector<std::string> said;
    switch (cn::remote_step(r, 0.013, said)) {
      case cn::RemoteStep::new_level:
        levels++;
        break;
      case cn::RemoteStep::frame:
        frames++;
        break;
      case cn::RemoteStep::dropped:
        dropped = true;
        break;
      default:
        break;
    }
  }
  CHECK(dropped && !r.active);
  CHECK(levels == 1);
  CHECK_MSG(frames > 20, std::to_string(frames));  // a step reads every frame that has come
  CHECK(r.why == "the broadcast ended");
  CHECK(state_text(r.demo.state) == whole_end(match));
  gn::tcp_close_listener(port.listener);
}

TEST(through_our_node_with_a_password_the_client_answers_the_challenge) {
  const std::vector<uint8_t> match = cl::write_multiview_match(cl::highlight_match());
  Port port;
  std::string error;
  CHECK(gn::tcp_listen(port.listener, gn::loopback_v4(0), &error));
  cn::TvNode n;
  cn::TvConfig cfg;
  cfg.delay_s = 0.2;
  cfg.password = "secret";
  CHECK(cn::tv_open(n, gn::loopback_v4(0), port.listener.local, cfg, error));
  cn::StreamWatch good, none;
  CHECK(cn::watch_open(good, n.listener.local, "eve", "secret", error));
  CHECK(cn::watch_open(none, n.listener.local, "bob", "", error));
  cl::Demo seen, nothing;
  cn::watch_demo(seen);
  cn::watch_demo(nothing);
  bool none_live = true;
  for (int i = 0; i < 4000; i++) {
    pump(port, match, 4000);
    cn::tv_update(n, i * 0.02);
    cn::watch_take(good, seen);
    if (none_live) none_live = cn::watch_take(none, nothing);
  }
  CHECK_MSG(!none_live && none.why == "You need to provide a common auth method.", none.why);
  CHECK(good.records > 100);
  // What was seen through the node plays to the whole match's end.
  cl::Demo d;
  CHECK_MSG(cl::open_demo(seen.bytes, d, error), error);
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
  }
  CHECK(state_text(d.state) == whole_end(match));
  cn::tv_close(n);
  gn::tcp_close_listener(port.listener);
}

TEST(a_broadcast_address_needs_its_port) {
  cn::Remote r;
  std::string error;
  CHECK(!cn::remote_watch(r, "192.168.1.20", "eve", "", error));
  CHECK(error.find("host and port") != std::string::npos);
  CHECK(!r.active);
}
