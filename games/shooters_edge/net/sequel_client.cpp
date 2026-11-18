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

#include "shooters_edge/net/classic_ext.hpp"
#include "shooters_edge/net/protocol.hpp"

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
  if (c.cfg.fetch && c.cfg.ask_origins) info_set(u, "*ge_ext", std::to_string(kGeDownloadOrigin));
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

// THE DOWNLOADS: the next file asked for, or the held `begin` sent once none is left.
void next_fetch(SequelClientEnd& c) {
  c.fetching.clear();
  c.fetched_bytes.clear();
  c.fetched_origin = false;
  if (!c.fetch_queue.empty()) {
    c.fetching = c.fetch_queue.front();
    c.fetch_queue.erase(c.fetch_queue.begin());
    sequel_client_command(c, "download " + c.fetching);
    return;
  }
  if (!c.held_begin.empty()) sequel_client_command(c, c.held_begin);
  c.held_begin.clear();
}

bool lacks(const SequelClientEnd& c, const std::string& path) {
  return c.downloaded.count(path) == 0 && (!c.cfg.read_file || c.cfg.read_file(path).empty());
}

// `precache`: the files lacked queued, `begin` held until they are fetched.
void start_fetch(SequelClientEnd& c, const std::string& begin) {
  c.fetch_queue.clear();
  const std::vector<std::string>& models = sequel_demo(c).state.models;
  if (models.size() > 1 && models[1].rfind("maps/", 0) == 0 && lacks(c, models[1]))
    c.fetch_queue.push_back(models[1]);
  if (c.cfg.fetch_level) {  // what the level names: models, sounds and the players' figures
    const auto queue = [&c](const std::string& path) {
      if (path.empty() || path[0] == '*' || path[0] == '#' || path.find('/') == std::string::npos)
        return;
      if (lacks(c, path) &&
          std::find(c.fetch_queue.begin(), c.fetch_queue.end(), path) == c.fetch_queue.end())
        c.fetch_queue.push_back(path);
    };
    const cl::DemoState& st = sequel_demo(c).state;
    for (size_t i = 2; i < models.size(); i++) queue(models[i]);
    for (size_t i = 1; i < st.sounds.size(); i++)
      if (!st.sounds[i].empty() && st.sounds[i][0] != '*') queue("sound/" + st.sounds[i]);
    for (const std::string& look : st.looks) {
      const size_t slash = look.find('/');
      if (slash == std::string::npos || slash == 0 || slash + 1 >= look.size()) continue;
      const std::string figure = look.substr(0, slash);
      queue("players/" + figure + "/tris.md2");
      queue("players/" + figure + "/" + look.substr(slash + 1) + ".pcx");
    }
  }
  for (const std::string& w : c.cfg.want)
    if (lacks(c, w) &&
        std::find(c.fetch_queue.begin(), c.fetch_queue.end(), w) == c.fetch_queue.end())
      c.fetch_queue.push_back(w);
  c.held_begin = begin;
  next_fetch(c);
}

// The download pieces and archives the reader took off this payload.
void take_downloads(SequelClientEnd& c) {
  for (auto& [path, origin] : sequel_demo(c).download_origins)
    if (path == c.fetching) {
      c.fetched_origin = true;
      c.fetching_origin = std::move(origin);
    }
  sequel_demo(c).download_origins.clear();
  for (cl::Demo::DownloadPiece& p : sequel_demo(c).download_pieces) {
    if (c.fetching.empty()) continue;
    if (p.size < 0) {
      c.downloads_refused++;
      next_fetch(c);
      continue;
    }
    c.fetched_bytes.insert(c.fetched_bytes.end(), p.bytes.begin(), p.bytes.end());
    if (p.percent < 100) {
      sequel_client_command(c, "nextdl");
      continue;
    }
    c.downloaded[c.fetching] = c.fetched_bytes;
    if (c.cfg.keep_file)
      c.cfg.keep_file(c.fetching, c.fetched_bytes, c.fetched_origin ? &c.fetching_origin : nullptr);
    next_fetch(c);
  }
  sequel_demo(c).download_pieces.clear();
}

// The lines the server stuffed: the signon's prompts answered, the rest kept.
void run_stuffed(SequelClientEnd& c) {
  for (const std::string& text : sequel_demo(c).stuffed) {
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
        const std::string begin = "begin" + (w.size() > 1 ? " " + w[1] : std::string());
        if (c.cfg.fetch)
          start_fetch(c, begin);
        else
          sequel_client_command(c, begin);
        c.signon_steps++;
      } else if (w[0] == "reconnect") {
        c.stage = ClientStage::signon;
        sequel_client_command(c, "new");
      }
    }
  }
  sequel_demo(c).stuffed.clear();
}

// One payload as one block of the recording, then read.
void read_payload(SequelClientEnd& c, const uint8_t* data, size_t size) {
  if (size == 0) return;  // a keep-alive
  const int levels = sequel_demo(c).state.levels;
  const auto len = static_cast<uint32_t>(size);
  for (int k = 0; k < 4; k++) sequel_demo(c).bytes.push_back(static_cast<uint8_t>(len >> (8 * k)));
  sequel_demo(c).bytes.insert(sequel_demo(c).bytes.end(), data, data + size);
  for (;;) {
    const cl::DemoRead got = cl::next_sequel(sequel_demo(c), c.error);
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
  if (sequel_demo(c).state.levels != levels) c.levels += sequel_demo(c).state.levels - levels;
  cl::DemoState& st = sequel_demo(c).state;
  for (const cl::DemoPrint& p : st.prints)
    if (c.prints.size() < kKept) c.prints.push_back(p.text);
  if (c.cfg.into != nullptr) {  // the window's: kept until it takes them, the read bytes dropped
    SequelNews& n = c.news;
    n.prints.insert(n.prints.end(), st.prints.begin(), st.prints.end());
    n.effects.insert(n.effects.end(), st.effects.begin(), st.effects.end());
    n.started.insert(n.started.end(), st.started.begin(), st.started.end());
    n.stopped.insert(n.stopped.end(), st.stopped.begin(), st.stopped.end());
    if (!st.centre.empty()) n.centre = std::move(st.centre);
    st.centre.clear();
    cl::Demo& d = sequel_demo(c);
    d.bytes.erase(d.bytes.begin(), d.bytes.begin() + static_cast<std::ptrdiff_t>(d.pos));
    d.pos = 0;
  }
  sequel_demo(c).state.prints.clear();
  sequel_demo(c).state.effects.clear();
  sequel_demo(c).state.started.clear();
  sequel_demo(c).state.stopped.clear();
  take_downloads(c);
  run_stuffed(c);
  if (sequel_demo(c).reconnect) {  // the server restarted: a fresh connect
    sequel_demo(c).reconnect = false;
    c.stage = ClientStage::challenge;
    c.last_ask = -1;
  }
  if (sequel_demo(c).ended) {
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
    int32_t acked = sequel_demo(c).last_frame;
    if (c.cfg.whole_every > 0 && c.now - c.last_whole >= c.cfg.whole_every) c.last_whole = c.now;
    // For 150 ms (past the server's next 10 Hz frame, whatever moves come between) no frame is
    // acknowledged, so the server's next one is whole.
    if (c.cfg.whole_every > 0 && c.now - c.last_whole < 0.15) acked = -1;
    write_sequel_move(w, acked, c.cmds, c.ch.outgoing_sequence);
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
  cl::sequel_stream_begin(sequel_demo(c));
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
  // A connected server that has gone quiet: given up, so the game can say so and leave.
  if ((c.stage == ClientStage::signon || c.stage == ClientStage::active) && c.cfg.timeout > 0 &&
      c.now - c.ch.last_received > c.cfg.timeout) {
    c.stage = ClientStage::dropped;
    c.refused = "the server stopped answering";
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
    for (int k = 0; k < 4; k++) sequel_demo(c).bytes.push_back(0xFF);
    c.recording = false;
  }
}

}  // namespace shooters_edge::net
