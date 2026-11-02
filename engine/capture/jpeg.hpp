// JPEG, THE ENGINE'S OWN (docs/replays.md § Screenshots): the baseline form every viewer opens,
// written with no library, so a screenshot packs small on every platform and every build.
//
// THE ENCODER writes a baseline JFIF file: RGB to YCbCr by JFIF's full-range BT.601, the chroma
// at full size (`subsample` false, 4:4:4: a HUD's thin coloured letters keep their edges) or
// averaged over each 2x2 block (4:2:0, about a third smaller), each 8x8 block through an INTEGER
// DCT (a fixed-point table of the orthonormal transform, so the same picture packs to the same
// bytes everywhere), quantized by the standard luma and chroma tables scaled by `quality` (1..100,
// the usual scale: 50 the tables as they stand, 100 every step 1), and coded with the standard
// Huffman tables. A side not a multiple of the block repeats its last row or column.
//
// THE DECODER reads baseline sequential JPEG with Huffman tables (what the encoder writes, and the
// common camera and tool output: any 8-bit baseline file with one to three components, a luma
// sampled up to 2x2 and the chroma 1x1, no restart markers). It exists so the tests and the
// `screenshot` lab can measure what a quality costs; a game never reads a screenshot back.
//
// ge:summary The engine's own JPEG: a baseline encoder (integer DCT, 4:4:4 or 4:2:0) and decoder
// ge:keywords jpeg jpg jfif baseline dct quantize huffman quality chroma subsample 420 444
// ge:keywords encode_jpeg decode_jpeg screenshot still
// ge:skill replay
// ge:test tests/capture_jpeg_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "lab/raster.hpp"

namespace ge::capture {

// `rgba` (`width` x `height`, rows top to bottom, alpha ignored) as a baseline JPEG at `quality`
// (clamped to 1..100), its chroma halved each way when `subsample`.
std::vector<uint8_t> encode_jpeg(int width, int height, std::span<const uint8_t> rgba, int quality,
                                 bool subsample);

// The picture a baseline JPEG holds, opaque; nothing, with the reason in `error`, for a file this
// decoder does not take (progressive, arithmetic, 12-bit, restart markers) or a damaged one.
std::optional<lab::Picture> decode_jpeg(std::span<const uint8_t> bytes, std::string& error);

}  // namespace ge::capture
