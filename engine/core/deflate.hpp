// THE DEFLATER: bytes into a raw deflate stream (RFC 1951, no zlib header or checksum), the
// inverse of core/inflate.hpp's `inflate_raw`. One block with the fixed Huffman codes: each byte
// either a literal or part of a match found through a hash of the next three bytes (the newest
// 32 places with that hash tried, a match of 3 to 258 bytes up to 32 768 back, the longest kept,
// the first found on a tie). No dynamic codes: the streams it writes are small (a game's message
// of a few kilobytes), where the dynamic block's header costs about what its codes save. Any
// inflater reads the result; a stream is the same bytes for the same input on every machine.
//
// ge:summary The deflater: bytes into one raw deflate block with the fixed Huffman codes
// ge:keywords deflate compress raw-deflate lz77 fixed-huffman zpacket
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

}  // namespace ge::core
