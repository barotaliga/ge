// QUAKE II'S CINEMATICS: the `.cin` video files the campaign plays between its units (the intro, a
// unit's end, the end of the game). Read whole into memory and decoded a frame at a time.
//
// THE FILE (all numbers little-endian 32-bit):
//   header       width, height, sound rate (Hz, 0: no sound), sample width (1 or 2 bytes),
//                channels (1 or 2); no magic, no frame count
//   counts       256 rows of 256 bytes: row p holds how often each value follows the value p;
//                one Huffman tree a row is built from it at open
//   frames       command (0 a frame, 1 a 768-byte RGB palette then a frame, 2 or more the end),
//                the picture block's size, the block (the pixel count, then the bits), the sound
//   end          a command of 2 or more, or the file's end
//
// THE PICTURE is one palette index a pixel, rows from the top, coded by an order-1 Huffman code:
// the tree of the previous pixel's value (0 at a frame's start) reads the next. Bits are taken
// least significant first. A tree is built from its row's counts: the two smallest non-zero
// nodes not yet joined (ties to the lower node number; leaves are 0..255, joined nodes 256 on in
// the order made) become the 0 and the 1 child of the next node; its root is the last node made.
//
// THE CLOCK is 14 frames a second. Frame k carries floor((k+1)*rate/14) - floor(k*rate/14)
// samples, so the remainder carries from frame to frame. 16-bit samples are signed, 8-bit mono
// is unsigned (128 silent), 8-bit stereo is signed; channels interleave left first.
//
// `write_cin` is the fixture writer the tests and labs use (the stock files are never committed).
//
// ge:summary Quake II's .cin cinematics: the header, the order-1 Huffman picture, the sound a frame
// ge:keywords cin cinematic video movie huffman eou intro victory 14-fps fixture
// ge:skill classic
// ge:test tests/classic_content_cin_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::classic::content {

inline constexpr int kCinFps = 14;

struct CinFrame {
  std::vector<uint8_t> pixels;         // width * height palette indices, rows from the top
  std::array<uint8_t, 768> palette{};  // the palette in force (RGB, 0..255)
  std::vector<float> left, right;      // this frame's sound, -1..1 (mono: the same on both)
};

struct Cin {
  int width = 0, height = 0;
  int rate = 0, sample_bytes = 0, channels = 0;
  // Each context's tree: node n (256..510) has children kids[n-256][0], [1]; root[p] is the
  // context p's root (a leaf when the row has fewer than two values: that context is refused).
  std::array<std::array<std::array<uint16_t, 2>, 255>, 256> kids{};
  std::array<uint16_t, 256> root{};
  std::span<const uint8_t> bytes;
  size_t at = 0;      // the next chunk's offset
  int64_t frame = 0;  // the next frame's number
  std::array<uint8_t, 768> palette{};
  bool ended = false;
};

// Open a .cin held in `bytes` (kept by the caller while it plays). False with `error` naming
// `where`, what is wrong, at which byte, and the next step.
bool open_cin(std::span<const uint8_t> bytes, std::string_view where, Cin* out, std::string* error);

enum class CinStep { frame, end, fault };

// The next frame into `out`; `end` at the end marker or the file's end, `fault` with `error` on a
// damaged frame (playing stops there).
CinStep next_cin_frame(Cin& cin, CinFrame* out, std::string* error);

// The whole movie's sound from a freshly opened `cin`, read past the pictures without decoding
// them (a movie's sound plays as one recording): -1..1 a channel, a mono movie's on both.
void cin_sound(const Cin& cin, std::vector<float>* left, std::vector<float>* right);

// A .cin of `frames` (each width * height indices), `palette` sent with the first, the sound
// `sound` (interleaved 16-bit, rate 0: none), the counts taken from the frames themselves.
std::vector<uint8_t> write_cin(int width, int height, std::span<const std::vector<uint8_t>> frames,
                               std::span<const uint8_t> palette, int rate, int channels,
                               std::span<const int16_t> sound);

}  // namespace ge::classic::content
