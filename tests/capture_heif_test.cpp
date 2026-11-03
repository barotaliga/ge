// The HEIF still (engine/capture/heif.hpp): written and read back field for field as AVIF and as
// HEIC, the item's bytes where its location says, and a damaged file refused with the reason.
//
// ge:summary Tests: the HEIF still written and read back as AVIF and HEIC, damage refused
// ge:keywords test heif avif heic still box iloc ispe round-trip refuse
// ge:skill replay testing
// ge:test tests/capture_heif_test.cpp
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "capture/heif.hpp"
#include "check.hpp"

namespace gc = ge::capture;

TEST(a_still_reads_back_as_written) {
  for (const gc::HeifKind kind : {gc::HeifKind::kAvif, gc::HeifKind::kHeic}) {
    gc::HeifStill s;
    s.kind = kind;
    s.width = 1920;
    s.height = 1080;
    s.config = {0x81, 0x08, 0x0c, 0x00, 0x0a, 0x0b};
    for (int i = 0; i < 1000; ++i) s.data.push_back(static_cast<uint8_t>(i * 7));
    const std::vector<uint8_t> file = gc::write_heif(s);
    CHECK(std::memcmp(&file[4], "ftyp", 4) == 0);
    CHECK(std::memcmp(&file[8], kind == gc::HeifKind::kAvif ? "avif" : "heic", 4) == 0);
    gc::HeifStill back;
    std::string error;
    CHECK_MSG(gc::read_heif(file, back, error), error);
    CHECK(back.kind == kind);
    CHECK(back.width == 1920 && back.height == 1080);
    CHECK(back.config == s.config);
    CHECK(back.data == s.data);
    // The picture's bytes are the file's last, behind the mdat box's header.
    CHECK(std::memcmp(&file[file.size() - s.data.size() - 4], "mdat", 4) == 0);
  }
}

TEST(a_damaged_file_is_refused_with_the_reason) {
  gc::HeifStill s;
  s.width = 64;
  s.height = 32;
  s.config = {1, 2, 3, 4};
  s.data = {9, 9, 9};
  std::vector<uint8_t> file = gc::write_heif(s);
  gc::HeifStill back;
  std::string error;
  CHECK(!gc::read_heif(std::vector<uint8_t>{0, 0, 0, 8, 'j', 'u', 'n', 'k'}, back, error));
  CHECK(!error.empty());
  std::vector<uint8_t> cut(file.begin(), file.end() - 2);
  CHECK(!gc::read_heif(cut, back, error));
  CHECK_MSG(!error.empty(), "a file cut short said nothing");
}
