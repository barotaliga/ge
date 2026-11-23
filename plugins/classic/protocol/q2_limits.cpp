// Quake II's extended limits (q2_limits.hpp): the minor's clip, the configstring remap and the
// codings of the extended fields.
//
// ge:summary Quake II's extended limits, defined: the remap, the byte codings, 23-bit coordinates
// ge:keywords q2 extended-limits coord23 q2_cs_remap
// ge:skill classic net
// ge:test tests/classic_q2_limits_test.cpp
#include "classic/protocol/q2_limits.hpp"

#include <cmath>

namespace ge::classic::protocol {

namespace {

// Rounds to the nearest integer, halves away from zero, in double.
int round_away(double v) {
  return v >= 0 ? static_cast<int>(std::floor(v + 0.5)) : -static_cast<int>(std::floor(-v + 0.5));
}
uint8_t clamp_byte(double v, int lo) {
  const int n = round_away(v);
  return static_cast<uint8_t>(n < lo ? lo : n > 255 ? 255 : n);
}

}  // namespace

int q2_clip_minor(int minor) {
  if (minor < kQ2MinorLowest) minor = kQ2MinorLowest;
  if (minor > kQ2MinorHighest) minor = kQ2MinorHighest;
  return minor == 1016 ? 1015 : minor;
}

int q2_cs_remap(int index, const Q2CsLayout& from, const Q2CsLayout& to) {
  if (index < 0 || index >= from.end) return -1;
  if (index < kQ2CsStatusBar) return index;
  if (index < from.air_accel) {  // the status bar runs on through the strings after it
    const int at = index - kQ2CsStatusBar;
    return at < to.air_accel - kQ2CsStatusBar ? kQ2CsStatusBar + at : -1;
  }
  struct Block {
    int Q2CsLayout::*start;
    int Q2CsLayout::*next;
  };
  static constexpr Block kBlocks[] = {
      {&Q2CsLayout::air_accel, &Q2CsLayout::max_clients},
      {&Q2CsLayout::max_clients, &Q2CsLayout::map_checksum},
      {&Q2CsLayout::map_checksum, &Q2CsLayout::models},
      {&Q2CsLayout::models, &Q2CsLayout::sounds},
      {&Q2CsLayout::sounds, &Q2CsLayout::images},
      {&Q2CsLayout::images, &Q2CsLayout::lights},
      {&Q2CsLayout::lights, &Q2CsLayout::items},
      {&Q2CsLayout::items, &Q2CsLayout::player_skins},
      {&Q2CsLayout::player_skins, &Q2CsLayout::general},
      {&Q2CsLayout::general, &Q2CsLayout::end},
  };
  for (const Block& b : kBlocks) {
    if (index >= from.*b.next) continue;
    const int at = index - from.*b.start;
    return at < to.*b.next - to.*b.start ? to.*b.start + at : -1;
  }
  return -1;
}

uint8_t q2_alpha_to_wire(float alpha) { return clamp_byte(static_cast<double>(alpha) * 255, 1); }
float q2_alpha_from_wire(uint8_t b) { return b == 0 ? 1.0f : static_cast<float>(b) / 255.0f; }

uint8_t q2_scale_to_wire(float scale) {
  const uint8_t b = clamp_byte(static_cast<double>(scale) * 16, 1);
  return b == 16 ? 0 : b;
}
float q2_scale_from_wire(uint8_t b) { return b == 0 ? 1.0f : static_cast<float>(b) / 16.0f; }

uint8_t q2_loop_volume_to_wire(float volume) {
  const uint8_t b = clamp_byte(static_cast<double>(volume) * 255, 1);
  return b == 255 ? 0 : b;
}
float q2_loop_volume_from_wire(uint8_t b) { return b == 0 ? 1.0f : static_cast<float>(b) / 255.0f; }

uint8_t q2_loop_attenuation_to_wire(float attenuation) {
  if (attenuation < 0) return 192;  // none
  const uint8_t b = clamp_byte(static_cast<double>(attenuation) * 64, 1);
  return b == 192 ? 0 : b;  // static (3)
}
float q2_loop_attenuation_from_wire(uint8_t b) {
  if (b == 0) return 3.0f;
  if (b == 192) return -1.0f;
  return static_cast<float>(b) / 64.0f;
}

void write_q2_coord23(net::Writer& w, int32_t value, int32_t from) {
  const int64_t delta = static_cast<int64_t>(value) - from;
  if (delta >= -16384 && delta <= 16383) {
    net::write_u16(w, static_cast<uint16_t>(static_cast<uint32_t>(delta) << 1));
    return;
  }
  const uint32_t v = (static_cast<uint32_t>(value) << 1) | 1u;
  net::write_u8(w, static_cast<uint8_t>(v));
  net::write_u8(w, static_cast<uint8_t>(v >> 8));
  net::write_u8(w, static_cast<uint8_t>(v >> 16));
}

int32_t read_q2_coord23(net::Reader& r, int32_t from) {
  const uint32_t lo = net::read_u8(r);
  if (!(lo & 1)) {
    const uint32_t w = lo | (static_cast<uint32_t>(net::read_u8(r)) << 8);
    return from + (static_cast<int16_t>(static_cast<uint16_t>(w)) >> 1);
  }
  uint32_t v = lo | (static_cast<uint32_t>(net::read_u8(r)) << 8) |
               (static_cast<uint32_t>(net::read_u8(r)) << 16);
  v >>= 1;  // 23 bits
  return static_cast<int32_t>(v << 9) >> 9;
}

}  // namespace ge::classic::protocol
