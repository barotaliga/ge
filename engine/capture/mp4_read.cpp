// The reader of the engine's own fragmented MP4 files (mp4_read.hpp).
//
// ge:summary read_mp4: the box walk, the moov's tracks, each fragment's runs, the mfra
// ge:keywords mp4 read box walk moov trak moof traf trun mdat mfra cut
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#include "capture/mp4_read.hpp"

namespace ge::capture {
namespace {

struct Box {
  std::string type;
  size_t at = 0, body = 0, end = 0;
};

struct Reader {
  std::span<const uint8_t> b;
  std::string& error;

  uint64_t be(size_t at, int n) const {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) v = (v << 8) | b[at + static_cast<size_t>(i)];
    return v;
  }
  std::string four(size_t at) const {
    return std::string(reinterpret_cast<const char*>(&b[at]), 4);
  }
  bool fits(size_t at, size_t n, size_t limit) const { return at <= limit && n <= limit - at; }

  // The box at `at` inside `limit`: false at the end; `cut` when its header or body runs past.
  bool next(size_t at, size_t limit, Box& out, bool& cut) const {
    cut = false;
    if (at >= limit) return false;
    if (!fits(at, 8, limit)) {
      cut = true;
      return false;
    }
    uint64_t size = be(at, 4);
    out.type = four(at + 4);
    out.at = at;
    out.body = at + 8;
    if (size == 1) {
      if (!fits(at, 16, limit)) {
        cut = true;
        return false;
      }
      size = be(at + 8, 8);
      out.body = at + 16;
    } else if (size == 0) {
      size = limit - at;
    }
    if (size < out.body - at || !fits(at, static_cast<size_t>(size), limit)) {
      cut = true;
      return false;
    }
    out.end = at + static_cast<size_t>(size);
    return true;
  }

  // Each child box of `parent` handed to `f` (which returns false to stop with an error).
  template <typename F>
  bool children(const Box& parent, size_t from, F f) const {
    size_t at = from;
    Box c;
    bool cut = false;
    while (next(at, parent.end, c, cut)) {
      if (!f(c)) return false;
      at = c.end;
    }
    if (cut) {
      error = "a box inside " + parent.type + " at byte " + std::to_string(at) + " runs past it";
      return false;
    }
    return true;
  }

  bool need(const Box& x, size_t n) const {
    if (x.end - x.body >= n) return true;
    error = x.type + " at byte " + std::to_string(x.at) + " is " + std::to_string(x.end - x.body) +
            " bytes, under the " + std::to_string(n) + " it needs";
    return false;
  }

  bool entry(const Box& e, Mp4Track& t) const {
    t.entry = e.type;
    size_t kids = 0;
    if (t.handler == "vide") {
      if (!need(e, 78)) return false;
      t.width = static_cast<uint32_t>(be(e.body + 24, 2));
      t.height = static_cast<uint32_t>(be(e.body + 26, 2));
      kids = e.body + 78;
    } else {
      if (!need(e, 28)) return false;
      t.channels = static_cast<uint32_t>(be(e.body + 16, 2));
      t.rate = static_cast<uint32_t>(be(e.body + 24, 4) >> 16);
      kids = e.body + 28;
    }
    return children(e, kids, [&](const Box& c) {
      if (c.type == "av1C" || c.type == "avcC" || c.type == "hvcC" || c.type == "dOps") {
        t.config_type = c.type;
        t.config.assign(b.begin() + static_cast<std::ptrdiff_t>(c.body),
                        b.begin() + static_cast<std::ptrdiff_t>(c.end));
      }
      return true;
    });
  }

  bool stbl(const Box& x, Mp4Track& t) const {
    return children(x, x.body, [&](const Box& c) {
      if (c.type != "stsd") return true;
      if (!need(c, 8)) return false;
      Box e;
      bool cut = false;
      if (be(c.body + 4, 4) == 0 || !next(c.body + 8, c.end, e, cut)) {
        error = "the stsd at byte " + std::to_string(c.at) + " has no sample entry";
        return false;
      }
      return entry(e, t);
    });
  }

  bool trak(const Box& k, Mp4Track& t) const {
    return children(k, k.body, [&](const Box& c) {
      if (c.type == "tkhd") {
        if (!need(c, 16)) return false;
        const bool v1 = b[c.body] == 1;
        t.id = static_cast<uint32_t>(be(c.body + (v1 ? 20 : 12), 4));
      } else if (c.type == "edts") {
        return children(c, c.body, [&](const Box& e) {
          if (e.type != "elst") return true;
          if (!need(e, 20)) return false;
          if (be(e.body + 4, 4) > 0) {
            t.edit_start = static_cast<int32_t>(be(e.body + 12, 4));
          }
          return true;
        });
      } else if (c.type == "mdia") {
        return children(c, c.body, [&](const Box& m) {
          if (m.type == "mdhd") {
            if (!need(m, 16)) return false;
            const bool v1 = b[m.body] == 1;
            t.timescale = be(m.body + (v1 ? 20 : 12), 4);
          } else if (m.type == "hdlr") {
            if (!need(m, 12)) return false;
            t.handler = four(m.body + 8);
          } else if (m.type == "minf") {
            return children(m, m.body,
                            [&](const Box& n) { return n.type == "stbl" ? stbl(n, t) : true; });
          }
          return true;
        });
      }
      return true;
    });
  }

  bool moov(const Box& x, Mp4File& f) const {
    return children(x, x.body, [&](const Box& c) {
      if (c.type == "mvhd") {
        if (!need(c, 16)) return false;
        const bool v1 = b[c.body] == 1;
        f.movie_timescale = be(c.body + (v1 ? 20 : 12), 4);
      } else if (c.type == "trak") {
        Mp4Track t;
        if (!trak(c, t)) return false;
        f.tracks.push_back(std::move(t));
      } else if (c.type == "mvex") {
        return children(c, c.body, [&](const Box& m) {
          if (m.type != "mehd") return true;
          const bool v1 = b[m.body] == 1;
          if (!need(m, v1 ? 12 : 8)) return false;
          f.fragment_duration = be(m.body + 4, v1 ? 8 : 4);
          return true;
        });
      }
      return true;
    });
  }

  // A fragment's samples, their offsets from the file's start.
  bool moof(const Box& x, const Mp4File& f, std::vector<Mp4Sample>& out) const {
    return children(x, x.body, [&](const Box& c) {
      if (c.type != "traf") return true;
      uint32_t id = 0;
      int64_t base = 0;
      const Mp4Track* track = nullptr;
      return children(c, c.body, [&](const Box& r) {
        if (r.type == "tfhd") {
          if (!need(r, 8)) return false;
          id = static_cast<uint32_t>(be(r.body + 4, 4));
          for (const Mp4Track& t : f.tracks) {
            if (t.id == id) track = &t;
          }
          if (!track) {
            error = "the traf at byte " + std::to_string(c.at) + " names track " +
                    std::to_string(id) + ", which the moov has not";
            return false;
          }
        } else if (r.type == "tfdt") {
          const bool v1 = b[r.body] == 1;
          if (!need(r, v1 ? 12 : 8)) return false;
          base = static_cast<int64_t>(be(r.body + 4, v1 ? 8 : 4));
        } else if (r.type == "trun") {
          if (!track) {
            error = "the trun at byte " + std::to_string(r.at) + " comes before its tfhd";
            return false;
          }
          if (!need(r, 8)) return false;
          const uint32_t flags = static_cast<uint32_t>(be(r.body, 4)) & 0xFFFFFF;
          const uint64_t n = be(r.body + 4, 4);
          if ((flags & 0x701) != 0x701) {
            error = "the trun at byte " + std::to_string(r.at) +
                    " lacks a data offset or each sample's duration, size or flags";
            return false;
          }
          size_t at = r.body + 8;
          const int64_t data = static_cast<int32_t>(be(at, 4));
          at += 4;
          if (flags & 0x4) at += 4;
          const size_t per = 12 + ((flags & 0x800) ? 4 : 0);
          if (!fits(at, static_cast<size_t>(n) * per, r.end)) {
            error = "the trun at byte " + std::to_string(r.at) + " has fewer samples than " +
                    std::to_string(n);
            return false;
          }
          size_t offset = x.at + static_cast<size_t>(data);
          int64_t t = base;
          for (uint64_t i = 0; i < n; i++, at += per) {
            Mp4Sample s;
            s.track = id;
            s.t = t;
            s.duration = static_cast<int64_t>(be(at, 4));
            s.size = static_cast<size_t>(be(at + 4, 4));
            s.key = (be(at + 8, 4) & 0x00010000) == 0;
            s.offset = offset;
            s.ms = track->timescale > 0 ? t * 1000 / static_cast<int64_t>(track->timescale) : 0;
            out.push_back(s);
            offset += s.size;
            t += s.duration;
          }
          base = t;
        }
        return true;
      });
    });
  }

  bool mfra(const Box& x, Mp4File& f) const {
    return children(x, x.body, [&](const Box& c) {
      if (c.type != "tfra") return true;
      if (!need(c, 16)) return false;
      const bool v1 = b[c.body] == 1;
      const uint32_t id = static_cast<uint32_t>(be(c.body + 4, 4));
      const uint32_t sizes = static_cast<uint32_t>(be(c.body + 8, 4));
      const size_t traf = ((sizes >> 4) & 3) + 1, trun = ((sizes >> 2) & 3) + 1,
                   sample = (sizes & 3) + 1;
      const uint64_t n = be(c.body + 12, 4);
      const size_t per = (v1 ? 16 : 8) + traf + trun + sample;
      size_t at = c.body + 16;
      if (!fits(at, static_cast<size_t>(n) * per, c.end)) {
        error = "the tfra at byte " + std::to_string(c.at) + " has fewer entries than " +
                std::to_string(n);
        return false;
      }
      for (uint64_t i = 0; i < n; i++, at += per) {
        Mp4Seek s;
        s.track = id;
        s.t = static_cast<int64_t>(be(at, v1 ? 8 : 4));
        s.moof = static_cast<int64_t>(be(at + (v1 ? 8 : 4), v1 ? 8 : 4));
        f.seeks.push_back(s);
      }
      return true;
    });
  }
};

}  // namespace

std::optional<Mp4File> read_mp4(std::span<const uint8_t> bytes, std::string& error) {
  Reader r{bytes, error};
  Mp4File f;
  size_t at = 0;
  Box x;
  bool cut = false;
  std::vector<Mp4Sample> pending;
  int64_t pending_moof = -1;
  while (r.next(at, bytes.size(), x, cut)) {
    if (x.type == "ftyp") {
      if (!r.need(x, 8)) return std::nullopt;
      f.major = r.four(x.body);
      for (size_t i = x.body + 8; i + 4 <= x.end; i += 4) f.brands.push_back(r.four(i));
    } else if (x.type == "moov") {
      if (!r.moov(x, f)) return std::nullopt;
    } else if (x.type == "moof") {
      pending.clear();
      if (!r.moof(x, f, pending)) return std::nullopt;
      pending_moof = static_cast<int64_t>(x.at);
    } else if (x.type == "mdat") {
      if (pending_moof < 0) {
        error = "the mdat at byte " + std::to_string(x.at) + " follows no moof";
        return std::nullopt;
      }
      for (const Mp4Sample& s : pending) {
        if (s.offset < x.body || s.size > x.end - s.offset || s.offset > x.end) {
          error = "a sample of the fragment at byte " + std::to_string(pending_moof) +
                  " lies outside the mdat after it";
          return std::nullopt;
        }
      }
      f.samples.insert(f.samples.end(), pending.begin(), pending.end());
      f.fragments.push_back(pending_moof);
      pending.clear();
      pending_moof = -1;
    } else if (x.type == "mfra") {
      if (!r.mfra(x, f)) return std::nullopt;
    }
    at = x.end;
  }
  f.cut = cut;
  if (f.tracks.empty()) {
    error = "no moov with a track (the file is " + std::to_string(bytes.size()) + " bytes)";
    return std::nullopt;
  }
  return f;
}

}  // namespace ge::capture
