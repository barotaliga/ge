// THE WIRE OF THE CLASSIC PROTOCOL FAMILY: what the first game's protocol and its sequel's share
// below their messages. The sizes of a message and a datagram, the net channel's headers, the
// quantization of coordinates and angles, the readers and writers of the field kinds and the
// CRC-16. Every multi-byte field is little endian; a string is its bytes and a NUL, and a reader
// drops byte 0xFF inside one and stops at the buffer's end when no NUL comes. A game's own
// protocol (its version, ports, messages, move checksum and usercmd) is built on these in the
// game's module (Shooter's Edge's: games/shooters_edge/net/classic_wire.hpp, sequel_net.hpp).
//
// QUANTIZATION (held by tests/golden/classic_protocol_quantize.txt):
//   coord    i16  write: round half away from zero of `f x 8` computed in 32-bit float; read n / 8
//   angle    u8   write: round_half_away(f x 256 / 360) & 255 in double; read SIGNED x 360 / 256
//   angle16  u16  write: round_half_away(f x 65536 / 360) & 65535; read signed x 360 / 65536
// Wider coordinates are negotiated by a game's protocol: under `Coords::all` every coord is a
// float and every angle an angle16; under `Coords::origins` only entity and player ORIGINS are
// floats.
//
// THE CRC-16: polynomial 0x1021, initial 0xFFFF, msb first, no reflection, no final XOR.
//
// ge:summary The classic protocol family's wire: sizes, quantization, fields, CRC-16
// ge:keywords classic-protocol protocol-family wire quantize coord angle angle16 crc16
// ge:keywords string-field little-endian coords float-coords write_coord read_coord max-packet
// ge:keywords netchan-header from_hex to_hex
// ge:skill classic shooter net
// ge:test tests/classic_protocol_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "net/wire.hpp"

namespace ge::classic::protocol {

namespace gn = ge::net;

constexpr size_t kMaxMsgLen = 1450;   // the largest in-band message
constexpr size_t kMaxPacket = 1450;   // the largest datagram either side sends
constexpr size_t kServerHeader = 8;   // the net channel's header from a server
constexpr size_t kClientHeader = 10;  // from a client (the qport added)

// Which coordinates travel as floats (negotiated by a game's protocol).
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

// ---- The CRC ---------------------------------------------------------------------------------
uint16_t crc16(const uint8_t* data, size_t size);

// "1a2b..." as bytes (tests and the labs; "-" is no bytes); false on an odd length or a
// non-hex character.
bool from_hex(const std::string& hex, std::string& out);
std::string to_hex(const uint8_t* data, size_t size);

}  // namespace ge::classic::protocol
