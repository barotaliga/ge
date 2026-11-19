// QUAKE II'S MATCH BROADCAST ON THE REPLAY FILE (docs/tv.md § Quake II's recordings): the
// Quake II's client recording written as a replay file version 2 while it is played, so a TV node
// (engine/tv/node.hpp) takes it as any game's stream. Quake II has no broadcaster of its own, so
// this is its only one: our recording's blocks in, our file out.
//
// A BROADCAST takes the recording's blocks as they come (each one server packet's messages, its
// length taken off), ten a chunk (a second of the game's 10 Hz frames) through Quake II's face
// (q2_demo.hpp), each chunk a one-second block on the `q2` track. The bytes are exactly
// what `pack_demo` writes for the finished recording with the same `keyframe_seconds`: a
// broadcast is the file being written, and `pack_demo` is built on it.
//
// A LATE VIEWER: a TV node sends a joiner the blocks from the newest keyframe. Quake II's frames
// are deltas against frames before them, and its level starts once, so a broadcast keyframes only
// its first block (`keyframe_seconds` 0): a viewer who joins late is sent the match from its start
// (it packs to a few hundred bytes a second) and reads it through to now.
//
// ge:summary Quake II's recording broadcast as a replay file v2 stream while it is played
// ge:keywords q2 broadcast tv replay-v2 stream live-recording late-viewer keyframe dm2
// ge:keywords q2_broadcast_begin q2_broadcast_block q2_broadcast_end
// ge:skill replay shooter
// ge:test tests/shooters_edge_q2_tv_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "replay/stream.hpp"
#include "shooters_edge/client/q2_demo.hpp"

namespace shooters_edge::client {

inline constexpr uint32_t kQ2Track = 3;  // the replay file's track Quake II's chunks are on

// Not moved once begun: the writer appends to `out`.
struct Q2Broadcast {
  std::vector<uint8_t> out;  // the file so far (its trailer's index counts from its start)
  size_t taken = 0;          // of `out`, the bytes `q2_broadcast_fresh` handed over
  ge::replay::StreamWriter writer;
  Q2Coder coder;
  Q2PackReport report;
  std::vector<std::vector<uint8_t>> pending;  // this chunk's blocks so far
  int keyframe_seconds = 0;                   // 0: the first block alone
  int64_t chunks = 0, blocks = 0;
  bool ended = false;
};

// The broadcast begun: the replay file's header in `out`. `bytes` is the finished recording's
// size when known (a file's header says it; a live broadcast's says 0).
void q2_broadcast_begin(Q2Broadcast& b, int keyframe_seconds, uint64_t bytes = 0,
                        const ge::replay::Meta& meta = {});
// One of the recording's blocks (its messages, the length taken off); every tenth closes a block
// of the file.
void q2_broadcast_block(Q2Broadcast& b, std::span<const uint8_t> block);
// The recording ended: the last blocks closed and the trailer written.
void q2_broadcast_end(Q2Broadcast& b, const ge::replay::Meta& final_meta = {});
// The file's bytes written since the last call (a TV node's feed).
std::span<const uint8_t> q2_broadcast_fresh(Q2Broadcast& b);

}  // namespace shooters_edge::client
