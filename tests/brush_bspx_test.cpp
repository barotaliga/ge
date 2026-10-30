// BSPX lumps (plugins/brush/read_v29.cpp): a directory appended after the fifteen lumps of a 29 or
// BSP2 fixture reads back by name in any case; RGBLIGHTING becomes the map's colour lighting; a
// file with no BSPX reads as before; a directory or a lump past the file's end, and colour
// lighting of the wrong size, are refused naming it.
//
// ge:summary Test: BSPX lumps read by name, RGBLIGHTING as colour, damaged directories refused
// ge:keywords test bspx rgblighting extra-lumps refuse fixture
// ge:skill brush testing
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

std::vector<uint8_t> written(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), f, &bytes, &error));
  return bytes;
}

void put32(std::vector<uint8_t>* b, uint32_t v) {
  for (int k = 0; k < 4; ++k) b->push_back(uint8_t(v >> (8 * k)));
}

struct Extra {
  std::string name;
  std::vector<uint8_t> bytes;
};

// `bytes` with a BSPX directory and `lumps` appended at the next four-byte boundary.
std::vector<uint8_t> with_bspx(std::vector<uint8_t> bytes, const std::vector<Extra>& lumps) {
  while (bytes.size() % 4) bytes.push_back(0);
  const size_t dir = bytes.size();
  for (char c : std::string("BSPX")) bytes.push_back(uint8_t(c));
  put32(&bytes, uint32_t(lumps.size()));
  size_t at = dir + 8 + 32 * lumps.size();
  for (const Extra& l : lumps) {
    for (size_t k = 0; k < 24; ++k) bytes.push_back(k < l.name.size() ? uint8_t(l.name[k]) : 0);
    put32(&bytes, uint32_t(at));
    put32(&bytes, uint32_t(l.bytes.size()));
    at += (l.bytes.size() + 3) & ~size_t(3);
  }
  for (const Extra& l : lumps) {
    bytes.insert(bytes.end(), l.bytes.begin(), l.bytes.end());
    while (bytes.size() % 4) bytes.push_back(0);
  }
  return bytes;
}

bool reads(const std::vector<uint8_t>& bytes, br::Map* m, std::string* error) {
  const bool ok = br::read_bsp(bytes, m, error);
  if (!ok) std::printf("  %s\n", error->c_str());
  return ok;
}

}  // namespace

TEST(bspx_lumps_read_back_by_name_and_colour_the_lighting) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    br::Map plain;
    std::string error;
    CHECK(reads(written(f), &plain, &error));
    CHECK(plain.bspx.empty());
    std::vector<uint8_t> rgb(plain.light_rgb.size());
    for (size_t i = 0; i < rgb.size(); ++i) rgb[i] = uint8_t(i % 3 == 0 ? 200 : 40);
    const std::vector<uint8_t> bytes =
        with_bspx(written(f), {{"LMSHIFT", {4, 4, 4}}, {"RGBLIGHTING", rgb}});
    br::Map m;
    CHECK(reads(bytes, &m, &error));
    CHECK(m.bspx.size() == 2);
    const br::BspxLump* shift = br::bspx_lump(m, "lmshift");
    CHECK(shift != nullptr && shift->bytes == (std::vector<uint8_t>{4, 4, 4}));
    CHECK(br::bspx_lump(m, "none") == nullptr);
    CHECK(m.light_rgb == rgb);
    CHECK(m.surfaces.size() == plain.surfaces.size());
  }
}

TEST(a_damaged_bspx_directory_is_refused_naming_it) {
  br::Map m;
  std::string error;
  std::vector<uint8_t> bytes = with_bspx(written(br::Family::v29), {{"LMSHIFT", {1, 2, 3}}});
  std::vector<uint8_t> past = bytes;
  const size_t entry = past.size() - 4 - 32;  // the one entry, before its 4 padded bytes
  past[entry + 28] = 0xff;                    // its length's low byte
  past[entry + 29] = 0xff;
  CHECK(!br::read_bsp(past, &m, &error));
  CHECK(error.find("LMSHIFT") != std::string::npos);
  std::vector<uint8_t> many = bytes;
  many[entry - 4] = 0xff;  // the count
  CHECK(!br::read_bsp(many, &m, &error));
  CHECK(error.find("BSPX") != std::string::npos);
  const std::vector<uint8_t> short_rgb =
      with_bspx(written(br::Family::v29), {{"RGBLIGHTING", {1, 2, 3}}});
  CHECK(!br::read_bsp(short_rgb, &m, &error));
  CHECK(error.find("RGBLIGHTING") != std::string::npos);
}
