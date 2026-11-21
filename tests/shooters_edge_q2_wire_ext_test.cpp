// games/shooters_edge/server/q2_wire_ext.cpp: Quake II's later protocols on the server's wire. A
// solid packs into its long form and the two special ones stay; the server data's tail is each
// protocol's; the frame's top carries the extra flags in the command byte's top bits and the
// suppress byte's high nibble, leaves out what the client keeps, and names the viewer's slot on a
// whole frame; a client asking for no gibs, no footsteps and its own entity left alone gets them
// so; the signon packs into streams on a newer minor and into one game state on an older one.
//
// ge:summary Test: Quake II's protocol 35 and 36 wire: long solids, the frame top, the streams
// ge:keywords test q2 protocol-35 protocol-36 long-solid frame-top extra-flags gamestate
// ge:keywords configstring-stream baseline-stream nogibs nofootsteps
// ge:skill shooter net testing
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace {

namespace sv = shooters_edge::server;

TEST(a_solid_packs_into_its_long_form) {
  CHECK(sv::q2_long_solid(0) == 0);
  CHECK(sv::q2_long_solid(31) == 31);
  // x 16 (2 units of 8), down 24 (3), up 32 (8 * 8 - 32).
  const int solid = 2 | (3 << 5) | (8 << 10);
  const int32_t packed = sv::q2_long_solid(solid);
  CHECK((packed & 0xFF) == 16);
  CHECK(((packed >> 8) & 0xFF) == 24);
  CHECK(((packed >> 16) & 0xFFFF) - 32768 == 32);
}

TEST(the_server_datas_tail_is_each_protocols) {
  std::vector<uint8_t> out;
  sv::Q2WireOptions opt;
  sv::q2_server_data_tail(out, opt);
  CHECK(out.empty());  // 34: nothing
  opt.protocol = 35;
  opt.minor = 1905;
  sv::q2_server_data_tail(out, opt);
  CHECK(out == (std::vector<uint8_t>{0, 0x71, 0x07, 0, 0}));
  out.clear();
  opt.protocol = 36;
  opt.minor = 1024;
  sv::q2_server_data_tail(out, opt);
  CHECK(out == (std::vector<uint8_t>{0x00, 0x04, 2, 0, 0}));
  out.clear();
  opt.minor = 1015;
  sv::q2_server_data_tail(out, opt);
  CHECK(out == (std::vector<uint8_t>{0xF7, 0x03, 2, 0, 0, 0}));
}

TEST(the_frame_top_carries_the_extra_flags_and_the_slot) {
  sv::Q2Wire w;
  w.opt.protocol = 36;
  w.opt.minor = 1024;
  w.slot = 3;
  sv::Q2PlayerState p;
  p.origin[2] = 40;     // extra: origin z (bit 3)
  p.stats[1] = 100;     // extra: stats (bit 5)
  p.view_angles[0] = 7; // left out: the client predicts its own view
  std::vector<uint8_t> out;
  sv::q2_write_frame_top_ext(out, w, 5, p);
  const int extra = (out[0] >> 5) << 4 | out[5] >> 4;
  CHECK((out[0] & 0x1F) == 20);
  CHECK((extra & (1 << 3)) != 0);
  CHECK((extra & (1 << 5)) != 0);
  CHECK((extra & (1 << 6)) != 0);  // a whole frame names the viewer's slot
  const uint32_t word = uint32_t(out[1]) | uint32_t(out[2]) << 8 | uint32_t(out[3]) << 16 |
                        uint32_t(out[4]) << 24;
  CHECK((word & 0x07FFFFFFu) == 5);
  CHECK((word >> 27) == 31);  // no frame to delta from
  CHECK(w.last_player.view_angles[0] == 0);
  // The slot is the message's last byte (a short from minor 1022).
  CHECK(out[out.size() - 2] == 3 && out.back() == 0);
  // The next frame deltas: no slot again.
  w.last_frame = 5;
  out.clear();
  sv::q2_write_frame_top_ext(out, w, 6, p);
  const int again = (out[0] >> 5) << 4 | out[5] >> 4;
  CHECK((again & (1 << 6)) == 0);
  CHECK((again & ((1 << 3) | (1 << 5))) == 0);  // nothing changed
}

TEST(a_clients_asks_shape_its_entities) {
  sv::Q2Wire w;
  w.opt.protocol = 36;
  w.slot = 0;
  w.asked.no_gibs = true;
  w.asked.no_footsteps = true;
  const sv::Q2EntityState base;
  sv::Q2EntityState gib;
  gib.present = true;
  gib.effects = 0x2;
  sv::q2_entity_for_ext(w, 5, base, gib);
  CHECK(!gib.present);
  sv::Q2EntityState walker;
  walker.present = true;
  walker.event = 2;
  sv::q2_entity_for_ext(w, 6, base, walker);
  CHECK(walker.present && walker.event == 0);
  // The viewer's own entity keeps what the client holds.
  sv::Q2EntityState self;
  self.present = true;
  self.origin[0] = 99;
  sv::q2_entity_for_ext(w, 1, base, self);
  CHECK(self.origin[0] == 0);
}

TEST(the_signon_packs_into_streams_or_one_game_state) {
  sv::Q2Signon parts;
  for (int i = 0; i < 3; i++)
    parts.config_strings.emplace_back(i, std::vector<uint8_t>{13, uint8_t(i), 0, 'a', 0});
  parts.baselines.emplace_back(1, std::vector<uint8_t>{14, 0x01, 1, 9});
  std::vector<std::vector<uint8_t>> out;
  sv::q2_signon_streams(parts, 1024, out);
  CHECK(out.size() == 2);
  CHECK(out[0][0] == 25 && out[1][0] == 26);
  CHECK(out[0].size() == 1 + 3 * 4 + 2);
  CHECK(out[1] == (std::vector<uint8_t>{26, 0x01, 1, 9, 0, 0}));
  sv::q2_signon_streams(parts, 1020, out);
  CHECK(out.size() == 1);
  CHECK(out[0][0] == 23);
  CHECK(out[0].size() == 1 + 3 * 4 + 2 + 3 + 2);
}

}  // namespace
