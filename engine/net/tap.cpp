// The live tap's frames: a mark and a pcap record (net/tap.hpp).
//
// ge:summary The live tap: pcap records behind a mark, mirrored over loopback UDP
// ge:keywords net tap live mirror frame pcap record loopback
// ge:skill net
// ge:test tests/net_tap_test.cpp
#include "net/tap.hpp"

#include <cstring>

namespace ge::net {

namespace {
constexpr size_t kPcapHeader = 24;  // the file's global header, ahead of every record
}  // namespace

bool open_tap(Tap& tap, uint16_t port, std::string* error) {
  tap = Tap{};
  if (!udp_open(tap.socket, loopback_v4(0), error)) return false;
  tap.to = loopback_v4(port);
  tap.scratch = make_capture();
  return true;
}

void close_tap(Tap& tap) { udp_close(tap.socket); }

void tap_datagram(Tap& tap, int64_t at_us, const Address& from, const Address& to,
                  const uint8_t* data, size_t size) {
  if (tap.socket.handle < 0) return;
  tap.scratch.bytes.resize(kPcapHeader);
  tap.scratch.packets = 0;
  capture_datagram(tap.scratch, at_us, from, to, data, size);
  const size_t record = tap.scratch.bytes.size() - kPcapHeader;
  tap.frame.resize(sizeof kTapMark + record);
  std::memcpy(tap.frame.data(), kTapMark, sizeof kTapMark);
  std::memcpy(tap.frame.data() + sizeof kTapMark, tap.scratch.bytes.data() + kPcapHeader, record);
  if (udp_send(tap.socket, tap.to, tap.frame.data(), tap.frame.size())) {
    tap.mirrored++;
  } else {
    tap.failed++;
  }
}

bool read_tap_frame(const uint8_t* frame, size_t size, CapturedDatagram& out) {
  if (size < sizeof kTapMark || std::memcmp(frame, kTapMark, sizeof kTapMark) != 0) return false;
  const Capture header = make_capture();
  std::vector<uint8_t> file(header.bytes);
  file.insert(file.end(), frame + sizeof kTapMark, frame + size);
  std::vector<CapturedDatagram> ds;
  if (!read_capture(file.data(), file.size(), ds, nullptr) || ds.size() != 1) return false;
  out = std::move(ds.front());
  return true;
}

}  // namespace ge::net
