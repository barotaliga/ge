// A RECORDED MATCH PLAYED BACK (docs/servers.md § Recording): a recording the server made
// (record.hpp) read into the game's own viewer (`ServerGame::watch_*`), stepped a frame at a time
// or sought to any frame from the keyframe before it, and seen from any player's view
// (`watch_view`). A recording holds every player's part, so any of them can be followed.
//
//   playback_open(pb, game, bytes, error)   the header, every block read, the first keyframe
//   playback_seek(pb, frame, error)         the viewer at `frame` frames after the recording began
//   playback_step(pb, error)                the next frame; false at the end
//   playback_view(pb, player, out)          what that player sees now, as the game's bytes
//
// Frames count from the recording's start (`start_frame` in its meta is the server's frame then),
// so frame `n` of a playback is the server's frame `start_frame + n`.
//
// ge:summary A recorded match played back: seek to any frame, step, and any player's view
// ge:keywords playback recording watch seek step player-view watch_view playback_open
// ge:keywords playback_seek playback_step playback_view any-player multiview
// ge:skill replay net
// ge:test tests/server_record_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/stream.hpp"
#include "server/server.hpp"

namespace ge::server {

struct Playback {
  Playback() = default;
  Playback(const Playback&) = delete;
  Playback& operator=(const Playback&) = delete;
  ~Playback();

  const ServerGame* game = nullptr;
  replay::StreamHeader header;
  int64_t start_frame = 0;  // the server's frame when the recording began
  struct Key {
    int64_t frame = 0;  // frames played before it
    std::vector<uint8_t> bytes;
  };
  std::vector<Key> keys;                     // in order; the first at frame 0
  std::vector<std::vector<uint8_t>> frames;  // every frame's bytes, in order
  void* watch = nullptr;                     // the game's viewer
  int64_t at = 0;                            // frames played into the viewer
};

// The recording read; false with `error` (not a recording, damaged, or a game that cannot watch).
bool playback_open(Playback& pb, const ServerGame& game, std::span<const uint8_t> bytes,
                   std::string& error);
// The viewer at `frame` (0 .. frames.size()), from the newest keyframe at or before it.
bool playback_seek(Playback& pb, int64_t frame, std::string& error);
// The next frame played; false at the end (`error` empty) or on damage.
bool playback_step(Playback& pb, std::string& error);
// How many players the viewer holds now.
int playback_players(const Playback& pb);
// What `player` sees now, appended to `out`.
void playback_view(const Playback& pb, int player, std::vector<uint8_t>& out);

}  // namespace ge::server
