// The TV node and the viewer's end (node.hpp): the source split into blocks, the delay, each
// viewer's place, chat and the count, and the messages both ways.
//
// ge:summary The TV node's source split, delay, viewers' places, chat and count; the wire
// ge:keywords tv node wire hello block piece chat viewers ended refused
// ge:skill replay net
// ge:test tests/tv_node_test.cpp
#include "tv/node.hpp"

#include <algorithm>

namespace ge::tv {

namespace {

constexpr size_t kMostHeader = size_t{1} << 20;  // a header still not whole past this is damage
constexpr size_t kMostText = 512;                // a chat line or a name, bytes

void put_u(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}

void put_text(std::vector<uint8_t>& o, std::string_view s) {
  s = s.substr(0, kMostText);
  put_u(o, s.size());
  o.insert(o.end(), s.begin(), s.end());
}

struct In {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  uint64_t u() {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      if (at >= b.size()) break;
      const uint8_t c = b[at++];
      v |= static_cast<uint64_t>(c & 0x7F) << shift;
      if ((c & 0x80) == 0) return v;
    }
    bad = true;
    return 0;
  }
  uint8_t byte() {
    if (at >= b.size()) {
      bad = true;
      return 0;
    }
    return b[at++];
  }
  std::string text() {
    const uint64_t n = u();
    if (bad || n > kMostText || n > b.size() - at) {
      bad = true;
      return {};
    }
    std::string s(reinterpret_cast<const char*>(b.data() + at), static_cast<size_t>(n));
    at += static_cast<size_t>(n);
    return s;
  }
};

Viewer* viewer_of(Node& node, int link) {
  for (Viewer& v : node.viewers)
    if (v.link == link) return &v;
  return nullptr;
}

// The newest keyframe block out, else the first block out; -1 when nothing is out.
int64_t start_block(const Node& node) {
  for (int64_t k = node.released - 1; k >= node.first; k--)
    if (node.blocks[static_cast<size_t>(k - node.first)].keyframe) return k;
  return node.released > node.first ? node.first : -1;
}

void split(Node& node, int64_t now_ms) {
  size_t at = 0;
  if (!node.have_header) {
    replay::StreamReader r;
    std::string error;
    if (!replay::read_stream_header(node.pending, r, error)) {
      if (!replay::is_stream(node.pending) && node.pending.size() >= 8) {
        node.error = "the source is not a replay file";
      } else if (node.pending.size() > kMostHeader) {
        node.error = "the source's header is damaged: " + error;
      }
      return;
    }
    if (!node.config.game.empty() && r.header.game != node.config.game) {
      node.error = "the source is a " + r.header.game + " match; this TV shows " + node.config.game;
      return;
    }
    node.have_header = true;
    node.parsed = r.header;
    node.header.assign(node.pending.begin(),
                       node.pending.begin() + static_cast<std::ptrdiff_t>(r.at));
    at = r.at;
  }
  for (;;) {
    replay::BlockPeek p;
    std::string error;
    const int got = replay::peek_block(node.pending, at, p, error);
    if (got == 0) break;
    if (got < 0) {
      if (error.empty()) {
        node.finished = true;  // the trailer: the source is done
        at = node.pending.size();
      } else {
        node.error = error;
      }
      break;
    }
    Held h;
    h.bytes.assign(node.pending.begin() + static_cast<std::ptrdiff_t>(at),
                   node.pending.begin() + static_cast<std::ptrdiff_t>(at + p.size));
    h.time_ms = p.time_ms;
    h.keyframe = p.keyframe;
    if (!node.anchored) {
      node.anchored = true;
      node.anchor_game_ms = p.time_ms;
      node.anchor_clock_ms = now_ms;
    }
    node.blocks.push_back(std::move(h));
    node.blocks_in++;
    at += p.size;
  }
  node.pending.erase(node.pending.begin(), node.pending.begin() + static_cast<std::ptrdiff_t>(at));
}

std::vector<uint8_t> msg_of(Msg kind) { return {static_cast<uint8_t>(kind)}; }

std::vector<uint8_t> viewers_msg(int64_t n) {
  auto m = msg_of(Msg::viewers);
  put_u(m, static_cast<uint64_t>(std::max<int64_t>(n, 0)));
  return m;
}

std::vector<uint8_t> refused_msg(std::string_view why) {
  auto m = msg_of(Msg::refused);
  put_text(m, why);
  return m;
}

// Every block from the viewer's place that its link takes now, in pieces; false when it is full.
bool send_blocks(Node& node, Viewer& v, const Send& send) {
  const int64_t end = node.first + static_cast<int64_t>(node.blocks.size());
  while (v.next < node.released && v.next < end) {
    const Held& h = node.blocks[static_cast<size_t>(v.next - node.first)];
    while (v.piece < h.bytes.size()) {
      const size_t n = std::min(kPiece, h.bytes.size() - v.piece);
      auto m = msg_of(Msg::block);
      m.push_back(v.piece + n < h.bytes.size() ? 1 : 0);
      m.insert(m.end(), h.bytes.begin() + static_cast<std::ptrdiff_t>(v.piece),
               h.bytes.begin() + static_cast<std::ptrdiff_t>(v.piece + n));
      if (!send(v.link, m)) return false;
      node.bytes_out += static_cast<int64_t>(m.size());
      v.piece += n;
    }
    v.next++;
    v.piece = 0;
  }
  return true;
}

}  // namespace

bool tv_feed(Node& node, std::span<const uint8_t> bytes, int64_t now_ms) {
  if (!node.error.empty()) return false;
  if (node.finished) return true;
  node.bytes_in += static_cast<int64_t>(bytes.size());
  node.pending.insert(node.pending.end(), bytes.begin(), bytes.end());
  split(node, now_ms);
  return node.error.empty();
}

void tv_end(Node& node) { node.finished = true; }

void tv_join(Node& node, int link) {
  if (viewer_of(node, link) != nullptr) return;
  Viewer v;
  v.link = link;
  node.viewers.push_back(v);
}

void tv_leave(Node& node, int link) {
  std::erase_if(node.viewers, [link](const Viewer& v) { return v.link == link; });
}

bool tv_receive(Node& node, int link, std::span<const uint8_t> msg) {
  Viewer* v = viewer_of(node, link);
  if (v == nullptr || msg.empty()) return false;
  In in{msg, 1};
  switch (static_cast<Msg>(msg[0])) {
    case Msg::hello: {
      const uint8_t version = in.byte();
      v->relay = in.byte() == 1;
      v->name = in.text();
      if (in.bad || version != kWireVersion) return false;
      v->greeted = true;
      return true;
    }
    case Msg::chat: {
      ChatLine line;
      line.who = in.text();
      line.text = in.text();
      if (in.bad) return false;
      if (!v->relay) line.who = v->name;  // a viewer speaks for itself; a relay for its own
      if (node.config.relay) {
        node.up.push_back(line);
      } else {
        node.chat.push_back(line);
        node.down.push_back(std::move(line));
      }
      return true;
    }
    case Msg::viewers: {
      const uint64_t n = in.u();
      if (in.bad || !v->relay) return false;
      v->under = static_cast<int64_t>(std::min<uint64_t>(n, uint64_t{1} << 40));
      return true;
    }
    default:
      return false;
  }
}

int64_t tv_catch_up_blocks(const Node& node) {
  const int64_t s = start_block(node);
  return s < 0 ? 0 : node.released - s;
}

int64_t tv_viewer_count(const Node& node) {
  int64_t n = 0;
  for (const Viewer& v : node.viewers)
    if (v.greeted) n += v.relay ? v.under : 1;
  return n;
}

void tv_update(Node& node, int64_t now_ms, const Send& send) {
  // What the delay lets out.
  const int64_t end = node.first + static_cast<int64_t>(node.blocks.size());
  while (node.released < end) {
    const Held& h = node.blocks[static_cast<size_t>(node.released - node.first)];
    if (node.config.delay_ms > 0 &&
        h.time_ms - node.anchor_game_ms + node.anchor_clock_ms + node.config.delay_ms > now_ms)
      break;
    node.released++;
  }
  const int64_t count = node.tree >= 0 ? node.tree : tv_viewer_count(node);
  int greeted = 0;
  for (Viewer& v : node.viewers) {
    if (!v.greeted) continue;
    greeted++;
    if (node.config.max_viewers > 0 && greeted > node.config.max_viewers) {
      send(v.link, refused_msg("this TV is full"));
      v.greeted = false;
      continue;
    }
    if (!v.started) {
      if (!node.have_header) continue;
      auto m = msg_of(Msg::header);
      m.insert(m.end(), node.header.begin(), node.header.end());
      if (!send(v.link, m)) continue;
      node.bytes_out += static_cast<int64_t>(m.size());
      v.started = true;
    }
    if (v.next < 0) v.next = start_block(node);
    for (const ChatLine& line : node.down) {
      auto m = msg_of(Msg::chat);
      put_text(m, line.who);
      put_text(m, line.text);
      send(v.link, m);
    }
    if (v.next >= 0 && !send_blocks(node, v, send)) continue;
    if (v.told != count && send(v.link, viewers_msg(count))) v.told = count;
    if (node.finished && !v.ended && v.next >= end && node.released >= end &&
        send(v.link, msg_of(Msg::ended)))
      v.ended = true;
  }
  node.down.clear();
  // Let go of what no one needs: blocks before both the start a joiner gets and every viewer's
  // place.
  int64_t keep = start_block(node);
  if (keep < 0) keep = node.first;
  for (const Viewer& v : node.viewers) {
    if (!v.greeted) continue;
    keep = std::min(keep, v.next >= 0 ? v.next : node.first);
  }
  while (node.first < keep && !node.blocks.empty()) {
    node.blocks.pop_front();
    node.first++;
  }
}

std::vector<uint8_t> tv_hello(std::string_view name, bool relay) {
  auto m = msg_of(Msg::hello);
  m.push_back(kWireVersion);
  m.push_back(relay ? 1 : 0);
  put_text(m, name);
  return m;
}

std::vector<uint8_t> tv_chat(std::string_view who, std::string_view text) {
  auto m = msg_of(Msg::chat);
  put_text(m, who);
  put_text(m, text);
  return m;
}

std::vector<uint8_t> tv_count(int64_t viewers) { return viewers_msg(viewers); }

bool watch_receive(Watch& w, std::span<const uint8_t> msg, std::vector<uint8_t>* fresh) {
  if (fresh != nullptr) fresh->clear();
  w.bytes += static_cast<int64_t>(msg.size());
  if (msg.empty()) {
    w.error = "an empty message";
    return false;
  }
  In in{msg, 1};
  switch (static_cast<Msg>(msg[0])) {
    case Msg::header:
      if (w.have_header) {
        w.error = "a second header";
        return false;
      }
      w.have_header = true;
      if (w.keep) w.stream.insert(w.stream.end(), msg.begin() + 1, msg.end());
      if (fresh != nullptr) fresh->assign(msg.begin() + 1, msg.end());
      return true;
    case Msg::block: {
      if (!w.have_header || msg.size() < 2) {
        w.error = "a block before the header";
        return false;
      }
      w.piece.insert(w.piece.end(), msg.begin() + 2, msg.end());
      if (msg[1] == 1) return true;
      if (w.keep) w.stream.insert(w.stream.end(), w.piece.begin(), w.piece.end());
      if (fresh != nullptr) fresh->swap(w.piece);
      w.piece.clear();
      w.blocks++;
      return true;
    }
    case Msg::chat: {
      ChatLine line;
      line.who = in.text();
      line.text = in.text();
      if (in.bad) break;
      w.chat.push_back(std::move(line));
      return true;
    }
    case Msg::viewers:
      w.viewers = static_cast<int64_t>(in.u());
      if (in.bad) break;
      return true;
    case Msg::ended:
      w.ended = true;
      return true;
    case Msg::refused:
      w.refused = in.text();
      return !in.bad;
    default:
      break;
  }
  w.error = "a message the wire does not have (kind " + std::to_string(msg[0]) + ")";
  return false;
}

}  // namespace ge::tv
