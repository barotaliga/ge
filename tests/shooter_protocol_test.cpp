// Tests: the classic protocol family's wire and info strings (plugins/shooter/protocol/wire.hpp,
// info.hpp): coordinates and angles quantized bit-exact against
// tests/golden/shooter_protocol_quantize.txt (rounding half away from zero, with the truncating
// form kept beside it), the fields' widths under each coordinate form, strings read past 0xFF
// bytes, and info strings set, read and refused, with names cleaned and made unique.
//
// ge:summary Tests: the protocol family's quantization, field widths, strings and info strings
// ge:keywords test protocol-family quantize coord angle angle16 field-width info-string golden
// ge:skill shooter net testing
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/protocol/info.hpp"
#include "shooter/protocol/wire.hpp"

namespace sp = ge::shooter::protocol;
namespace gn = ge::net;

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
  const auto rows = ge::test::golden_words("tests/golden/shooter_protocol_quantize.txt");
  CHECK_MSG(rows.size() == 21, "tests/golden/shooter_protocol_quantize.txt: 21 rows expected");
  for (const auto& r : rows) {
    const float in = as_float(r[1]);
    if (r[0] == "coord") {
      const int16_t n = sp::coord_to_wire(in);
      CHECK_MSG(static_cast<uint16_t>(n) == hex(r[2]), "coord " + r[1] + " quantized wrongly");
      CHECK_MSG(bits_of(sp::coord_from_wire(n)) == hex(r[3]), "coord " + r[1] + " read wrongly");
      // The truncating form (the 1996 writer) differs on the rows that round away.
      const auto trunc = static_cast<int16_t>(static_cast<int>(in * 8));
      CHECK(static_cast<uint16_t>(trunc) == hex(r[4]));
    } else {
      const uint8_t b = sp::angle_to_wire(in);
      CHECK_MSG(b == hex(r[2]), "angle " + r[1] + " byte quantized wrongly");
      CHECK_MSG(bits_of(sp::angle_from_wire(b)) == hex(r[3]), "angle " + r[1] + " byte read");
      const uint16_t w = sp::angle16_to_wire(in);
      CHECK_MSG(w == hex(r[4]), "angle " + r[1] + " angle16 quantized wrongly");
      CHECK_MSG(bits_of(sp::angle16_from_wire(w)) == hex(r[5]), "angle " + r[1] + " angle16 read");
    }
  }
}

TEST(fields_take_their_widths_under_each_coordinate_form) {
  uint8_t buf[64];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  sp::write_coord(w, 1.5f);
  CHECK(w.size == 2);
  sp::write_coord(w, 1.5f, sp::Coords::all);
  CHECK(w.size == 6);
  sp::write_origin(w, 1.5f, sp::Coords::origins);
  CHECK(w.size == 10);
  sp::write_angle(w, 90, sp::Coords::all);
  CHECK(w.size == 12);
  sp::write_string(w, "hi");
  CHECK(w.size == 15);
  gn::Reader r = gn::make_reader(buf, w.size);
  CHECK(sp::read_coord(r) == 1.5f);
  CHECK(sp::read_coord(r, sp::Coords::all) == 1.5f);
  CHECK(sp::read_origin(r, sp::Coords::origins) == 1.5f);
  CHECK(sp::read_angle(r, sp::Coords::all) == 90.0f);
  CHECK(sp::read_string(r) == "hi");
  CHECK(!r.bad);
}

TEST(strings_drop_ff_bytes_on_read) {
  const uint8_t raw[] = {'a', 0xff, 'b', 0};
  gn::Reader r = gn::make_reader(raw, sizeof raw);
  CHECK(sp::read_string(r) == "ab");
}

TEST(info_strings_set_get_and_refuse) {
  std::string info;
  CHECK(sp::info_set(info, "name", "alice"));
  CHECK(sp::info_set(info, "team", "red"));
  CHECK(info == "\\name\\alice\\team\\red");
  CHECK(sp::info_get(info, "team") == "red");
  CHECK(sp::info_set(info, "name", "bob"));
  CHECK(sp::info_get(info, "name") == "bob");
  CHECK(!sp::info_set(info, "bad\\key", "x"));
  CHECK(!sp::info_set(info, "k", "a\"b"));
  sp::info_remove(info, "team");
  CHECK(sp::info_get(info, "team").empty());
  CHECK(sp::info_set(info, "*ver", "1"));
  CHECK(sp::info_set(info, "_secret", "1"));
  CHECK(sp::info_public(info).find("_secret") == std::string::npos);
  CHECK(sp::info_public(info).find("*ver") != std::string::npos);
  CHECK(sp::info_valid(info));
  CHECK(sp::sanitize_name("  ok  ") == "ok");
  CHECK(sp::sanitize_name("a;b") == "player");
  CHECK(sp::sanitize_name("Console") == "player");
  CHECK(sp::unique_name("ok", {"ok"}) == "(1)ok");
  CHECK(sp::unique_name("ok", {"ok", "(1)ok"}) == "(2)ok");
  CHECK(sp::unique_name("free", {"ok"}) == "free");
}
