// THE SEQUEL'S MATCH BROADCAST ON THE REPLAY FILE (docs/tv.md § The sequel's recordings): the
// sequel's client recording written as a replay file version 2 while it is played, so a TV node
// (engine/tv/node.hpp) takes it as any game's stream. The sequel has no broadcaster of its own, so
// this is its only one: our recording's blocks in, our file out.
//
// A BROADCAST takes the recording's blocks as they come (each one server packet's messages, its
// length taken off), ten a chunk (a second of the game's 10 Hz frames) through the sequel's face
// (sequel_demo.hpp), each chunk a one-second block on the `sequel` track. The bytes are exactly
// what `pack_demo` writes for the finished recording with the same `keyframe_seconds`: a
// broadcast is the file being written, and `pack_demo` is built on it.
//
// A LATE VIEWER: a TV node sends a joiner the blocks from the newest keyframe. The sequel's frames
// are deltas against frames before them, and its level starts once, so a broadcast keyframes only
// its first block (`keyframe_seconds` 0): a viewer who joins late is sent the match from its start
// (it packs to a few hundred bytes a second) and reads it through to now.
//
// ge:summary The sequel's recording broadcast as a replay file v2 stream while it is played
// ge:keywords sequel broadcast tv replay-v2 stream live-recording late-viewer keyframe dm2
// ge:keywords sequel_broadcast_begin sequel_broadcast_block sequel_broadcast_end
// ge:skill replay shooter
// ge:test tests/shooters_edge_sequel_tv_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "replay/stream.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

namespace shooters_edge::client {

inline constexpr uint32_t kSequelTrack = 3;  // the replay file's track the sequel's chunks are on

// Not moved once begun: the writer appends to `out`.
struct SequelBroadcast {
  std::vector<uint8_t> out;  // the file so far (its trailer's index counts from its start)
  size_t taken = 0;          // of `out`, the bytes `sequel_broadcast_fresh` handed over
  ge::replay::StreamWriter writer;
  SequelCoder coder;
  SequelPackReport report;
  std::vector<std::vector<uint8_t>> pending;  // this chunk's blocks so far
  int keyframe_seconds = 0;                   // 0: the first block alone
  int64_t chunks = 0, blocks = 0;
  bool ended = false;
};

// The broadcast begun: the replay file's header in `out`. `bytes` is the finished recording's
// size when known (a file's header says it; a live broadcast's says 0).
void sequel_broadcast_begin(SequelBroadcast& b, int keyframe_seconds, uint64_t bytes = 0);
// One of the recording's blocks (its messages, the length taken off); every tenth closes a block
// of the file.
void sequel_broadcast_block(SequelBroadcast& b, std::span<const uint8_t> block);
// The recording ended: the last blocks closed and the trailer written.
void sequel_broadcast_end(SequelBroadcast& b);
// The file's bytes written since the last call (a TV node's feed).
std::span<const uint8_t> sequel_broadcast_fresh(SequelBroadcast& b);

}  // namespace shooters_edge::client
