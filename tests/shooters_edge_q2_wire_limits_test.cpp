// Quake II's entity and player deltas under the extended limits
// (games/shooters_edge/server/q2_wire_limits.cpp): the fifth flags byte, word model indexes, the
// looping sound's word, more effects, alpha and scale, the extended solid, 23-bit coordinates,
// the split blend and 64 stats; level 0 is the ordinary wire.
//
// ge:summary Tests the extended-limits deltas: entity fields, 23-bit coordinates, the player
// ge:keywords test q2 extended-limits entity-delta player-state coord23 model16
// ge:skill shooter net testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "classic/protocol/q2_limits.hpp"
#include "net/wire.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace sv = shooters_edge::server;
namespace cp = ge::classic::protocol;
namespace gn = ge::net;

namespace {

uint64_t header(gn::Reader& r) {
  uint64_t f = gn::read_u8(r);
  if (f & 0x80) f |= uint64_t(gn::read_u8(r)) << 8;
  if (f & 0x8000) f |= uint64_t(gn::read_u8(r)) << 16;
  if (f & 0x800000) f |= uint64_t(gn::read_u8(r)) << 24;
  if (f & 0x80000000ull) f |= uint64_t(gn::read_u8(r)) << 32;
  return f;
}

}  // namespace

TEST(level_0_is_the_ordinary_wire) {
  sv::Q2EntityState a, b;
  b.origin[0] = 80;
  b.model[0] = 3;
  std::vector<uint8_t> x, y;
  sv::q2_write_entity_ext(x, sv::q2_entity_flags_ext(a, b, 5, false, 0), 5, a, b, 0);
  sv::q2_write_entity(y, sv::q2_entity_flags(a, b, 5, false), 5, b);
  CHECK(x == y);
  sv::Q2PlayerState p, q;
  q.stats[3] = 7;
  x.clear();
  y.clear();
  sv::q2_write_player_ext(x, p, q, 0);
  sv::q2_write_player(y, p, q);
  CHECK(x == y);
}

TEST(an_extended_entity_carries_its_wide_fields_in_order) {
  sv::Q2EntityState a, b;
  b.model[0] = 300;  // past a byte: every model index a word
  b.model[1] = 2;
  b.sound = 1500;
  b.loop_volume = cp::q2_loop_volume_to_wire(0.5f);
  b.more_effects = 0x1234;
  b.alpha = cp::q2_alpha_to_wire(0.5f);
  b.scale = cp::q2_scale_to_wire(2);
  b.solid = 31;
  const uint64_t f = sv::q2_entity_flags_ext(a, b, 4000, false, 1);
  CHECK(f & cp::kQ2UModel16);
  CHECK(f & cp::kQ2UScale);
  std::vector<uint8_t> out;
  sv::q2_write_entity_ext(out, f, 4000, a, b, 1);
  gn::Reader r = gn::make_reader(out.data(), out.size());
  const uint64_t got = header(r);
  CHECK((got & ~uint64_t(0x80808080u)) == (f & ~uint64_t(0x80808080u)));
  CHECK(gn::read_u16(r) == 4000);  // the number a word
  CHECK(gn::read_u16(r) == 300 && gn::read_u16(r) == 2);
  const uint16_t sound = gn::read_u16(r);
  CHECK((sound & cp::kQ2LoopIndexMask) == 1500 && (sound & cp::kQ2LoopVolume) &&
        !(sound & cp::kQ2LoopAttenuation));
  CHECK(gn::read_u8(r) == 128);      // the loop's volume
  CHECK(gn::read_u32(r) == 31);      // the solid a long
  CHECK(gn::read_u16(r) == 0x1234);  // more effects, a word
  CHECK(gn::read_u8(r) == 128);      // alpha
  CHECK(gn::read_u8(r) == 32);       // scale
  CHECK(!r.bad && gn::remaining(r) == 0);
}

TEST(the_extended_solid_holds_each_side_in_a_byte) {
  const int classic = 2 | (3 << 5) | (8 << 10);  // half-width 16, 24 down, 32 up
  CHECK(sv::q2_ext_solid(classic) == int32_t(16 | 16 << 8 | 24 << 16 | 64 << 24));
  CHECK(sv::q2_ext_solid(31) == 31 && sv::q2_ext_solid(0) == 0);
}

TEST(the_second_level_sends_origins_as_23_bit_coordinates) {
  sv::Q2EntityState a, b;
  a.origin[0] = 1000;
  b.origin[0] = 1008;   // a small change: a word
  b.origin[1] = 40000;  // past a word: three bytes, absolute
  const uint64_t f = sv::q2_entity_flags_ext(a, b, 9, false, 2);
  std::vector<uint8_t> out;
  sv::q2_write_entity_ext(out, f, 9, a, b, 2);
  gn::Reader r = gn::make_reader(out.data(), out.size());
  header(r);
  CHECK(gn::read_u8(r) == 9);
  CHECK(cp::read_q2_coord23(r, a.origin[0]) == 1008);
  CHECK(cp::read_q2_coord23(r, a.origin[1]) == 40000);
  CHECK(!r.bad && gn::remaining(r) == 0);
}

TEST(the_second_levels_player_has_64_stats_and_a_split_blend) {
  sv::Q2PlayerState a, b;
  b.origin[2] = 30000;
  b.time = 2040;  // milliseconds: a word
  b.damage_blend[3] = 200;
  b.stats[40] = 9;
  b.gun_index = 300 | (2 << 13);
  std::vector<uint8_t> out;
  sv::q2_write_player_ext(out, a, b, 2);
  gn::Reader r = gn::make_reader(out.data(), out.size());
  CHECK(gn::read_u8(r) == 17);
  const uint16_t f = gn::read_u16(r);
  CHECK((f & 2) && (f & 8) && (f & 1024) && (f & 4096));
  for (int k = 0; k < 3; k++) cp::read_q2_coord23(r, 0);
  CHECK(gn::read_u16(r) == 2040);
  CHECK(gn::read_u16(r) == (300 | (2 << 13)));  // the gun's index a word
  CHECK(gn::read_u8(r) == 0x80);                // the damage blend's alpha alone
  CHECK(gn::read_u8(r) == 200);
  CHECK(gn::read_varint(r) == (1ull << 40));
  CHECK(gn::read_u16(r) == 9);
  CHECK(!r.bad && gn::remaining(r) == 0);
}
