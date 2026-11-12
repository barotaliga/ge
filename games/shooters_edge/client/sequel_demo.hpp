// THE SEQUEL'S CLIENT RECORDINGS ON THE REPLAY FILE VERSION 2 (docs/tv.md § The sequel's
// recordings): the sequel's face, which turns one of its client recordings (`.dm2`: blocks of a
// 32-bit length and one server packet's messages, ended by a length of -1) into version 2 chunks
// and back to the same bytes.
//
// Every block is walked message by message and every field goes through the field codec
// (engine/replay/codec.hpp) as the residual from its prediction: an entity's or the player's
// origin from the line through its last two, an angle and a frame from the last, the entity
// number from the one before it in the list, the frame number from the last plus one, a stat from
// its last value, a sound's point from its entity's origin. An entity's flag bytes are coded after
// its number, under that entity's own last flags, so the bytes come out in the file's order but
// are learned per entity. Text, names and the bytes no model covers go through the byte model. A
// block that does not walk whole (an unknown message, a cut string) is written as its bytes, so
// every recording comes back.
//
// Ten blocks (a second of the game's 10 Hz frames) go in one chunk; a keyframe chunk every
// `keyframe_seconds` starts the models and the predictions over, so a reader can begin there.
//
// `sequel_fixture` writes a made-up recording of the same layout (a signon, then frames of players
// and entities that move, with sounds and effects), for the tests and the lab where no game pack
// is at hand.
//
// ge:summary The sequel's .dm2 recordings packed into replay file v2 and back byte for byte
// ge:keywords sequel dm2 client-recording replay-v2 face field-codec pack unpack round-trip
// ge:keywords entity-delta playerinfo packetentities configstring temp_entity fixture
// ge:skill replay shooter
// ge:test tests/shooters_edge_sequel_demo_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace shooters_edge::client {

// Whether `bytes` is a whole sequel client recording: blocks of 1..1400 bytes, the first opening
// with the server data, then the -1 end and nothing after it.
bool is_sequel_recording(std::span<const uint8_t> bytes);

struct SequelPackReport {
  size_t blocks = 0, walked = 0, kept = 0;  // dm2 blocks; walked whole; kept as their bytes
  size_t chunks = 0, keyframes = 0;
  size_t messages = 0;
};

struct SequelFace;  // the predictions and models, kept between chunks

struct SequelCoder {
  std::unique_ptr<SequelFace> face;
  SequelCoder();
  ~SequelCoder();
  SequelCoder(SequelCoder&&) noexcept;
  SequelCoder& operator=(SequelCoder&&) noexcept;
};

// The blocks `blocks` (each a dm2 block's messages, its length taken off) as one chunk; `keyframe`
// starts the face over first.
void pack_sequel_chunk(SequelCoder& c, const std::vector<std::span<const uint8_t>>& blocks,
                       bool keyframe, std::vector<uint8_t>& chunk, SequelPackReport* report);
// A chunk back into dm2 blocks (each with its length) appended to `out`. False with `error` when
// the chunk is damaged.
bool unpack_sequel_chunk(SequelCoder& c, std::span<const uint8_t> chunk, bool keyframe,
                         std::vector<uint8_t>& out, std::string& error);
// The recording's blocks, each its messages without the length (the recording checked first with
// `is_sequel_recording`).
std::vector<std::span<const uint8_t>> sequel_blocks(std::span<const uint8_t> recording);

// A made-up recording of `seconds` of play off `seed`: `players` moving players and `things`
// other entities, with sounds, muzzle flashes and effects, framed as the game's own.
std::vector<uint8_t> sequel_fixture(int seconds, uint32_t seed, int players = 4, int things = 24);

}  // namespace shooters_edge::client
