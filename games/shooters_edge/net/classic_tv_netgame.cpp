// A recording as the TV for single-player clients (classic_tv_netgame.hpp).
//
// ge:summary The netgame face: the single-player transport, the signon parts and the paced play
// ge:keywords tv netgame single-player protocol-15 transport fragment ack resend keepalive
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_netgame_test.cpp
#include "shooters_edge/net/classic_tv_netgame.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;

// The transport's flags (the header's first word, big-endian, over the 16-bit length).
constexpr uint32_t kData = 0x00010000u, kAck = 0x00020000u, kEom = 0x00080000u;
constexpr uint32_t kUnreliable = 0x00100000u, kCtl = 0x80000000u;
constexpr uint32_t kFlagMask = 0xFFFF0000u, kLenMask = 0x0000FFFFu;
constexpr size_t kHeader = 8, kFragment = 1024, kMostReliable = 8000, kMostDatagram = 1472;
constexpr int kNetVersion = 3;
// The game's name a connect asks with, a wire constant spelled by its letters.
constexpr char kGameName[] = {'Q', 'U', 'A', 'K', 'E', 0};
constexpr uint8_t kReqConnect = 0x01, kRepAccept = 0x81, kRepReject = 0x82;
constexpr int64_t kResendUs = 1000000, kKeepaliveUs = 5000000, kReacceptUs = 2000000;
constexpr int64_t kMostSilenceUs = 1000000;  // a silence in the recording cut to a second

// The server messages the walk looks at, and those a frame sends unreliable.
enum : uint8_t {
  kNop = 1,
  kDisconnect = 2,
  kSound = 6,
  kTime = 7,
  kStuffText = 9,
  kSetAngle = 10,
  kServerInfo = 11,
  kClientData = 15,
  kParticle = 18,
  kDamage = 19,
  kTempEntity = 23,
  kSignonNum = 25,
};
// The client's.
enum : uint8_t { kClNop = 1, kClDisconnect = 2, kClMove = 3, kClStringCmd = 4 };

bool frame_message(uint8_t id) {
  return (id & 0x80) != 0 || id == kTime || id == kClientData || id == kSound || id == kParticle ||
         id == kTempEntity || id == kSetAngle || id == kDamage;
}

void put_be32(std::vector<uint8_t>& o, uint32_t v) {
  o.push_back(static_cast<uint8_t>(v >> 24));
  o.push_back(static_cast<uint8_t>(v >> 16));
  o.push_back(static_cast<uint8_t>(v >> 8));
  o.push_back(static_cast<uint8_t>(v));
}
uint32_t be32(const uint8_t* p) {
  return uint32_t{p[0]} << 24 | uint32_t{p[1]} << 16 | uint32_t{p[2]} << 8 | uint32_t{p[3]};
}
float le_f32(const uint8_t* p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) v |= uint32_t{p[i]} << (8 * i);
  return std::bit_cast<float>(v);
}

void send(NetgameTv& tv, NetgameViewer& v, uint32_t flags, uint32_t seq, const uint8_t* payload,
          size_t n, int64_t now) {
  std::vector<uint8_t> d;
  d.reserve(kHeader + n);
  put_be32(d, flags | static_cast<uint32_t>(kHeader + n));
  put_be32(d, seq);
  d.insert(d.end(), payload, payload + n);
  gn::transport_send(*tv.t, v.addr, d.data(), d.size(), now);
  v.sent_us = now;
}

void send_control(NetgameTv& tv, const gn::Address& to, const std::vector<uint8_t>& body,
                  int64_t now) {
  std::vector<uint8_t> d;
  put_be32(d, kCtl | static_cast<uint32_t>(4 + body.size()));
  d.insert(d.end(), body.begin(), body.end());
  gn::transport_send(*tv.t, to, d.data(), d.size(), now);
}

// A message for the viewer's reliable queue: joined to the last one waiting when it fits, or on
// its own (`own`: a level's first part, which nothing may follow inside its message).
void queue_reliable(NetgameViewer& v, std::span<const uint8_t> b, bool own) {
  if (b.empty()) return;
  if (!own && !v.sealed && !v.queue.empty() && v.queue.back().size() + b.size() <= kMostReliable) {
    v.queue.back().insert(v.queue.back().end(), b.begin(), b.end());
    return;
  }
  v.queue.emplace_back(b.begin(), b.end());
  v.sealed = own;
}

void send_fragment(NetgameTv& tv, NetgameViewer& v, int64_t now) {
  v.frag = std::min(kFragment, v.flight.size() - v.flight_at);
  const bool last = v.flight_at + v.frag >= v.flight.size();
  send(tv, v, kData | (last ? kEom : 0u), v.send_seq, v.flight.data() + v.flight_at, v.frag, now);
  v.waiting_ack = true;
  v.frag_sent_us = now;
}

void pump_reliable(NetgameTv& tv, NetgameViewer& v, int64_t now) {
  if (v.waiting_ack) {
    if (now - v.frag_sent_us >= kResendUs) send_fragment(tv, v, now);
    return;
  }
  if (v.flight_at >= v.flight.size()) {
    if (v.queue.empty()) return;
    v.flight = std::move(v.queue.front());
    v.queue.pop_front();
    if (v.queue.empty()) v.sealed = false;
    v.flight_at = 0;
  }
  send_fragment(tv, v, now);
}

// The messages of blocks [b0, b1] from offset `from` in b0 to `to` in b1, each handed to `f`.
// Stuffed text is left out: a recording's commands are its player's, never a viewer's.
template <class F>
void each_message(const NetgameTv& tv, size_t b0, uint32_t from, size_t b1, uint32_t to, F f) {
  for (size_t b = b0; b <= b1 && b < tv.blocks.size(); b++) {
    const NetgameBlock& blk = tv.blocks[b];
    const uint8_t* base = tv.bytes.data() + blk.at;
    for (size_t i = 0; i < blk.marks.size(); i++) {
      const uint32_t s = blk.marks[i];
      const uint32_t e = i + 1 < blk.marks.size() ? blk.marks[i + 1] : blk.size;
      if (b == b0 && s < from) continue;
      if (b == b1 && s >= to) break;
      if (base[s] == kStuffText || base[s] == kSignonNum) continue;
      f(std::span<const uint8_t>(base + s, e - s));
    }
  }
}

// A level's signon part (0: to `signonnum` 1, the server info with it; 1: to 2; 2: to 3).
void queue_part(NetgameTv& tv, NetgameViewer& v, int part) {
  if (tv.levels.empty()) return;
  const NetgameLevel& L = tv.levels[static_cast<size_t>(tv.level)];
  const size_t b0 = part == 0 ? L.first : L.signon[static_cast<size_t>(part - 1)];
  const uint32_t from = part == 0 ? 0 : L.cut[static_cast<size_t>(part - 1)];
  if (part == 0) {  // one message: the client takes nothing else while it loads
    std::vector<uint8_t> all;
    each_message(tv, b0, from, L.signon[0], L.cut[0],
                 [&](std::span<const uint8_t> m) { all.insert(all.end(), m.begin(), m.end()); });
    all.push_back(kSignonNum);
    all.push_back(1);
    queue_reliable(v, all, true);
    return;
  }
  each_message(tv, b0, from, L.signon[static_cast<size_t>(part)], L.cut[static_cast<size_t>(part)],
               [&](std::span<const uint8_t> m) { queue_reliable(v, m, false); });
  const std::array<uint8_t, 2> stage{kSignonNum, static_cast<uint8_t>(part + 1)};
  queue_reliable(v, stage, false);
}

// The viewer sent back to the current level's start: "reconnect", then its first part.
void restart(NetgameTv& tv, NetgameViewer& v, bool reconnect) {
  if (reconnect) {
    static constexpr uint8_t kReconnect[] = {kStuffText, 'r', 'e', 'c', 'o',  'n',
                                             'n',        'e', 'c', 't', '\n', 0};
    queue_reliable(v, kReconnect, false);
  }
  queue_part(tv, v, 0);
  v.epoch = tv.epoch;
  v.stage = NetgameStage::loading;
}

void command(NetgameTv& tv, NetgameViewer& v, std::string_view c) {
  while (!c.empty() && c.front() == ' ') c.remove_prefix(1);
  const std::string_view word = c.substr(0, c.find_first_of(" \n"));
  if (word == "prespawn") {
    if (v.epoch != tv.epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::loading) {
      queue_part(tv, v, 1);
      v.stage = NetgameStage::prespawned;
    }
  } else if (word == "spawn") {
    if (v.epoch != tv.epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::prespawned) {
      queue_part(tv, v, 2);
      v.stage = NetgameStage::spawned;
    }
  } else if (word == "begin") {
    if (v.epoch != tv.epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::spawned) {
      v.stage = NetgameStage::playing;
    }
  } else if (word == "name") {
    std::string n(c.substr(word.size()));
    std::erase_if(n, [](char ch) { return ch == '"' || ch == '\n'; });
    while (!n.empty() && n.front() == ' ') n.erase(n.begin());
    v.name = n.substr(0, 31);
  }
}

// A client's message stream (a reliable message or an unreliable datagram's payload).
void client_messages(NetgameTv& tv, NetgameViewer& v, std::span<const uint8_t> m) {
  size_t at = 0;
  while (at < m.size() && v.stage != NetgameStage::free) {
    const uint8_t id = m[at++];
    if (id == kClNop) continue;
    if (id == kClDisconnect) {
      v = NetgameViewer{};
      return;
    }
    if (id == kClMove) {
      at += 15;  // the time, three angles, three speeds, the buttons and the impulse
      continue;
    }
    if (id != kClStringCmd) return;  // a message this face does not know ends the stream
    const size_t s = at;
    while (at < m.size() && m[at] != 0) at++;
    command(tv, v, std::string_view(reinterpret_cast<const char*>(m.data() + s), at - s));
    at++;
  }
}

NetgameViewer* find_viewer(NetgameTv& tv, const gn::Address& a) {
  for (NetgameViewer& v : tv.viewers)
    if (v.stage != NetgameStage::free && v.addr == a) return &v;
  return nullptr;
}

void control(NetgameTv& tv, const uint8_t* d, size_t n, const gn::Address& from, int64_t now) {
  const uint32_t w = be32(d);
  if ((w & kFlagMask) != kCtl || (w & kLenMask) != n || n < 5) return;
  if (d[4] != kReqConnect) return;  // the server and rule queries are not answered
  const size_t s = 5;
  size_t e = s;
  while (e < n && d[e] != 0) e++;
  if (e >= n || std::string_view(reinterpret_cast<const char*>(d + s), e - s) != kGameName) return;
  if (e + 1 >= n || d[e + 1] != kNetVersion) {
    const std::string why = "Incompatible version.\n";
    std::vector<uint8_t> body{kRepReject};
    body.insert(body.end(), why.begin(), why.end());
    body.push_back(0);
    send_control(tv, from, body, now);
    return;
  }
  NetgameViewer* v = find_viewer(tv, from);
  if (v == nullptr || now - v->accepted_us >= kReacceptUs) {
    if (v == nullptr) {
      for (NetgameViewer& f : tv.viewers)
        if (f.stage == NetgameStage::free) {
          v = &f;
          break;
        }
    }
    if (v == nullptr) {
      const std::string why = "This TV is full.\n";
      std::vector<uint8_t> body{kRepReject};
      body.insert(body.end(), why.begin(), why.end());
      body.push_back(0);
      send_control(tv, from, body, now);
      return;
    }
    *v = NetgameViewer{};
    v->addr = from;
    v->accepted_us = now;
    v->heard_us = now;
    restart(tv, *v, false);
  }
  const uint32_t port = tv.t->local.port;  // this same port: one socket, one hole in a NAT
  send_control(tv, from,
               {kRepAccept, static_cast<uint8_t>(port), static_cast<uint8_t>(port >> 8), 0, 0},
               now);
}

void datagram(NetgameTv& tv, const uint8_t* d, size_t n, const gn::Address& from, int64_t now) {
  if (n < kHeader) return;
  const uint32_t w = be32(d);
  if ((w & kCtl) != 0) {
    control(tv, d, n, from, now);
    return;
  }
  NetgameViewer* v = find_viewer(tv, from);
  if (v == nullptr) return;
  const size_t len = w & kLenMask;
  if (len < kHeader || len > n) return;
  const uint32_t seq = be32(d + 4);
  const std::span<const uint8_t> payload(d + kHeader, len - kHeader);
  v->heard_us = now;
  if ((w & kUnreliable) != 0) {
    if (seq < v->unrel_in) return;
    v->unrel_in = seq + 1;
    client_messages(tv, *v, payload);
  } else if ((w & kAck) != 0) {
    if (v->waiting_ack && seq == v->send_seq) {
      v->waiting_ack = false;
      v->flight_at += v->frag;
      v->send_seq++;
      if (v->flight_at >= v->flight.size()) v->flight.clear(), v->flight_at = 0;
      pump_reliable(tv, *v, now);
    }
  } else if ((w & kData) != 0) {
    send(tv, *v, kAck, seq, nullptr, 0, now);
    if (seq != v->recv_seq) return;
    v->recv_seq++;
    v->partial.insert(v->partial.end(), payload.begin(), payload.end());
    if ((w & kEom) != 0) {
      const std::vector<uint8_t> msg = std::move(v->partial);
      v->partial.clear();
      client_messages(tv, *v, msg);
    }
  }
}

// One block played to every seated viewer: its frame messages unreliable (with the recorded view
// as a `setangle`), the rest reliable.
void play_block(NetgameTv& tv, const NetgameBlock& blk, int64_t now) {
  const uint8_t* base = tv.bytes.data() + blk.at;
  std::vector<std::vector<uint8_t>> grams(1);
  std::vector<uint8_t> rel;
  auto unrel = [&](std::span<const uint8_t> m) {
    if (m.size() > kFragment) {
      rel.insert(rel.end(), m.begin(), m.end());
      return;
    }
    if (grams.back().size() + m.size() > kFragment) grams.emplace_back();
    grams.back().insert(grams.back().end(), m.begin(), m.end());
  };
  for (size_t i = 0; i < blk.marks.size(); i++) {
    const uint32_t s = blk.marks[i];
    const uint32_t e = i + 1 < blk.marks.size() ? blk.marks[i + 1] : blk.size;
    const uint8_t id = base[s];
    if (id == kNop || id == kDisconnect || id == kStuffText || id == kServerInfo ||
        id == kSignonNum)
      continue;
    const std::span<const uint8_t> m(base + s, e - s);
    if (frame_message(id)) {
      unrel(m);
    } else {
      rel.insert(rel.end(), m.begin(), m.end());
    }
  }
  if (!grams.front().empty()) {  // the recorder's view
    std::array<uint8_t, 4> sa{kSetAngle};
    for (size_t k = 0; k < 3; k++)
      sa[k + 1] = static_cast<uint8_t>(static_cast<int32_t>(blk.angles[k] * 256.0f / 360.0f) & 255);
    unrel(sa);
  }
  for (NetgameViewer& v : tv.viewers) {
    if (v.stage != NetgameStage::playing || v.epoch != tv.epoch) continue;
    for (const std::vector<uint8_t>& g : grams)
      if (!g.empty()) send(tv, v, kUnreliable, v.unrel_seq++, g.data(), g.size(), now);
    if (!rel.empty()) queue_reliable(v, rel, false);
  }
}

// The play clock on to `now`: due blocks played, a level's end turned into the next level's
// start (or the first's, round again).
void play(NetgameTv& tv, int64_t now) {
  if (tv.ended || tv.levels.empty()) return;
  if (tv.start_us < 0) {
    tv.start_us = now;
    tv.next = tv.levels[0].play;
  }
  for (int guard = 0; guard < 4096; guard++) {
    const NetgameLevel& L = tv.levels[static_cast<size_t>(tv.level)];
    if (tv.next >= L.end) {
      size_t nl = static_cast<size_t>(tv.level) + 1;
      if (nl >= tv.levels.size()) {
        if (!tv.loop) {
          tv.ended = true;
          return;
        }
        nl = 0;
        tv.rounds++;
      }
      tv.level = static_cast<int>(nl);
      tv.epoch++;
      tv.next = tv.levels[nl].play;
      tv.start_us = now;
      for (NetgameViewer& v : tv.viewers)  // a loading viewer is turned round at its "prespawn"
        if (v.stage == NetgameStage::prespawned || v.stage == NetgameStage::spawned ||
            v.stage == NetgameStage::playing)
          restart(tv, v, true);
      continue;
    }
    const NetgameBlock& blk = tv.blocks[tv.next];
    if (tv.start_us + blk.due_us > now) return;
    play_block(tv, blk, now);
    tv.next++;
    tv.frames++;
  }
}

}  // namespace

bool udp_tv_netgame_open(NetgameTv& tv, std::vector<uint8_t> bytes, bool loop, std::string& error) {
  cl::Demo demo;
  if (!cl::open_demo(std::move(bytes), demo, error)) return false;
  if (demo.state.format == cl::DemoFormat::sequel) {
    error = "a recording of the sequel, which the single-player client does not play";
    return false;
  }
  if (demo.state.format != cl::DemoFormat::single) {
    cl::ConvertOptions o;
    o.no_end = true;
    cl::ConvertNotes notes;
    std::vector<uint8_t> out;
    if (!cl::convert_demo(std::move(demo.bytes), cl::DemoFormat::single, o, out, notes, error) ||
        !cl::open_demo(std::move(out), demo, error))
      return false;
  }
  tv = NetgameTv{};
  tv.loop = loop;
  tv.bytes = std::move(demo.bytes);
  // The blocks after the track line: a length, the view's three angles, the messages.
  size_t at = 0;
  while (at < tv.bytes.size() && at < 16 && tv.bytes[at] != '\n') at++;
  at++;
  cl::Demo walk = demo;
  NetgameLevel cur;
  bool has_info = false;
  std::array<bool, 3> seen{};  // the level's signon numbers met
  int64_t clock = 0;
  float last_time = -1;
  auto close_level = [&](size_t end) {
    if (has_info && (seen[0] || seen[1] || seen[2])) {
      // A stage the recording does not mark ends where the next marked one does (or the last
      // marked one, after it): its part is empty, and the face's own `signonnum` still asks.
      for (size_t k = 0; k < 3; k++) {
        if (seen[k]) continue;
        size_t j = k;
        while (j < 3 && !seen[j]) j++;
        if (j == 3) {
          j = k;
          while (!seen[j]) j--;
        }
        cur.signon[k] = cur.signon[j];
        cur.cut[k] = cur.cut[j];
      }
      cur.end = end;
      cur.play = cur.signon[2] + 1;
      if (cur.play > end) cur.play = end;
      tv.levels.push_back(cur);
    }
  };
  while (at + 16 <= tv.bytes.size()) {
    uint32_t n = 0;
    std::memcpy(&n, tv.bytes.data() + at, 4);
    if (n > (1u << 16) || at + 16 + n > tv.bytes.size()) break;
    NetgameBlock blk;
    for (size_t k = 0; k < 3; k++) blk.angles[k] = le_f32(tv.bytes.data() + at + 4 + 4 * k);
    blk.at = static_cast<uint32_t>(at + 16);
    blk.size = n;
    cl::DemoMsg m;
    m.b = std::span<const uint8_t>(tv.bytes.data() + blk.at, n);
    m.marks = &blk.marks;
    std::string why;
    if (!cl::read_single_messages(walk, m, why)) {
      error = "block " + std::to_string(tv.blocks.size()) + " does not read: " + why;
      return false;
    }
    const size_t bi = tv.blocks.size();
    const uint8_t* base = tv.bytes.data() + blk.at;
    for (size_t i = 0; i < blk.marks.size(); i++) {
      const uint32_t s = blk.marks[i];
      const uint32_t e = i + 1 < blk.marks.size() ? blk.marks[i + 1] : n;
      const uint8_t id = base[s];
      if (id == kServerInfo) {
        if (has_info) close_level(bi);
        cur = NetgameLevel{};
        cur.first = bi;
        has_info = true;
        seen = {};
        clock = 0;
        last_time = -1;
      } else if (id == kSignonNum && s + 1 < n && has_info) {
        const int k = base[s + 1];
        if (k >= 1 && k <= 3) {
          cur.signon[static_cast<size_t>(k - 1)] = bi;
          cur.cut[static_cast<size_t>(k - 1)] = e;
          seen[static_cast<size_t>(k - 1)] = true;
        }
      } else if (id == kTime && s + 5 <= n) {
        const float t = le_f32(base + s + 1);
        if (last_time >= 0) {
          const auto dt = static_cast<int64_t>((t - last_time) * 1e6f);
          clock += std::clamp<int64_t>(dt, 0, kMostSilenceUs);
        }
        last_time = t;
      }
    }
    blk.due_us = clock;
    blk.level = static_cast<int>(tv.levels.size());
    tv.blocks.push_back(std::move(blk));
    at += 16 + n;
  }
  close_level(tv.blocks.size());
  if (tv.levels.empty()) {
    error = "no level in the recording has a whole signon (it starts partway into a level)";
    return false;
  }
  return true;
}

void udp_tv_netgame_start(NetgameTv& tv, gn::Transport& t, const NetgameTvConfig& cfg) {
  tv.t = &t;
  tv.cfg = cfg;
  tv.viewers.assign(static_cast<size_t>(std::max(1, cfg.max_viewers)), NetgameViewer{});
}

void udp_tv_netgame_frame(NetgameTv& tv, int64_t now_us) {
  if (tv.t == nullptr) return;
  std::array<uint8_t, kMostDatagram> buf{};
  gn::Address from;
  for (int i = 0; i < 1024; i++) {
    const int n = gn::transport_receive(*tv.t, buf.data(), buf.size(), from, now_us);
    if (n < 0) break;
    datagram(tv, buf.data(), static_cast<size_t>(n), from, now_us);
  }
  play(tv, now_us);
  for (NetgameViewer& v : tv.viewers) {
    if (v.stage == NetgameStage::free) continue;
    if (now_us - v.heard_us > static_cast<int64_t>(tv.cfg.drop_s * 1e6)) {
      v = NetgameViewer{};
      continue;
    }
    pump_reliable(tv, v, now_us);
    if (v.stage != NetgameStage::playing && now_us - v.sent_us >= kKeepaliveUs) {
      const uint8_t nop = kNop;
      send(tv, v, kUnreliable, v.unrel_seq++, &nop, 1, now_us);
    }
  }
}

int udp_tv_netgame_viewers(const NetgameTv& tv) {
  return static_cast<int>(std::count_if(tv.viewers.begin(), tv.viewers.end(), [](const auto& v) {
    return v.stage != NetgameStage::free;
  }));
}

void udp_tv_netgame_stop(NetgameTv& tv, int64_t now_us) {
  if (tv.t == nullptr) return;
  for (NetgameViewer& v : tv.viewers) {
    if (v.stage == NetgameStage::free) continue;
    const uint8_t bye = kDisconnect;
    send(tv, v, kData | kEom, v.send_seq, &bye, 1, now_us);
    v = NetgameViewer{};
  }
}

}  // namespace shooters_edge::net
