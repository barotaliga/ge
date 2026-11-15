// A MATCH RECORDED BY THE SERVER, the same bytes a TV node broadcasts (docs/servers.md §
// Recording, docs/tv.md § The server as a source): any game on the server core that fills
// `ServerGame::tv_frame` and `tv_keyframe` is recorded as it runs, whole, not one player's view.
// After each server frame the game's frame bytes go into the stream's `frames` track; every
// `keyframe_seconds` a block opens with the game's whole state on the `keyframe` track and is the
// stream's keyframe, where a TV node starts a late viewer. A block is a second of frames. The
// stream is a replay file version 2 (engine/replay/stream.hpp) whose new bytes (`record_fresh`)
// feed a TV node
// (`tv_feed`, node.hpp) as the server writes them, and whose whole is the match's file.
//
//   record_begin(src, server, error)   after server_open: the header, the first keyframe
//   every frame: server_frame(server); record_frame(src, server);
//                tv_feed(node, record_fresh(src), now_ms);
//   record_finish(src, server)         the last block and the trailer
//
// A stream that saw a match end closes with its report (the closing summary, docs/accounts.md §
// The match report), so the archive that keeps the file can join it to the hub's match.
//
// The header names the game by its binary, with the map, the seed, the server's name and the
// frame's length in microseconds in its meta. Times are the server's clock, so a run over the
// simulated network writes the same bytes every time.
//
// ge:summary A match recorded by the server: its frames and keyframes as a replay stream
// ge:keywords record recording server-record tv server-source broadcast record_begin record_frame
// ge:keywords tv_frame
// ge:keywords tv_keyframe record_fresh record_finish keyframe-seconds
// ge:skill replay net
// ge:test tests/server_record_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/stream.hpp"
#include "server/server.hpp"

namespace ge::server {

inline constexpr uint32_t kRecordKeyframeTrack = 1, kRecordFramesTrack = 2;

struct Recording {
  int keyframe_seconds = 10;  // a keyframe block every this many seconds
  replay::StreamWriter writer;
  std::vector<uint8_t> bytes;  // the stream so far
  size_t fresh_at = 0;         // `bytes` already handed out
  int64_t per_block = 1;       // frames a block (a second's)
  int64_t in_block = 0;        // frames in the open block
  bool block_keyframe = false;
  bool open = false, finished = false;
  std::vector<uint8_t> scratch;
  uint64_t frame_bytes = 0, keyframe_bytes = 0, keyframes = 0;
  uint64_t reports_before = 0;  // the server's newest report's id when the stream began (0: none)
  bool summarised = false;      // the trailer closed with a match report
};

// The header and the first keyframe, at the server's frame now. False with `error` when the game
// has no broadcast (`tv_frame` or `tv_keyframe` null).
bool record_begin(Recording& src, const Server& s, std::string& error);
// The server's last frame added; the block closed after a second's frames, the next one opened
// with a keyframe when one is due.
void record_frame(Recording& src, const Server& s);
// The stream's bytes written since the last call.
std::span<const uint8_t> record_fresh(Recording& src);
// The last block and the trailer (`frames`, `digest`); when the game reported a match while the
// stream ran, the newest as the closing summary: `report` (its bytes in hex, net/report.hpp), and
// the hub's meta words `duration_ms`, `team_score` and `score` (hub/archive.hpp).
void record_finish(Recording& src, const Server& s);

}  // namespace ge::server
