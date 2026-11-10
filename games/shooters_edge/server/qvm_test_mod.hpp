// THE COMPILED-MODULE TEST MOD: a small game module assembled by hand (plugins/progs/qvm_asm.hpp)
// that drives the module server (qvm_server.hpp) through its exports and system calls the way a
// real match mod does, so the tests and the `se-qvm` lab run without any mod's files.
//
// - init returns its game data block (records of 512 bytes, 64 entities, no extension fields, the
//   strings direct: it never asks for references);
// - load entities precaches the player's model, reads the map's entity text token by token and
//   spawns one entity for every block after the world's, each thinking every 0.1 s and counting
//   its thinks in its `frags`;
// - start frame counts the frames (`kQvmTestFrames`, a word in its memory);
// - connect prints "connected" to everyone; put in server gives the player its model, box, the
//   test arena's start spot, the walking move, a solid box and 100 health;
// - client command answers `hello` with "hi" to the asker (handled), anything else unhandled.
//
// ge:summary The compiled-module test mod: a hand-assembled game module for the module server
// ge:keywords qvm test-mod hand-assembled game-module game-data-block exports qvm_test_mod
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace shooters_edge::server {

// Addresses in the test mod's memory a test reads.
constexpr int32_t kQvmTestFrames = 40;  // start frames counted
constexpr int32_t kQvmTestBlocks = 44;  // entity blocks the map's text had
constexpr int32_t kQvmTestEntitySize = 512;
constexpr int32_t kQvmTestMaxEntities = 64;

std::vector<uint8_t> qvm_test_mod();

}  // namespace shooters_edge::server
