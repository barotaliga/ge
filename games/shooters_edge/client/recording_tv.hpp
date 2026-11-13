// A GROWING RECORDING BROADCAST ON THE REPLAY FILE (docs/tv.md § The watcher): a recording that is
// still being written (a client's recording, kept by a spectator on a server with no broadcaster)
// handed to a TV node (engine/tv/node.hpp) as a replay file version 2 while it grows.
//
// The recording's bytes are taken as they come and closed as a block of the `recording` track
// (zstd) once a second of the recording's own clock, the track `pack_demo` keeps a client
// recording on, so `unpack_demo` gives the recording back byte for byte, whether the file is
// finished or still growing. The bytes are not walked: the track is kept as it was written.
//
// A LATE VIEWER: the recording's messages are deltas against what came before them, so only the
// first block is a keyframe and a viewer who joins late is sent the match from its start, as a
// sequel broadcast's is (sequel_tv.hpp).
//
//   recording_broadcast_begin(b, meta)       then, as the recording grows:
//   recording_broadcast_add(b, bytes)        recording_broadcast_tick(b, now_ms)  (closes a second)
//   recording_broadcast_fresh(b)             the bytes a TV node has not had (`tv_feed`)
//   recording_broadcast_end(b)               the last block and the trailer
//
// ge:summary A recording still being written, broadcast as a replay file v2 in one-second blocks
// ge:keywords watcher broadcast growing-recording tv replay-v2 stream late-viewer recording-track
// ge:keywords recording_broadcast_begin recording_broadcast_add recording_broadcast_tick
// ge:skill replay shooter
// ge:test tests/shooters_edge_classic_watcher_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "replay/stream.hpp"

namespace shooters_edge::client {

// The replay file's track a recording kept as its bytes is on (`pack_demo`'s too).
inline constexpr uint32_t kRecordingTrack = 2;

// Not moved once begun: the writer appends to `out`.
struct RecordingBroadcast {
  std::vector<uint8_t> out;  // the file so far
  size_t taken = 0;          // of `out`, the bytes `recording_broadcast_fresh` handed over
  ge::replay::StreamWriter writer;
  std::vector<uint8_t> pending;  // this second's bytes so far
  int64_t started_ms = -1, block_ms = 0, blocks = 0, bytes = 0, last_ms = 0;
  bool ended = false;
};

// The broadcast begun: the replay file's header in `out` (format "bytes", the size 0: live).
void recording_broadcast_begin(RecordingBroadcast& b, const ge::replay::Meta& meta = {});
// More of the recording.
void recording_broadcast_add(RecordingBroadcast& b, std::span<const uint8_t> bytes);
// The recording's clock: a second past the block's start closes it.
void recording_broadcast_tick(RecordingBroadcast& b, int64_t now_ms);
// The recording ended: the last block closed and the trailer written.
void recording_broadcast_end(RecordingBroadcast& b, const ge::replay::Meta& final_meta = {});
// The file's bytes written since the last call (a TV node's feed).
std::span<const uint8_t> recording_broadcast_fresh(RecordingBroadcast& b);

}  // namespace shooters_edge::client
