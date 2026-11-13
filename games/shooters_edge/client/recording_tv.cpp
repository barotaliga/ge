// A growing recording broadcast on the replay file (recording_tv.hpp).
//
// ge:summary The growing recording's broadcast: bytes in, one-second zstd blocks out
// ge:keywords watcher broadcast growing-recording stream_add recording-track live
// ge:skill replay shooter
// ge:test tests/shooters_edge_classic_watcher_test.cpp
#include "shooters_edge/client/recording_tv.hpp"

#include <string>

namespace shooters_edge::client {

namespace gr = ge::replay;

namespace {

constexpr const char* kGame = "shooters_edge";
constexpr int64_t kBlockMs = 1000;

void close_block(RecordingBroadcast& b) {
  if (b.pending.empty()) return;
  gr::stream_add(b.writer, kRecordingTrack, b.block_ms, b.pending);
  gr::stream_end_block(b.writer, b.blocks == 0);
  b.pending.clear();
  b.blocks++;
}

}  // namespace

void recording_broadcast_begin(RecordingBroadcast& b, const gr::Meta& meta) {
  b.out.clear();
  b.taken = 0;
  b.pending.clear();
  b.started_ms = -1;
  b.block_ms = b.blocks = b.bytes = b.last_ms = 0;
  b.ended = false;
  gr::StreamHeader h;
  h.game = kGame;
  h.meta = {{"format", "bytes"}, {"bytes", "0"}, {"keyframe_seconds", "0"}};
  h.meta.insert(h.meta.end(), meta.begin(), meta.end());
  h.tracks = {{kRecordingTrack, gr::TrackKind::messages, gr::TrackCodec::zstd, "recording"}};
  gr::stream_begin(b.writer, h, b.out);
}

void recording_broadcast_add(RecordingBroadcast& b, std::span<const uint8_t> bytes) {
  if (b.ended) return;
  b.pending.insert(b.pending.end(), bytes.begin(), bytes.end());
  b.bytes += static_cast<int64_t>(bytes.size());
}

void recording_broadcast_tick(RecordingBroadcast& b, int64_t now_ms) {
  if (b.ended) return;
  if (b.started_ms < 0) b.started_ms = now_ms;
  b.last_ms = now_ms - b.started_ms;
  if (b.last_ms - b.block_ms < kBlockMs) return;
  close_block(b);
  b.block_ms = b.last_ms;
}

void recording_broadcast_end(RecordingBroadcast& b, const gr::Meta& final_meta) {
  if (b.ended) return;
  close_block(b);
  gr::Meta last = {{"duration_ms", std::to_string(b.last_ms)},
                   {"recording_bytes", std::to_string(b.bytes)}};
  last.insert(last.end(), final_meta.begin(), final_meta.end());
  gr::stream_finish(b.writer, last);
  b.ended = true;
}

std::span<const uint8_t> recording_broadcast_fresh(RecordingBroadcast& b) {
  const std::span<const uint8_t> fresh = std::span<const uint8_t>(b.out).subspan(b.taken);
  b.taken = b.out.size();
  return fresh;
}

}  // namespace shooters_edge::client
