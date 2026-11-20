// The scripted single-player client (q1_netgame_watcher.hpp): the transport's header, the
// connect, the reliable fragments joined and acknowledged, the signon answered stage by stage, the
// frames read with the game's own reader.
//
// ge:summary A scripted single-player client's transport and signon, from the transport's facts
// ge:keywords netgame single-player protocol-15 watcher scripted-client signon reliable resend
// ge:skill net shooter
// ge:test tests/shooters_edge_q1_tv_netgame_test.cpp
#include "shooters_edge/net/q1_netgame_watcher.hpp"

#include <algorithm>
#include <cstring>

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;
namespace gn = ge::net;

// The header's flags (porting/shooters-edge/netgame-transport.md § The header).
constexpr uint32_t kData = 0x00010000u, kAck = 0x00020000u, kEom = 0x00080000u,
                   kUnreliable = 0x00100000u, kControl = 0x80000000u;
constexpr uint8_t kNop = 1, kTime = 7, kUpdateFrags = 14, kStuffText = 9, kSetAngle = 10,
                  kSignonNum = 25;
constexpr uint8_t kCcAccept = 0x81, kClcStringCmd = 4, kClcMove = 3;

void be32(std::vector<uint8_t>& o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>(v >> s));
}
uint32_t rd32(const uint8_t* p) {
  return uint32_t{p[0]} << 24 | uint32_t{p[1]} << 16 | uint32_t{p[2]} << 8 | p[3];
}

void send_raw(NetgameWatcher& w, const std::vector<uint8_t>& d, int64_t now) {
  gn::transport_send(*w.t, w.server, d.data(), d.size(), now);
}

void connect(NetgameWatcher& w, int64_t now) {
  std::vector<uint8_t> d;
  const char game[] = {'Q', 'U', 'A', 'K', 'E', 0};  // the game's name, a wire constant
  be32(d, kControl | static_cast<uint32_t>(4 + 1 + sizeof(game) + 1));
  d.push_back(0x01);
  d.insert(d.end(), game, game + sizeof(game));
  d.push_back(3);  // the net version
  send_raw(w, d, now);
  if (w.first_connect_us < 0) w.first_connect_us = now;
}

void flush(NetgameWatcher& w, int64_t now) {
  if (w.pending && now - w.flight_us >= 1000000) {  // resent after a second, as the client does
    send_raw(w, w.in_flight, now);
    w.flight_us = now;
    w.resends++;
  }
  if (w.pending || w.outbox.empty()) return;
  std::vector<uint8_t> body;
  for (const std::string& c : w.outbox) {
    body.push_back(kClcStringCmd);
    body.insert(body.end(), c.begin(), c.end());
    body.push_back(0);
  }
  w.outbox.clear();
  std::vector<uint8_t> d;
  be32(d, kData | kEom | static_cast<uint32_t>(8 + body.size()));
  be32(d, w.send_seq);
  d.insert(d.end(), body.begin(), body.end());
  send_raw(w, d, now);
  w.in_flight = d;
  w.flight_us = now;
  w.pending = true;
}

void messages(NetgameWatcher& w, const std::vector<uint8_t>& m, bool reliable, int64_t now) {
  if (m.size() == 1 && m[0] == kNop) return;  // a keepalive
  cl::DemoMsg dm;
  dm.b = m;
  std::vector<uint32_t> marks;
  dm.marks = &marks;
  std::string why;
  if (!cl::read_single_messages(w.view, dm, why) && w.read_error.empty())
    w.read_error = why.empty() ? "a message did not read" : why;
  for (const cl::DemoPrint& p : w.view.state.prints) w.prints.push_back(p.text);
  for (const uint32_t at : marks)  // the frags: the reader keeps no scoreboard
    if (m[at] == kUpdateFrags && at + 3 < m.size() && m[at + 1] < w.frags.size())
      w.frags[m[at + 1]] = static_cast<int16_t>(m[at + 2] | m[at + 3] << 8);
  w.view.state.prints.clear();
  if (!reliable) {
    w.frames++;
    if (m[0] == kTime && m.size() >= 5) {
      float at = 0;
      std::memcpy(&at, m.data() + 1, 4);
      if (at < w.last_time) w.time_fell = true;
      w.last_time = at;
    }
    for (const cl::DemoEntity& e : w.view.state.entities) w.entity_updates += e.present ? 1 : 0;
    if (m.size() >= 4 && m[m.size() - 4] == kSetAngle) w.setangles++;
    return;
  }
  w.reliable_messages++;
  // A signon number stands at the end of its message; stuffed text anywhere.
  if (m.size() >= 2 && m[m.size() - 2] == kSignonNum) {
    const int n = m[m.size() - 1];
    if (n == w.signon + 1) {
      w.signon = n;
      if (n == 1) w.outbox.push_back("prespawn");
      if (n == 2) {
        w.outbox.push_back("name \"viewer\"\n");
        w.outbox.push_back("color 0 0\n");
        w.outbox.push_back("spawn ");
      }
      if (n == 3) {
        w.outbox.push_back("begin");
        w.seatings++;
        w.last_seated_us = now;
        if (w.seated_us < 0) w.seated_us = now;
      }
    }
  }
  static const std::string kRe = "reconnect\n";
  for (size_t i = 0; i + kRe.size() < m.size(); i++)
    if (m[i] == kStuffText && std::memcmp(m.data() + i + 1, kRe.data(), kRe.size()) == 0) {
      w.reconnects++;
      w.signon = 0;
      w.frags = {};
      w.last_time = -1;  // the level starts again: its clock with it
    }
}

}  // namespace

void netgame_watch(NetgameWatcher& w, gn::Transport& t, const gn::Address& server) {
  w.t = &t;
  w.server = server;
}

void netgame_watch_command(NetgameWatcher& w, const std::string& text) { w.outbox.push_back(text); }

void netgame_watch_tick(NetgameWatcher& w, int64_t now) {
  if (!w.accepted && (w.first_connect_us < 0 || (now - w.first_connect_us) % 500000 < 13000))
    connect(w, now);
  uint8_t buf[2048];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*w.t, buf, sizeof buf, from, now);
    if (n < 8) {
      if (n < 0) break;
      continue;
    }
    w.bytes_in += n;
    const uint32_t word = rd32(buf);
    if ((word & kControl) != 0) {
      if (buf[4] == kCcAccept && n >= 7) {
        w.accepted = true;
        w.server.port = static_cast<uint16_t>(buf[5] | buf[6] << 8);
      }
      continue;
    }
    if (from != w.server) continue;
    const uint32_t seq = rd32(buf + 4);
    const size_t len = std::min<size_t>(word & 0xFFFF, static_cast<size_t>(n));
    const std::vector<uint8_t> payload(buf + 8, buf + len);
    if ((word & kAck) != 0) {
      if (w.pending && seq == w.send_seq) w.pending = false, w.send_seq++;
    } else if ((word & kData) != 0) {
      std::vector<uint8_t> ack;
      be32(ack, kAck | 8u);
      be32(ack, seq);
      send_raw(w, ack, now);
      if (seq != w.recv_seq) continue;  // a fragment seen before, acknowledged again
      w.recv_seq++;
      w.partial.insert(w.partial.end(), payload.begin(), payload.end());
      if ((word & kEom) != 0) {
        const std::vector<uint8_t> msg = std::move(w.partial);
        w.partial.clear();
        messages(w, msg, true, now);
      }
    } else if ((word & kUnreliable) != 0 && !payload.empty()) {
      messages(w, payload, false, now);
    }
  }
  flush(w, now);
  if (w.signon == 3 && w.frames > 0) {  // a move a tick once watching, as a client does
    std::vector<uint8_t> d;
    be32(d, kUnreliable | 24u);
    be32(d, w.unrel_out++);
    d.push_back(kClcMove);
    d.resize(8 + 14, 0);  // the time, the angles, the three speeds
    d.push_back(w.buttons);
    d.push_back(w.impulse);
    send_raw(w, d, now);
  }
}

}  // namespace shooters_edge::net
