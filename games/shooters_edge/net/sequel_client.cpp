// The scripted sequel client (sequel_client.hpp): the connectionless steps, the signon as the
// server stuffs it, the payloads read as the sequel's recording, and the moves.
//
// ge:summary The scripted sequel client's steps: connect, the stuffed signon, frames, moves
// ge:keywords sequel-client sequel_client_tick stufftext signon begin move ack-frame
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_client.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace cl = shooters_edge::client;

namespace {

constexpr double kRetry = 1.0;  // s between connectionless retries, and keep-alives in the signon
constexpr size_t kKept = 256;   // stuffed lines and prints kept

void send_raw(SequelClientEnd& c, const uint8_t* data, size_t size) {
  gn::transport_send(*c.transport, c.server, data, size, static_cast<int64_t>(c.now * 1e6));
  c.bytes_out += static_cast<int64_t>(size);
}

void send_oob(SequelClientEnd& c, const std::string& text) {
  const std::vector<uint8_t> b = sequel_oob(text);
  send_raw(c, b.data(), b.size());
}

std::string userinfo_of(const SequelClientEnd& c) {
  std::string u;
  info_set(u, "name", c.cfg.name);
  info_set(u, "skin", c.cfg.skin);
  info_set(u, "rate", std::to_string(c.cfg.rate));
  info_set(u, "msg", "1");
  info_set(u, "hand", "0");
  if (c.cfg.spectator) info_set(u, "spectator", "1");
  if (!c.cfg.password.empty()) info_set(u, "password", c.cfg.password);
  return u;
}

void send_connect(SequelClientEnd& c) {
  send_oob(c, "connect " + std::to_string(kSequelNetProtocol) + " " + std::to_string(c.cfg.qport) +
                  " " + std::to_string(c.challenge) + " \"" + userinfo_of(c) + "\"\n");
}

void connected(SequelClientEnd& c) {
  c.stage = ClientStage::signon;
  c.connected_at = c.now;
  channel_setup(c.ch, true, c.cfg.qport, c.now);
  channel_set_rate(c.ch, 0);
  sequel_client_command(c, "new");
}

void read_oob(SequelClientEnd& c, const uint8_t* data, size_t size) {
  const std::string text = sequel_oob_text(data, size);
  if (text.rfind("print\n", 0) == 0) {
    const std::string said = text.substr(6);
    if (c.stage == ClientStage::challenge || c.stage == ClientStage::connect) {
      c.refused = said;
      c.stage = ClientStage::dropped;
    } else if (c.prints.size() < kKept) {
      c.prints.push_back(said);
    }
    return;
  }
  const std::vector<std::string> w = sequel_words(text);
  if (w.empty()) return;
  if (w[0] == "challenge" && w.size() >= 2 && c.stage == ClientStage::challenge) {
    c.challenge = static_cast<int32_t>(std::strtol(w[1].c_str(), nullptr, 10));
    c.stage = ClientStage::connect;
    c.last_ask = c.now;
    send_connect(c);
  } else if (w[0] == "client_connect" && c.stage == ClientStage::connect) {
    connected(c);
  }
}

// The lines the server stuffed: the signon's prompts answered, the rest kept.
void run_stuffed(SequelClientEnd& c) {
  for (const std::string& text : c.demo.stuffed) {
    size_t from = 0;
    while (from < text.size()) {
      size_t end = text.find_first_of("\n;", from);
      if (end == std::string::npos) end = text.size();
      const std::string line = text.substr(from, end - from);
      from = end + 1;
      if (line.empty()) continue;
      if (c.stuffed.size() < kKept) c.stuffed.push_back(line);
      const std::vector<std::string> w = sequel_words(line);
      if (w.empty()) continue;
      if (w[0] == "cmd" && line.size() > 4) {
        sequel_client_command(c, line.substr(4));
        c.signon_steps++;
      } else if (w[0] == "precache") {
        sequel_client_command(c, "begin" + (w.size() > 1 ? " " + w[1] : std::string()));
        c.signon_steps++;
      } else if (w[0] == "reconnect") {
        c.stage = ClientStage::signon;
        sequel_client_command(c, "new");
      }
    }
  }
  c.demo.stuffed.clear();
}

// One payload as one block of the recording, then read.
void read_payload(SequelClientEnd& c, const uint8_t* data, size_t size) {
  if (size == 0) return;  // a keep-alive
  const int levels = c.demo.state.levels;
  const auto len = static_cast<uint32_t>(size);
  for (int k = 0; k < 4; k++) c.demo.bytes.push_back(static_cast<uint8_t>(len >> (8 * k)));
  c.demo.bytes.insert(c.demo.bytes.end(), data, data + size);
  for (;;) {
    const cl::DemoRead got = cl::next_sequel(c.demo, c.error);
    if (got == cl::DemoRead::error) {
      c.stage = ClientStage::dropped;
      c.refused = "could not read the server: " + c.error;
      return;
    }
    if (got == cl::DemoRead::end) break;
    c.frames++;
    if (c.stage == ClientStage::signon) {
      c.stage = ClientStage::active;
      c.active_at = c.now;
    }
  }
  if (c.demo.state.levels != levels) c.levels += c.demo.state.levels - levels;
  for (const cl::DemoPrint& p : c.demo.state.prints)
    if (c.prints.size() < kKept) c.prints.push_back(p.text);
  c.demo.state.prints.clear();
  c.demo.state.effects.clear();
  c.demo.state.started.clear();
  c.demo.state.stopped.clear();
  run_stuffed(c);
  if (c.demo.reconnect) {  // the server restarted: a fresh connect
    c.demo.reconnect = false;
    c.stage = ClientStage::challenge;
    c.last_ask = -1;
  }
  if (c.demo.ended) {
    c.stage = ClientStage::dropped;
    if (c.refused.empty()) c.refused = "the server ended the connection";
  }
}

void send_packet(SequelClientEnd& c, int64_t now_us) {
  const bool active = c.stage == ClientStage::active;
  if (active && c.cfg.send_every > 1 && c.tick % c.cfg.send_every != 0) return;
  if (!active && channel_reliable_backlog(c.ch) == 0 && c.now - c.last_ask < kRetry) return;
  c.last_ask = c.now;
  uint8_t payload[kSequelPacket];
  gn::Writer w = gn::make_writer(payload, sizeof payload - kClientHeader);
  if (active) {
    sh::MoveCmd m = c.cfg.brain ? c.cfg.brain(c.tick) : sh::MoveCmd{};
    const double ms = c.last_tick < 0 ? 13 : (c.now - c.last_tick) * 1000.0;
    m.msec = static_cast<uint8_t>(std::clamp(ms, 1.0, 250.0));
    c.cmds[0] = c.cmds[1];
    c.cmds[1] = c.cmds[2];
    c.cmds[2] = sequel_cmd(m);
    write_sequel_move(w, c.demo.last_frame, c.cmds, c.ch.outgoing_sequence);
    c.moves++;
  }
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, payload, w.overflow ? 0 : w.size, c.now, out);
  gn::transport_send(*c.transport, c.server, out, n, now_us);
  c.bytes_out += static_cast<int64_t>(n);
}

}  // namespace

void sequel_client_start(SequelClientEnd& c, gn::Transport& t, const gn::Address& server,
                         const SequelClientConfig& cfg, double now) {
  c = SequelClientEnd{};
  c.cfg = cfg;
  c.transport = &t;
  c.server = server;
  c.now = now;
  cl::sequel_stream_begin(c.demo);
  if (!cfg.via.empty()) {
    std::vector<gn::Address> chain = cfg.via;
    chain.push_back(server);
    ge::forward::dial_start(c.dial, chain, static_cast<uint64_t>(cfg.seed) * 7919u + cfg.qport,
                            static_cast<int64_t>(now * 1e6));
    c.dialing = true;
  }
}

void sequel_client_command(SequelClientEnd& c, const std::string& line) {
  std::vector<uint8_t> b = {kClcStringCmd};
  b.insert(b.end(), line.begin(), line.end());
  b.push_back(0);
  channel_reliable(c.ch, b.data(), b.size());
}

void sequel_client_tick(SequelClientEnd& c, int64_t now_us) {
  c.now = static_cast<double>(now_us) / 1e6;
  c.tick++;
  gn::transport_pump(*c.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*c.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    c.datagrams_in++;
    c.bytes_in += n;
    if (!c.cfg.via.empty() && ge::forward::dial_read(c.dial, buf, size, from, now_us)) continue;
    if (c.dialing || from != c.server || c.stage == ClientStage::dropped) continue;
    if (is_out_of_band(buf, size)) {
      read_oob(c, buf, size);
      continue;
    }
    if (c.stage != ClientStage::signon && c.stage != ClientStage::active) continue;
    size_t at = 0;
    if (!channel_process(c.ch, buf, size, c.now, at)) continue;
    read_payload(c, buf + at, size - at);
  }
  if (!c.cfg.via.empty()) {
    ge::forward::dial_update(c.dial, *c.transport, now_us);
    if (c.dialing && c.dial.state == ge::forward::DialState::routed) {
      c.dialing = false;
      c.server = c.dial.first;  // the game's handshake goes to the first hop
    } else if (c.dialing && (c.dial.state == ge::forward::DialState::refused ||
                             c.dial.state == ge::forward::DialState::timed_out)) {
      c.dialing = false;
      c.stage = ClientStage::dropped;
      c.refused = "the route through the forwarders failed: " + c.dial.why;
    }
    if (c.dialing) {
      c.last_tick = c.now;
      return;
    }
  }
  switch (c.stage) {
    case ClientStage::challenge:
      if (c.last_ask < 0 || c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_oob(c, "getchallenge\n");
      }
      break;
    case ClientStage::connect:
      if (c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_connect(c);
      }
      break;
    case ClientStage::signon:
    case ClientStage::active:
      send_packet(c, now_us);
      break;
    case ClientStage::dropped:
      break;
  }
  c.last_tick = c.now;
}

void sequel_client_stop(SequelClientEnd& c) {
  if (c.stage == ClientStage::signon || c.stage == ClientStage::active) {
    const uint8_t bye[] = {kClcStringCmd, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 0};
    for (int k = 0; k < 3; k++) {
      uint8_t out[kMaxPacket];
      const size_t n = channel_transmit(c.ch, bye, sizeof bye, c.now, out);
      send_raw(c, out, n);
    }
    c.stage = ClientStage::dropped;
    if (c.refused.empty()) c.refused = "left";
  }
  if (c.recording) {
    for (int k = 0; k < 4; k++) c.demo.bytes.push_back(0xFF);
    c.recording = false;
  }
}

}  // namespace shooters_edge::net
