// The replay file version 2's bytes (stream.hpp): varints, the header, blocks and their sums, the
// trailer; zstd per track per block through engine/vfs.
//
// ge:summary Replay file v2's bytes: varints, header, blocks with sums, trailer, zstd per track
// ge:keywords replay-v2 varint leb128 fnv1a block-sum zstd trailer header encode decode
// ge:skill replay
// ge:test tests/replay_stream_test.cpp
#include "replay/stream.hpp"

#include <cstring>

#include "vfs/zstd.hpp"

namespace ge::replay {

namespace {

constexpr uint8_t kMagic[8] = {'g', 'e', 'r', 'e', 'p', 'l', 'a', 'y'};
constexpr uint8_t kSync[4] = {'G', 'B', 'L', 'K'};
constexpr uint8_t kEnd[4] = {'G', 'E', 'N', 'D'};
constexpr uint8_t kIndexTail[4] = {'G', 'E', 'I', 'X'};
// A track's packed bytes in one block may unpack to at most this much (a damaged size refused).
constexpr size_t kMostTrackBytes = size_t{64} << 20;

void put_u(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}
void put_s(std::vector<uint8_t>& o, int64_t v) {
  put_u(o, (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63));
}
void put_bytes(std::vector<uint8_t>& o, std::span<const uint8_t> b) {
  put_u(o, b.size());
  o.insert(o.end(), b.begin(), b.end());
}
void put_text(std::vector<uint8_t>& o, const std::string& s) {
  put_bytes(o, {reinterpret_cast<const uint8_t*>(s.data()), s.size()});
}
void put_meta(std::vector<uint8_t>& o, const Meta& m) {
  put_u(o, m.size());
  for (const auto& [k, v] : m) {
    put_text(o, k);
    put_text(o, v);
  }
}

uint32_t fnv1a32(std::span<const uint8_t> b) {
  uint32_t h = 2166136261u;
  for (const uint8_t c : b) h = (h ^ c) * 16777619u;
  return h;
}

struct In {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  uint64_t u() {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      if (at >= b.size()) {
        bad = true;
        return 0;
      }
      const uint8_t c = b[at++];
      v |= static_cast<uint64_t>(c & 0x7F) << shift;
      if ((c & 0x80) == 0) return v;
    }
    bad = true;
    return 0;
  }
  int64_t s() {
    const uint64_t z = u();
    return static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1);
  }
  uint8_t byte() {
    if (at >= b.size()) {
      bad = true;
      return 0;
    }
    return b[at++];
  }
  std::span<const uint8_t> take(size_t n) {
    if (n > b.size() - at) {
      bad = true;
      at = b.size();
      return {};
    }
    const auto r = b.subspan(at, n);
    at += n;
    return r;
  }
  std::span<const uint8_t> bytes() { return take(static_cast<size_t>(u())); }
  std::string text() {
    const auto t = bytes();
    return {reinterpret_cast<const char*>(t.data()), t.size()};
  }
  Meta meta() {
    Meta m;
    const uint64_t n = u();
    for (uint64_t i = 0; i < n && !bad; i++) {
      std::string k = text();
      m.emplace_back(std::move(k), text());
    }
    return m;
  }
  bool magic(const uint8_t* want, size_t n) {
    const auto t = take(n);
    return !bad && std::memcmp(t.data(), want, n) == 0;
  }
};

const TrackInfo* track_of(const StreamHeader& h, uint32_t id) {
  for (const TrackInfo& t : h.tracks)
    if (t.id == id) return &t;
  return nullptr;
}

}  // namespace

bool is_stream(std::span<const uint8_t> bytes) {
  return bytes.size() >= 8 && std::memcmp(bytes.data(), kMagic, 8) == 0;
}

void stream_begin(StreamWriter& w, const StreamHeader& h, std::vector<uint8_t>& out) {
  w = StreamWriter{};
  w.out = &out;
  w.header = h;
  out.insert(out.end(), kMagic, kMagic + 8);
  out.push_back(static_cast<uint8_t>(kStreamVersion & 0xFF));
  out.push_back(static_cast<uint8_t>(kStreamVersion >> 8));
  put_text(out, h.game);
  put_s(out, h.game_version);
  put_meta(out, h.meta);
  put_bytes(out, h.blob);
  put_u(out, h.tracks.size());
  for (const TrackInfo& t : h.tracks) {
    put_u(out, t.id);
    out.push_back(static_cast<uint8_t>(t.kind));
    out.push_back(static_cast<uint8_t>(t.codec));
    put_text(out, t.name);
  }
}

void stream_add(StreamWriter& w, uint32_t track, int64_t time_ms, std::span<const uint8_t> bytes) {
  if (w.block_start_ms < 0) w.block_start_ms = time_ms;
  w.pending.push_back({track, time_ms, {bytes.begin(), bytes.end()}});
}

void stream_end_block(StreamWriter& w, bool keyframe) {
  if (w.pending.empty()) return;
  std::vector<uint8_t> body;
  put_u(body, static_cast<uint64_t>(w.blocks));
  put_s(body, w.block_start_ms);
  body.push_back(keyframe ? 1 : 0);
  size_t present = 0;
  for (const TrackInfo& t : w.header.tracks)
    for (const Chunk& c : w.pending)
      if (c.track == t.id) {
        present++;
        break;
      }
  put_u(body, present);
  for (const TrackInfo& t : w.header.tracks) {
    std::vector<uint8_t> plain;
    int64_t last = w.block_start_ms;
    bool any = false;
    for (const Chunk& c : w.pending) {
      if (c.track != t.id) continue;
      any = true;
      put_u(plain, static_cast<uint64_t>(c.time_ms - last));
      last = c.time_ms;
      put_bytes(plain, c.bytes);
    }
    if (!any) continue;
    put_u(body, t.id);
    body.push_back(static_cast<uint8_t>(t.codec));
    if (t.codec == TrackCodec::zstd)
      put_bytes(body, vfs::zstd_pack(plain));
    else
      put_bytes(body, plain);
  }
  std::vector<uint8_t>& out = *w.out;
  w.index.push_back({static_cast<int64_t>(out.size()), w.block_start_ms, keyframe});
  out.insert(out.end(), kSync, kSync + 4);
  put_bytes(out, body);
  const uint32_t sum = fnv1a32(body);
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(sum >> (8 * i)));
  w.blocks++;
  w.pending.clear();
  w.block_start_ms = -1;
}

void stream_finish(StreamWriter& w, const Meta& final_meta) {
  stream_end_block(w, false);
  std::vector<uint8_t>& out = *w.out;
  const auto offset = static_cast<uint64_t>(out.size());
  std::vector<uint8_t> body;
  put_meta(body, final_meta);
  put_u(body, w.index.size());
  for (const BlockEntry& e : w.index) {
    put_u(body, static_cast<uint64_t>(e.offset));
    put_s(body, e.time_ms);
    body.push_back(e.keyframe ? 1 : 0);
  }
  out.insert(out.end(), kEnd, kEnd + 4);
  put_bytes(out, body);
  for (int i = 0; i < 8; i++) out.push_back(static_cast<uint8_t>(offset >> (8 * i)));
  out.insert(out.end(), kIndexTail, kIndexTail + 4);
}

bool read_stream_header(std::span<const uint8_t> bytes, StreamReader& r, std::string& error) {
  r = StreamReader{};
  r.bytes = bytes;
  In in{bytes};
  if (!in.magic(kMagic, 8)) {
    error = "not a version 2 replay file (no \"gereplay\" at its start)";
    return false;
  }
  const int version = in.byte() | (in.byte() << 8);
  if (version != kStreamVersion) {
    error = "a replay file of version " + std::to_string(version) + "; this engine reads version " +
            std::to_string(kStreamVersion) + "; next: open it with the engine that wrote it";
    return false;
  }
  StreamHeader& h = r.header;
  h.game = in.text();
  h.game_version = static_cast<int32_t>(in.s());
  h.meta = in.meta();
  const auto blob = in.bytes();
  h.blob.assign(blob.begin(), blob.end());
  const uint64_t n = in.u();
  for (uint64_t i = 0; i < n && !in.bad; i++) {
    TrackInfo t;
    t.id = static_cast<uint32_t>(in.u());
    t.kind = static_cast<TrackKind>(in.byte());
    t.codec = static_cast<TrackCodec>(in.byte());
    t.name = in.text();
    h.tracks.push_back(std::move(t));
  }
  if (in.bad) {
    error = "the replay file's header is cut short or damaged; next: the file is incomplete";
    return false;
  }
  r.at = in.at;
  return true;
}

bool next_block(StreamReader& r, Block& b, std::string& error) {
  error.clear();
  In in{r.bytes, r.at};
  if (in.at >= r.bytes.size()) return false;
  if (r.bytes.size() - in.at >= 4 && std::memcmp(r.bytes.data() + in.at, kEnd, 4) == 0)
    return false;
  if (!in.magic(kSync, 4)) {
    error = "no block at offset " + std::to_string(r.at) + "; next: the file is damaged there";
    return false;
  }
  const auto body = in.bytes();
  const auto sum_bytes = in.take(4);
  if (in.bad) {
    error = "the block at offset " + std::to_string(r.at) + " is cut short (the file ends there)";
    return false;
  }
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++)
    sum |= static_cast<uint32_t>(sum_bytes[static_cast<size_t>(i)]) << (8 * i);
  if (sum != fnv1a32(body)) {
    error =
        "the block at offset " + std::to_string(r.at) + " fails its sum; next: the file is damaged";
    return false;
  }
  In bi{body};
  b = Block{};
  b.index = static_cast<int64_t>(bi.u());
  b.time_ms = bi.s();
  b.keyframe = (bi.byte() & 1) != 0;
  const uint64_t present = bi.u();
  for (uint64_t k = 0; k < present && !bi.bad; k++) {
    const auto id = static_cast<uint32_t>(bi.u());
    const auto codec = static_cast<TrackCodec>(bi.byte());
    const auto packed = bi.bytes();
    std::vector<uint8_t> plain;
    if (codec == TrackCodec::zstd) {
      bool truncated = false;
      if (!vfs::zstd_unpack(packed, plain, truncated, error, kMostTrackBytes) || truncated) {
        error = std::string("track ")
                    .append(std::to_string(id))
                    .append(" in the block at offset ")
                    .append(std::to_string(r.at))
                    .append(" does not unpack: ")
                    .append(error);
        return false;
      }
    } else {
      plain.assign(packed.begin(), packed.end());
    }
    if (track_of(r.header, id) == nullptr) {
      error = "the block at offset " + std::to_string(r.at) + " names track " + std::to_string(id) +
              ", which the header does not list";
      return false;
    }
    In ci{plain};
    int64_t t = b.time_ms;
    while (ci.at < plain.size() && !ci.bad) {
      t += static_cast<int64_t>(ci.u());
      const auto bytes = ci.bytes();
      b.chunks.push_back({id, t, {bytes.begin(), bytes.end()}});
    }
    if (ci.bad) bi.bad = true;
  }
  if (bi.bad) {
    error = "the block at offset " + std::to_string(r.at) + " is damaged inside";
    return false;
  }
  r.at = in.at;
  return true;
}

std::optional<Trailer> read_trailer(std::span<const uint8_t> bytes) {
  if (bytes.size() < 12 || std::memcmp(bytes.data() + bytes.size() - 4, kIndexTail, 4) != 0)
    return std::nullopt;
  uint64_t offset = 0;
  for (int i = 0; i < 8; i++)
    offset |= static_cast<uint64_t>(bytes[bytes.size() - 12 + static_cast<size_t>(i)]) << (8 * i);
  if (offset >= bytes.size() - 12) return std::nullopt;
  In in{bytes, static_cast<size_t>(offset)};
  if (!in.magic(kEnd, 4)) return std::nullopt;
  In body{in.bytes()};
  Trailer t;
  t.meta = body.meta();
  const uint64_t n = body.u();
  for (uint64_t i = 0; i < n && !body.bad; i++) {
    BlockEntry e;
    e.offset = static_cast<int64_t>(body.u());
    e.time_ms = body.s();
    e.keyframe = (body.byte() & 1) != 0;
    t.index.push_back(e);
  }
  if (in.bad || body.bad) return std::nullopt;
  return t;
}

}  // namespace ge::replay
