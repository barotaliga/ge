// Tests: the classic protocol's wire (games/shooters_edge/net/classic_wire.hpp, classic_ext.hpp,
// classic_info.hpp): coordinates and angles quantized bit-exact against
// tests/golden/shooters_edge_classic_quantize.txt (rounding half away from zero, with the
// truncating form kept beside it), the CRC-16 and the move checksum (72 vectors and a sweep)
// against tests/golden/shooters_edge_classic_checksum.txt and verified by default, the
// fields' widths under each coordinate form, the delta usercmd's bits, the extension sets and the
// info strings.
//
// ge:summary Tests: classic quantization, CRC and move-checksum vectors, usercmds, info strings
// ge:keywords test classic-protocol quantize coord angle angle16 crc16 move-checksum usercmd
// ge:keywords info-string extensions first-tier golden
// ge:skill net shooter testing
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/net/classic_ext.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_wire.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sh = ge::shooter;

namespace {

float as_float(const std::string& hex) {
  const auto u = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
  float f;
  std::memcpy(&f, &u, sizeof f);
  return f;
}

uint32_t bits_of(float f) {
  uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}

unsigned hex(const std::string& s) { return static_cast<unsigned>(std::stoul(s, nullptr, 16)); }

}  // namespace

TEST(coordinates_and_angles_match_the_quantize_vectors) {
  const auto rows = ge::test::golden_words("tests/golden/shooters_edge_classic_quantize.txt");
  CHECK_MSG(rows.size() == 21, "tests/golden/shooters_edge_classic_quantize.txt: 21 rows expected");
  for (const auto& r : rows) {
    const float in = as_float(r[1]);
    if (r[0] == "coord") {
      const int16_t n = cn::coord_to_wire(in);
      CHECK_MSG(static_cast<uint16_t>(n) == hex(r[2]), "coord " + r[1] + " quantized wrongly");
      CHECK_MSG(bits_of(cn::coord_from_wire(n)) == hex(r[3]), "coord " + r[1] + " read wrongly");
      // The truncating form (the 1996 writer) differs on the rows that round away.
      const auto trunc = static_cast<int16_t>(static_cast<int>(in * 8));
      CHECK(static_cast<uint16_t>(trunc) == hex(r[4]));
    } else {
      const uint8_t b = cn::angle_to_wire(in);
      CHECK_MSG(b == hex(r[2]), "angle " + r[1] + " byte quantized wrongly");
      CHECK_MSG(bits_of(cn::angle_from_wire(b)) == hex(r[3]), "angle " + r[1] + " byte read");
      const uint16_t w = cn::angle16_to_wire(in);
      CHECK_MSG(w == hex(r[4]), "angle " + r[1] + " angle16 quantized wrongly");
      CHECK_MSG(bits_of(cn::angle16_from_wire(w)) == hex(r[5]), "angle " + r[1] + " angle16 read");
    }
  }
}

TEST(the_crc_and_the_move_checksum_match_their_vectors) {
  const auto rows = ge::test::golden_words("tests/golden/shooters_edge_classic_checksum.txt");
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
                                      " is wrong: check classic_checksum_table.cpp and the CRC");
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

TEST(fields_take_their_widths_under_each_coordinate_form) {
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  cn::write_coord(w, 1.5f);
  CHECK(w.size == 2);
  cn::write_coord(w, 1.5f, cn::Coords::all);
  CHECK(w.size == 6);
  cn::write_origin(w, 1.5f, cn::Coords::origins);
  CHECK(w.size == 10);
  cn::write_angle(w, 90, cn::Coords::all);
  CHECK(w.size == 12);
  cn::write_string(w, "hi");
  CHECK(w.size == 15);
  gn::Reader r = gn::make_reader(buf, w.size);
  CHECK(cn::read_coord(r) == 1.5f);
  CHECK(cn::read_coord(r, cn::Coords::all) == 1.5f);
  CHECK(cn::read_origin(r, cn::Coords::origins) == 1.5f);
  CHECK(cn::read_angle(r, cn::Coords::all) == 90.0f);
  CHECK(cn::read_string(r) == "hi");
  CHECK(!r.bad);
}

TEST(strings_drop_ff_bytes_on_read) {
  const uint8_t raw[] = {'a', 0xff, 'b', 0};
  gn::Reader r = gn::make_reader(raw, sizeof raw);
  CHECK(cn::read_string(r) == "ab");
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

TEST(info_strings_set_get_and_refuse) {
  std::string info;
  CHECK(cn::info_set(info, "name", "alice"));
  CHECK(cn::info_set(info, "team", "red"));
  CHECK(info == "\\name\\alice\\team\\red");
  CHECK(cn::info_get(info, "team") == "red");
  CHECK(cn::info_set(info, "name", "bob"));
  CHECK(cn::info_get(info, "name") == "bob");
  CHECK(!cn::info_set(info, "bad\\key", "x"));
  CHECK(!cn::info_set(info, "k", "a\"b"));
  cn::info_remove(info, "team");
  CHECK(cn::info_get(info, "team").empty());
  CHECK(cn::info_set(info, "*ver", "1"));
  CHECK(cn::info_set(info, "_secret", "1"));
  CHECK(cn::info_public(info).find("_secret") == std::string::npos);
  CHECK(cn::info_public(info).find("*ver") != std::string::npos);
  CHECK(cn::info_valid(info));
  CHECK(cn::sanitize_name("  ok  ") == "ok");
  CHECK(cn::sanitize_name("a;b") == "player");
  CHECK(cn::sanitize_name("Console") == "player");
  CHECK(cn::unique_name("ok", {"ok"}) == "(1)ok");
  CHECK(cn::unique_name("ok", {"ok", "(1)ok"}) == "(2)ok");
  CHECK(cn::unique_name("free", {"ok"}) == "free");
}
