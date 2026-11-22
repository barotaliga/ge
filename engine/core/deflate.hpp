// THE DEFLATER: bytes into a raw deflate stream (RFC 1951, no zlib header or checksum), the
// inverse of core/inflate.hpp's `inflate_raw`. One block with the fixed Huffman codes: each byte
// either a literal or part of a match found through a hash of the next three bytes (the newest
// 32 places with that hash tried, a match of 3 to 258 bytes up to 32 768 back, the longest kept,
// the first found on a tie). No dynamic codes: the streams it writes are small (a game's message
// of a few kilobytes), where the dynamic block's header costs about what its codes save. Any
// inflater reads the result; a stream is the same bytes for the same input on every machine.
//
// A STREAM fed piece by piece (`DeflateStream`): each piece is one fixed-code block that matches
// only within itself, then a sync flush (an empty stored block, so the piece ends on a byte and a
// reader inflates all of it at once); `deflate_finish` writes the final empty block and the
// wrapper's tail. Wrapped as zlib (RFC 1950: the header 78 01, the Adler-32 at the end) or gzip
// (RFC 1952: the ten-byte header, the CRC-32 and the length at the end) or raw.
//
// ge:summary The deflater: bytes into one raw deflate block with the fixed Huffman codes
// ge:keywords deflate compress raw-deflate lz77 fixed-huffman zpacket sync-flush zlib-stream
// ge:keywords gzip-writer adler32
// ge:skill determinism net
// ge:test tests/core_deflate_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ge::core {

// `bytes` as one raw deflate stream (one final block with the fixed codes; empty input gives the
// three-byte empty block).
std::vector<uint8_t> deflate_raw(std::span<const uint8_t> bytes);

enum class DeflateWrap : uint8_t { raw, zlib, gzip };
struct DeflateStream {
  DeflateWrap wrap = DeflateWrap::zlib;
  bool started = false, finished = false;
  uint32_t adler = 1, crc = 0;
  uint64_t length = 0;  // the bytes fed, ever
};
// `bytes` appended to `out` as one block and a sync flush (the wrapper's header first, once).
void deflate_sync(DeflateStream& s, std::span<const uint8_t> bytes, std::vector<uint8_t>& out);
// The stream's end appended to `out`: the final empty block and the wrapper's tail (once).
void deflate_finish(DeflateStream& s, std::vector<uint8_t>& out);
// Adler-32 of `bytes` continued from `a` (1 at the start).
uint32_t adler32(const uint8_t* bytes, size_t n, uint32_t a = 1);

}  // namespace ge::core
