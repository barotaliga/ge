// THE ONE FILE INSIDE: bytes that may be packed or archived, peeled to the single file they hold,
// told apart by their first bytes, never by a name. Any reader of a file a player hands the game
// (a recorded game today) calls `unpack_one` first and then sees only the plain bytes.
//
// THE LAYERS (`Wrapping`), each peeled in turn until none is left (at most four deep, so
// `.tar.gz` is two):
//
// - gzip (`1F 8B`): through core/inflate.hpp; the name in its header, when it has one, is kept.
// - zstd (`28 B5 2F FD`): through zstd.hpp, frame after frame. A frame cut short still gives what
//   it holds (`Unpacked::truncated`), so a recording cut off by a crash plays to where it stopped.
// - xz (`FD 37 7A 58 5A 00`): through lzma.hpp.
// - tar (a 512-byte header whose checksum holds), zip (`PK 03 04`, or the empty zip's `PK 05 06`)
//   and 7z (`37 7A BC AF 27 1C`): archives. Their regular files are listed (directories skipped);
//   exactly one may be taken. With `pick` given, only the files it accepts count (a game passes
//   the names it reads, so a recording beside a readme opens); without it, every file counts.
//   None or several is refused, naming the files there are.
//
// Anything else is plain bytes and comes back as it was, with no layer (`layers` empty). A layer
// that would unpack to more than `kUnpackLimit` bytes is refused.
//
// ge:summary The one file inside packed or archived bytes: gzip, zstd, xz, tar, zip and 7z peeled
// ge:keywords unpack archive single-file one-file gz gzip zst zstd xz tar tgz tar-gz zip 7z
// ge:keywords seven-zip wrapping layers magic sniff pick truncated unpack_one wrapping_of
// ge:keywords write_gzip
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::vfs {

enum class Wrapping : uint8_t { none, gzip, zstd, xz, tar, zip, seven };

inline constexpr size_t kUnpackLimit = size_t{1} << 31;  // 2 GiB: past any recording or map

// The layer `bytes` open with (none: plain bytes).
Wrapping wrapping_of(std::span<const uint8_t> bytes);
// "gzip", "zstd", "xz", "tar", "zip", "7z" or "none".
std::string_view wrapping_name(Wrapping w);

struct Unpacked {
  std::vector<uint8_t> bytes;    // the file
  std::string name;              // its name in the archive or the gzip header ("" when none)
  std::vector<Wrapping> layers;  // outermost first; empty when the bytes were plain
  bool truncated = false;        // a zstd frame was cut short (what it held is in `bytes`)
};

// `bytes` peeled to the one file they hold (see above). False with the reason in `error`, which
// names the layer and the next action.
bool unpack_one(std::vector<uint8_t> bytes, Unpacked& out, std::string& error,
                const std::function<bool(std::string_view name)>& pick = {});

// A gzip file of `bytes` in stored deflate blocks named `name`, for tests and fixtures.
std::vector<uint8_t> write_gzip(std::span<const uint8_t> bytes, const std::string& name = "");

}  // namespace ge::vfs
