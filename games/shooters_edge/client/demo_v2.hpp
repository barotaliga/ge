// THE CLASSIC RECORDINGS ON THE REPLAY FILE VERSION 2 (docs/tv.md § The field codec): this game's
// face, which turns a recording (multiview, client or single-player) into a version 2 file and
// back to the same bytes.
//
// A MULTIVIEW RECORDING is walked record by record and message by message (the layouts
// demo_multiview.cpp and demo_client.cpp read), and every field goes through the field codec
// (engine/replay/codec.hpp) as the residual from its prediction: a player's and an entity's origin
// from the line through its last two, an angle from its last, a stat, a ping, a frag count from
// the last one written for it, a sound's point from its entity's origin. Bytes with no model (the
// text, the lists, the hidden records, a message the walk does not know) go through the byte model.
// A record whose body does not walk whole is written as its bytes, so every file comes back.
//
// The records go into one-second blocks on the `multiview` track (kind messages, codec field), a
// keyframe block every `keyframe_seconds`: there the models and predictions start over and the
// extension bits are written, so a reader can start at any keyframe. A file that ends inside a
// record (cut short) keeps its last bytes as the last block's tail.
//
// A CLIENT OR SINGLE-PLAYER RECORDING (and anything else) is kept as its bytes on the `recording`
// track (codec zstd), 256 KiB a block: it comes back the same, smaller only by zstd.
//
// THE HUB'S WORDS (docs/hub.md § The archive): the recording is read through once first
// (`demo_hub_meta`) and what a hub's archive files a demo by goes in the meta: the header's `map`
// (the first level's), `mode` (`coop`; with teamplay `2on2` up to four players, else `4on4`;
// without, `1on1` for two, else `ffa`; `single` for a single-player recording), `server` (the
// server's `hostname`), a `player` for each one who played (`name`, or `name<TAB>team`, in the
// order first seen; spectators left out); the trailer's `duration_ms`, a `score` for each player
// (`name<TAB>frags`, the last count seen) and, with teams, a `team_score` for each team (the sum of
// its players'). `DemoPackOptions::meta` adds the caller's own (`start`, `title`, `tag`).
//
// ge:summary Shooter's Edge's face on replay file v2: classic recordings packed and back exactly
// ge:keywords replay-v2 face field-codec multiview client-recording single-player-recording
// ge:keywords pack unpack residual prediction ged
// ge:keywords pack_demo unpack_demo keyframe round-trip compress
// ge:keywords hub-meta archive map mode player score team-score demo_hub_meta
// ge:skill replay shooter
// ge:test tests/shooters_edge_demo_v2_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/stream.hpp"

namespace shooters_edge::client {

struct DemoPackOptions {
  int keyframe_seconds = 30;  // a keyframe block every this many one-second blocks
  ge::replay::Meta meta;      // added to the header's after the game's (`start`, `title`, `tag`)
};

// The hub's words for `recording` (above): `header` and `final` appended to. Nothing when the
// recording does not open; what it held up to a damaged frame when one does not read.
void demo_hub_meta(std::span<const uint8_t> recording, ge::replay::Meta& header,
                   ge::replay::Meta& final_meta);

// The parts of a multiview recording the pack's bytes are counted by (inside the messages').
enum class DemoPart : uint8_t {
  record_head,    // each record's time, kind, mask and the walk's flags
  kept,           // bodies kept as their bytes, and a cut file's tail
  player_head,    // a player's slot and field bits
  player_frame,   // and animation frame
  player_origin,  // origin
  player_angles,  // view angles
  player_other,   // model, skin, effects, weapon frame
  entity_word,    // an entity delta's number and flags
  entity_origin,
  entity_angles,
  entity_other,  // model, frame, skin, effects and the rest
  text,          // every string
  message_id,    // each message's id and the end of each body
  sound,         // sounds: the entity, channel, sound and point
  stat,          // stats, frags, pings
  count,
};
const char* demo_part_name(DemoPart part);

// What a pack wrote, for a lab or a command's answer.
struct DemoPackReport {
  std::string format;                        // "multiview", "bytes"
  size_t records = 0, walked = 0, kept = 0;  // records; walked whole; kept as their bytes
  size_t blocks = 0, keyframes = 0;
  size_t tail = 0;  // bytes after the last whole record
  // The packed bytes each message id took, and each part, to a fraction of a byte.
  std::array<double, 256> message_bytes{};
  std::array<double, static_cast<size_t>(DemoPart::count)> part_bytes{};
};

// `recording` (a plain recording: unpack a packed one first) into a version 2 file on `out`.
bool pack_demo(std::span<const uint8_t> recording, std::vector<uint8_t>& out, std::string& error,
               const DemoPackOptions& options = {}, DemoPackReport* report = nullptr);
// A version 2 file written by `pack_demo` back into the recording's bytes. False with the reason
// in `error` (not this game's, damaged); a file cut short gives back what its whole blocks hold.
bool unpack_demo(std::span<const uint8_t> file, std::vector<uint8_t>& out, std::string& error);

}  // namespace shooters_edge::client
