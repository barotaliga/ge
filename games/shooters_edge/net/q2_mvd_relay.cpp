// Quake II's multiview relay (q2_mvd_relay.hpp): the upstream connections, the channels and their
// delay buffers, played files and their seeks, recordings of a channel, the operator's words.
//
// ge:summary Quake II's relay: GTV connections, channels, delay buffers, mvdplay, operator words
// ge:keywords mvdconnect mvdplay mvdchannels mvdservers mvdkill mvdspawn mvdseek mvdskip
// ge:keywords mvdpause mvdcontrol mvdisconnect backoff delay-buffer stream-start stream-stop
// ge:skill net shooter replay
// ge:test tests/shooters_edge_q2_mvd_relay_test.cpp
#include "shooters_edge/net/q2_mvd_relay.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "core/inflate.hpp"
#include "shooters_edge/client/demo_mvd2.hpp"
#include "shooters_edge/net/q2_mvd_server.hpp"
#include "shooters_edge/server/q2_mvd2.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

constexpr double kFrame = 0.1;          // a stream's frame, s
constexpr double kMostBackoff = 18000;  // 5 hours
constexpr size_t kChunk = 32768;

std::string addr_text(const gn::Address& a) { return gn::to_string(a); }

// A length in words: "0 secs", "1 min, 5 secs", "2 hours".
std::string time_words(double seconds) {
  int64_t s = static_cast<int64_t>(seconds);
  const int64_t days = s / 86400, hours = s / 3600 % 24, mins = s / 60 % 60;
  s %= 60;
  std::string t;
  const auto part = [&t](int64_t n, const char* one) {
    if (n == 0) return;
    if (!t.empty()) t += ", ";
    t += std::to_string(n) + " " + one + (n == 1 ? "" : "s");
  };
  part(days, "day");
  part(hours, "hour");
  part(mins, "min");
  part(s, "sec");
  return t.empty() ? "0 secs" : t;
}

void put_u16(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}
void put_u32(std::vector<uint8_t>& b, uint32_t v) {
  put_u16(b, v & 0xFFFF);
  put_u16(b, v >> 16);
}
void put_str(std::vector<uint8_t>& b, const std::string& s) {
  b.insert(b.end(), s.begin(), s.end());
  b.push_back(0);
}

Q2GtvConn* conn_of(Q2Relay& r, int id) {
  for (Q2GtvConn& c : r.conns)
    if (c.id == id) return &c;
  return nullptr;
}

void conn_say(Q2Relay& r, const Q2GtvConn& c, const std::string& text) {
  relay::say(r, "[" + c.name + "] " + text);
}

void chan_say(Q2Relay& r, const Q2Channel& c, const std::string& text) {
  relay::say(r, "[" + c.name + "] " + text);
}

void send(Q2GtvConn& c, uint8_t op, const std::vector<uint8_t>& payload, double now) {
  std::vector<uint8_t> m;
  put_u16(m, static_cast<uint32_t>(payload.size() + 1));
  m.push_back(op);
  m.insert(m.end(), payload.begin(), payload.end());
  gn::tcp_send_bytes(c.stream, m.data(), m.size());
  c.sent = now;
}

// The frames the channel waits for before playing at first: `mvd_wait_delay` in frames.
int wait_frames(const Q2Relay& r) { return std::max(0, r.cfg.wait_delay) * 10; }

void destroy_channel(Q2Relay& r, int id, const std::string& why);

void close_rec(Q2Channel& c) {
  if (c.rec == nullptr) return;
  if (c.rec_gz) {
    std::vector<uint8_t> tail;
    ge::core::deflate_finish(c.gz, tail);
    std::fwrite(tail.data(), 1, tail.size(), c.rec);
  }
  std::fclose(c.rec);
  c.rec = nullptr;
}

void rec_write(Q2Channel& c, const std::vector<uint8_t>& bytes) {
  if (c.rec == nullptr) return;
  if (!c.rec_gz) {
    std::fwrite(bytes.data(), 1, bytes.size(), c.rec);
    return;
  }
  std::vector<uint8_t> z;
  ge::core::deflate_sync(c.gz, bytes, z);
  std::fwrite(z.data(), 1, z.size(), c.rec);
}

// One block (its length taken off) in a file's form: the length, then the bytes.
std::vector<uint8_t> framed(const std::vector<uint8_t>& block) {
  std::vector<uint8_t> b;
  put_u16(b, static_cast<uint32_t>(block.size()));
  b.insert(b.end(), block.begin(), block.end());
  return b;
}

bool is_gamestate(const std::vector<uint8_t>& block) {
  return !block.empty() && (block[0] & 0x1F) == sv::kMv2ServerData;
}

Q2Channel& make_channel(Q2Relay& r, int id, const std::string& name, int conn) {
  auto c = std::make_unique<Q2Channel>();
  c->id = id;
  c->name = name;
  c->conn = conn;
  c->target = wait_frames(r);
  const int units = std::clamp(r.cfg.buffer_size, 2, 256);
  c->most = size_t(units) * kChunk;
  std::string error;
  client::ConvertOptions o;
  o.pov = -2;  // reads only
  o.no_end = true;
  client::convert_begin(c->base, client::DemoFormat::q2_multiview, client::DemoFormat::q2, o,
                        error);
  const uint8_t magic[] = {'M', 'V', 'D', '2'};
  client::convert_feed(c->base, magic);
  r.channels.push_back(std::move(c));
  return *r.channels.back();
}

void destroy_conn(Q2Relay& r, Q2GtvConn& g, const std::string& why);

// A block read in: the reader takes a gamestate at once; after the first, everything waits in
// the delay buffer.
void take_block(Q2Relay& r, Q2Channel& c, std::vector<uint8_t> block) {
  if (!c.have_state) {
    if (!is_gamestate(block)) return;  // nothing to read it against yet
    relay::feed(r, c, block);
    c.have_state = true;
    c.state = Q2ChanState::wait;
    c.target = wait_frames(r);
    relay::listed(r);
    return;
  }
  if (c.buffered + block.size() > c.most) {
    if (c.state == Q2ChanState::wait) {
      if (Q2GtvConn* g = conn_of(r, c.conn))
        destroy_conn(r, *g, "Delay buffer overflowed in waiting state");
      return;
    }
    chan_say(r, c, "<warn> Delay buffer overflowed!");
    relay::tell(r, c.id, "Delay buffer overflowed!");
    c.buffer.clear();
    c.buffered = 0;
    c.state = Q2ChanState::wait;
    c.target = 50;
    if (Q2GtvConn* g = conn_of(r, c.conn); g != nullptr && g->state == Q2GtvState::reading) {
      conn_say(r, *g, "-=- Sending stream stop request...");
      send(*g, kGtvStreamStop, {}, r.now);
      g->state = Q2GtvState::suspending;
    }
    return;
  }
  c.buffered += block.size();
  c.buffer.push_back(std::move(block));
}

// --- the upstream ---

void drop_conn(Q2Relay& r, Q2GtvConn& g, const std::string& why) {
  conn_say(r, g, "<warn> " + why);
  const bool was_up = g.state != Q2GtvState::connecting && g.state != Q2GtvState::disconnected;
  gn::tcp_close(g.stream);
  if (!was_up) {
    g.backoff += 15;
  } else {
    relay::tell(r, g.id, "Lost connection to the game server!");
    g.backoff = g.hello ? 5 : g.backoff + 30;
  }
  g.backoff = std::min(g.backoff, kMostBackoff);
  g.state = Q2GtvState::disconnected;
  g.magic = g.hello = g.suspended = false;
  g.in.clear();
  g.retry_at = r.now + g.backoff;
  conn_say(r, g, "-=- Reconnecting in " + time_words(g.backoff) + ".");
}

// Marked for removal; its channel plays out what it holds.
void destroy_conn(Q2Relay& r, Q2GtvConn& g, const std::string& why) {
  conn_say(r, g, "=X= " + why);
  if (Q2Channel* c = relay::channel(r, g.id); c != nullptr && c->have_state)
    relay::tell(r, g.id, "Disconnected from the game server!");
  gn::tcp_close(g.stream);
  g.retry_at = -1;
}

bool dial(Q2Relay& r, Q2GtvConn& g) {
  std::string error;
  g.stream = gn::TcpStream{};
  if (!gn::tcp_connect(g.stream, g.addr, &error)) return false;
  g.state = Q2GtvState::connecting;
  g.heard = r.now;
  return true;
}

void hello(Q2GtvConn& g, const Q2Relay& r, double now) {
  std::vector<uint8_t> h;
  put_u16(h, kGtvProtocol);
  put_u32(h, kGtfStringCmds);  // no deflate: the relay reads the stream as it is
  put_u32(h, 0);
  put_str(h, g.user.empty() ? r.cfg.username : g.user);
  put_str(h, g.pass.empty() ? r.cfg.upstream_password : g.pass);
  put_str(h, r.cfg.version);
  send(g, kGtvHello, h, now);
}

void read_conn(Q2Relay& r, Q2GtvConn& g) {
  gn::tcp_pump(g.stream);
  if (g.state == Q2GtvState::connecting) {
    if (g.stream.connecting) return;
    if (g.stream.closed) return drop_conn(r, g, "Unable to connect to " + addr_text(g.addr));
    conn_say(r, g, "-=- Connected to the game server!");
    const uint8_t magic[] = {'M', 'V', 'D', '2'};
    gn::tcp_send_bytes(g.stream, magic, sizeof magic);
    g.state = Q2GtvState::preparing;
    g.heard = r.now;
  }
  const size_t got = gn::tcp_take_bytes(g.stream, g.in);
  if (got > 0) {
    g.heard = r.now;
    g.bytes_in += got;
  }
  size_t at = 0;
  if (!g.magic) {
    if (g.in.size() < 4) {
      if (g.stream.closed) drop_conn(r, g, "Server has closed connection.");
      return;
    }
    if (std::memcmp(g.in.data(), "MVD2", 4) != 0) return destroy_conn(r, g, "Not a MVD/GTV stream");
    g.magic = true;
    at = 4;
    conn_say(r, g, "-=- Sending client hello...");
    hello(g, r, r.now);
  }
  while (g.retry_at >= 0 && g.in.size() - at >= 2) {
    const size_t len = g.in[at] | size_t(g.in[at + 1]) << 8;
    if (len == 0) {
      g.in.clear();
      return drop_conn(r, g, "End of MVD/GTV stream");
    }
    if (len > kGtvServerMost) return destroy_conn(r, g, "Oversize message");
    if (g.in.size() - at - 2 < len) break;
    const uint8_t op = g.in[at + 2];
    std::vector<uint8_t> payload(g.in.begin() + std::ptrdiff_t(at + 3),
                                 g.in.begin() + std::ptrdiff_t(at + 2 + len));
    at += 2 + len;
    switch (op) {
      case kGtsHello: {
        if (g.hello) return destroy_conn(r, g, "Duplicated server hello");
        if (payload.size() < 4) return destroy_conn(r, g, "Read past end of message");
        g.flags = payload[0] | uint32_t(payload[1]) << 8 | uint32_t(payload[2]) << 16 |
                  uint32_t(payload[3]) << 24;
        g.hello = true;
        g.state = Q2GtvState::connected;
        conn_say(r, g, "-=- Server hello done.");
        if (!r.spawned && r.port != nullptr) q2_relay_spawn(r, *r.port, r.now);
        if (Q2Channel* c = relay::channel(r, g.id); c != nullptr && c->have_state)
          relay::tell(r, g.id, "Restored connection to the game server!");
        break;
      }
      case kGtsPong:
        break;
      case kGtsStreamStart:
        if (g.state != Q2GtvState::waiting)
          return destroy_conn(
              r, g, "Unexpected stream start ack in state " + std::to_string(int(g.state)));
        g.state = Q2GtvState::reading;
        conn_say(r, g, "-=- Stream start ack received.");
        break;
      case kGtsStreamStop:
        if (g.state != Q2GtvState::suspending)
          return destroy_conn(
              r, g, "Unexpected stream stop ack in state " + std::to_string(int(g.state)));
        g.state = Q2GtvState::connected;
        conn_say(r, g, "-=- Stream stop ack received.");
        break;
      case kGtsStreamData: {
        if (g.state == Q2GtvState::suspending) break;  // after a stop asked: discarded
        if (g.state != Q2GtvState::reading)
          return destroy_conn(r, g, "Unexpected stream data packet");
        if (payload.empty()) {
          if (!g.suspended) conn_say(r, g, "-=- Stream suspended by server.");
          g.suspended = true;
          break;
        }
        if (g.suspended) conn_say(r, g, "-=- Stream resumed by server.");
        g.suspended = false;
        Q2Channel* c = relay::channel(r, g.id);
        if (c == nullptr) c = &make_channel(r, g.id, g.name, g.id);
        take_block(r, *c, std::move(payload));
        break;
      }
      case kGtsError:
        return destroy_conn(r, g, "Server side error occurred.");
      case kGtsBadRequest:
        return destroy_conn(r, g, "Server refused to process our request.");
      case kGtsNoAccess:
        return destroy_conn(r, g,
                            "You don't have permission to access MVD/GTV stream on this server.");
      case kGtsDisconnect:
        return destroy_conn(r, g, "Server has been shut down.");
      case kGtsReconnect:
        g.in.clear();
        return drop_conn(r, g, "Server has been restarted.");
      default:
        return destroy_conn(r, g, "Unknown command byte");
    }
  }
  if (g.retry_at < 0 || g.state == Q2GtvState::disconnected) return;
  g.in.erase(g.in.begin(), g.in.begin() + std::ptrdiff_t(std::min(at, g.in.size())));
  if (g.stream.closed) return drop_conn(r, g, "Server has closed connection.");
}

void run_conn(Q2Relay& r, Q2GtvConn& g) {
  if (g.state == Q2GtvState::disconnected) {
    if (r.now < g.retry_at) return;
    conn_say(r, g, "-=- Attempting to reconnect to " + g.typed + "...");
    if (!dial(r, g)) drop_conn(r, g, "Unable to connect to " + addr_text(g.addr));
    return;
  }
  read_conn(r, g);
  if (g.retry_at < 0 || g.state == Q2GtvState::disconnected) return;
  if (r.cfg.timeout > 0 && r.now - g.heard > std::min(r.cfg.timeout, 2073600.0))
    return drop_conn(r, g, "Server connection timed out.");
  if (g.state == Q2GtvState::connected && r.active) {
    const Q2Channel* c = relay::channel(r, g.id);
    const int count = std::max(10, (c != nullptr ? c->target : wait_frames(r)) / 2);
    std::vector<uint8_t> p;
    put_u16(p, static_cast<uint32_t>(count));
    conn_say(r, g, "-=- Sending stream start request...");
    send(g, kGtvStreamStart, p, r.now);
    g.state = Q2GtvState::waiting;
  } else if (g.state == Q2GtvState::reading && !r.active) {
    conn_say(r, g, "-=- Sending stream stop request...");
    send(g, kGtvStreamStop, {}, r.now);
    g.state = Q2GtvState::suspending;
  }
  if (g.hello && r.now - g.sent >= 60) send(g, kGtvPing, {}, r.now);
}

// --- played files ---

bool open_file(Q2Relay& r, Q2Channel& c, const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    destroy_channel(r, c.id, "Couldn't open " + path + ": No such file or directory");
    return false;
  }
  std::vector<uint8_t> bytes;
  uint8_t buf[65536];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
  std::fclose(f);
  if (bytes.size() >= 2 && bytes[0] == 0x1F && bytes[1] == 0x8B) {
    auto plain = ge::core::inflate_gzip(bytes);
    if (!plain) {
      destroy_channel(r, c.id, "Couldn't read " + path + ": Invalid data");
      return false;
    }
    bytes = std::move(*plain);
  }
  if (bytes.size() < 7 || std::memcmp(bytes.data(), "MVD2", 4) != 0 ||
      (bytes[6] & 0x1F) != sv::kMv2ServerData) {
    destroy_channel(r, c.id, "First message of " + path + " does not contain gamestate");
    return false;
  }
  c.data = std::move(bytes);
  c.pos = 4;
  c.snaps.clear();
  chan_say(r, c, "-=- Reading from " + path);
  return true;
}

// The next block of the file, or false at its end.
bool block_at(const std::vector<uint8_t>& data, size_t& pos, std::vector<uint8_t>& block) {
  if (pos + 2 > data.size()) return false;
  const size_t len = data[pos] | size_t(data[pos + 1]) << 8;
  if (len == 0 || pos + 2 + len > data.size()) return false;
  block.assign(data.begin() + std::ptrdiff_t(pos + 2),
               data.begin() + std::ptrdiff_t(pos + 2 + len));
  pos += 2 + len;
  return true;
}

bool next_block(Q2Channel& c, std::vector<uint8_t>& block) {
  return block_at(c.data, c.pos, block);
}

// The next file of the play list, or false when the list has ended.
bool next_file(Q2Relay& r, Q2Channel& c) {
  c.file_at++;
  if (c.file_at >= c.files.size()) {
    c.looped++;
    if (c.loops > 0 && c.looped >= c.loops) {
      destroy_channel(r, c.id, "End of play list reached");
      return false;
    }
    c.file_at = 0;
  }
  return open_file(r, c, c.files[c.file_at]);
}

void play_file(Q2Relay& r, Q2Channel& c) {
  if (c.paused || r.now < c.next) return;
  c.next = std::max(c.next + kFrame, r.now - kFrame);
  std::vector<uint8_t> block;
  for (;;) {
    if (!next_block(c, block)) {
      if (!next_file(r, c)) return;
      continue;
    }
    const bool level = is_gamestate(block);
    if (c.skip > 0 && !level) continue;
    if (level && c.skip > 0) c.skip--;
    if (c.skip > 0) continue;
    break;
  }
  if (is_gamestate(block)) c.level_frame = c.frame;
  relay::feed(r, c, block);
  if (!c.have_state) {
    c.have_state = true;
    c.state = Q2ChanState::read;
    relay::listed(r);
  }
  c.frame++;
  if (r.cfg.snaps > 0 && (c.frame - c.level_frame) % (int64_t(r.cfg.snaps) * 10) == 0) {
    Q2Snap s;
    s.pos = c.pos;
    s.frame = c.frame;
    if (client::convert_fork(c.base, s.base)) c.snaps.push_back(std::move(s));
  }
}

// --- network channels ---

void play_stream(Q2Relay& r, Q2Channel& c) {
  if (!c.have_state || r.now < c.next) return;
  c.next = std::max(c.next + kFrame, r.now - kFrame);
  Q2GtvConn* g = conn_of(r, c.conn);
  const bool streaming = g != nullptr && g->state == Q2GtvState::reading;
  if (c.state == Q2ChanState::wait) {
    const bool by_count = static_cast<int>(c.buffer.size()) >= c.target;
    const bool by_fill = c.buffered * 100 >= c.most * size_t(std::max(0, r.cfg.wait_percent));
    const bool any = !streaming && !c.buffer.empty();
    if (!streaming && c.buffer.empty()) return destroy_channel(r, c.id, "Ran out of buffers");
    if (!by_count && !by_fill && !any) return;
    chan_say(r, c,
             by_count || any ? "-=- Waiting finished, reading..."
                             : "-=- Buffering finished, reading...");
    relay::tell(r, c.id, "Streaming resumed.");
    c.state = Q2ChanState::read;
  }
  if (c.buffer.empty()) {
    if (g == nullptr) return destroy_channel(r, c.id, "End of MVD stream reached");
    if (!streaming) return destroy_channel(r, c.id, "Ran out of buffers");
    chan_say(r, c, "-=- Buffering data...");
    relay::tell(r, c.id, "Buffering data, please wait...");
    c.target = std::min(50 + 5 * c.underflows, wait_frames(r));
    c.underflows++;
    c.state = Q2ChanState::wait;
    send(*g, kGtvPing, {}, r.now);
    return;
  }
  std::vector<uint8_t> block = std::move(c.buffer.front());
  c.buffer.pop_front();
  c.buffered -= block.size();
  if (is_gamestate(block)) c.level_frame = c.frame;
  relay::feed(r, c, block);
  c.frame++;
}

void destroy_channel(Q2Relay& r, int id, const std::string& why) {
  for (size_t i = 0; i < r.channels.size(); i++) {
    Q2Channel& c = *r.channels[i];
    if (c.id != id) continue;
    chan_say(r, c, "=X= " + why);
    relay::tell(r, c.id, why);
    relay::evict(r, c);
    close_rec(c);
    r.channels.erase(r.channels.begin() + std::ptrdiff_t(i));
    relay::listed(r);
    return;
  }
}

// --- the operator's words ---

struct Opts {
  std::map<char, std::string> given;
  std::vector<std::string> rest;
  std::string bad;  // an option given a bad value, or not known
  bool help = false;
};

// Short and long options (`-n x`, `--name x`, `--name=x`); `takes` the short letters that take a
// value, `longs` each long name's letter.
Opts parse_opts(const std::vector<std::string>& w, const std::string& takes,
                const std::map<std::string, char>& longs) {
  Opts o;
  for (size_t i = 1; i < w.size(); i++) {
    const std::string& a = w[i];
    if (a.size() < 2 || a[0] != '-') {
      o.rest.push_back(a);
      continue;
    }
    char letter = 0;
    std::string value;
    bool has_value = false;
    if (a[1] == '-') {
      std::string name = a.substr(2);
      if (const size_t eq = name.find('='); eq != std::string::npos) {
        value = name.substr(eq + 1);
        has_value = true;
        name.resize(eq);
      }
      const auto it = longs.find(name);
      if (it == longs.end()) {
        o.bad = a;
        return o;
      }
      letter = it->second;
    } else {
      letter = a[1];
      if (a.size() > 2) {
        value = a.substr(2);
        has_value = true;
      }
    }
    if (letter == 'h') {
      o.help = true;
      continue;
    }
    if (takes.find(letter) == std::string::npos) {
      if (letter == 'a' || letter == 'z' || letter == 'r') {
        o.given[letter] = "";
        continue;
      }
      o.bad = a;
      return o;
    }
    if (!has_value) {
      if (i + 1 >= w.size()) {
        o.bad = a;
        return o;
      }
      value = w[++i];
    }
    o.given[letter] = value;
  }
  return o;
}

// The channel `[chanid]` names (empty: the only one); null with the reason in `out`.
Q2Channel* pick_channel(Q2Relay& r, const std::string& arg, std::string& out) {
  std::vector<Q2Channel*> listed;
  for (auto& c : r.channels)
    if (c->have_state) listed.push_back(c.get());
  if (listed.empty()) {
    out += "No active channels.\n";
    return nullptr;
  }
  if (arg.empty()) {
    if (listed.size() == 1) return listed[0];
    out += "Please specify an exact channel ID.\n";
    return nullptr;
  }
  const bool number = arg.find_first_not_of("0123456789") == std::string::npos;
  for (Q2Channel* c : listed)
    if ((number && c->id == std::atoi(arg.c_str())) || (!number && c->name == arg)) return c;
  out += "No such channel ID: " + arg + "\n";
  return nullptr;
}

Q2GtvConn* pick_conn(Q2Relay& r, const std::string& arg, std::string& out) {
  if (r.conns.empty()) {
    out += "No GTV connections.\n";
    return nullptr;
  }
  if (arg.empty()) {
    if (r.conns.size() == 1) return &r.conns[0];
    out += "Please specify an exact connection ID.\n";
    return nullptr;
  }
  const bool number = arg.find_first_not_of("0123456789") == std::string::npos;
  for (Q2GtvConn& g : r.conns)
    if ((number && g.id == std::atoi(arg.c_str())) || (!number && g.name == arg)) return &g;
  out += "No such connection ID: " + arg + "\n";
  return nullptr;
}

const char* state_word(Q2GtvState s) {
  static const char* const kWords[] = {"disconnected", "connecting", "preparing", "connected",
                                       "resuming",     "waiting",    "reading",   "suspending"};
  return kWords[int(s)];
}

std::string demos_dir(const Q2Relay& r) { return r.cfg.demos.empty() ? "demos" : r.cfg.demos; }

void cmd_connect(Q2Relay& r, const std::vector<std::string>& w, gn::Transport* port,
                 std::string& out) {
  const Opts o = parse_opts(w, "nup", {{"help", 'h'}, {"name", 'n'}, {"user", 'u'}, {"pass", 'p'}});
  const std::string hint = "Try 'mvdconnect --help' for more information.\n";
  if (!o.bad.empty()) {
    out += "Unknown option: " + o.bad + "\n" + hint;
    return;
  }
  if (o.help) {
    out +=
        "Usage: mvdconnect [-hn:u:p:] <address[:port]>\n"
        "Connect to the specified MVD/GTV server.\n"
        "  -h | --help             display this message\n"
        "  -n | --name=<string>    specify channel name as <string>\n"
        "  -u | --user=<string>    specify username as <string>\n"
        "  -p | --pass=<string>    specify password as <string>\n";
    return;
  }
  if (o.rest.empty()) {
    out += "Missing address argument.\n" + hint;
    return;
  }
  const std::string& typed = o.rest[0];
  gn::Address a;
  const bool ported = typed.find(':') != std::string::npos;
  if (!gn::parse_address(ported ? typed : typed + ":27910", a)) {
    out += "Bad server address: " + typed + "\n";
    return;
  }
  for (const Q2GtvConn& g : r.conns)
    if (g.addr == a) {
      out += "[" + g.name + "] <warn> Connection to " + addr_text(a) + " already exists.\n";
      return;
    }
  Q2GtvConn g;
  g.id = r.next_id++;
  g.name = o.given.count('n') ? o.given.at('n') : "net" + std::to_string(g.id);
  if (g.name.size() > 15) g.name.resize(15);
  g.typed = typed;
  g.addr = a;
  if (o.given.count('u')) g.user = o.given.at('u');
  if (o.given.count('p')) g.pass = o.given.at('p');
  if (port != nullptr) r.port = port;
  if (!dial(r, g)) {
    out += "Unable to connect to " + addr_text(a) + "\n";
    return;
  }
  out += "[" + g.name + "] -=- Connecting to " + addr_text(a) + "...\n";
  r.conns.push_back(std::move(g));
}

void cmd_play(Q2Relay& r, const std::vector<std::string>& w, gn::Transport* port, double now,
              std::string& out) {
  const Opts o =
      parse_opts(w, "lnr", {{"help", 'h'}, {"loop", 'l'}, {"name", 'n'}, {"replace", 'r'}});
  const std::string hint = "Try 'mvdplay --help' for more information.\n";
  if (!o.bad.empty()) {
    out += "Unknown option: " + o.bad + "\n" + hint;
    return;
  }
  if (o.help || o.rest.empty()) {
    if (!o.help) out += "Missing filename argument.\n" + hint;
    if (o.help)
      out +=
          "Usage: mvdplay [-hl:n:r:] [/]<filename> [...]\n"
          "Create new MVD channel and begin demo playback.\n"
          "  -h | --help             display this message\n"
          "  -l | --loop=<number>    replay <number> of times (0 means forever)\n"
          "  -n | --name=<string>    specify channel name as <string>\n"
          "  -r | --replace=<chanid> replace <chanid> playlist with new entries\n"
          "Final path is formatted as demos/<filename>.mvd2.\n"
          "Prepend slash to specify raw path.\n";
    return;
  }
  int loops = 1;
  if (o.given.count('l')) {
    loops = std::atoi(o.given.at('l').c_str());
    if (loops < 0) {
      out += "Invalid value for l option.\n" + hint;
      return;
    }
  }
  std::vector<std::string> files;
  for (const std::string& name : o.rest) {
    std::vector<std::string> tries;
    if (name[0] == '/') {
      tries.push_back(name.substr(1));
    } else {
      tries.push_back(demos_dir(r) + "/" + name);
      tries.push_back(demos_dir(r) + "/" + name + ".mvd2");
      tries.push_back(demos_dir(r) + "/" + name + ".mvd2.gz");
    }
    bool found = false;
    for (const std::string& t : tries)
      if (std::FILE* f = std::fopen(t.c_str(), "rb")) {
        std::fclose(f);
        files.push_back(t);
        found = true;
        break;
      }
    if (!found) out += "Couldn't open " + name + ": No such file or directory\n";
  }
  if (files.empty()) return;
  if (o.given.count('r')) {
    std::string why;
    Q2Channel* c = pick_channel(r, o.given.at('r'), why);
    if (c == nullptr || c->conn != -1) {
      out += c == nullptr ? why : "[" + c->name + "] Only demo channels can be replaced.\n";
      return;
    }
    c->files = std::move(files);
    c->file_at = 0;
    c->loops = loops;
    c->looped = 0;
    open_file(r, *c, c->files[0]);
    return;
  }
  if (port != nullptr) r.port = port;
  if (!r.spawned && r.port != nullptr) q2_relay_spawn(r, *r.port, now);
  const int id = r.next_id++;
  std::string name = o.given.count('n') ? o.given.at('n') : "dem" + std::to_string(id);
  if (name.size() > 15) name.resize(15);
  Q2Channel& c = make_channel(r, id, name, -1);
  c.files = std::move(files);
  c.loops = loops;
  c.next = now;
  open_file(r, c, c.files[0]);
}

// `[+-]<timespec|percent>[%]`: S, S.F, M:S or M:S.F (F tenths, as frames).
bool parse_time(const std::string& t, int64_t& frames) {
  int64_t mins = 0, secs = 0, tenths = 0;
  std::string s = t;
  if (const size_t colon = s.find(':'); colon != std::string::npos) {
    mins = std::atoll(s.substr(0, colon).c_str());
    s = s.substr(colon + 1);
  }
  if (const size_t dot = s.find('.'); dot != std::string::npos) {
    tenths = std::atoll(s.substr(dot + 1).c_str());
    s.resize(dot);
  }
  if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) return false;
  secs = std::atoll(s.c_str());
  frames = (mins * 60 + secs) * 10 + tenths;
  return true;
}

void seek_to(Q2Relay& r, Q2Channel& c, int64_t target, std::string& out) {
  if (target < c.level_frame) target = c.level_frame;
  if (target == c.frame) return;
  if (target < c.frame) {
    const Q2Snap* best = nullptr;
    for (const Q2Snap& s : c.snaps)
      if (s.frame <= target && s.frame >= c.level_frame &&
          (best == nullptr || s.frame > best->frame))
        best = &s;
    if (best == nullptr) {
      out += "[" + c.name + "] Couldn't seek backwards without snapshots!\n";
      return;
    }
    client::convert_fork(best->base, c.base);
    c.pos = best->pos;
    c.frame = best->frame;
  }
  std::vector<uint8_t> block;
  while (c.frame < target) {
    const size_t at = c.pos;
    if (!next_block(c, block)) break;
    if (is_gamestate(block)) {
      c.pos = at;  // a new map stops the seek
      break;
    }
    client::convert_feed(c.base, framed(block));
    std::vector<uint8_t> none;
    std::string error;
    client::convert_take(c.base, none, error);
    c.frame++;
  }
  relay::refork(r, c);
}

void cmd_seek(Q2Relay& r, const std::vector<std::string>& w, std::string& out) {
  if (w.size() < 2) {
    out += "Usage: mvdseek [+-]<timespec|percent>[%] [chanid]\n";
    return;
  }
  Q2Channel* c = pick_channel(r, w.size() > 2 ? w[2] : "", out);
  if (c == nullptr) return;
  if (c->conn != -1) {
    out += "[" + c->name + "] Seeking is only supported on demo channels.\n";
    return;
  }
  if (c->rec != nullptr) {
    out += "[" + c->name + "] Seeking is not yet supported during demo recording, sorry.\n";
    return;
  }
  std::string t = w[1];
  if (!t.empty() && t.back() == '%') {
    const double pct = std::atof(t.substr(0, t.size() - 1).c_str());
    if (pct < 0 || pct > 100) {
      out += "[" + c->name + "] Invalid percentage.\n";
      return;
    }
    if (c->data.empty()) {
      out += "[" + c->name + "] Unknown file size, can't seek.\n";
      return;
    }
    const auto want = static_cast<size_t>(double(c->data.size()) * pct / 100);
    // By position: forward from where it is, or back from the level's start.
    int64_t frames = c->frame;
    size_t pos = c->pos;
    std::vector<uint8_t> block;
    while (pos < want && block_at(c->data, pos, block) && !is_gamestate(block)) frames++;
    seek_to(r, *c, want < c->pos ? c->level_frame : frames, out);
    return;
  }
  const char sign = t[0] == '+' || t[0] == '-' ? t[0] : 0;
  int64_t frames = 0;
  if (!parse_time(sign != 0 ? t.substr(1) : t, frames)) {
    out += sign != 0 ? "Invalid relative timespec.\n" : "Invalid absolute timespec.\n";
    return;
  }
  const int64_t target = sign == '+'   ? c->frame + frames
                         : sign == '-' ? c->frame - frames
                                       : c->level_frame + frames;
  seek_to(r, *c, target, out);
}

void cmd_record(Q2Relay& r, const std::vector<std::string>& w, std::string& out) {
  const Opts o = parse_opts(w, "", {{"help", 'h'}, {"compress", 'z'}});
  if (o.help || o.rest.empty()) {
    out +=
        "Usage: mvdrecord [-hz] <filename> [chanid]\n"
        "Begin MVD recording on the specified channel.\n"
        "  -h | --help             display this message\n"
        "  -z | --compress         compress file with gzip\n";
    return;
  }
  Q2Channel* c = pick_channel(r, o.rest.size() > 1 ? o.rest[1] : "", out);
  if (c == nullptr) return;
  if (c->rec != nullptr) {
    out += "[" + c->name + "] Already recording into " + c->rec_path + ".\n";
    return;
  }
  const bool gz = o.given.count('z') != 0;
  std::string path = demos_dir(r) + "/" + o.rest[0];
  if (path.size() < 5 || path.substr(path.size() - 5) != ".mvd2") path += ".mvd2";
  if (gz) path += ".gz";
  sv::Mvd2Level level;
  if (!client::mvd2_reader_level(c->base, level)) {
    out += "[" + c->name + "] This channel is not ready yet.\n";
    return;
  }
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    out += "[" + c->name + "] Couldn't write demo: No such file or directory\n";
    return;
  }
  c->rec = f;
  c->rec_path = path;
  c->rec_gz = gz;
  c->gz = ge::core::DeflateStream{};
  c->gz.wrap = ge::core::DeflateWrap::gzip;
  std::vector<uint8_t> start;
  sv::mvd2_write_magic(start);
  sv::mvd2_write_level(start, level);
  rec_write(*c, start);
  out += "[" + c->name + "] Recording into " + path + "\n";
}

}  // namespace

namespace relay {

Q2Channel* channel(Q2Relay& r, int id) {
  for (auto& c : r.channels)
    if (c->id == id) return c.get();
  return nullptr;
}

void say(Q2Relay& r, const std::string& line) {
  if (r.log) r.log(line);
}

void feed(Q2Relay& r, Q2Channel& c, const std::vector<uint8_t>& block) {
  const std::vector<uint8_t> bytes = framed(block);
  rec_write(c, bytes);
  std::vector<uint8_t> none;
  std::string error;
  client::convert_feed(c.base, bytes);
  if (!client::convert_take(c.base, none, error)) {
    chan_say(r, c, "=X= " + error);
    return;
  }
  sv::Mvd2Level level;
  if (client::mvd2_reader_level(c.base, level) && level.config_strings.size() > 33) {
    std::string m = level.config_strings[33];
    if (m.rfind("maps/", 0) == 0) m = m.substr(5);
    if (m.size() > 4) m.resize(m.size() - 4);
    c.map = m;
  }
  feed_views(r, c, bytes);
}

}  // namespace relay

void q2_relay_frame(Q2Relay& r, int64_t now_us) {
  r.now = static_cast<double>(now_us) / 1e6;
  r.active = r.cfg.suspend_min <= 0 || r.now - r.activity < r.cfg.suspend_min * 60.0;
  for (Q2GtvConn& g : r.conns) run_conn(r, g);
  for (const Q2GtvConn& g : r.conns)
    if (g.retry_at < 0)
      if (Q2Channel* c = relay::channel(r, g.id)) c->conn = -2;  // no connection: it plays out
  r.conns.erase(std::remove_if(r.conns.begin(), r.conns.end(),
                               [](const Q2GtvConn& g) { return g.retry_at < 0; }),
                r.conns.end());
  for (size_t i = 0; i < r.channels.size();) {
    const int id = r.channels[i]->id;
    Q2Channel& c = *r.channels[i];
    if (c.conn == -1)
      play_file(r, c);
    else
      play_stream(r, c);
    if (i < r.channels.size() && r.channels[i]->id == id) i++;
  }
  if (r.spawned) relay::viewers_frame(r, now_us);
}

void q2_relay_farewell(Q2Relay& r, const std::string& text, bool reconnect) {
  for (auto& [key, rec] : r.viewers)
    if (rec.view) q2_tv_farewell(rec.view->face, text, reconnect);
  if (r.room) q2_tv_farewell(r.room->face, text, reconnect);
  q2_relay_stop(r);
}

void q2_relay_stop(Q2Relay& r) {
  for (Q2GtvConn& g : r.conns) gn::tcp_close(g.stream);
  r.conns.clear();
  for (auto& c : r.channels) close_rec(*c);
  relay::close_viewers(r);
  r.channels.clear();
  r.spawned = false;
}

int q2_relay_viewers(const Q2Relay& r) { return static_cast<int>(r.viewers.size()); }

bool q2_relay_command(Q2Relay& r, const std::vector<std::string>& w, const std::string& line,
                      gn::Transport* port, double now, std::string& out) {
  (void)line;
  if (w.empty()) return false;
  const std::string& v = w[0];
  r.now = std::max(r.now, now);
  if (v == "mvdconnect") {
    cmd_connect(r, w, port, out);
  } else if (v == "mvdplay") {
    cmd_play(r, w, port, now, out);
  } else if (v == "mvdspawn") {
    if (port != nullptr) r.port = port;
    if (r.port != nullptr && !r.spawned) q2_relay_spawn(r, *r.port, now);
  } else if (v == "mvdisconnect") {
    const Opts o = parse_opts(w, "", {{"help", 'h'}, {"all", 'a'}});
    if (o.help) {
      out +=
          "Usage: mvdisconnect [-ah] [conn_id]\n"
          "Destroy specified MVD/GTV server connection.\n"
          "  -a | --all       destroy all connections\n"
          "  -h | --help      display this message\n";
    } else if (o.given.count('a')) {
      if (r.conns.empty()) {
        out += "No GTV connections.\n";
      } else {
        for (Q2GtvConn& g : r.conns) {
          gn::tcp_close(g.stream);
          if (Q2Channel* c = relay::channel(r, g.id)) c->conn = -2;
        }
        r.conns.clear();
        out += "Destroyed all GTV connections.\n";
      }
    } else if (Q2GtvConn* g = pick_conn(r, o.rest.empty() ? "" : o.rest[0], out)) {
      out += "[" + g->name + "] =X= Connection destroyed.\n";
      gn::tcp_close(g->stream);
      if (Q2Channel* c = relay::channel(r, g->id)) c->conn = -2;
      const int id = g->id;
      r.conns.erase(std::remove_if(r.conns.begin(), r.conns.end(),
                                   [id](const Q2GtvConn& x) { return x.id == id; }),
                    r.conns.end());
    }
  } else if (v == "mvdkill") {
    if (Q2Channel* c = pick_channel(r, w.size() > 1 ? w[1] : "", out)) {
      const int id = c->id;
      r.conns.erase(std::remove_if(r.conns.begin(), r.conns.end(),
                                   [id](const Q2GtvConn& x) { return x.id == id; }),
                    r.conns.end());
      destroy_channel(r, id, "Channel was killed.");
    }
  } else if (v == "mvdchannels") {
    if (r.channels.empty()) {
      out += "No MVD channels.\n";
      return true;
    }
    const bool recs = w.size() > 1 && !w[1].empty() && w[1][0] == 'r';
    out +=
        recs ? "id name         map      size name\n-- ------------ -------- ---- --------------\n"
             : "id name         map      spc plr stat buf pckt address       \n"
               "-- ------------ -------- --- --- ---- --- ---- --------------\n";
    for (const auto& cp : r.channels) {
      const Q2Channel& c = *cp;
      char row[256];
      if (recs) {
        std::string size = "-";
        if (c.rec != nullptr) size = std::to_string(std::ftell(c.rec) / 1024) + "K";
        std::snprintf(row, sizeof row, "%2d %-12.12s %-8.8s %4s %s\n", c.id, c.name.c_str(),
                      c.map.c_str(), size.c_str(), c.rec != nullptr ? c.rec_path.c_str() : "-");
        out += row;
        continue;
      }
      int spc = 0;
      for (const auto& [key, viewer] : r.viewers) spc += viewer.chan == c.id ? 1 : 0;
      sv::Mvd2Level level;
      int plr = 0;
      if (client::mvd2_reader_level(c.base, level))
        for (const sv::Mvd2Player& p : level.base.players) plr += p.in ? 1 : 0;
      const char* st = c.state == Q2ChanState::dead   ? "DEAD"
                       : c.state == Q2ChanState::wait ? "WAIT"
                                                      : "READ";
      int buf = 0;
      std::string where = "<disconnected>";
      if (c.conn == -1) {
        buf = c.data.empty() ? 0 : static_cast<int>(c.pos * 100 / c.data.size());
        if (c.file_at < c.files.size()) where = c.files[c.file_at];
      } else {
        buf = c.most == 0 ? 0 : static_cast<int>(c.buffered * 100 / c.most);
        for (const Q2GtvConn& g : r.conns)
          if (g.id == c.conn) where = g.typed;
      }
      std::snprintf(row, sizeof row, "%2d %-12.12s %-8.8s %3d %3d %-4s %3d %4d %s\n", c.id,
                    c.name.c_str(), c.map.c_str(), spc, plr, st, buf,
                    static_cast<int>(c.buffer.size()), where.c_str());
      out += row;
    }
  } else if (v == "mvdservers") {
    if (r.conns.empty()) {
      out += "No GTV connections.\n";
      return true;
    }
    out +=
        "id name         state        ratio lastmsg address       \n"
        "-- ------------ ------------ ----- ------- --------------\n";
    for (const Q2GtvConn& g : r.conns) {
      char row[256];
      std::snprintf(row, sizeof row, "%2d %-12.12s %-12.12s %4d%% %7d %s\n", g.id, g.name.c_str(),
                    state_word(g.state), 100, static_cast<int>((r.now - g.heard) * 1000),
                    g.typed.c_str());
      out += row;
    }
  } else if (v == "mvdcontrol") {
    const Opts o = parse_opts(w, "ln", {{"help", 'h'}, {"loop", 'l'}, {"name", 'n'}});
    const std::string hint = "Try 'mvdcontrol --help' for more information.\n";
    if (o.help) {
      out +=
          "Usage: mvdcontrol [-hl:n:] [chanid]\n"
          "Change attributes of existing MVD channel.\n"
          "  -h | --help             display this message\n"
          "  -l | --loop=<number>    replay <number> of times (0 means forever)\n"
          "  -n | --name=<string>    specify channel name as <string>\n";
      return true;
    }
    if (!o.bad.empty() || o.given.empty()) {
      out += (o.bad.empty() ? "At least one option needed.\n" : "Unknown option: " + o.bad + "\n") +
             hint;
      return true;
    }
    if (o.given.count('l') && std::atoi(o.given.at('l').c_str()) < 0) {
      out += "Invalid value for l option.\n" + hint;
      return true;
    }
    Q2Channel* c = pick_channel(r, o.rest.empty() ? "" : o.rest[0], out);
    if (c != nullptr && o.given.count('n')) {
      std::string name = o.given.at('n').substr(0, 15);
      out += "[" + c->name + "] Channel renamed to " + name + ".\n";
      c->name = std::move(name);
      relay::listed(r);
    }
  } else if (v == "mvdpause") {
    if (Q2Channel* c = pick_channel(r, w.size() > 1 ? w[1] : "", out)) {
      if (c->conn != -1)
        out += "[" + c->name + "] Only demo channels can be paused.\n";
      else
        c->paused = !c->paused;
    }
  } else if (v == "mvdskip") {
    std::string why;
    Q2Channel* c = pick_channel(r, w.size() > 1 ? w[1] : "", why);
    if (c == nullptr) {
      out += "Usage: mvdskip [chan_id] [count]\n";
      return true;
    }
    const int count = std::max(1, w.size() > 2 ? std::atoi(w[2].c_str()) : 1);
    if (c->conn != -1) {
      out += "[" + c->name + "] Maps can be skipped only on demo channels.\n";
      return true;
    }
    out += "[" + c->name + "] -=- Skipping map" + (count == 1 ? "...\n" : "s...\n");
    c->skip = count;
  } else if (v == "mvdseek") {
    cmd_seek(r, w, out);
  } else if (v == "mvdrecord" && r.spawned) {
    cmd_record(r, w, out);
  } else if (v == "mvdstop" && r.spawned) {
    std::string why;
    Q2Channel* c = pick_channel(r, w.size() > 1 ? w[1] : "", why);
    if (c == nullptr) {
      out += "Usage: mvdstop [chanid]\n";
    } else if (c->rec == nullptr) {
      out += "[" + c->name + "] Not recording a demo.\n";
    } else {
      close_rec(*c);
      out += "[" + c->name + "] Stopped recording.\n";
    }
  } else {
    return false;
  }
  return true;
}

}  // namespace shooters_edge::net
