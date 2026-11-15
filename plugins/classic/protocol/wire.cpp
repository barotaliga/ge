// The protocol family's wire (wire.hpp): the quantizers, the field codec and the CRC.
//
// ge:summary The protocol family's wire, defined: rounding, fields, CRC-16, hex
// ge:keywords round-half-away crc16 hex
// ge:skill classic shooter net
// ge:test tests/classic_protocol_test.cpp
#include "classic/protocol/wire.hpp"

#include <cmath>
#include <cstring>

namespace ge::classic::protocol {

namespace {

// Rounds halves away from zero (2.5 -> 3, -2.5 -> -3), in double: exact for every float input.
int64_t round_away(double v) {
  return v >= 0 ? static_cast<int64_t>(std::floor(v + 0.5))
                : -static_cast<int64_t>(std::floor(-v + 0.5));
}

}  // namespace

int16_t coord_to_wire(float f) {
  const float scaled = f * 8.0f;  // in float, as the grid is defined
  return static_cast<int16_t>(static_cast<uint16_t>(round_away(scaled) & 0xffff));
}
float coord_from_wire(int16_t n) { return static_cast<float>(n) * (1.0f / 8.0f); }

uint8_t angle_to_wire(float deg) {
  return static_cast<uint8_t>(round_away(static_cast<double>(deg) * 256.0 / 360.0) & 255);
}
float angle_from_wire(uint8_t b) {
  return static_cast<float>(static_cast<double>(static_cast<int8_t>(b)) * 360.0 / 256.0);
}
uint16_t angle16_to_wire(float deg) {
  return static_cast<uint16_t>(round_away(static_cast<double>(deg) * 65536.0 / 360.0) & 65535);
}
float angle16_from_wire(uint16_t w) {
  return static_cast<float>(static_cast<double>(static_cast<int16_t>(w)) * 360.0 / 65536.0);
}

void write_byte(gn::Writer& w, int v) { gn::write_u8(w, static_cast<uint8_t>(v & 0xff)); }
void write_char(gn::Writer& w, int v) { gn::write_u8(w, static_cast<uint8_t>(v & 0xff)); }
void write_short(gn::Writer& w, int v) { gn::write_u16(w, static_cast<uint16_t>(v & 0xffff)); }
void write_long(gn::Writer& w, int32_t v) { gn::write_u32(w, static_cast<uint32_t>(v)); }
void write_float(gn::Writer& w, float v) {
  uint32_t u;
  std::memcpy(&u, &v, 4);
  gn::write_u32(w, u);
}
void write_string(gn::Writer& w, const char* s) { gn::write_bytes(w, s, std::strlen(s) + 1); }
void write_coord(gn::Writer& w, float f, Coords c) {
  if (c == Coords::all)
    write_float(w, f);
  else
    gn::write_u16(w, static_cast<uint16_t>(coord_to_wire(f)));
}
void write_origin(gn::Writer& w, float f, Coords c) {
  if (c != Coords::shorts)
    write_float(w, f);
  else
    gn::write_u16(w, static_cast<uint16_t>(coord_to_wire(f)));
}
void write_angle(gn::Writer& w, float deg, Coords c) {
  if (c == Coords::all)
    write_angle16(w, deg);
  else
    gn::write_u8(w, angle_to_wire(deg));
}
void write_angle16(gn::Writer& w, float deg) { gn::write_u16(w, angle16_to_wire(deg)); }

int read_byte(gn::Reader& r) { return gn::read_u8(r); }
int read_char(gn::Reader& r) { return static_cast<int8_t>(gn::read_u8(r)); }
int read_short(gn::Reader& r) { return static_cast<int16_t>(gn::read_u16(r)); }
int32_t read_long(gn::Reader& r) { return static_cast<int32_t>(gn::read_u32(r)); }
float read_float(gn::Reader& r) {
  const uint32_t u = gn::read_u32(r);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
std::string read_string(gn::Reader& r) {
  std::string s;
  while (!r.bad && r.at < r.size) {
    const uint8_t b = r.data[r.at++];
    if (b == 0) return s;
    if (b == 0xff) continue;
    s.push_back(static_cast<char>(b));
  }
  r.bad = true;  // no terminator before the end: the message is cut short
  return s;
}
float read_coord(gn::Reader& r, Coords c) {
  if (c == Coords::all) return read_float(r);
  return coord_from_wire(static_cast<int16_t>(gn::read_u16(r)));
}
float read_origin(gn::Reader& r, Coords c) {
  if (c != Coords::shorts) return read_float(r);
  return coord_from_wire(static_cast<int16_t>(gn::read_u16(r)));
}
float read_angle(gn::Reader& r, Coords c) {
  if (c == Coords::all) return read_angle16(r);
  return angle_from_wire(gn::read_u8(r));
}
float read_angle16(gn::Reader& r) { return angle16_from_wire(gn::read_u16(r)); }

uint16_t crc16(const uint8_t* data, size_t size) {
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < size; i++) {
    crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) != 0 ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

bool from_hex(const std::string& hex, std::string& out) {
  out.clear();
  if (hex == "-") return true;
  if (hex.size() % 2) return false;
  auto nib = [](char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < hex.size(); i += 2) {
    const int a = nib(hex[i]), b = nib(hex[i + 1]);
    if (a < 0 || b < 0) return false;
    out.push_back(static_cast<char>(a * 16 + b));
  }
  return true;
}

std::string to_hex(const uint8_t* data, size_t size) {
  static const char* d = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < size; i++) {
    s.push_back(d[data[i] >> 4]);
    s.push_back(d[data[i] & 15]);
  }
  return s;
}

}  // namespace ge::classic::protocol
