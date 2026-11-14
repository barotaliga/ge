// A client recording played into the classic clients' face (classic_tv_recording.hpp): its records
// walked by their times, packets handed on, commands' angles as the view, round and round.
//
// ge:summary A client recording's records walked by time into the classic clients' face
// ge:keywords classic-tv client-recording records udp_tv_recording_update loop command-size
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_udp_test.cpp
#include "shooters_edge/net/classic_tv_recording.hpp"

#include <cmath>
#include <cstring>
#include <span>
#include <utility>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

namespace {

constexpr uint32_t kMostPacket = 1u << 16;
constexpr int64_t kRoundGapUs = 2000000;
constexpr double kMostGapS = 1;  // a longer silence in the recording plays as this long

float f32_at(const std::vector<uint8_t>& b, size_t at) {
  float v = 0;
  std::memcpy(&v, b.data() + at, 4);  // little-endian
  return v;
}

uint32_t u32_at(const std::vector<uint8_t>& b, size_t at) {
  uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);
  return v;
}

// The record at `at` and its size; 0 when none whole is there.
size_t record_size(const std::vector<uint8_t>& b, size_t at, int cmd_size) {
  if (at + 5 > b.size()) return 0;
  const float t = f32_at(b, at);
  const uint8_t type = b[at + 4];
  if (!std::isfinite(t)) return 0;
  size_t n = 5;
  if (type == 0) {
    n += static_cast<size_t>(cmd_size) + 12;
  } else if (type == 2) {
    n += 8;
  } else if (type == 1) {
    if (at + 9 > b.size()) return 0;
    const uint32_t len = u32_at(b, at + 5);
    if (len < 8 || len > kMostPacket) return 0;
    n += 4 + len;
  } else {
    return 0;
  }
  return at + n <= b.size() ? n : 0;
}

// True when the records from `at` walk whole with `cmd_size` for `count` records or to the end.
bool walks(const std::vector<uint8_t>& b, size_t at, int cmd_size, int count) {
  for (int i = 0; i < count && at < b.size(); i++) {
    const size_t n = record_size(b, at, cmd_size);
    if (n == 0) return false;
    at += n;
  }
  return true;
}

}  // namespace

bool udp_tv_recording_open(UdpTvRecording& d, std::vector<uint8_t> bytes, bool loop,
                           std::string& error) {
  d = UdpTvRecording{};
  d.loop = loop;
  client::Demo demo;
  if (!client::open_demo(std::move(bytes), demo, error)) return false;
  if (demo.state.format != client::DemoFormat::client) {
    error = "the file is not a multiplayer client's recording";
    return false;
  }
  d.bytes = std::move(demo.bytes);
  d.first = d.at = demo.pos;
  d.cmd_size = walks(d.bytes, d.first, 24, 64) ? 24 : 28;
  if (!walks(d.bytes, d.first, d.cmd_size, 64)) {
    error = "the recording's first records do not read: the file is damaged";
    return false;
  }
  if (d.first + 5 <= d.bytes.size()) d.first_time = f32_at(d.bytes, d.first);
  return true;
}

void udp_tv_recording_update(UdpTvRecording& d, ClassicTvUdp& face, int64_t now_us) {
  if (d.start_us < 0) d.start_us = now_us;
  for (int k = 0; k < 4096 && !d.ended; k++) {
    const size_t n = record_size(d.bytes, d.at, d.cmd_size);
    if (n == 0) {
      if (!d.loop || d.packets == 0) {
        d.ended = true;
        return;
      }
      d.at = d.first;
      d.skipped = 0;
      d.rounds++;
      d.start_us = now_us + kRoundGapUs;  // a pause between rounds, however short the recording
      face.count_raise = static_cast<int32_t>(d.rounds);
      continue;
    }
    const double t = static_cast<double>(f32_at(d.bytes, d.at));
    if (d.at == d.first) d.last_time = t;
    if (t - d.last_time > kMostGapS) d.skipped += t - d.last_time - kMostGapS;  // dead air cut
    d.last_time = t;
    const double due = t - static_cast<double>(d.first_time) - d.skipped;
    if (due > static_cast<double>(now_us - d.start_us) / 1e6) return;
    const uint8_t type = d.bytes[d.at + 4];
    if (type == 0) {
      const size_t a = d.at + 5 + static_cast<size_t>(d.cmd_size);
      udp_tv_view(face, d.stream,
                  {f32_at(d.bytes, a), f32_at(d.bytes, a + 4), f32_at(d.bytes, a + 8)});
    } else if (type == 1) {
      const std::span<const uint8_t> packet(d.bytes.data() + d.at + 9, n - 9);
      d.packets++;
      if (!udp_tv_packet(face, d.stream, packet)) {
        d.ended = true;  // the face's log says why
        return;
      }
    }
    d.at += n;
  }
}

}  // namespace shooters_edge::net
