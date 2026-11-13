// RECORDING ON THE PROXY (docs/tv.md § Proxy): every game datagram a proxy passes on, both ways,
// kept as a replay file (`.ged`, engine/replay/stream.hpp), so a player who plays through a near
// proxy has the match without the game recording it. The proxy does not read the game: the file
// holds the datagrams as they went, the client's in track 1 ("to_server") and the server's in
// track 2 ("to_client"), each a `messages` chunk stamped with its milliseconds since the start,
// zstd-packed a block a second. A game that knows its own datagrams reads them back
// (`proxy_read_recording`) and plays them, as its own demo reader plays a demo. Only the first
// block is a keyframe: an opaque stream can be read from its start alone.
//
//   proxy_record_begin(rec, game, meta, now)            the header (meta: whatever the caller has)
//   config.tap = [&](const Passed& p) { proxy_record(rec, p.to_server, p.datagram, p.now_us); };
//   proxy_record_finish(rec, now)                       the last block and the trailer; rec.bytes
//
// OBSERVERS: the file grows a whole block at a time, so its new bytes (`proxy_record_fresh`) are
// a TV node's source as they come (`tv::tv_feed`, engine/tv/node.hpp): a player's friends watch
// the match through a TV node beside the proxy, from its start whenever they join (the only
// keyframe), with the node's delay.
//
// Within a millisecond the file keeps each direction's order, not the order between the two.
//
// ge:summary Recording on the proxy: the game datagrams it passes, both ways, as a replay file
// ge:keywords proxy record recording replay-file ged datagrams to_server to_client messages-track
// ge:keywords proxy_record_begin proxy_record proxy_record_finish proxy_read_recording
// ge:keywords proxy_record_fresh observers tv-source
// ge:skill net replay
// ge:test tests/proxy_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/stream.hpp"

namespace ge::proxy {

inline constexpr uint32_t kTrackToServer = 1, kTrackToClient = 2;

struct ProxyRecording {
  replay::StreamWriter writer;
  std::vector<uint8_t> bytes;  // the file so far (whole once finished)
  int64_t start_us = 0;
  int64_t block_us = 1000000;  // a block's length
  int64_t block_at_ms = 0;     // the open block's start
  int64_t last_ms = 0;
  bool open = false, finished = false;
  uint64_t to_server = 0, to_client = 0;  // datagrams kept
  uint64_t payload = 0;                   // their bytes
  size_t fresh_at = 0;                    // `bytes` already handed out by `proxy_record_fresh`
};

// The header: `game` and `meta` as the caller has them, the recorder's own keys added
// (`recorder` proxy, `start_us`).
void proxy_record_begin(ProxyRecording& r, const std::string& game, replay::Meta meta,
                        int64_t now_us);
// One datagram, at `now_us` (never before the last one's).
void proxy_record(ProxyRecording& r, bool to_server, std::span<const uint8_t> datagram,
                  int64_t now_us);
// The last block and the trailer (`duration_ms`, `to_server`, `to_client`).
void proxy_record_finish(ProxyRecording& r, int64_t now_us);

// The file's bytes written since the last call: the header, then whole blocks, then the trailer.
std::span<const uint8_t> proxy_record_fresh(ProxyRecording& r);

struct Recorded {
  bool to_server = false;
  int64_t time_ms = 0;
  std::vector<uint8_t> bytes;
};
// The datagrams back in time order (the client's first within a millisecond); false with the
// reason when the file is not a proxy's recording or is damaged.
bool proxy_read_recording(std::span<const uint8_t> file, std::vector<Recorded>& out,
                          std::string& error);

}  // namespace ge::proxy
