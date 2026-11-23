// Quake II's extended limits (plugins/classic/protocol/q2_limits.hpp): the configstring layouts
// and their remap, the minor's clip, the byte codings and the 23-bit coordinate.
//
// ge:summary Tests Quake II's extended limits: layouts, remap, codings, 23-bit coordinates
// ge:keywords test q2 extended-limits coord23 q2_cs_remap
// ge:skill classic net testing
#include <cstdint>

#include "check.hpp"
#include "classic/protocol/q2_limits.hpp"

namespace cp = ge::classic::protocol;
namespace gn = ge::net;

TEST(each_layouts_blocks_follow_one_another_by_their_limits) {
  for (bool ext : {false, true}) {
    const cp::Q2CsLayout& l = cp::q2_cs_layout(ext);
    const cp::Q2Limits& n = cp::q2_limits(ext);
    CHECK(l.max_clients == l.air_accel + 1 && l.map_checksum == l.air_accel + 2);
    CHECK(l.models == l.map_checksum + 1);
    CHECK(l.sounds == l.models + n.models && l.images == l.sounds + n.sounds);
    CHECK(l.lights == l.images + n.images && l.items == l.lights + n.lights);
    CHECK(l.player_skins == l.items + n.items && l.general == l.player_skins + n.clients);
    CHECK(l.end == l.general + n.general);
  }
  CHECK(cp::kQ2CsExtended.end == 13630 && cp::kQ2Cs.end == 2080);
}

TEST(a_configstring_moves_to_the_same_slot_in_the_other_layout) {
  CHECK(cp::q2_cs_remap(0, cp::kQ2Cs, cp::kQ2CsExtended) == 0);
  CHECK(cp::q2_cs_remap(5, cp::kQ2Cs, cp::kQ2CsExtended) == 5);
  CHECK(cp::q2_cs_remap(28, cp::kQ2Cs, cp::kQ2CsExtended) == 28);  // the status bar's tail
  CHECK(cp::q2_cs_remap(29, cp::kQ2Cs, cp::kQ2CsExtended) == 59);
  CHECK(cp::q2_cs_remap(31, cp::kQ2Cs, cp::kQ2CsExtended) == 61);
  CHECK(cp::q2_cs_remap(32 + 7, cp::kQ2Cs, cp::kQ2CsExtended) == 62 + 7);
  CHECK(cp::q2_cs_remap(288 + 3, cp::kQ2Cs, cp::kQ2CsExtended) == 8254 + 3);
  CHECK(cp::q2_cs_remap(1312 + 2, cp::kQ2Cs, cp::kQ2CsExtended) == 12862 + 2);
  CHECK(cp::q2_cs_remap(2079, cp::kQ2Cs, cp::kQ2CsExtended) == 13629);
  CHECK(cp::q2_cs_remap(2080, cp::kQ2Cs, cp::kQ2CsExtended) == -1);
  // Back: a model past the ordinary 256 has no slot; the status bar's far tail neither.
  CHECK(cp::q2_cs_remap(62 + 300, cp::kQ2CsExtended, cp::kQ2Cs) == -1);
  CHECK(cp::q2_cs_remap(40, cp::kQ2CsExtended, cp::kQ2Cs) == -1);
  CHECK(cp::q2_cs_remap(10302 + 9, cp::kQ2CsExtended, cp::kQ2Cs) == 544 + 9);
  for (int i = 0; i < cp::kQ2Cs.end; i++)
    CHECK(cp::q2_cs_remap(cp::q2_cs_remap(i, cp::kQ2Cs, cp::kQ2CsExtended), cp::kQ2CsExtended,
                          cp::kQ2Cs) == i);
}

TEST(a_clients_minor_is_clipped_into_the_window) {
  CHECK(cp::q2_clip_minor(900) == 1015 && cp::q2_clip_minor(1016) == 1015);
  CHECK(cp::q2_clip_minor(1024) == 1024 && cp::q2_clip_minor(5000) == 1026);
}

TEST(the_extended_bytes_code_their_defaults_as_zero) {
  CHECK(cp::q2_alpha_to_wire(0.5f) == 128 && cp::q2_alpha_to_wire(0) == 1);
  CHECK(cp::q2_alpha_from_wire(0) == 1.0f);
  CHECK(cp::q2_scale_to_wire(1) == 0 && cp::q2_scale_to_wire(2) == 32);
  CHECK(cp::q2_scale_from_wire(32) == 2.0f && cp::q2_scale_from_wire(0) == 1.0f);
  CHECK(cp::q2_loop_volume_to_wire(1) == 0 && cp::q2_loop_volume_to_wire(0.5f) == 128);
  CHECK(cp::q2_loop_attenuation_to_wire(3) == 0 && cp::q2_loop_attenuation_to_wire(-1) == 192);
  CHECK(cp::q2_loop_attenuation_to_wire(1) == 64);
  CHECK(cp::q2_loop_attenuation_from_wire(0) == 3.0f);
  CHECK(cp::q2_loop_attenuation_from_wire(192) == -1.0f);
  CHECK(cp::q2_loop_attenuation_from_wire(64) == 1.0f);
}

TEST(a_23_bit_coordinate_is_a_short_change_or_three_absolute_bytes) {
  struct Case {
    int32_t value, from;
    size_t size;
  };
  const Case cases[] = {{100, 90, 2},     {-16384, 0, 2},        {16383, 0, 2},
                        {16384, 0, 3},    {-16385, 0, 3},        {4194303, 0, 3},
                        {-4194304, 0, 3}, {4194303, 4194300, 2}, {-4194304, 4194303, 3}};
  for (const Case& c : cases) {
    uint8_t buf[8];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    cp::write_q2_coord23(w, c.value, c.from);
    CHECK_MSG(w.size == c.size, std::to_string(c.value));
    gn::Reader r = gn::make_reader(buf, w.size);
    CHECK_MSG(cp::read_q2_coord23(r, c.from) == c.value, std::to_string(c.value));
    CHECK(!r.bad && gn::remaining(r) == 0);
  }
  // The short form's low bit is 0 and the long one's 1.
  uint8_t buf[4];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cp::write_q2_coord23(w, 3, 0);
  CHECK(buf[0] == 6 && buf[1] == 0);
  w = gn::make_writer(buf, sizeof buf);
  cp::write_q2_coord23(w, 20000, 0);
  CHECK((buf[0] & 1) == 1);
}
