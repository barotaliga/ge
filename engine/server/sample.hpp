// THE SAMPLE GAME: the smallest game a server can run, so the server core is tested and measured
// on its own (tests/server_core_test.cpp, the `server-core` lab) without any real game linked.
// Each bot walks a ring of 1000 cells a step left, right or not at all each frame, off the run's
// seed; the hash folds every position and the frame count. 60 frames a second.
//
// ITS BROADCAST (`tv_frame`, `tv_keyframe`; engine/tv/server_source.hpp): a frame is each bot's
// step as two bits, four bots a byte; a keyframe is the frame count and every bot's cell
// (varints). `SampleWatch` is its viewer: handed a stream's chunks from a keyframe on, it keeps the
// ring and its hash, which reaches the server's digest at the same frame.
//
// ge:summary The server core's sample game: bots walking a ring, a hash, for the tests and lab
// ge:keywords server sample-game servergame test-fixture walkers ring sample_game
// ge:keywords sample-watch tv-frame tv-keyframe broadcast
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "server/server.hpp"

namespace ge::server {

const ServerGame& sample_game();

// THE SAMPLE GAME'S VIEWER: the ring as a broadcast keeps it.
struct SampleWatch {
  bool started = false;
  int64_t frames = 0;
  std::vector<int32_t> at;
  std::string error;
};
// A keyframe's bytes: the viewer starts over from them. False with `error` when damaged.
bool sample_watch_keyframe(SampleWatch& w, std::span<const uint8_t> bytes);
// A frame's bytes, after the keyframe or the frame before. False with `error` when damaged.
bool sample_watch_frame(SampleWatch& w, std::span<const uint8_t> bytes);
// The ring's hash as the viewer holds it (the server's `hash` at the same frame).
uint64_t sample_watch_hash(const SampleWatch& w);

}  // namespace ge::server
