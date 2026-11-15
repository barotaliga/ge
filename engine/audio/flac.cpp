// The FLAC writer and reader (flac.hpp).
//
// ge:summary FLAC's frames, subframes, Rice partitions and CRCs, written and read
// ge:keywords flac encode decode rice fixed-predictor lpc mid-side crc
// ge:skill audio
// ge:test tests/audio_flac_test.cpp
#include "audio/flac.hpp"

#include <algorithm>
#include <array>

namespace ge::audio {

namespace {

constexpr int kBlock = 4096;

struct Crc {
  std::array<uint8_t, 256> c8{};
  std::array<uint16_t, 256> c16{};
  Crc() {
    for (int i = 0; i < 256; i++) {
      uint8_t a = uint8_t(i);
      for (int b = 0; b < 8; b++) a = uint8_t((a & 0x80) ? (a << 1) ^ 0x07 : a << 1);
      c8[size_t(i)] = a;
      uint16_t w = uint16_t(i << 8);
      for (int b = 0; b < 8; b++) w = uint16_t((w & 0x8000) ? (w << 1) ^ 0x8005 : w << 1);
      c16[size_t(i)] = w;
    }
  }
};
const Crc& crc() {
  static const Crc c;
  return c;
}
uint8_t crc8(const uint8_t* p, size_t n) {
  uint8_t v = 0;
  for (size_t i = 0; i < n; i++) v = crc().c8[v ^ p[i]];
  return v;
}
uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t v = 0;
  for (size_t i = 0; i < n; i++) v = uint16_t((v << 8) ^ crc().c16[(v >> 8) ^ p[i]]);
  return v;
}

struct Bits {
  std::vector<uint8_t>& out;
  uint64_t acc = 0;
  int n = 0;
  void put(uint64_t v, int bits) {
    for (int left = bits; left > 0;) {
      const int take = std::min(left, 32);
      left -= take;
      acc = (acc << take) | ((v >> left) & ((uint64_t(1) << take) - 1));
      n += take;
      while (n >= 8) {
        n -= 8;
        out.push_back(uint8_t(acc >> n));
      }
    }
  }
  void unary(uint32_t zeros) {
    while (zeros >= 32) {
      put(0, 32);
      zeros -= 32;
    }
    put(1, int(zeros) + 1);
  }
  void align() {
    if (n > 0) put(0, 8 - n);
  }
};

uint32_t zigzag(int32_t v) { return v >= 0 ? uint32_t(v) << 1 : (uint32_t(-(v + 1)) << 1) | 1; }

void residual(const int32_t* x, int n, int order, int32_t* r) {
  for (int i = order; i < n; i++) {
    int64_t e = x[i];
    switch (order) {
      case 1:
        e -= x[i - 1];
        break;
      case 2:
        e -= 2 * int64_t(x[i - 1]) - x[i - 2];
        break;
      case 3:
        e -= 3 * int64_t(x[i - 1]) - 3 * int64_t(x[i - 2]) + x[i - 3];
        break;
      case 4:
        e -= 4 * int64_t(x[i - 1]) - 6 * int64_t(x[i - 2]) + 4 * int64_t(x[i - 3]) - x[i - 4];
        break;
      default:
        break;
    }
    r[i] = int32_t(e);
  }
}

struct Plan {
  int order = 0;  // -1 verbatim, -2 constant
  int porder = 0;
  std::vector<uint8_t> k;
  uint64_t bits = 0;
};

// The cheapest partitioning of r[order..n), estimated from the partitions' sums.
void rice_plan(const int32_t* r, int n, int order, Plan* p) {
  int maxp = 0;
  while (maxp < 8 && (n % (2 << maxp)) == 0 && (n >> (maxp + 1)) > order) maxp++;
  const int parts = 1 << maxp, ps = n >> maxp;
  std::vector<uint64_t> sum(size_t(parts), 0), cnt(size_t(parts), 0);
  for (int i = order; i < n; i++) {
    sum[size_t(i / ps)] += zigzag(r[i]);
    cnt[size_t(i / ps)]++;
  }
  p->bits = ~uint64_t(0);
  for (int po = maxp; po >= 0; po--) {
    const int np = 1 << po, step = parts / np;
    uint64_t total = 6;
    std::vector<uint8_t> ks(static_cast<size_t>(np));
    for (int j = 0; j < np; j++) {
      uint64_t s = 0, c = 0;
      for (int q = 0; q < step; q++) {
        s += sum[size_t(j * step + q)];
        c += cnt[size_t(j * step + q)];
      }
      uint64_t best = ~uint64_t(0);
      for (int k = 0; k <= 30; k++) {
        const uint64_t b = (s >> k) + c * uint64_t(k + 1);
        if (b < best) {
          best = b;
          ks[size_t(j)] = uint8_t(k);
        }
        if ((s >> k) == 0) break;
      }
      total += 5 + best;
    }
    if (total < p->bits) {
      p->bits = total;
      p->porder = po;
      p->k = ks;
    }
    // Merge for the next order: the sums are kept at the finest level and summed per step.
  }
}

Plan plan_channel(const int32_t* x, int n, int bps, std::vector<int32_t>& r) {
  Plan best;
  best.order = -1;
  best.bits = uint64_t(n) * uint64_t(bps);
  if (std::all_of(x, x + n, [&](int32_t v) { return v == x[0]; })) {
    best.order = -2;
    best.bits = uint64_t(bps);
    return best;
  }
  for (int order = 0; order <= 4 && order < n; order++) {
    residual(x, n, order, r.data());
    Plan p;
    p.order = order;
    rice_plan(r.data(), n, order, &p);
    p.bits += uint64_t(order) * uint64_t(bps);
    if (p.bits < best.bits) best = p;
  }
  return best;
}

void write_subframe(Bits& w, const int32_t* x, int n, int bps, const Plan& p,
                    std::vector<int32_t>& r) {
  if (p.order == -2) {
    w.put(0, 8);  // pad, constant, no wasted bits
    w.put(uint64_t(uint32_t(x[0])), bps);
    return;
  }
  if (p.order == -1) {
    w.put(0x02, 8);  // pad, verbatim, no wasted bits
    for (int i = 0; i < n; i++) w.put(uint64_t(uint32_t(x[i])), bps);
    return;
  }
  w.put(uint64_t(0x10 | (p.order << 1)), 8);  // pad, fixed of the order, no wasted bits
  for (int i = 0; i < p.order; i++) w.put(uint64_t(uint32_t(x[i])), bps);
  residual(x, n, p.order, r.data());
  w.put(1, 2);  // Rice with 5-bit parameters
  w.put(uint64_t(p.porder), 4);
  const int ps = n >> p.porder;
  for (int j = 0; j < (1 << p.porder); j++) {
    const int k = p.k[size_t(j)];
    w.put(uint64_t(k), 5);
    for (int i = std::max(j * ps, p.order); i < (j + 1) * ps; i++) {
      const uint32_t u = zigzag(r[i]);
      w.unary(u >> k);
      if (k > 0) w.put(u & ((uint64_t(1) << k) - 1), k);
    }
  }
}

void utf8_number(std::vector<uint8_t>& out, uint64_t v) {
  if (v < 0x80) {
    out.push_back(uint8_t(v));
    return;
  }
  int extra = v < 0x800 ? 1 : v < 0x10000 ? 2 : v < 0x200000 ? 3 : v < 0x4000000 ? 4 : 5;
  out.push_back(uint8_t((0xff00 >> (extra + 1)) | (v >> (6 * extra))));
  for (int i = extra - 1; i >= 0; i--) out.push_back(uint8_t(0x80 | ((v >> (6 * i)) & 0x3f)));
}

void put24(std::vector<uint8_t>& o, uint32_t v) {
  o.push_back(uint8_t(v >> 16));
  o.push_back(uint8_t(v >> 8));
  o.push_back(uint8_t(v));
}
void put32le(std::vector<uint8_t>& o, uint32_t v) {
  for (int i = 0; i < 4; i++) o.push_back(uint8_t(v >> (8 * i)));
}

}  // namespace

std::vector<uint8_t> flac_encode(std::span<const int32_t> samples, int channels, int rate, int bits,
                                 const std::vector<std::pair<std::string, std::string>>& tags) {
  std::vector<uint8_t> out = {'f', 'L', 'a', 'C'};
  const uint64_t frames = samples.size() / size_t(channels);
  // STREAMINFO.
  out.push_back(0x00);
  put24(out, 34);
  {
    Bits w{out};
    w.put(kBlock, 16);
    w.put(kBlock, 16);
    w.put(0, 24);
    w.put(0, 24);
    w.put(uint64_t(rate), 20);
    w.put(uint64_t(channels - 1), 3);
    w.put(uint64_t(bits - 1), 5);
    w.put(frames, 36);
    for (int i = 0; i < 4; i++) w.put(0, 32);  // MD5 unset
  }
  // VORBIS_COMMENT, the last block.
  std::vector<uint8_t> vc;
  const std::string vendor = "ge";
  put32le(vc, uint32_t(vendor.size()));
  vc.insert(vc.end(), vendor.begin(), vendor.end());
  put32le(vc, uint32_t(tags.size()));
  for (const auto& [k, v] : tags) {
    const std::string f = k + "=" + v;
    put32le(vc, uint32_t(f.size()));
    vc.insert(vc.end(), f.begin(), f.end());
  }
  out.push_back(0x84);
  put24(out, uint32_t(vc.size()));
  out.insert(out.end(), vc.begin(), vc.end());

  std::vector<std::vector<int32_t>> ch(static_cast<size_t>(channels), std::vector<int32_t>(kBlock));
  std::vector<int32_t> side(kBlock), mid(kBlock), r(kBlock);
  uint64_t number = 0;
  for (uint64_t at = 0; at < frames; at += kBlock, number++) {
    const int n = int(std::min<uint64_t>(kBlock, frames - at));
    for (int i = 0; i < n; i++)
      for (int c = 0; c < channels; c++)
        ch[size_t(c)][size_t(i)] =
            samples[size_t((at + uint64_t(i)) * uint64_t(channels)) + size_t(c)];
    // The channel layout: independent, or for stereo the cheapest of the four.
    int assign = channels - 1;
    std::vector<std::pair<const int32_t*, int>> subs;  // samples, bits
    std::vector<Plan> plans;
    for (int c = 0; c < channels; c++) {
      subs.emplace_back(ch[size_t(c)].data(), bits);
      plans.push_back(plan_channel(subs.back().first, n, bits, r));
    }
    if (channels == 2) {
      for (int i = 0; i < n; i++) {
        side[size_t(i)] = ch[0][size_t(i)] - ch[1][size_t(i)];
        mid[size_t(i)] = (ch[0][size_t(i)] + ch[1][size_t(i)]) >> 1;
      }
      const Plan ps = plan_channel(side.data(), n, bits + 1, r);
      const Plan pm = plan_channel(mid.data(), n, bits, r);
      const uint64_t costs[4] = {plans[0].bits + plans[1].bits, plans[0].bits + ps.bits,
                                 ps.bits + plans[1].bits, pm.bits + ps.bits};
      const int pick = int(std::min_element(costs, costs + 4) - costs);
      if (pick == 1) {
        assign = 8;
        subs = {{ch[0].data(), bits}, {side.data(), bits + 1}};
        plans = {plans[0], ps};
      } else if (pick == 2) {
        assign = 9;
        subs = {{side.data(), bits + 1}, {ch[1].data(), bits}};
        plans = {ps, plans[1]};
      } else if (pick == 3) {
        assign = 10;
        subs = {{mid.data(), bits}, {side.data(), bits + 1}};
        plans = {pm, ps};
      }
    }
    const size_t start = out.size();
    out.push_back(0xff);
    out.push_back(0xf8);
    out.push_back(uint8_t((n == kBlock ? 12 : 7) << 4));
    out.push_back(uint8_t(assign << 4));
    utf8_number(out, number);
    if (n != kBlock) {
      out.push_back(uint8_t((n - 1) >> 8));
      out.push_back(uint8_t(n - 1));
    }
    out.push_back(crc8(&out[start], out.size() - start));
    Bits w{out};
    for (size_t c = 0; c < subs.size(); c++)
      write_subframe(w, subs[c].first, n, subs[c].second, plans[c], r);
    w.align();
    const uint16_t c16 = crc16(&out[start], out.size() - start);
    out.push_back(uint8_t(c16 >> 8));
    out.push_back(uint8_t(c16));
  }
  return out;
}

namespace {

struct Reader {
  std::span<const uint8_t> b;
  uint64_t pos = 0;  // in bits
  bool bad = false;
  uint32_t get(int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
      if ((pos >> 3) >= b.size()) {
        bad = true;
        return 0;
      }
      v = (v << 1) | ((b[size_t(pos >> 3)] >> (7 - (pos & 7))) & 1);
      pos++;
    }
    return uint32_t(v);
  }
  int32_t sget(int n) {
    if (n == 0) return 0;
    const uint32_t v = get(n);
    return n == 32 ? int32_t(v) : int32_t(v << (32 - n)) >> (32 - n);
  }
  uint32_t unary() {
    uint32_t z = 0;
    while (!bad && get(1) == 0) z++;
    return z;
  }
  void align() { pos = (pos + 7) & ~uint64_t(7); }
};

bool read_residual(Reader& rd, int n, int order, int32_t* r) {
  const uint32_t method = rd.get(2);
  if (method > 1) return false;
  const int pbits = method == 0 ? 4 : 5, escape = method == 0 ? 15 : 31;
  const int po = int(rd.get(4));
  const int ps = n >> po;
  if ((ps << po) != n || ps < order) return false;
  for (int j = 0; j < (1 << po); j++) {
    const int k = int(rd.get(pbits));
    const int from = j == 0 ? order : j * ps, to = (j + 1) * ps;
    if (k == escape) {
      const int raw = int(rd.get(5));
      for (int i = from; i < to; i++) r[i] = rd.sget(raw);
    } else {
      for (int i = from; i < to; i++) {
        const uint32_t u = (rd.unary() << k) | rd.get(k);
        r[i] = (u & 1) ? -int32_t(u >> 1) - 1 : int32_t(u >> 1);
      }
    }
    if (rd.bad) return false;
  }
  return true;
}

bool read_subframe(Reader& rd, int n, int bps, int32_t* x) {
  rd.get(1);
  const int type = int(rd.get(6));
  int wasted = 0;
  if (rd.get(1)) wasted = int(rd.unary()) + 1;
  bps -= wasted;
  if (bps <= 0 || bps > 32) return false;
  if (type == 0) {
    const int32_t v = rd.sget(bps);
    std::fill(x, x + n, v);
  } else if (type == 1) {
    for (int i = 0; i < n; i++) x[i] = rd.sget(bps);
  } else if (type >= 8 && type <= 12) {
    const int order = type - 8;
    if (order > n) return false;
    for (int i = 0; i < order; i++) x[i] = rd.sget(bps);
    if (!read_residual(rd, n, order, x)) return false;
    for (int i = order; i < n; i++) {
      int64_t p = 0;
      switch (order) {
        case 1:
          p = x[i - 1];
          break;
        case 2:
          p = 2 * int64_t(x[i - 1]) - x[i - 2];
          break;
        case 3:
          p = 3 * int64_t(x[i - 1]) - 3 * int64_t(x[i - 2]) + x[i - 3];
          break;
        case 4:
          p = 4 * int64_t(x[i - 1]) - 6 * int64_t(x[i - 2]) + 4 * int64_t(x[i - 3]) - x[i - 4];
          break;
        default:
          break;
      }
      x[i] = int32_t(x[i] + p);
    }
  } else if (type >= 32) {
    const int order = type - 31;
    if (order > n) return false;
    for (int i = 0; i < order; i++) x[i] = rd.sget(bps);
    const int prec = int(rd.get(4)) + 1;
    if (prec == 16) return false;
    const int shift = rd.sget(5);
    if (shift < 0) return false;
    int32_t coef[32];
    for (int i = 0; i < order; i++) coef[i] = rd.sget(prec);
    if (!read_residual(rd, n, order, x)) return false;
    for (int i = order; i < n; i++) {
      int64_t s = 0;
      for (int j = 0; j < order; j++) s += int64_t(coef[j]) * x[i - 1 - j];
      x[i] = int32_t(x[i] + (s >> shift));
    }
  } else {
    return false;
  }
  if (wasted)
    for (int i = 0; i < n; i++) x[i] = int32_t(uint32_t(x[i]) << wasted);
  return !rd.bad;
}

}  // namespace

bool flac_decode(std::span<const uint8_t> bytes, std::vector<int32_t>* samples, FlacInfo* info,
                 std::string* error) {
  *info = FlacInfo();
  samples->clear();
  const auto fail = [&](const std::string& why) {
    *error = why;
    return false;
  };
  if (bytes.size() < 8 ||
      std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) != "fLaC")
    return fail("not a FLAC file (no fLaC mark at its start)");
  size_t at = 4;
  bool streaminfo = false;
  for (bool last = false; !last;) {
    if (at + 4 > bytes.size()) return fail("the FLAC file ends inside its metadata");
    last = (bytes[at] & 0x80) != 0;
    const int type = bytes[at] & 0x7f;
    const size_t len = size_t(bytes[at + 1]) << 16 | size_t(bytes[at + 2]) << 8 | bytes[at + 3];
    at += 4;
    if (at + len > bytes.size()) return fail("a FLAC metadata block runs past the file's end");
    const uint8_t* p = &bytes[at];
    if (type == 0 && len >= 34) {
      Reader rd{bytes.subspan(at, 34)};
      rd.get(16 + 16 + 24 + 24);
      info->rate = int(rd.get(20));
      info->channels = int(rd.get(3)) + 1;
      info->bits = int(rd.get(5)) + 1;
      info->frames = uint64_t(rd.get(4)) << 32 | rd.get(32);
      streaminfo = true;
    } else if (type == 4 && len >= 8) {
      const auto le = [&](size_t o) {
        return uint32_t(p[o]) | uint32_t(p[o + 1]) << 8 | uint32_t(p[o + 2]) << 16 |
               uint32_t(p[o + 3]) << 24;
      };
      size_t o = 4 + le(0);
      const uint32_t count = o + 4 <= len ? le(o) : 0;
      o += 4;
      for (uint32_t i = 0; i < count && o + 4 <= len; i++) {
        const uint32_t fl = le(o);
        o += 4;
        if (o + fl > len) break;
        const std::string f(reinterpret_cast<const char*>(p + o), fl);
        o += fl;
        const size_t eq = f.find('=');
        if (eq != std::string::npos) info->tags.emplace_back(f.substr(0, eq), f.substr(eq + 1));
      }
    }
    at += len;
  }
  if (!streaminfo) return fail("the FLAC file has no STREAMINFO block");
  static const int kBits[] = {0, 8, 12, 0, 16, 20, 24, 32};
  std::vector<std::vector<int32_t>> ch;
  uint64_t number = 0;
  while (at + 2 <= bytes.size()) {
    if (bytes[at] != 0xff || (bytes[at + 1] & 0xfe) != 0xf8) {
      at++;  // trailing tags or padding
      continue;
    }
    const size_t start = at;
    const auto bad = [&](const std::string& why) {
      return fail("FLAC frame " + std::to_string(number) + " (byte " + std::to_string(start) +
                  "): " + why + "; the file is damaged, take it again from its source");
    };
    Reader rd{bytes.subspan(at)};
    rd.get(16);
    const int bsc = int(rd.get(4)), rc = int(rd.get(4)), cc = int(rd.get(4)), sc = int(rd.get(3));
    rd.get(1);
    // The frame or sample number, UTF-8 coded.
    uint32_t first = rd.get(8);
    int extra = 0;
    while (extra < 7 && (first & (0x80u >> extra))) extra++;
    if (extra == 1 || extra > 7) return bad("its number does not read");
    for (int i = 1; i < extra; i++) rd.get(8);
    int n = 0;
    if (bsc == 1)
      n = 192;
    else if (bsc >= 2 && bsc <= 5)
      n = 576 << (bsc - 2);
    else if (bsc == 6)
      n = int(rd.get(8)) + 1;
    else if (bsc == 7)
      n = int(rd.get(16)) + 1;
    else if (bsc >= 8)
      n = 256 << (bsc - 8);
    else
      return bad("a reserved block size");
    if (rc == 12)
      rd.get(8);
    else if (rc == 13 || rc == 14)
      rd.get(16);
    else if (rc == 15)
      return bad("an invalid sample rate");
    const int bps = sc == 0 ? info->bits : kBits[sc];
    if (bps == 0) return bad("a reserved sample size");
    const size_t hlen = size_t(rd.pos >> 3);
    if (rd.bad || start + hlen >= bytes.size()) return bad("its header runs past the file's end");
    if (crc8(&bytes[start], hlen) != bytes[start + hlen]) return bad("its header's CRC-8 fails");
    rd.get(8);
    const int nch = cc < 8 ? cc + 1 : 2;
    if (cc > 10 || nch != info->channels) return bad("a channel layout the stream does not have");
    ch.assign(size_t(nch), std::vector<int32_t>(size_t(n)));
    for (int c = 0; c < nch; c++) {
      const bool is_side = (cc == 8 && c == 1) || (cc == 9 && c == 0) || (cc == 10 && c == 1);
      if (!read_subframe(rd, n, bps + (is_side ? 1 : 0), ch[size_t(c)].data()))
        return bad("subframe " + std::to_string(c) + " does not read");
    }
    rd.align();
    const size_t flen = size_t(rd.pos >> 3);
    if (start + flen + 2 > bytes.size()) return bad("it runs past the file's end");
    const uint16_t want = uint16_t(bytes[start + flen] << 8 | bytes[start + flen + 1]);
    if (crc16(&bytes[start], flen) != want) return bad("its CRC-16 fails");
    at = start + flen + 2;
    for (int i = 0; i < n; i++) {
      int32_t a = ch[0][size_t(i)], b = nch > 1 ? ch[1][size_t(i)] : 0;
      if (cc == 8)
        b = a - b;
      else if (cc == 9)
        a = a + b;
      else if (cc == 10) {
        const int64_t m = (int64_t(a) << 1) | (b & 1);
        a = int32_t((m + b) >> 1);
        b = int32_t((m - b) >> 1);
      }
      samples->push_back(a);
      if (nch > 1) samples->push_back(b);
      for (int c = 2; c < nch; c++) samples->push_back(ch[size_t(c)][size_t(i)]);
    }
    number++;
  }
  if (info->frames == 0) info->frames = samples->size() / size_t(info->channels);
  if (samples->size() != info->frames * uint64_t(info->channels))
    return fail("the FLAC file holds " + std::to_string(samples->size() / size_t(info->channels)) +
                " samples a channel, its STREAMINFO says " + std::to_string(info->frames) +
                "; the file is cut short, take it again from its source");
  return true;
}

}  // namespace ge::audio
