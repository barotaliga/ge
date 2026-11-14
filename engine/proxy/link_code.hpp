// THE LINK'S CODER (docs/tv.md § Proxy): a game datagram coded against a reference datagram the
// other end holds, for the slow link between two proxies, with nothing of the game's in it. A
// game's next datagram is mostly its last one with a few fields moved, so the coder writes the XOR
// with the reference, each byte through the field codec's range coder (engine/replay/codec.hpp) in
// a bit tree under a context: the reference byte's high nibble at that place (16 contexts), or one
// more past the reference's end. The bytes that did not change cost a fraction of a bit each, and
// the tree learns which places move.
//
// THE MODELS LEARN ALONG THE REFERENCES: the coding of datagram N from reference R starts from the
// models as they stood after R was coded (`LinkModels`, kept beside each datagram at both ends), so
// each end reaches the same models whatever the link lost: a datagram's models depend only on its
// chain of references, each one the other end confirmed, never on a datagram it may not have.
//
// THE BYTES: the coder's output without its first byte (always 0) and its trailing zeros (the coder
// is flushed to the value in its last range with the most trailing zero bits, and the reader reads
// zeros past the end). The datagram's length is coded first, less the reference's: mostly 0, a
// fraction of a bit.
//
// ge:summary The link's coder: a datagram's XOR with its reference, range coded by learned models
// ge:keywords proxy link-codec range-coder xor reference models learned code_datagram
// ge:keywords decode_datagram field-codec slow-link compress
// ge:skill net
// ge:test tests/proxy_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "replay/codec.hpp"

namespace ge::proxy {

inline constexpr int kCodeContexts = 17;  // the reference byte's high nibble, and past its end

struct LinkModels {
  std::array<replay::Prob, size_t{kCodeContexts} * 256> tree;
  replay::IntModel size;  // the datagram's length less the reference's
  LinkModels();
};

// `d` coded against `ref` into `out`, `models` taken from the reference's and left as `d`'s. Its
// size; 0 when it would not be smaller than `d` or would not fit `capacity` (the models are then
// left as they came).
size_t code_datagram(std::span<const uint8_t> d, std::span<const uint8_t> ref, LinkModels& models,
                     uint8_t* out, size_t capacity);
// The datagram back from its coding, the same `ref` and the reference's `models` (left as the
// datagram's); false when the bytes are broken.
bool decode_datagram(std::span<const uint8_t> coded, std::span<const uint8_t> ref,
                     LinkModels& models, std::vector<uint8_t>& out, size_t most);

}  // namespace ge::proxy
