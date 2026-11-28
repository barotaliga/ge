// A demo series' bytes (series.hpp): the header, entries with their sums, the merge.
//
// ge:summary Demo series (.geds) bytes: header, entries with sums, reading, merging
// ge:keywords demo-series geds entry-sum fnv1a merge read_series series_add
// ge:skill replay
// ge:test tests/replay_series_test.cpp
#include "replay/series.hpp"

#include <cstring>

namespace ge::replay {

namespace {

constexpr uint8_t kMagic[8] = {'g', 'e', 's', 'e', 'r', 'i', 'e', 's'};
constexpr uint8_t kSync[4] = {'G', 'S', 'E', 'N'};
constexpr size_t kHeaderFixed = sizeof kMagic + 2;

void put_u(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}
void put_bytes(std::vector<uint8_t>& o, std::span<const uint8_t> b) {
  put_u(o, b.size());
  o.insert(o.end(), b.begin(), b.end());
}
void put_text(std::vector<uint8_t>& o, std::string_view s) {
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
  std::span<const uint8_t> take(uint64_t n) {
    if (n > b.size() - at) {
      bad = true;
      at = b.size();
      return {};
    }
    const auto r = b.subspan(at, static_cast<size_t>(n));
    at += static_cast<size_t>(n);
    return r;
  }
  std::span<const uint8_t> bytes() { return take(u()); }
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
};

void put_entry(std::vector<uint8_t>& out, std::string_view name, const Meta& meta,
               std::span<const uint8_t> ged) {
  out.insert(out.end(), kSync, kSync + sizeof kSync);
  const size_t body = out.size();
  put_text(out, name);
  put_meta(out, meta);
  put_bytes(out, ged);
  const uint32_t sum = fnv1a32(std::span<const uint8_t>(out).subspan(body));
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(sum >> (8 * i)));
}

}  // namespace

std::string meta_get(const Meta& m, std::string_view key) {
  std::string out;
  for (const auto& [k, v] : m)
    if (k == key) out = v;
  return out;
}

Meta ged_meta(std::span<const uint8_t> ged) {
  Meta out;
  StreamReader r;
  std::string error;
  if (!read_stream_header(ged, r, error)) return out;
  out.emplace_back("game", r.header.game);
  for (const auto& kv : r.header.meta) out.push_back(kv);
  if (const std::optional<Trailer> t = read_trailer(ged))
    for (const auto& kv : t->meta)
      if (kv.first != "report") out.push_back(kv);
  return out;
}

void series_begin(std::vector<uint8_t>& out, const Meta& meta) {
  out.assign(kMagic, kMagic + sizeof kMagic);
  out.push_back(static_cast<uint8_t>(kSeriesVersion & 0xFF));
  out.push_back(static_cast<uint8_t>(kSeriesVersion >> 8));
  put_meta(out, meta);
}

bool series_entry(std::string_view name, std::span<const uint8_t> ged, std::vector<uint8_t>& out,
                  std::string& error) {
  if (!is_stream(ged)) {
    error = "'" + std::string(name) + "' is not a replay file (.ged), so it cannot join a series";
    return false;
  }
  out.clear();
  put_entry(out, name, ged_meta(ged), ged);
  return true;
}

bool series_add(std::vector<uint8_t>& out, std::string_view name, std::span<const uint8_t> ged,
                std::string& error) {
  std::vector<uint8_t> entry;
  if (!series_entry(name, ged, entry, error)) return false;
  out.insert(out.end(), entry.begin(), entry.end());
  return true;
}

bool is_series(std::span<const uint8_t> bytes) {
  return bytes.size() >= kHeaderFixed && std::memcmp(bytes.data(), kMagic, sizeof kMagic) == 0;
}

bool read_series(std::span<const uint8_t> bytes, SeriesView& out, std::string& error) {
  out = SeriesView{};
  if (!is_series(bytes)) {
    error = "not a demo series (.geds): it does not start with the series' magic";
    return false;
  }
  out.version = bytes[8] | (bytes[9] << 8);
  if (out.version > kSeriesVersion) {
    error = "the series is version " + std::to_string(out.version) + "; this build reads up to " +
            std::to_string(kSeriesVersion) + ", so update the game to open it";
    return false;
  }
  In in{bytes, kHeaderFixed};
  out.meta = in.meta();
  if (in.bad) {
    error = "the series' header is damaged (its meta runs past the file's end)";
    return false;
  }
  while (in.at < bytes.size()) {
    const size_t at = in.at;
    const std::string where =
        "entry " + std::to_string(out.entries.size() + 1) + " at byte " + std::to_string(at);
    if (bytes.size() - at < sizeof kSync || std::memcmp(bytes.data() + at, kSync, 4) != 0) {
      out.cut = where + ": no entry starts there (damaged)";
      break;
    }
    in.at += sizeof kSync;
    const size_t body = in.at;
    SeriesEntry e;
    e.offset = at;
    e.name = in.text();
    e.meta = in.meta();
    e.bytes = in.bytes();
    if (in.bad || bytes.size() - in.at < 4) {
      out.cut = where + ": cut short";
      break;
    }
    uint32_t sum = 0;
    for (int i = 0; i < 4; i++) sum |= static_cast<uint32_t>(bytes[in.at + i]) << (8 * i);
    if (sum != fnv1a32(bytes.subspan(body, in.at - body))) {
      out.cut = where + " ('" + e.name + "'): its sum does not hold (damaged)";
      break;
    }
    in.at += 4;
    out.entries.push_back(std::move(e));
  }
  return true;
}

bool series_merge(std::span<const SeriesPart> parts, const Meta& meta, std::vector<uint8_t>& out,
                  std::string& error) {
  Meta head = meta;
  std::vector<SeriesView> views(parts.size());
  for (size_t i = 0; i < parts.size(); i++) {
    const SeriesPart& p = parts[i];
    if (is_series(p.bytes)) {
      if (!read_series(p.bytes, views[i], error)) {
        error.insert(0, p.name + ": ");
        return false;
      }
      if (!views[i].cut.empty()) {
        error = p.name + ": " + views[i].cut + "; merge its whole entries by splitting it first";
        return false;
      }
      if (head.empty()) head = views[i].meta;
    } else if (!is_stream(p.bytes)) {
      error = p.name + " is neither a replay file (.ged) nor a demo series (.geds)";
      return false;
    }
  }
  series_begin(out, head);
  for (size_t i = 0; i < parts.size(); i++) {
    if (is_series(parts[i].bytes)) {
      for (const SeriesEntry& e : views[i].entries) put_entry(out, e.name, e.meta, e.bytes);
    } else {
      put_entry(out, parts[i].name, ged_meta(parts[i].bytes), parts[i].bytes);
    }
  }
  return true;
}

}  // namespace ge::replay
