// The pcap writer and reader: the global header (magic a1b2c3d4, version 2.4, link type 101,
// RAW IP), each record's header (seconds, microseconds, the captured and the original length),
// and the IPv4 (with its header checksum) or IPv6 and UDP headers round each datagram. The UDP
// checksum is left zero, which IPv4 allows and every packet tool reads.
//
// ge:summary pcap writing and reading: IPv4/IPv6 and UDP headers round each datagram
// ge:keywords net capture pcap linktype-raw ipv4-header ipv6-header udp-header checksum
// ge:skill net
// ge:test tests/net_capture_test.cpp
#include "net/capture.hpp"

#include <fstream>
#include <iterator>

#include "net/wire.hpp"

namespace ge::net {

namespace {

constexpr uint32_t kMagic = 0xa1b2c3d4;
constexpr uint32_t kLinkRaw = 101;

void put16be(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(static_cast<uint8_t>(v >> 8));
  b.push_back(static_cast<uint8_t>(v));
}
void put32le(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// The IP bytes a capture writes for an address: a simulated node in 198.18.0.0/15.
void ip_of(const Address& a, uint8_t out[16]) {
  if (a.family == Family::sim) {
    const uint32_t n = sim_node(a);
    out[0] = 198;
    out[1] = static_cast<uint8_t>(18 + ((n >> 16) & 1));
    out[2] = static_cast<uint8_t>(n >> 8);
    out[3] = static_cast<uint8_t>(n);
    return;
  }
  for (int i = 0; i < 16; i++) out[i] = a.bytes[i];
}

Address address_of(const uint8_t* ip, bool v6, uint16_t port) {
  Address a;
  if (v6) {
    a.family = Family::ipv6;
    for (int i = 0; i < 16; i++) a.bytes[i] = ip[i];
  } else if (ip[0] == 198 && (ip[1] & 0xfe) == 18) {
    a = sim_address(static_cast<uint32_t>((ip[1] & 1) << 16 | ip[2] << 8 | ip[3]), 0);
  } else {
    a.family = Family::ipv4;
    for (int i = 0; i < 4; i++) a.bytes[i] = ip[i];
  }
  a.port = port;
  return a;
}

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }

}  // namespace

Capture make_capture() {
  Capture c;
  put32le(c.bytes, kMagic);
  c.bytes.push_back(2);  // version 2.4, little endian like every other field here
  c.bytes.push_back(0);
  c.bytes.push_back(4);
  c.bytes.push_back(0);
  put32le(c.bytes, 0);      // the zone's offset
  put32le(c.bytes, 0);      // the timestamps' accuracy
  put32le(c.bytes, 65535);  // the snapshot length
  put32le(c.bytes, kLinkRaw);
  return c;
}

void capture_datagram(Capture& c, int64_t at_us, const Address& from, const Address& to,
                      const uint8_t* data, size_t size) {
  const bool v6 = from.family == Family::ipv6 || to.family == Family::ipv6;
  const size_t ip_len = v6 ? 40 : 20;
  const size_t total = ip_len + 8 + size;
  if (c.limit_bytes && c.bytes.size() + 16 + total > c.limit_bytes) {
    c.dropped++;
    return;
  }
  const int64_t t = at_us < 0 ? 0 : at_us;
  put32le(c.bytes, static_cast<uint32_t>(t / 1000000));
  put32le(c.bytes, static_cast<uint32_t>(t % 1000000));
  put32le(c.bytes, static_cast<uint32_t>(total));
  put32le(c.bytes, static_cast<uint32_t>(total));
  uint8_t src[16] = {}, dst[16] = {};
  ip_of(from, src);
  ip_of(to, dst);
  if (v6) {
    c.bytes.push_back(0x60);
    for (int i = 0; i < 3; i++) c.bytes.push_back(0);
    put16be(c.bytes, static_cast<uint16_t>(8 + size));
    c.bytes.push_back(17);  // UDP
    c.bytes.push_back(64);  // hop limit
    c.bytes.insert(c.bytes.end(), src, src + 16);
    c.bytes.insert(c.bytes.end(), dst, dst + 16);
  } else {
    uint8_t h[20] = {0x45,
                     0,
                     static_cast<uint8_t>(total >> 8),
                     static_cast<uint8_t>(total),
                     0,
                     0,
                     0x40,
                     0,
                     64,
                     17,
                     0,
                     0};  // don't fragment, TTL 64, UDP
    for (int i = 0; i < 4; i++) {
      h[12 + i] = src[i];
      h[16 + i] = dst[i];
    }
    uint32_t sum = 0;
    for (int i = 0; i < 20; i += 2) sum += static_cast<uint32_t>(h[i] << 8 | h[i + 1]);
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    const auto check = static_cast<uint16_t>(~sum);
    h[10] = static_cast<uint8_t>(check >> 8);
    h[11] = static_cast<uint8_t>(check);
    c.bytes.insert(c.bytes.end(), h, h + 20);
  }
  put16be(c.bytes, from.port);
  put16be(c.bytes, to.port);
  put16be(c.bytes, static_cast<uint16_t>(8 + size));
  put16be(c.bytes, 0);  // no UDP checksum
  c.bytes.insert(c.bytes.end(), data, data + size);
  c.packets++;
}

bool read_capture(const uint8_t* bytes, size_t size, std::vector<CapturedDatagram>& out,
                  std::string* error) {
  out.clear();
  Reader r = make_reader(bytes, size);
  const uint32_t magic = read_u32(r);
  read_view(r, 16);  // the version, the zone, the accuracy, the snapshot length
  const uint32_t link = read_u32(r);
  if (r.bad || magic != kMagic || link != kLinkRaw) {
    if (error) {
      *error =
          "not a little-endian microsecond pcap of raw IP (link type 101): write it with "
          "ge::net::make_capture, or convert it with editcap first";
    }
    return false;
  }
  size_t index = 0;
  while (remaining(r) > 0) {
    const uint32_t sec = read_u32(r);
    const uint32_t usec = read_u32(r);
    const uint32_t incl = read_u32(r);
    read_u32(r);
    const uint8_t* p = read_view(r, incl);
    const auto bad = [&](const char* what) {
      if (error) {
        *error = "pcap record " + std::to_string(index) + ": " + what +
                 "; only whole UDP datagrams over IPv4 or IPv6 are read";
      }
      return false;
    };
    if (!p) return bad("cut short");
    const bool v6 = incl > 0 && (p[0] >> 4) == 6;
    const size_t ip_len = v6 ? 40 : static_cast<size_t>(incl > 0 ? (p[0] & 0x0f) * 4 : 0);
    if (incl < ip_len + 8 || (!v6 && (p[0] >> 4) != 4)) return bad("not IPv4 or IPv6");
    if (p[v6 ? 6 : 9] != 17) return bad("not UDP");
    const uint8_t* u = p + ip_len;
    const size_t udp_len = be16(u + 4);
    if (udp_len < 8 || ip_len + udp_len > incl) return bad("a UDP length past the record");
    CapturedDatagram d;
    d.at_us = static_cast<int64_t>(sec) * 1000000 + usec;
    d.from = address_of(p + (v6 ? 8 : 12), v6, be16(u));
    d.to = address_of(p + (v6 ? 24 : 16), v6, be16(u + 2));
    d.data.assign(u + 8, u + udp_len);
    out.push_back(std::move(d));
    index++;
  }
  return true;
}

bool save_capture(const Capture& c, const std::string& path, std::string* error) {
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    if (error) *error = "cannot open " + path + " to write: check the directory exists";
    return false;
  }
  f.write(reinterpret_cast<const char*>(c.bytes.data()),
          static_cast<std::streamsize>(c.bytes.size()));
  f.close();
  if (!f && error) *error = "cannot write all of " + path + ": check the disk has room";
  return static_cast<bool>(f);
}

bool load_capture(const std::string& path, std::vector<CapturedDatagram>& out, std::string* error) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    if (error) *error = "cannot open " + path + ": check the path";
    return false;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  std::string why;
  if (!read_capture(bytes.data(), bytes.size(), out, &why)) {
    if (error) *error = path + ": " + why;
    return false;
  }
  return true;
}

}  // namespace ge::net
