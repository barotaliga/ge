// THE CLASSIC PROTOCOL'S WIRE (version 28): the sizes, the quantization of coordinates and angles,
// the readers and writers of its field kinds, the CRC, the move checksum and the delta-coded
// usercmd. Every multi-byte field is little endian; a string is its bytes and a NUL, and a reader
// drops byte 0xFF inside one and stops at the buffer's end when no NUL comes.
//
// QUANTIZATION (held by tests/golden/shooters_edge_classic_quantize.txt):
//   coord    i16  write: round half away from zero of `f x 8` computed in 32-bit float; read n / 8
//   angle    u8   write: round_half_away(f x 256 / 360) & 255 in double; read SIGNED x 360 / 256
//   angle16  u16  write: round_half_away(f x 65536 / 360) & 65535; read signed x 360 / 65536
// Wider coordinates are negotiated (classic_ext.hpp): under the x family's float-coords bit every
// coord is a float and every angle an angle16 (`Coords::all`); under the demo family's float
// coords only entity and player ORIGINS are floats (`Coords::origins`).
//
// THE MOVE CHECKSUM (`move_checksum`): the first 60 bytes of the move's body (the loss byte to
// the end of the third usercmd), then four bytes of a 1024-byte key table at `sequence mod 1020`
// (the first XOR the sequence's low byte, the third XOR its second byte), through CRC-16
// (polynomial 0x1021, initial 0xFFFF, msb first, no reflection, no final XOR); the checksum is
// the CRC's low byte. The key table is an arbitrary constant this tree does not carry: it is a
// seam (`ChecksumTable`), and without one the byte cannot be computed, so a server that is not
// handed a table does not verify it (classic_server.hpp, `verify_move_checksum`). Any table
// supplied later must reproduce tests/golden/shooters_edge_classic_checksum.txt.
//
// THE DELTA USERCMD: a bits byte (pitch 0x01, yaw 0x80, roll 0x02, forward 0x04, side 0x08, up
// 0x10, buttons 0x20, impulse 0x40), then the changed fields in the order pitch, yaw, roll,
// forward, side, up, buttons, impulse (whatever the bits' numeric order), then msec always.
// Angles travel as angle16, the axes as shorts, buttons and impulse as bytes.
//
// ge:summary The classic protocol's wire: quantization, field readers and writers, CRC, usercmds
// ge:keywords classic-protocol wire quantize coord angle angle16 crc16 move-checksum
// ge:keywords checksum-table delta-usercmd usercmd string-field little-endian coords float-coords
// ge:keywords write_coord read_coord write_delta_usercmd read_delta_usercmd crc16 move_checksum
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "net/wire.hpp"
#include "shooter/pmove.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;
namespace sh = ge::shooter;

constexpr int kProtocolVersion = 28;
constexpr size_t kMaxMsgLen = 1450;   // the largest in-band message
constexpr size_t kMaxPacket = 1450;   // the largest datagram either side sends
constexpr size_t kMinMtu = 1350;      // the smallest MTU a client may ask for
constexpr size_t kServerHeader = 8;   // the net channel's header from a server
constexpr size_t kClientHeader = 10;  // from a client (the qport added)
constexpr int kChecksumCovered = 60;  // bytes of the move's body the checksum covers
constexpr int kChecksumTableSize = 1024;
constexpr uint16_t kPortMaster = 27000, kPortClient = 27001, kPortServer = 27500;
constexpr uint16_t kPortRelay = 27900;

// Which coordinates travel as floats (negotiated; classic_ext.hpp).
enum class Coords : uint8_t { shorts, origins, all };

// ---- Quantization -------------------------------------------------------------------------
int16_t coord_to_wire(float f);
float coord_from_wire(int16_t n);
uint8_t angle_to_wire(float deg);
float angle_from_wire(uint8_t b);
uint16_t angle16_to_wire(float deg);
float angle16_from_wire(uint16_t w);
// A coordinate as a receiver reads it back (the round trip through the grid).
inline float coord_grid(float f) { return coord_from_wire(coord_to_wire(f)); }

// ---- Writers (onto ge::net's bounded writer; overflow sets `w.overflow`) -------------------
void write_byte(gn::Writer& w, int v);
void write_char(gn::Writer& w, int v);
void write_short(gn::Writer& w, int v);
void write_long(gn::Writer& w, int32_t v);
void write_float(gn::Writer& w, float v);
void write_string(gn::Writer& w, const char* s);
inline void write_string(gn::Writer& w, const std::string& s) { write_string(w, s.c_str()); }
void write_coord(gn::Writer& w, float f, Coords c = Coords::shorts);
void write_origin(gn::Writer& w, float f, Coords c);  // an origin: float under `origins` too
void write_angle(gn::Writer& w, float deg, Coords c = Coords::shorts);
void write_angle16(gn::Writer& w, float deg);

// ---- Readers (`r.bad` set past the end; a value read past it is 0) ------------------------
int read_byte(gn::Reader& r);
int read_char(gn::Reader& r);
int read_short(gn::Reader& r);
int32_t read_long(gn::Reader& r);
float read_float(gn::Reader& r);
std::string read_string(gn::Reader& r);
float read_coord(gn::Reader& r, Coords c = Coords::shorts);
float read_origin(gn::Reader& r, Coords c);
float read_angle(gn::Reader& r, Coords c = Coords::shorts);
float read_angle16(gn::Reader& r);

// ---- CRC and the move checksum --------------------------------------------------------------
uint16_t crc16(const uint8_t* data, size_t size);

// The key table (classic_checksum_table.cpp): a wire constant kept for interop.
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

// "1a2b..." as bytes (tests and the lab); false on an odd length or a non-hex character.
bool from_hex(const std::string& hex, std::string& out);
std::string to_hex(const uint8_t* data, size_t size);

}  // namespace shooters_edge::net
