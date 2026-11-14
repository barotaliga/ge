// QOI, THE PICTURE A SAVE CARRIES (docs/saves.md § Pictures of a save): a lossless format for
// 8-bit RGBA, written and read here from its one-page public specification. A 14-byte header
// ("qoif", width and height big-endian, channels 4, colorspace 0), then each pixel as one of six
// ops (a run of the last pixel, an index into a 64-entry table of seen pixels hashed by
// (r*3 + g*5 + b*7 + a*11) % 64, a small or a luma-wise difference from the last pixel, or the
// pixel whole as RGB or RGBA), then seven zero bytes and a one. About PNG's size, many times
// faster, and a few dozen lines each way.
//
// ge:summary QOI encode and decode: the lossless RGBA picture format a save's picture is kept in
// ge:keywords qoi picture image encode decode lossless rgba thumbnail
// ge:skill save
// ge:test tests/save_picture_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::save {

// `rgba` (width * height * 4 bytes, rows top down) as a QOI file in `out`.
void qoi_encode(int width, int height, std::span<const uint8_t> rgba, std::vector<uint8_t>& out);

// A QOI file read into `rgba` (always four channels). False with `problem` when the header, a
// size or the end marker is wrong, or the pixels run short.
bool qoi_decode(std::span<const uint8_t> qoi, int& width, int& height, std::vector<uint8_t>& rgba,
                std::string& problem);

}  // namespace ge::save
