// The radar track, defined: its three chunks written and read, a track added to a finished stream.
//
// ge:summary The radar track defined: roster, positions and outline chunks; a track added
// ge:keywords radar chunk varint zigzag roster positions outline stream_add_track
// ge:skill replay
// ge:test tests/replay_radar_test.cpp
#include "replay/radar.hpp"

#include <algorithm>

namespace ge::replay {

namespace {

void put_u(std::vector<uint8_t>& out, uint64_t v) {
  while (v >= 0x80) {
    out.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  out.push_back(static_cast<uint8_t>(v));
}

void put_s(std::vector<uint8_t>& out, int64_t v) {
  put_u(out, (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63));
}

void put_text(std::vector<uint8_t>& out, const std::string& s) {
  put_u(out, s.size());
  out.insert(out.end(), s.begin(), s.end());
}

struct Reader {
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
      v |= static_cast<uint64_t>(c & 0x7f) << shift;
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
  std::string text() {
    const uint64_t n = u();
    if (n > b.size() - std::min(at, b.size())) {
      bad = true;
      return {};
    }
    std::string s(reinterpret_cast<const char*>(b.data() + at), n);
    at += n;
    return s;
  }
};

int32_t coord(int64_t v) {
  return static_cast<int32_t>(std::clamp<int64_t>(v, -(int64_t{1} << 30), int64_t{1} << 30));
}

void grow(Radar& r, int32_t x, int32_t y) {
  if (!r.any) {
    r.min_x = r.max_x = x;
    r.min_y = r.max_y = y;
    r.any = true;
    return;
  }
  r.min_x = std::min(r.min_x, x);
  r.max_x = std::max(r.max_x, x);
  r.min_y = std::min(r.min_y, y);
  r.max_y = std::max(r.max_y, y);
}

}  // namespace

std::vector<uint8_t> radar_roster(std::span<const RadarPlayer> players) {
  std::vector<uint8_t> out = {'R'};
  put_u(out, players.size());
  for (const RadarPlayer& p : players) {
    out.push_back(p.slot);
    put_text(out, p.name);
    put_text(out, p.team);
  }
  return out;
}

std::vector<uint8_t> radar_positions(std::span<const RadarDot> dots) {
  std::vector<uint8_t> out = {'P'};
  put_u(out, dots.size());
  for (const RadarDot& d : dots) {
    out.push_back(d.slot);
    out.push_back(d.flags);
    put_s(out, d.x);
    put_s(out, d.y);
    out.push_back(d.heading);
  }
  return out;
}

std::vector<uint8_t> radar_outline(std::span<const RadarLine> lines) {
  std::vector<uint8_t> out = {'O'};
  put_u(out, lines.size());
  for (const RadarLine& l : lines) {
    put_s(out, l.x0);
    put_s(out, l.y0);
    put_s(out, l.x1);
    put_s(out, l.y1);
  }
  return out;
}

bool read_radar(std::span<const uint8_t> chunk, int64_t time_ms, Radar& r) {
  Reader in{chunk};
  const uint8_t tag = in.byte();
  const uint64_t n = in.u();
  if (in.bad || n > chunk.size()) return false;  // each entry takes a byte at least
  if (tag == 'R') {
    std::vector<RadarPlayer> roster;
    roster.reserve(n);
    for (uint64_t i = 0; i < n && !in.bad; i++) {
      RadarPlayer p;
      p.slot = in.byte();
      p.name = in.text();
      p.team = in.text();
      roster.push_back(std::move(p));
    }
    if (in.bad) return false;
    r.roster = std::move(roster);
    return true;
  }
  if (tag == 'P') {
    RadarFrame f;
    f.time_ms = time_ms;
    f.dots.reserve(n);
    for (uint64_t i = 0; i < n && !in.bad; i++) {
      RadarDot d;
      d.slot = in.byte();
      d.flags = in.byte();
      d.x = coord(in.s());
      d.y = coord(in.s());
      d.heading = in.byte();
      f.dots.push_back(d);
    }
    if (in.bad) return false;
    for (const RadarDot& d : f.dots) grow(r, d.x, d.y);
    r.frames.push_back(std::move(f));
    return true;
  }
  if (tag == 'O') {
    std::vector<RadarLine> lines;
    lines.reserve(n);
    for (uint64_t i = 0; i < n && !in.bad; i++) {
      RadarLine l;
      l.x0 = coord(in.s());
      l.y0 = coord(in.s());
      l.x1 = coord(in.s());
      l.y1 = coord(in.s());
      lines.push_back(l);
    }
    if (in.bad) return false;
    for (const RadarLine& l : lines) {
      grow(r, l.x0, l.y0);
      grow(r, l.x1, l.y1);
    }
    r.outline = std::move(lines);
    return true;
  }
  return false;
}

bool read_radar_file(std::span<const uint8_t> file, Radar& r, std::string& error) {
  r = Radar{};
  StreamReader sr;
  if (!read_stream_header(file, sr, error)) return false;
  uint32_t id = 0;
  bool found = false;
  for (const TrackInfo& t : sr.header.tracks) {
    if (t.kind == TrackKind::radar) {
      id = t.id;
      found = true;
      break;
    }
  }
  if (!found) return true;
  Block b;
  while (next_block(sr, b, error)) {
    for (const Chunk& c : b.chunks) {
      if (c.track == id) read_radar(c.bytes, c.time_ms, r);  // a chunk that does not read: skipped
    }
  }
  if (!error.empty()) return false;
  std::stable_sort(r.frames.begin(), r.frames.end(),
                   [](const RadarFrame& x, const RadarFrame& y) { return x.time_ms < y.time_ms; });
  return true;
}

bool stream_add_track(std::span<const uint8_t> file, const TrackInfo& track,
                      const std::vector<Chunk>& chunks, std::vector<uint8_t>& out,
                      std::string& error, const Meta& meta) {
  StreamReader sr;
  if (!read_stream_header(file, sr, error)) return false;
  for (const TrackInfo& t : sr.header.tracks) {
    if (t.id == track.id) {
      error = "track " + std::to_string(track.id) + " is already in the file; pick another id";
      return false;
    }
  }
  std::vector<Block> blocks;
  Block b;
  while (next_block(sr, b, error)) blocks.push_back(std::move(b));
  if (!error.empty()) return false;
  StreamHeader h = sr.header;
  h.tracks.push_back(track);
  const Meta had = h.meta;  // a key the file has keeps its values; a new one takes all it is given
  for (const auto& [k, v] : meta) {
    const bool have =
        std::any_of(had.begin(), had.end(),
                    [&](const std::pair<std::string, std::string>& m) { return m.first == k; });
    if (!have) h.meta.emplace_back(k, v);
  }
  std::vector<Chunk> add = chunks;
  std::stable_sort(add.begin(), add.end(),
                   [](const Chunk& x, const Chunk& y) { return x.time_ms < y.time_ms; });
  out.clear();
  StreamWriter w;
  stream_begin(w, h, out);
  size_t next = 0;
  for (size_t i = 0; i < blocks.size(); i++) {
    const Block& blk = blocks[i];
    const bool last = i + 1 == blocks.size();
    w.block_start_ms = blk.time_ms;  // the block keeps its time whatever is added first
    for (const Chunk& c : blk.chunks) stream_add(w, c.track, c.time_ms, c.bytes);
    while (next < add.size() && (last || add[next].time_ms < blocks[i + 1].time_ms)) {
      stream_add(w, track.id, std::max(add[next].time_ms, blk.time_ms), add[next].bytes);
      next++;
    }
    stream_end_block(w, blk.keyframe);
  }
  if (blocks.empty() && !add.empty()) {  // a file with no block yet: the track's chunks alone
    for (const Chunk& c : add) stream_add(w, track.id, c.time_ms, c.bytes);
    stream_end_block(w, true);
  }
  if (const std::optional<Trailer> t = read_trailer(file)) stream_finish(w, t->meta);
  return true;
}

}  // namespace ge::replay
