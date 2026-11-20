// THE QUAKEWORLD PROTOCOL'S OWN WIRE (version 28): its version, ports and sizes, the move checksum
// and the delta-coded usercmd, over the classic protocol family's fields, quantization, CRC-16, net
// channel and info strings (plugins/classic/protocol, named here through net/protocol.hpp).
//
// THE MOVE CHECKSUM (`move_checksum`): the first 60 bytes of the move's body (the loss byte to
// the end of the third usercmd), then four bytes of a 1024-byte key table at `sequence mod 1020`
// (the first XOR the sequence's low byte, the third XOR its second byte), through CRC-16
// (polynomial 0x1021, initial 0xFFFF, msb first, no reflection, no final XOR); the checksum is
// the CRC's low byte. The key table is an arbitrary constant this tree does not carry: it is a
// seam (`ChecksumTable`), and without one the byte cannot be computed, so a server that is not
// handed a table does not verify it (qw_server.hpp, `verify_move_checksum`). Any table
// supplied later must reproduce tests/golden/shooters_edge_qw_checksum.txt.
//
// THE DELTA USERCMD: a bits byte (pitch 0x01, yaw 0x80, roll 0x02, forward 0x04, side 0x08, up
// 0x10, buttons 0x20, impulse 0x40), then the changed fields in the order pitch, yaw, roll,
// forward, side, up, buttons, impulse (whatever the bits' numeric order), then msec always.
// Angles travel as angle16, the axes as shorts, buttons and impulse as bytes.
//
// ge:summary The QuakeWorld protocol's own wire: version, ports, the move checksum, usercmd deltas
// ge:keywords qw-protocol wire move-checksum checksum-table delta-usercmd usercmd
// ge:keywords write_delta_usercmd read_delta_usercmd move_checksum protocol-version
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_wire_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "shooter/pmove.hpp"
#include "shooters_edge/net/protocol.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;

constexpr int kProtocolVersion = 28;
constexpr size_t kMinMtu = 1350;      // the smallest MTU a client may ask for
constexpr int kChecksumCovered = 60;  // bytes of the move's body the checksum covers
constexpr int kChecksumTableSize = 1024;
constexpr uint16_t kPortMaster = 27000, kPortClient = 27001, kPortServer = 27500;
constexpr uint16_t kPortRelay = 27900;

// ---- The move checksum -----------------------------------------------------------------------
// The key table (qw_checksum_table.cpp): a wire constant kept for interop.
extern const uint8_t kChecksumTable[kChecksumTableSize];
// The checksum byte of a move body sent in the packet numbered `sequence`: the first 60 bytes of
// the body, four key bytes at `sequence mod 1020` (the first and third XORed with the sequence's
// low and second bytes), and the CRC-16's low byte over them.
uint8_t move_checksum(const uint8_t* body, size_t size, uint32_t sequence);

// ---- The delta usercmd -----------------------------------------------------------------------
constexpr uint8_t kCmPitch = 0x01, kCmRoll = 0x02, kCmForward = 0x04, kCmSide = 0x08;
constexpr uint8_t kCmUp = 0x10, kCmButtons = 0x20, kCmImpulse = 0x40, kCmYaw = 0x80;

void write_delta_usercmd(gn::Writer& w, const sh::MoveCmd& from, const sh::MoveCmd& to);
// `to` from `from` and the bytes; the bits byte read in `bits` when given.
sh::MoveCmd read_delta_usercmd(gn::Reader& r, const sh::MoveCmd& from, uint8_t* bits = nullptr);
// A command as the wire carries it (its angles on the angle16 grid).
sh::MoveCmd usercmd_grid(const sh::MoveCmd& c);

}  // namespace shooters_edge::net
