// Quake II's match broadcast on the replay file (q2_tv.hpp).
//
// ge:summary Quake II's broadcast: blocks in, ten a chunk through the face, one-second blocks out
// ge:keywords q2 broadcast chunk keyframe stream_add replay-v2 live
// ge:skill replay shooter
// ge:test tests/shooters_edge_q2_tv_test.cpp
#include "shooters_edge/client/q2_tv.hpp"

#include <string>

namespace shooters_edge::client {

namespace gr = ge::replay;

namespace {

constexpr const char* kGame = "shooters_edge";
constexpr int64_t kChunkMs = 1000;  // ten 10 Hz frames
constexpr size_t kChunkBlocks = 10;

void close_chunk(Q2Broadcast& b) {
  if (b.pending.empty()) return;
  const bool keyframe = b.keyframe_seconds > 0 ? b.chunks % b.keyframe_seconds == 0 : b.chunks == 0;
  std::vector<std::span<const uint8_t>> some;
  some.reserve(b.pending.size());
  for (const std::vector<uint8_t>& block : b.pending) some.emplace_back(block);
  std::vector<uint8_t> chunk;
  pack_q2_chunk(b.coder, some, keyframe, chunk, &b.report);
  gr::stream_add(b.writer, kQ2Track, b.chunks * kChunkMs, chunk);
  gr::stream_end_block(b.writer, keyframe);
  b.pending.clear();
  b.chunks++;
}

}  // namespace

void q2_broadcast_begin(Q2Broadcast& b, int keyframe_seconds, uint64_t bytes,
                        const gr::Meta& meta) {
  b.out.clear();
  b.taken = 0;
  b.keyframe_seconds = keyframe_seconds;
  gr::StreamHeader h;
  h.game = kGame;
  h.meta = {{"format", "q2"},
            {"bytes", std::to_string(bytes)},
            {"keyframe_seconds", std::to_string(keyframe_seconds)}};
  h.meta.insert(h.meta.end(), meta.begin(), meta.end());
  h.tracks = {{kQ2Track, gr::TrackKind::messages, gr::TrackCodec::field, "q2"}};
  gr::stream_begin(b.writer, h, b.out);
}

void q2_broadcast_block(Q2Broadcast& b, std::span<const uint8_t> block) {
  if (b.ended) return;
  b.pending.emplace_back(block.begin(), block.end());
  b.blocks++;
  if (b.pending.size() == kChunkBlocks) close_chunk(b);
}

void q2_broadcast_end(Q2Broadcast& b, const gr::Meta& final_meta) {
  if (b.ended) return;
  close_chunk(b);
  gr::Meta last = {{"duration_ms", std::to_string(b.blocks * 100)},
                   {"records", std::to_string(b.report.blocks)}};
  last.insert(last.end(), final_meta.begin(), final_meta.end());
  gr::stream_finish(b.writer, last);
  b.ended = true;
}

std::span<const uint8_t> q2_broadcast_fresh(Q2Broadcast& b) {
  const std::span<const uint8_t> fresh = std::span<const uint8_t>(b.out).subspan(b.taken);
  b.taken = b.out.size();
  return fresh;
}

}  // namespace shooters_edge::client
