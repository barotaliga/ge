// THE ENGINE'S HALF OF A HOST TABLE: what every game hands every mod the same way. `fill_host`
// writes the table's size, the ABI and the engine's libm (engine/core/libm: the same bits on every
// platform, so a mod's sine is the game's); the game then sets `ctx`, its random stream, its log
// and its own services table. `random_u32` is the stream's next word for a game whose stream is a
// `core::Rng` (the word is the double `next` draws, times 2^32, so both read one stream).
//
// ge:summary The engine half of a mod host table: size, ABI, the engine's libm, the stream's word
// ge:keywords mod host host-table libm fill_host random_u32 rng stream services
// ge:skill mod determinism
// ge:test tests/mod_loader_test.cpp
#pragma once

#include <cstdint>

#include "core/prng.hpp"
#include "mod/abi.hpp"

namespace ge::mod {

void fill_host(ge_mod_host_v1& h);

inline uint32_t random_u32(core::Rng& r) {
  return static_cast<uint32_t>(core::next(r) * 4294967296.0);
}

}  // namespace ge::mod
