// ZSTANDARD FRAMES, packed and unpacked: the codec a recording is packed with as it is written,
// and the reader of any `.zst` file (RFC 8878). A packed file is one plain zstd frame (several
// when it was flushed: the frames simply follow one another), so stock `zstd -d` opens it too.
//
// THE SETTING (`ZstdLevel`'s default) was chosen by the `se-demo-pack` lab on real recordings
// (docs/goals/M23j.md has the table): level 19 with the level's own 8 MiB window. On a 22 MB
// four-a-side multiview recording it packs 9.3 times (gzip -9: 7.1), unpacks at about 1.5 GB/s
// and packs at about 3 MB/s, many times a match's recording rate. The long-distance matcher and a
// 128 MiB window gained nothing on recordings (the same size or larger) and tripled the packer's
// memory; xz packs 16% smaller but unpacks eleven times slower and cannot be flushed as it goes.
//
// THE STREAM (`ZstdStream`): bytes go in as they are recorded and the packed frame comes out
// behind them. `zstd_flush` ends the current block so every byte given so far can be unpacked
// from what came out (a recording cut by a crash keeps all of it up to the last flush);
// `zstd_end` ends the frame. The packer's memory (about 90 MiB at the chosen setting:
// `zstd_pack_memory`) is held from `zstd_begin` to `zstd_end`.
//
// UNPACKING refuses more than `limit` bytes of output (a bomb) and a frame that names a window
// over 2 GiB. A frame cut short is not refused: what it holds comes back with `truncated` set, so a
// recording cut off mid-match still plays to where it stopped.
//
// ge:summary Zstandard frames: a recording packed as it is written, any .zst file unpacked
// ge:keywords zstd zstandard zst compress decompress pack unpack long-distance window level
// ge:keywords stream flush truncated recording packed-demo zstd_pack zstd_unpack zstd_begin
// ge:keywords zstd_pack_memory memory
// ge:skill vfs
// ge:test tests/vfs_zstd_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ge::vfs {

struct ZstdLevel {
  int level = 19;           // 1 (fastest) .. 22 (smallest); 19 is the chosen setting
  int window_log = 0;       // the window, 2^window_log bytes; 0 the level's own (8 MiB at 19)
  bool long_match = false;  // the long-distance matcher
};

// The first four bytes of every zstd frame.
inline constexpr uint8_t kZstdMagic[4] = {0x28, 0xB5, 0x2F, 0xFD};
// Whether `bytes` opens with a zstd frame's magic.
bool is_zstd(std::span<const uint8_t> bytes);

// The memory a stream at `level` holds while it packs, bytes (measured on a stream begun).
size_t zstd_pack_memory(const ZstdLevel& level = {});

// `bytes` packed as one frame at `level`.
std::vector<uint8_t> zstd_pack(std::span<const uint8_t> bytes, const ZstdLevel& level = {});

// Every frame in `bytes` unpacked into `out`. False with the reason in `error` when it is not zstd,
// is damaged, or would unpack to more than `limit` bytes. `truncated` says the last frame was cut
// short (what it held is in `out`).
bool zstd_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, bool& truncated,
                 std::string& error, size_t limit = size_t{1} << 31);

struct ZstdStream {
  struct Context;
  std::unique_ptr<Context, void (*)(Context*)> ctx{nullptr, nullptr};
  std::vector<uint8_t> out;  // the packed bytes so far
  int64_t fed = 0;           // plain bytes given so far
  bool ended = false;
};

// A stream begun at `level` (its packer allocated).
void zstd_begin(ZstdStream& s, const ZstdLevel& level = {});
// `bytes` given to the stream; packed bytes appear in `s.out` as blocks fill.
void zstd_write(ZstdStream& s, std::span<const uint8_t> bytes);
// The current block ended: `s.out` now unpacks to every byte given so far.
void zstd_flush(ZstdStream& s);
// The frame ended and the packer freed; `s.out` is the whole file.
void zstd_end(ZstdStream& s);

}  // namespace ge::vfs
