// Tests: the classic protocol's own wire (games/shooters_edge/net/qw_wire.hpp,
// qw_ext.hpp): the CRC-16 and the move checksum (72 vectors and a sweep) against
// tests/golden/shooters_edge_qw_checksum.txt and verified by default, the delta usercmd's
// bits and the extension sets. The family's fields and info strings are
// tests/classic_protocol_test.cpp's.
//
// ge:summary Tests: the classic move-checksum vectors, usercmd deltas, the extension sets
// ge:keywords test classic-protocol crc16 move-checksum usercmd extensions first-tier golden
// ge:skill net shooter testing
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/net/qw_wire.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sh = ge::shooter;

namespace {

unsigned hex(const std::string& s) { return static_cast<unsigned>(std::stoul(s, nullptr, 16)); }

}  // namespace

TEST(the_crc_and_the_move_checksum_match_their_vectors) {
  const auto rows = ge::test::golden_words("tests/golden/shooters_edge_qw_checksum.txt");
  int crcs = 0, chks = 0, sweeps = 0;
  for (const auto& r : rows) {
    if (r[0] == "crc16") {
      const std::string bytes = ge::test::unhex(r[1]);
      const uint16_t crc = cn::crc16(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
      CHECK_MSG(crc == hex(r[2]), "crc16 of " + r[1] + " is wrong");
      crcs++;
    } else if (r[0] == "chk") {
      const std::string body = ge::test::unhex(r[2]);
      const auto seq = static_cast<uint32_t>(std::stoul(r[1]));
      const uint8_t got =
          cn::move_checksum(reinterpret_cast<const uint8_t*>(body.data()), body.size(), seq);
      CHECK_MSG(got == hex(r[3]), "move checksum at sequence " + r[1] + " of " + r[2] +
                                      " is wrong: check qw_checksum_table.cpp and the CRC");
      chks++;
    } else if (r[0] == "sweep") {
      const std::string body = ge::test::unhex(r[1]);
      uint32_t h = 0x811c9dc5u;
      for (uint32_t s = static_cast<uint32_t>(std::stoul(r[2]));
           s <= static_cast<uint32_t>(std::stoul(r[3])); s++) {
        h ^= cn::move_checksum(reinterpret_cast<const uint8_t*>(body.data()), body.size(), s);
        h *= 16777619u;
      }
      CHECK_MSG(h == hex(r[4]), "the checksum sweep's digest moved: a key byte is wrong");
      sweeps++;
    }
  }
  CHECK(crcs == 4 && chks == 72 && sweeps == 1);
}

TEST(the_server_verifies_the_move_checksum_by_default) {
  const cn::ServerConfig cfg;
  CHECK(cfg.verify_move_checksum);
  // Only the first 60 bytes are covered.
  uint8_t body[70];
  for (int i = 0; i < 70; i++) body[i] = static_cast<uint8_t>((i * 37 + 11) & 255);
  CHECK(cn::move_checksum(body, 60, 7) == cn::move_checksum(body, 70, 7));
}

TEST(a_delta_usercmd_writes_only_what_changed_and_reads_back) {
  sh::MoveCmd from;
  from.msec = 13;
  sh::MoveCmd to = from;
  to.angles = {45, 90, 0};
  to.forward = 400;
  to.buttons = 1;
  uint8_t buf[32];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_delta_usercmd(w, from, to);
  // bits, pitch, yaw, forward, buttons, msec
  CHECK(w.size == 1 + 2 + 2 + 2 + 1 + 1);
  CHECK(buf[0] == (cn::kCmPitch | cn::kCmYaw | cn::kCmForward | cn::kCmButtons));
  gn::Reader r = gn::make_reader(buf, w.size);
  uint8_t bits = 0;
  const sh::MoveCmd got = cn::read_delta_usercmd(r, from, &bits);
  CHECK(!r.bad && remaining(r) == 0);
  CHECK(got.angles.x == 45 && got.angles.y == 90 && got.forward == 400 && got.buttons == 1);
  CHECK(got.msec == 13);
  // An unchanged command is the bits byte and msec.
  w = gn::make_writer(buf, sizeof buf);
  cn::write_delta_usercmd(w, to, to);
  CHECK(w.size == 2 && buf[0] == 0);
}

TEST(the_first_tier_is_offered_and_agreed_bit_by_bit) {
  const cn::Ext offered = cn::first_tier();
  CHECK(offered.has(cn::ExtFamily::x, cn::kXChunkedDl));
  CHECK(offered.has(cn::ExtFamily::z, cn::kZPmType));
  CHECK(!offered.has(cn::ExtFamily::x, cn::kXFloatCoords));
  CHECK(cn::first_tier(true).has(cn::ExtFamily::x, cn::kXFloatCoords));
  cn::Ext asked;
  asked[cn::ExtFamily::x] = cn::kXChunkedDl | cn::kXClientProgs | cn::kXFloatCoords;
  const cn::Ext a = cn::agree(offered, asked);
  CHECK(a[cn::ExtFamily::x] == (asked[cn::ExtFamily::x] & offered[cn::ExtFamily::x]));
  CHECK(cn::coords_of(a) == cn::Coords::shorts);
  cn::ExtFamily f;
  CHECK(cn::family_of_magic(cn::kMagicX, f) && f == cn::ExtFamily::x);
  CHECK(!cn::family_of_magic(0x12345678, f));
  CHECK(cn::describe_ext(cn::Ext{}) == "none");
}
