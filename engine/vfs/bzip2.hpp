// BZIP2 STREAMS unpacked: a `.bz2` file, a `.tar.bz2`'s outer layer and a zip entry of method 12,
// the forms many recordings of the classic games were shared in. A file of several streams (two
// `.bz2` files joined end to end) unpacks to all of them in turn, as stock `bzip2 -d` does. Any
// damage (a block's CRC, a stream's) is refused; more than `limit` bytes of output (a bomb) too.
// The writer packs a test's fixture.
//
// ge:summary bzip2 streams: a .bz2 file or a zip entry of method 12 unpacked; the fixture writer
// ge:keywords bzip2 bz2 tbz tar-bz2 burrows-wheeler compress decompress unpack zip-method-12
// ge:keywords is_bzip2 bzip2_unpack bzip2_pack
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::vfs {

// Whether `bytes` opens with a bzip2 stream's header ("BZh" and a block size of 1 to 9).
bool is_bzip2(std::span<const uint8_t> bytes);

// Every stream in `bytes` unpacked into `out`. False with the reason in `error` when it is not
// bzip2, is damaged or cut short, or would unpack to more than `limit` bytes.
bool bzip2_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, std::string& error,
                  size_t limit = size_t{1} << 31);

// `bytes` packed as one stream with 900 KB blocks, for tests and fixtures.
std::vector<uint8_t> bzip2_pack(std::span<const uint8_t> bytes);

}  // namespace ge::vfs
