// The HEIF still's boxes written and read (heif.hpp).
//
// ge:summary The HEIF still's boxes: ftyp, meta and its item boxes, mdat; written and parsed
// ge:keywords heif box full-box iloc ipma ispe colr writer reader
// ge:skill replay
// ge:test tests/capture_heif_test.cpp
#include "capture/heif.hpp"

#include <algorithm>
#include <cstring>

namespace ge::capture {

namespace {

void put8(std::vector<uint8_t>& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
void put16(std::vector<uint8_t>& b, uint32_t v) {
  put8(b, v >> 8);
  put8(b, v);
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
  put16(b, v >> 16);
  put16(b, v);
}
void put4cc(std::vector<uint8_t>& b, const char* t) {
  for (int i = 0; i < 4; ++i) put8(b, static_cast<uint8_t>(t[i]));
}

// A box: its size and type, then `body`. Sized once and written in place: GCC 13 at -O3 reads a
// small vector grown by inlined inserts as an overflow (-Wstringop-overflow), which -Werror stops.
std::vector<uint8_t> box(const char* type, const std::vector<uint8_t>& body) {
  const auto size = static_cast<uint32_t>(body.size() + 8);
  std::vector<uint8_t> b(size);
  for (size_t i = 0; i < 4; ++i) {
    b[i] = static_cast<uint8_t>(size >> (24 - 8 * i));
    b[4 + i] = static_cast<uint8_t>(type[i]);
  }
  std::copy(body.begin(), body.end(), b.begin() + 8);
  return b;
}

// A body to write into, grown from room for a small box rather than from a few bytes (the same
// GCC warning as `box`).
std::vector<uint8_t> fresh() {
  std::vector<uint8_t> b;
  b.reserve(64);
  return b;
}

// A full box's body starts with its version and 24 bits of flags.
std::vector<uint8_t> full(uint8_t version, uint32_t flags = 0) {
  std::vector<uint8_t> b{version, static_cast<uint8_t>(flags >> 16),
                         static_cast<uint8_t>(flags >> 8), static_cast<uint8_t>(flags)};
  b.reserve(64);  // the body put after it grows from here, not from a few bytes
  return b;
}

void append(std::vector<uint8_t>& to, const std::vector<uint8_t>& b) {
  to.insert(to.end(), b.begin(), b.end());
}

// The meta box with the item's data at `offset` in the file.
std::vector<uint8_t> meta(const HeifStill& s, uint32_t offset) {
  const bool avif = s.kind == HeifKind::kAvif;
  std::vector<uint8_t> m = full(0);
  std::vector<uint8_t> hdlr = full(0);
  put32(hdlr, 0);
  put4cc(hdlr, "pict");
  for (int i = 0; i < 3; ++i) put32(hdlr, 0);
  put8(hdlr, 0);  // an empty name
  append(m, box("hdlr", hdlr));
  std::vector<uint8_t> pitm = full(0);
  put16(pitm, 1);
  append(m, box("pitm", pitm));
  std::vector<uint8_t> iloc = full(0);
  put8(iloc, 0x44);  // 4-byte offsets and lengths
  put8(iloc, 0x00);  // no base offset
  put16(iloc, 1);    // one item
  put16(iloc, 1);    // item 1
  put16(iloc, 0);    // in this file
  put16(iloc, 1);    // one extent
  put32(iloc, offset);
  put32(iloc, static_cast<uint32_t>(s.data.size()));
  append(m, box("iloc", iloc));
  std::vector<uint8_t> infe = full(2);
  put16(infe, 1);
  put16(infe, 0);
  put4cc(infe, avif ? "av01" : "hvc1");
  put8(infe, 0);
  std::vector<uint8_t> iinf = full(0);
  put16(iinf, 1);
  append(iinf, box("infe", infe));
  append(m, box("iinf", iinf));
  std::vector<uint8_t> ipco = fresh();
  append(ipco, box(avif ? "av1C" : "hvcC", s.config));
  std::vector<uint8_t> ispe = full(0);
  put32(ispe, static_cast<uint32_t>(s.width));
  put32(ispe, static_cast<uint32_t>(s.height));
  append(ipco, box("ispe", ispe));
  std::vector<uint8_t> pixi = full(0);
  put8(pixi, 3);
  for (int i = 0; i < 3; ++i) put8(pixi, 8);
  append(ipco, box("pixi", pixi));
  std::vector<uint8_t> colr = fresh();
  put4cc(colr, "nclx");
  put16(colr, 1);   // BT.709 primaries
  put16(colr, 13);  // the sRGB transfer
  put16(colr, 1);   // the BT.709 matrix
  put8(colr, 0);    // limited range
  append(ipco, box("colr", colr));
  std::vector<uint8_t> ipma = full(0);
  put32(ipma, 1);
  put16(ipma, 1);
  put8(ipma, 4);
  put8(ipma, 0x81);  // the configuration, essential
  put8(ipma, 0x02);
  put8(ipma, 0x03);
  put8(ipma, 0x04);
  std::vector<uint8_t> iprp = box("ipco", ipco);
  append(iprp, box("ipma", ipma));
  append(m, box("iprp", iprp));
  return box("meta", m);
}

struct Reader {
  std::span<const uint8_t> b;
  bool ok(size_t at, size_t n) const { return at <= b.size() && n <= b.size() - at; }
  uint32_t u16(size_t at) const { return uint32_t{b[at]} << 8 | b[at + 1]; }
  uint32_t u32(size_t at) const { return u16(at) << 16 | u16(at + 2); }
  bool is(size_t at, const char* t) const { return std::memcmp(&b[at], t, 4) == 0; }
};

// The child boxes of [begin, end): each one's type at its offset and its body.
template <class Fn>
bool each_box(const Reader& r, size_t begin, size_t end, Fn&& fn) {
  for (size_t at = begin; at < end;) {
    if (!r.ok(at, 8)) return false;
    const size_t size = r.u32(at);
    if (size < 8 || at + size > end) return false;
    fn(at + 4, at + 8, at + size);
    at += size;
  }
  return true;
}

}  // namespace

std::vector<uint8_t> write_heif(const HeifStill& s) {
  const bool avif = s.kind == HeifKind::kAvif;
  std::vector<uint8_t> ftyp = fresh();
  put4cc(ftyp, avif ? "avif" : "heic");
  put32(ftyp, 0);
  put4cc(ftyp, avif ? "avif" : "heic");
  put4cc(ftyp, "mif1");
  put4cc(ftyp, "miaf");
  std::vector<uint8_t> out = box("ftyp", ftyp);
  // The meta box's size does not depend on the offset, so it is measured first.
  const size_t meta_size = meta(s, 0).size();
  const auto offset = static_cast<uint32_t>(out.size() + meta_size + 8);
  append(out, meta(s, offset));
  append(out, box("mdat", s.data));
  return out;
}

bool read_heif(std::span<const uint8_t> bytes, HeifStill& out, std::string& error) {
  const Reader r{bytes};
  out = {};
  bool brand = false, meta_seen = false, typed = false, sized = false;
  uint32_t offset = 0, length = 0;
  const bool boxes = each_box(r, 0, bytes.size(), [&](size_t type, size_t body, size_t end) {
    if (r.is(type, "ftyp") && end - body >= 4) {
      brand = true;
      out.kind = r.is(body, "avif") ? HeifKind::kAvif : HeifKind::kHeic;
    }
    if (!r.is(type, "meta")) return;
    meta_seen = true;
    each_box(r, body + 4, end, [&](size_t t, size_t b, size_t e) {
      if (r.is(t, "iloc") && e - b >= 22 && bytes[b + 4] == 0x44 && bytes[b + 5] == 0) {
        offset = r.u32(b + 14);
        length = r.u32(b + 18);
      } else if (r.is(t, "iinf") && e - b >= 6) {
        each_box(r, b + 6, e, [&](size_t, size_t ib, size_t ie) {
          if (ie - ib >= 12) {
            typed = true;
            out.kind = r.is(ib + 8, "av01") ? HeifKind::kAvif : HeifKind::kHeic;
            if (!r.is(ib + 8, "av01") && !r.is(ib + 8, "hvc1")) typed = false;
          }
        });
      } else if (r.is(t, "iprp")) {
        each_box(r, b, e, [&](size_t pt, size_t pb, size_t pe) {
          if (!r.is(pt, "ipco")) return;
          each_box(r, pb, pe, [&](size_t ct, size_t cb, size_t ce) {
            if (r.is(ct, "av1C") || r.is(ct, "hvcC")) out.config.assign(&bytes[cb], &bytes[ce]);
            if (r.is(ct, "ispe") && ce - cb >= 12) {
              out.width = static_cast<int>(r.u32(cb + 4));
              out.height = static_cast<int>(r.u32(cb + 8));
              sized = true;
            }
          });
        });
      }
    });
  });
  if (!boxes || !brand || !meta_seen) {
    error = "not a HEIF file (its boxes do not read, or it has no ftyp or meta)";
    return false;
  }
  if (!typed || out.config.empty() || !sized) {
    error = "its item is not one AV1 or HEVC picture with a configuration and a size";
    return false;
  }
  if (!r.ok(offset, length) || length == 0) {
    error = "its item's data lies outside the file";
    return false;
  }
  out.data.assign(&bytes[offset], &bytes[offset] + length);
  return true;
}

}  // namespace ge::capture
