// THE REPLAY FILE, VERSION 2 (`.ger`; docs/tv.md § The replay file, version 2): one binary format
// for a kept match and a live broadcast, the same bytes on disk and on the wire. A TV stream is a
// file being written: a viewer that joins reads the header, then blocks from a keyframe on.
//
//   header   magic "gereplay", u16 version 2, the game, its behaviour version, the meta (key and
//            value text pairs: map, mode, start, players...), the game's blob, the track table
//   block*   sync "GBLK", index, start time (ms), flags (1 = keyframe), then per track present:
//            track id, codec, packed size, packed bytes; and a 32-bit FNV-1a of the block's body
//   trailer  "GEND", the final meta, the index (each block's offset, time and flags), the
//            trailer's offset and "GEIX" as the file's last 12 bytes
//
// Numbers are LEB128 varints unless a size is given. A track's bytes in a block are its chunks,
// each (ms since the chunk before it, or the block's start, then length, bytes), packed by the
// track's codec: stored, zstd (one frame a block), or FIELD (the face's own coder output, stored
// as the face wrote it; the face resets its models at every keyframe block, so a viewer can start
// there).
//
// A file with no trailer (a live stream, or one cut short) reads to its last whole block: the
// reader walks the blocks from the header, checking each block's sum. The file's SHA-256, which a
// TV node lists its files by, is taken over the whole file by whoever lists it; it is not stored.
//
// ge:summary Replay file version 2: header, tracks, blocks with keyframes, trailer; disk and wire
// ge:keywords replay-v2 replay-file stream container track block keyframe trailer index sync
// ge:keywords meta chunk tv wire broadcast live late-joiner seek stream_writer stream_reader
// ge:keywords stream_begin stream_add stream_end_block stream_finish read_stream_header
// ge:keywords next_block read_trailer
// ge:skill replay
// ge:test tests/replay_stream_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ge::replay {

inline constexpr int kStreamVersion = 2;

// What a track holds (docs/tv.md's table). The values are the file's; never renumber one.
enum class TrackKind : uint8_t {
  meta = 1,
  inputs = 2,
  state = 3,
  messages = 4,
  keyframe = 5,
  events = 6,
  chat = 7,
  voice = 8,
  radar = 9,
};

enum class TrackCodec : uint8_t { stored = 0, zstd = 1, field = 2 };

struct TrackInfo {
  uint32_t id = 0;
  TrackKind kind = TrackKind::meta;
  TrackCodec codec = TrackCodec::zstd;
  std::string name;  // the game's word for it ("multiview", "inputs")
};

using Meta = std::vector<std::pair<std::string, std::string>>;

struct StreamHeader {
  std::string game;
  int32_t game_version = 1;
  Meta meta;
  std::vector<uint8_t> blob;  // the game's own header bytes
  std::vector<TrackInfo> tracks;
};

struct Chunk {
  uint32_t track = 0;
  int64_t time_ms = 0;  // since the stream's start
  std::vector<uint8_t> bytes;
};

struct Block {
  int64_t index = 0;
  int64_t time_ms = 0;  // the block's start
  bool keyframe = false;
  std::vector<Chunk> chunks;  // track by track in the header's order, each track's in time order
};

struct BlockEntry {
  int64_t offset = 0;  // of the block's sync, from the file's start
  int64_t time_ms = 0;
  bool keyframe = false;
};

struct Trailer {
  Meta meta;  // the final meta: duration, scores
  std::vector<BlockEntry> index;
};

struct StreamWriter {
  std::vector<uint8_t>* out = nullptr;
  StreamHeader header;
  int64_t blocks = 0;
  int64_t block_start_ms = -1;
  std::vector<Chunk> pending;  // the open block's chunks
  std::vector<BlockEntry> index;
};

// The header written onto `out`; the writer keeps `out` and appends every block to it.
void stream_begin(StreamWriter& w, const StreamHeader& h, std::vector<uint8_t>& out);
// A chunk for `track` at `time_ms` added to the open block (times never go back).
void stream_add(StreamWriter& w, uint32_t track, int64_t time_ms, std::span<const uint8_t> bytes);
// The open block written (nothing when it is empty). A keyframe block is where a reader may start.
void stream_end_block(StreamWriter& w, bool keyframe);
// The last block ended and the trailer written with the final meta.
void stream_finish(StreamWriter& w, const Meta& final_meta);

struct StreamReader {
  std::span<const uint8_t> bytes;
  StreamHeader header;
  size_t at = 0;  // the next block's offset
};

// The header read; false with the reason in `error` (not a version 2 file, a newer version,
// damage).
bool read_stream_header(std::span<const uint8_t> bytes, StreamReader& r, std::string& error);
// The next whole block into `b`. False at the end: the trailer, the bytes' end, or a block cut
// short or damaged (`error` says which; empty at a clean end).
bool next_block(StreamReader& r, Block& b, std::string& error);
// The trailer, when the file has one.
std::optional<Trailer> read_trailer(std::span<const uint8_t> bytes);
// Whether `bytes` opens as a version 2 replay file.
bool is_stream(std::span<const uint8_t> bytes);

}  // namespace ge::replay
