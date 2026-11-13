// A recording as the sequel's TV (sequel_tv_demo.hpp).
//
// ge:summary A recording of the sequel played into the sequel face at the game's pace, defined
// ge:keywords sequel-tv demo dm2 playback loop
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#include "shooters_edge/net/sequel_tv_demo.hpp"

#include <span>
#include <utility>

namespace shooters_edge::net {

namespace {

constexpr uint8_t kSvcServerData = 12, kSvcFrame = 20;
constexpr int64_t kFrameUs = 100000;  // the sequel's server frame

uint32_t u32_at(const std::vector<uint8_t>& b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 |
         uint32_t(b[at + 3]) << 24;
}

// The block at `at` (its messages), or empty at the end.
std::span<const uint8_t> block_at(const std::vector<uint8_t>& b, size_t at) {
  if (at + 4 > b.size()) return {};
  const uint32_t len = u32_at(b, at);
  if (len == 0xffffffffu || len == 0 || at + 4 + len > b.size()) return {};
  return {b.data() + at + 4, len};
}

}  // namespace

bool sequel_tv_demo_open(SequelTvDemo& d, std::vector<uint8_t> bytes, bool loop,
                         std::string& error) {
  d = SequelTvDemo{};
  d.bytes = std::move(bytes);
  d.loop = loop;
  const std::span<const uint8_t> first = block_at(d.bytes, 0);
  if (first.size() < 10 || first[0] != kSvcServerData) {
    error = "the file is not a recording of the sequel: its first block is not the server data";
    return false;
  }
  return true;
}

void sequel_tv_demo_update(SequelTvDemo& d, SequelTvFace& face, int64_t now_us) {
  if (d.due_us < 0) d.due_us = now_us;
  for (int n = 0; n < 4096 && !d.ended && now_us >= d.due_us; n++) {
    std::span<const uint8_t> block = block_at(d.bytes, d.at);
    if (block.empty()) {
      if (!d.loop || d.at == 0) {
        d.ended = true;
        return;
      }
      d.at = 0;
      d.rounds++;
      continue;
    }
    std::vector<uint8_t> raised;
    if (d.at == 0 && d.rounds > 0) {  // the count raised, so each viewer loads the level again
      raised.assign(block.begin(), block.end());
      const uint32_t count = uint32_t(raised[5]) | uint32_t(raised[6]) << 8 |
                             uint32_t(raised[7]) << 16 | uint32_t(raised[8]) << 24;
      const uint32_t next = count + static_cast<uint32_t>(d.rounds);
      for (size_t i = 0; i < 4; i++) raised[5 + i] = static_cast<uint8_t>(next >> (8 * i));
      block = raised;
    }
    d.at += 4 + block.size();
    const int64_t read = face.blocks;
    sequel_tv_block(face, block);
    if (face.blocks == read) {  // it did not read: the face's log says why
      d.ended = true;
      return;
    }
    for (const client::DemoSpan& s : face.spans)
      if (s.id == kSvcFrame) {
        d.frames++;
        d.due_us += kFrameUs;
        break;
      }
  }
}

}  // namespace shooters_edge::net
