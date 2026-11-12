// THE FIELD CODEC's coder (docs/tv.md § The field codec): an adaptive binary range coder and the
// integer and byte models a track's fields are written through. A field is predicted by its writer
// (the game's face or a track's schema), and what goes through here is the RESIDUAL, the value
// minus its prediction: small numbers when the prediction is good, so a good model spends a bit or
// two on a position that moved as expected.
//
// THE CODER is a carry-propagating binary range coder: a 32-bit range, probabilities of 11 bits,
// each moving toward the bit seen by a share that shrinks as it sees more (half at first, 1/32
// once it has seen 15 bits; a `Prob`'s top 5 bits count them), so a model started over at a
// keyframe learns again in a few dozen values. It is integers only, so its output is the same bytes
// on every compiler and OS (held by tests/replay_codec_test.cpp's fixed sizes and digest).
//
// AN INTEGER (`IntModel`, any int32) is its bit length (0..32) through a 6-level bit tree, then
// its sign, then the three bits under its top bit through a bit tree per length, then the rest
// raw. A writer keeps one model per field and per context it chooses (the field's last residual's
// size is a good one), so each field learns its own spread.
//
// A BYTE (`ByteModel`) is 8 bits through a bit tree under one of 256 contexts (the byte before it
// is the usual one): text, names and anything the face does not model.
//
// ge:summary The field codec's range coder and its adaptive integer and byte models
// ge:keywords field-codec range-coder arithmetic-coding entropy adaptive probability bit-tree
// ge:keywords residual prediction compress replay-v2 range_writer range_reader int_model byte_model
// ge:keywords put_int get_int put_byte get_byte put_bit get_bit
// ge:skill replay
// ge:test tests/replay_codec_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ge::replay {

// A probability that the next bit is 0, in 1/2048ths (the low 11 bits), and how many bits it has
// seen, up to 31 (the top 5); starts at even, having seen none.
using Prob = uint16_t;
inline constexpr Prob kProbHalf = 1024;

struct RangeWriter {
  std::vector<uint8_t>* out = nullptr;
  uint64_t low = 0;
  uint32_t range = 0xFFFFFFFFu;
  uint8_t cache = 0;
  uint64_t cache_size = 1;
};

struct RangeReader {
  const uint8_t* at = nullptr;
  const uint8_t* end = nullptr;
  uint32_t range = 0xFFFFFFFFu;
  uint32_t code = 0;
  // Set when the reader ran past its bytes (a cut or damaged block): what follows is garbage.
  bool bad = false;
};

// Starts writing onto `out` (appended to, never cleared).
void rc_begin(RangeWriter& w, std::vector<uint8_t>& out);
// Writes the last bytes; the writer may be begun again.
void rc_end(RangeWriter& w);
void rc_begin(RangeReader& r, const uint8_t* data, size_t size);

void put_bit(RangeWriter& w, Prob& p, int bit);
int get_bit(RangeReader& r, Prob& p);
// `count` bits (at most 32) at even odds, the highest first.
void put_raw(RangeWriter& w, uint32_t value, int count);
uint32_t get_raw(RangeReader& r, int count);

struct IntModel {
  std::array<Prob, 64> length{};             // the bit length's tree (6 levels)
  std::array<Prob, 33> sign{};               // by length
  std::array<Prob, size_t{33} * 8> under{};  // the 3 bits under the top bit, a tree per length
  IntModel();
};

void put_int(RangeWriter& w, IntModel& m, int32_t v);
int32_t get_int(RangeReader& r, IntModel& m);
// The bit length of |v| (0 for 0): the context a field's next value is often best coded in.
int int_length(int32_t v);

struct ByteModel {
  std::vector<Prob> tree;  // 256 contexts × 256 nodes
  ByteModel();
};

void put_byte(RangeWriter& w, ByteModel& m, uint8_t context, uint8_t b);
uint8_t get_byte(RangeReader& r, ByteModel& m, uint8_t context);

}  // namespace ge::replay
