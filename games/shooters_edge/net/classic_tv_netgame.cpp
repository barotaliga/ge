// A recording as the TV for single-player clients (classic_tv_netgame.hpp).
//
// ge:summary The netgame face: the single-player transport, the signon parts and the paced play
// ge:keywords tv netgame single-player protocol-15 transport fragment ack resend keepalive
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_netgame_test.cpp
#include "shooters_edge/net/classic_tv_netgame.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <map>
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
constexpr int kFramesForPlayers = 200;       // the frames read for the players' names

// The server messages the walk looks at, and those a frame sends unreliable.
enum : uint8_t {
  kNop = 1,
  kDisconnect = 2,
  kUpdateStat = 3,
  kSound = 6,
  kTime = 7,
  kPrint = 8,
  kStuffText = 9,
  kSetAngle = 10,
  kServerInfo = 11,
  kLightStyle = 12,
  kUpdateName = 13,
  kUpdateFrags = 14,
  kClientData = 15,
  kUpdateColors = 17,
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

NetgameFeed& feed_of(NetgameTv& tv, const NetgameViewer& v) {
  return tv.feeds[v.feed < tv.feeds.size() ? v.feed : 0];
}

// The messages of blocks [b0, b1] from offset `from` in b0 to `to` in b1, each handed to `f`.
// Stuffed text is left out: a recording's commands are its player's, never a viewer's.
template <class F>
void each_message(const NetgameFeed& fd, size_t b0, uint32_t from, size_t b1, uint32_t to, F f) {
  for (size_t b = b0; b <= b1 && b < fd.blocks.size(); b++) {
    const NetgameBlock& blk = fd.blocks[b];
    const uint8_t* base = fd.bytes.data() + blk.at;
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
  const NetgameFeed& fd = feed_of(tv, v);
  if (fd.levels.empty()) return;
  const NetgameLevel& L = fd.levels[static_cast<size_t>(fd.level)];
  const size_t b0 = part == 0 ? L.first : L.signon[static_cast<size_t>(part - 1)];
  const uint32_t from = part == 0 ? 0 : L.cut[static_cast<size_t>(part - 1)];
  if (part == 0) {  // one message: the client takes nothing else while it loads
    std::vector<uint8_t> all;
    each_message(fd, b0, from, L.signon[0], L.cut[0],
                 [&](std::span<const uint8_t> m) { all.insert(all.end(), m.begin(), m.end()); });
    all.push_back(kSignonNum);
    all.push_back(1);
    queue_reliable(v, all, true);
    return;
  }
  each_message(fd, b0, from, L.signon[static_cast<size_t>(part)], L.cut[static_cast<size_t>(part)],
               [&](std::span<const uint8_t> m) { queue_reliable(v, m, false); });
  const std::array<uint8_t, 2> stage{kSignonNum, static_cast<uint8_t>(part + 1)};
  queue_reliable(v, stage, false);
}

// The viewer sent back to its feed's current level start: "reconnect", then its first part.
void restart(NetgameTv& tv, NetgameViewer& v, bool reconnect) {
  if (reconnect) {
    static constexpr uint8_t kReconnect[] = {kStuffText, 'r', 'e', 'c', 'o',  'n',
                                             'n',        'e', 'c', 't', '\n', 0};
    queue_reliable(v, kReconnect, false);
  }
  queue_part(tv, v, 0);
  v.epoch = feed_of(tv, v).epoch;
  v.stage = NetgameStage::loading;
}

// What a viewer seated partway into a level has missed of the level's state since its signon:
// each player's name, frags and colours, each light style and each stat as the blocks played so
// far last set them, queued reliable (the frames themselves carry every entity in full).
void queue_catch_up(NetgameTv& tv, NetgameViewer& v) {
  const NetgameFeed& fd = feed_of(tv, v);
  const NetgameLevel& L = fd.levels[static_cast<size_t>(fd.level)];
  if (fd.next <= L.play) return;
  std::map<std::pair<uint8_t, uint8_t>, std::span<const uint8_t>> last;
  for (size_t b = L.play; b < fd.next && b < fd.blocks.size(); b++) {
    const NetgameBlock& blk = fd.blocks[b];
    const uint8_t* base = fd.bytes.data() + blk.at;
    for (size_t i = 0; i < blk.marks.size(); i++) {
      const uint32_t s = blk.marks[i];
      const uint32_t e = i + 1 < blk.marks.size() ? blk.marks[i + 1] : blk.size;
      const uint8_t id = base[s];
      if ((id == kUpdateStat || id == kLightStyle || id == kUpdateName || id == kUpdateFrags ||
           id == kUpdateColors) &&
          e > s + 1)
        last[{id, base[s + 1]}] = std::span<const uint8_t>(base + s, e - s);
    }
  }
  for (const auto& [key, m] : last) queue_reliable(v, m, false);
}

// A line on the viewer's console, reliable.
void print(NetgameViewer& v, const std::string& line) {
  std::vector<uint8_t> m{kPrint};
  m.insert(m.end(), line.begin(), line.end());
  m.push_back(0);
  queue_reliable(v, m, false);
}

// The player the viewer follows now (an index in `players`), or -1 (a recording of one view).
int following(const NetgameTv& tv, const NetgameViewer& v) {
  const int slot = v.feed < tv.feeds.size() ? tv.feeds[v.feed].slot : -1;
  for (size_t i = 0; i < tv.players.size(); i++)
    if (tv.players[i].slot == slot) return static_cast<int>(i);
  return tv.players.empty() ? -1 : 0;
}

// What a seated viewer of several players is told: who it follows and how to change it.
void say_following(const NetgameTv& tv, NetgameViewer& v) {
  const int p = following(tv, v);
  if (p < 0 || tv.players.size() < 2) return;
  print(v, "Following " + tv.players[static_cast<size_t>(p)].name + " (" + std::to_string(p + 1) +
               " of " + std::to_string(tv.players.size()) +
               "). Fire for the next player, jump for the one before, 1 to " +
               std::to_string(std::min<size_t>(8, tv.players.size())) + " to pick one.\n");
}

// The feed that rides player `p`'s view, converted the first time it is asked for and set on the
// first feed's clock and level; its index, or -1 with `error`.
// -2 when it is being converted on its own thread (NetgameTvConfig::convert_in_background).
int feed_for(NetgameTv& tv, size_t p, std::string& error);

void follow(NetgameTv& tv, NetgameViewer& v, int p) {
  if (tv.players.size() < 2) return;
  const int n = static_cast<int>(tv.players.size());
  p = ((p % n) + n) % n;
  std::string error;
  const int f = feed_for(tv, static_cast<size_t>(p), error);
  if (f == -2) {
    v.want_slot = tv.players[static_cast<size_t>(p)].slot;
    print(v, "Getting " + tv.players[static_cast<size_t>(p)].name + "'s view ready...\n");
    return;
  }
  v.want_slot = -1;
  if (f < 0) {
    print(v, "Can't follow " + tv.players[static_cast<size_t>(p)].name + " in this recording.\n");
    return;
  }
  if (static_cast<size_t>(f) == v.feed) return;
  v.feed = static_cast<size_t>(f);
  restart(tv, v, true);
}

void command(NetgameTv& tv, NetgameViewer& v, std::string_view c) {
  while (!c.empty() && c.front() == ' ') c.remove_prefix(1);
  const std::string_view word = c.substr(0, c.find_first_of(" \n"));
  const uint64_t epoch = feed_of(tv, v).epoch;
  if (word == "prespawn") {
    if (v.epoch != epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::loading) {
      queue_part(tv, v, 1);
      v.stage = NetgameStage::prespawned;
    }
  } else if (word == "spawn") {
    if (v.epoch != epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::prespawned) {
      queue_part(tv, v, 2);
      v.stage = NetgameStage::spawned;
    }
  } else if (word == "begin") {
    if (v.epoch != epoch) {
      restart(tv, v, true);
    } else if (v.stage == NetgameStage::spawned) {
      v.stage = NetgameStage::playing;
      queue_catch_up(tv, v);
      say_following(tv, v);
    }
  } else if (word == "name") {
    std::string n(c.substr(word.size()));
    std::erase_if(n, [](char ch) { return ch == '"' || ch == '\n'; });
    while (!n.empty() && n.front() == ' ') n.erase(n.begin());
    v.name = n.substr(0, 31);
  } else if (word == "follow" || word == "track") {
    std::string who(c.substr(word.size()));
    std::erase_if(who, [](char ch) { return ch == '"' || ch == '\n'; });
    while (!who.empty() && who.front() == ' ') who.erase(who.begin());
    while (!who.empty() && who.back() == ' ') who.pop_back();
    if (v.stage != NetgameStage::playing || tv.players.size() < 2) return;
    int pick = -1;
    if (!who.empty() &&
        std::all_of(who.begin(), who.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
      pick = std::atoi(who.c_str()) - 1;
    } else {
      // A name, or its start, in any case and either of the charset's colours.
      const auto plain = [](std::string t) {
        for (char& ch : t) {
          ch = static_cast<char>(static_cast<unsigned char>(ch) & 0x7F);
          if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        return t;
      };
      const std::string want = plain(who);
      for (size_t i = 0; i < tv.players.size() && pick < 0; i++)
        if (!want.empty() && plain(tv.players[i].name).rfind(want, 0) == 0)
          pick = static_cast<int>(i);
    }
    if (pick < 0 || pick >= static_cast<int>(tv.players.size())) {
      std::string names;
      for (size_t i = 0; i < tv.players.size(); i++)
        names += (i ? ", " : "") + std::to_string(i + 1) + " " + tv.players[i].name;
      print(v, "No player " + who + " here. The players: " + names + ".\n");
      return;
    }
    follow(tv, v, pick);
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
      // The time, three angles, three speeds, then the buttons and the impulse.
      if (at + 15 > m.size()) return;
      const uint8_t buttons = m[at + 13], impulse = m[at + 14];
      at += 15;
      const uint8_t pressed = buttons & ~v.buttons;
      v.buttons = buttons;
      if (v.stage != NetgameStage::playing) continue;
      const int now_following = following(tv, v);
      if ((pressed & 1) != 0) {
        follow(tv, v, now_following + 1);  // fire: the next player
      } else if ((pressed & 2) != 0) {
        follow(tv, v, now_following - 1);  // jump: the one before
      } else if (impulse >= 1 && impulse <= 8 && impulse <= tv.players.size()) {
        follow(tv, v, impulse - 1);  // a weapon key: that player
      }
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

// One block of feed `fi` played to its seated viewers: its frame messages unreliable (with the
// recorded view as a `setangle`), the rest reliable.
void play_block(NetgameTv& tv, size_t fi, const NetgameBlock& blk, int64_t now) {
  const NetgameFeed& fd = tv.feeds[fi];
  bool watched = false;
  for (const NetgameViewer& v : tv.viewers)
    watched = watched || (v.stage == NetgameStage::playing && v.feed == fi && v.epoch == fd.epoch);
  if (!watched) return;
  const uint8_t* base = fd.bytes.data() + blk.at;
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
    if (v.stage != NetgameStage::playing || v.feed != fi || v.epoch != fd.epoch) continue;
    for (const std::vector<uint8_t>& g : grams)
      if (!g.empty()) send(tv, v, kUnreliable, v.unrel_seq++, g.data(), g.size(), now);
    if (!rel.empty()) queue_reliable(v, rel, false);
  }
}

// Feed `f` set on the first feed's clock: its level, its start, and the next block the one due
// after `now` (the blocks before it are passed over, never sent).
void sync_feed(NetgameTv& tv, NetgameFeed& f, int64_t now) {
  const NetgameFeed& m = tv.feeds[0];
  f.level = std::min(m.level, static_cast<int>(f.levels.size()) - 1);
  f.start_us = m.start_us;
  f.epoch++;
  const NetgameLevel& L = f.levels[static_cast<size_t>(f.level)];
  f.next = L.play;
  while (f.next < L.end && f.start_us + f.blocks[f.next].due_us <= now) f.next++;
}

// The play clock on to `now`: the first feed's due blocks played, a level's end turned into the
// next level's start (or the first's, round again) for every feed; the other feeds' due blocks on
// the same clock.
void play(NetgameTv& tv, int64_t now) {
  if (tv.ended || tv.feeds.empty() || tv.feeds[0].levels.empty()) return;
  NetgameFeed& m = tv.feeds[0];
  if (m.start_us < 0) {
    m.start_us = now;
    m.next = m.levels[0].play;
    for (size_t i = 1; i < tv.feeds.size(); i++) sync_feed(tv, tv.feeds[i], now);
  }
  for (int guard = 0; guard < 4096; guard++) {
    const NetgameLevel& L = m.levels[static_cast<size_t>(m.level)];
    if (m.next >= L.end) {
      size_t nl = static_cast<size_t>(m.level) + 1;
      if (nl >= m.levels.size()) {
        if (!tv.loop) {
          tv.ended = true;
          return;
        }
        nl = 0;
        tv.rounds++;
      }
      m.level = static_cast<int>(nl);
      m.epoch++;
      m.next = m.levels[nl].play;
      m.start_us = now;
      for (size_t i = 1; i < tv.feeds.size(); i++) sync_feed(tv, tv.feeds[i], now);
      for (NetgameViewer& v : tv.viewers)  // a loading viewer is turned round at its "prespawn"
        if (v.stage == NetgameStage::prespawned || v.stage == NetgameStage::spawned ||
            v.stage == NetgameStage::playing)
          restart(tv, v, true);
      continue;
    }
    const NetgameBlock& blk = m.blocks[m.next];
    if (m.start_us + blk.due_us > now) break;
    play_block(tv, 0, blk, now);
    m.next++;
    tv.frames++;
  }
  for (size_t i = 1; i < tv.feeds.size(); i++) {  // the other views keep the first one's level
    NetgameFeed& f = tv.feeds[i];
    const NetgameLevel& L = f.levels[static_cast<size_t>(f.level)];
    while (f.next < L.end && f.start_us + f.blocks[f.next].due_us <= now)
      play_block(tv, i, f.blocks[f.next++], now);
  }
}

// `bytes`, a single-player recording, cut into `f`'s blocks and levels.
bool cut_feed(NetgameFeed& f, std::vector<uint8_t> bytes, cl::Demo walk, std::string& error) {
  f.bytes = std::move(bytes);
  // The blocks after the track line: a length, the view's three angles, the messages.
  size_t at = 0;
  while (at < f.bytes.size() && at < 16 && f.bytes[at] != '\n') at++;
  at++;
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
      f.levels.push_back(cur);
    }
  };
  while (at + 16 <= f.bytes.size()) {
    uint32_t n = 0;
    std::memcpy(&n, f.bytes.data() + at, 4);
    if (n > (1u << 16) || at + 16 + n > f.bytes.size()) break;
    NetgameBlock blk;
    for (size_t k = 0; k < 3; k++) blk.angles[k] = le_f32(f.bytes.data() + at + 4 + 4 * k);
    blk.at = static_cast<uint32_t>(at + 16);
    blk.size = n;
    cl::DemoMsg m;
    m.b = std::span<const uint8_t>(f.bytes.data() + blk.at, n);
    m.marks = &blk.marks;
    std::string why;
    if (!cl::read_single_messages(walk, m, why)) {
      error = "block " + std::to_string(f.blocks.size()) + " does not read: " + why;
      return false;
    }
    const size_t bi = f.blocks.size();
    const uint8_t* base = f.bytes.data() + blk.at;
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
    blk.level = static_cast<int>(f.levels.size());
    f.blocks.push_back(std::move(blk));
    at += 16 + n;
  }
  close_level(f.blocks.size());
  if (f.levels.empty()) {
    error = "no level in the recording has a whole signon (it starts partway into a level)";
    return false;
  }
  return true;
}

// A multiview recording converted to one player's view (`slot`, -1 the lowest playing) and cut.
bool convert_feed(NetgameFeed& f, const std::vector<uint8_t>& source, int slot,
                  std::string& error) {
  cl::ConvertOptions o;
  o.no_end = true;
  o.pov = slot;
  cl::ConvertNotes notes;
  std::vector<uint8_t> out;
  cl::Demo demo;
  if (!cl::convert_demo(source, cl::DemoFormat::single, o, out, notes, error) ||
      !cl::open_demo(std::move(out), demo, error))
    return false;
  f.slot = slot;
  return cut_feed(f, std::move(demo.bytes), demo, error);
}

// A converted view added to the feeds and set on the first feed's clock; its index, or -1 with
// `error` when its levels are not the first view's.
int add_feed(NetgameTv& tv, NetgameFeed f, std::string& error) {
  if (f.levels.size() != tv.feeds[0].levels.size()) {
    error = "the view of slot " + std::to_string(f.slot) + " has " +
            std::to_string(f.levels.size()) + " levels, the first view " +
            std::to_string(tv.feeds[0].levels.size());
    return -1;
  }
  tv.feeds.push_back(std::move(f));
  if (tv.feeds[0].start_us >= 0) {
    // Set on the clock as of the last block the first feed played.
    NetgameFeed& added = tv.feeds.back();
    const NetgameFeed& m = tv.feeds[0];
    const int64_t at = m.next > 0 && m.next <= m.blocks.size()
                           ? m.start_us + m.blocks[m.next - 1].due_us
                           : m.start_us;
    sync_feed(tv, added, at);
  }
  return static_cast<int>(tv.feeds.size()) - 1;
}

int feed_for(NetgameTv& tv, size_t p, std::string& error) {
  if (p >= tv.players.size()) {
    error = "no player " + std::to_string(p + 1);
    return -1;
  }
  const int slot = tv.players[p].slot;
  for (size_t i = 0; i < tv.feeds.size(); i++)
    if (tv.feeds[i].slot == slot) return static_cast<int>(i);
  if (!tv.source || tv.feeds.empty()) {
    error = "the recording has one view";
    return -1;
  }
  if (tv.cfg.convert_in_background) {
    for (const NetgamePending& q : tv.pending)
      if (q.slot == slot) return -2;
    NetgamePending q;
    q.slot = slot;
    q.done = std::async(std::launch::async, [source = tv.source, slot] {
      NetgameConverted c;
      c.ok = convert_feed(c.feed, *source, slot, c.error);
      return c;
    });
    tv.pending.push_back(std::move(q));
    return -2;
  }
  NetgameFeed f;
  if (!convert_feed(f, *tv.source, slot, error)) return -1;
  return add_feed(tv, std::move(f), error);
}

// The views converted on their own threads that are ready: added, and the viewers who asked for
// them moved there (or told they cannot be).
void take_converted(NetgameTv& tv) {
  for (size_t i = 0; i < tv.pending.size();) {
    NetgamePending& q = tv.pending[i];
    if (q.done.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      i++;
      continue;
    }
    NetgameConverted c = q.done.get();
    const int slot = q.slot;
    tv.pending.erase(tv.pending.begin() + static_cast<std::ptrdiff_t>(i));
    std::string error = c.error;
    const int f = c.ok ? add_feed(tv, std::move(c.feed), error) : -1;
    std::string name;
    for (const NetgamePlayer& pl : tv.players)
      if (pl.slot == slot) name = pl.name;
    for (NetgameViewer& v : tv.viewers) {
      if (v.stage == NetgameStage::free || v.want_slot != slot) continue;
      v.want_slot = -1;
      if (f < 0) {
        print(v, "Can't follow " + name + " in this recording.\n");
        continue;
      }
      v.feed = static_cast<size_t>(f);
      restart(tv, v, true);
    }
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
  tv = NetgameTv{};
  tv.loop = loop;
  NetgameFeed first;
  if (demo.state.format == cl::DemoFormat::single) {
    if (!cut_feed(first, std::move(demo.bytes), demo, error)) return false;
  } else {
    if (demo.state.format == cl::DemoFormat::multiview) {
      // The players a viewer may follow: every slot named and playing in the first frames (the
      // signon's names come with them).
      cl::Demo walk = demo;
      std::array<std::string, 32> seen;
      std::string why;
      for (int i = 0; i < kFramesForPlayers; i++) {
        if (cl::next_frame(walk, why) != cl::DemoRead::frame) break;
        for (size_t s = 0; s < 32; s++)
          if (seen[s].empty() && !walk.state.names[s].empty() && !walk.state.spectator[s])
            seen[s] = walk.state.names[s];
      }
      for (size_t s = 0; s < 32; s++)
        if (!seen[s].empty()) tv.players.push_back({static_cast<int>(s), seen[s]});
    }
    if (!convert_feed(first, demo.bytes, -1, error)) return false;
    if (!tv.players.empty()) first.slot = tv.players[0].slot;  // -1 rides the lowest playing
    if (tv.players.size() > 1)
      tv.source = std::make_shared<const std::vector<uint8_t>>(std::move(demo.bytes));
  }
  tv.feeds.push_back(std::move(first));
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
  take_converted(tv);
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

bool udp_tv_netgame_follow(NetgameTv& tv, size_t viewer, size_t player, int64_t now_us,
                           std::string& error) {
  (void)now_us;
  if (viewer >= tv.viewers.size() || tv.viewers[viewer].stage == NetgameStage::free) {
    error = "no viewer " + std::to_string(viewer);
    return false;
  }
  if (player >= tv.players.size()) {
    error = "no player " + std::to_string(player + 1) + " (the recording has " +
            std::to_string(tv.players.size()) + ")";
    return false;
  }
  if (feed_for(tv, player, error) == -1) return false;
  follow(tv, tv.viewers[viewer], static_cast<int>(player));  // now, or when the view is ready
  return true;
}

std::string udp_tv_netgame_following(const NetgameTv& tv) {
  if (tv.players.size() < 2) return "";
  std::string names;
  for (const NetgameViewer& v : tv.viewers) {
    if (v.stage == NetgameStage::free) continue;
    const int p = following(tv, v);
    if (p < 0) continue;
    std::string n = tv.players[static_cast<size_t>(p)].name;
    for (char& ch : n) ch = static_cast<char>(static_cast<unsigned char>(ch) & 0x7F);
    names += (names.empty() ? "" : ", ") + n;
  }
  return names.empty() ? "" : " (following " + names + ")";
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
