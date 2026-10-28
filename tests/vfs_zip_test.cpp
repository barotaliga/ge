// The zip archive (engine/vfs/zip.hpp): stored entries written by the fixture writer read back
// by name in any case and with either separator, a deflated entry (its blocks written by a stock
// zlib) unpacked and checked, and damage refused naming what is wrong: no end record, a central
// record cut short, a method this reader does not take, a flipped byte under the CRC-32.
//
// ge:summary Tests the zip reader: stored and deflated entries, names, and the damage it refuses
// ge:keywords test vfs zip archive stored deflate crc32 refuse damaged
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "vfs/zip.hpp"

namespace vfs = ge::vfs;

namespace {

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

std::vector<uint8_t> sample() {
  return vfs::write_stored_zip({{"ID1/README.TXT", bytes("the shareware notes")},
                                {"maps/start.bsp", {1, 2, 3, 4, 5}},
                                {"empty", {}}});
}

}  // namespace

TEST(zip_reads_stored_entries_by_name) {
  const auto zip = sample();
  vfs::Zip dir;
  std::string error;
  CHECK_MSG(vfs::read_zip(zip, &dir, &error), error);
  CHECK(dir.entries.size() == 3);
  const vfs::ZipEntry* readme = vfs::zip_find(dir, "id1\\readme.txt");
  CHECK(readme != nullptr);
  std::vector<uint8_t> out;
  CHECK(readme && vfs::zip_unpack(zip, *readme, &out, &error));
  CHECK(std::string(out.begin(), out.end()) == "the shareware notes");
  const vfs::ZipEntry* empty = vfs::zip_find(dir, "EMPTY");
  CHECK(empty && vfs::zip_unpack(zip, *empty, &out, &error) && out.empty());
  CHECK(vfs::zip_find(dir, "missing") == nullptr);
}

TEST(zip_unpacks_a_deflated_entry) {
  // "the snow, the snow, the snow on the mountain" deflated by a stock zlib (fixed Huffman), with
  // no zlib wrapper, as a zip stores it.
  const std::vector<uint8_t> raw{43,  201, 72,  85,  40,  206, 203, 47,  215, 81,  40,  193, 96, 41,
                                 228, 231, 129, 217, 185, 249, 165, 121, 37,  137, 153, 121, 0};
  const std::string want = "the snow, the snow, the snow on the mountain";
  std::vector<uint8_t> zip = vfs::write_stored_zip({{"s.txt", raw}});
  // Turn the stored entry into a deflated one: method 8, the unpacked size and CRC in both
  // headers (the local header at 0, the central one after the data).
  vfs::Zip dir;
  std::string error;
  CHECK(vfs::read_zip(zip, &dir, &error));
  const size_t central = dir.entries[0].data + raw.size();
  for (const size_t h : {size_t(0), central}) {
    const size_t method = h == 0 ? 8 : 10, crc = h == 0 ? 14 : 16, size = h == 0 ? 22 : 24;
    zip[h + method] = 8;
    const uint32_t c = ge::vfs::crc32_of(bytes(want));
    for (int i = 0; i < 4; i++) {
      zip[h + crc + size_t(i)] = uint8_t(c >> (8 * i));
      zip[h + size + size_t(i)] = uint8_t(want.size() >> (8 * i));
    }
  }
  CHECK_MSG(vfs::read_zip(zip, &dir, &error), error);
  std::vector<uint8_t> out;
  CHECK_MSG(vfs::zip_unpack(zip, dir.entries[0], &out, &error), error);
  CHECK(std::string(out.begin(), out.end()) == want);
}

TEST(zip_refuses_damage) {
  const auto good = sample();
  vfs::Zip dir;
  std::string error;
  auto refused = [&](const std::vector<uint8_t>& z, const std::string& word) {
    const bool ok = vfs::read_zip(z, &dir, &error);
    return !ok && error.find(word) != std::string::npos;
  };
  CHECK(refused(std::vector<uint8_t>(good.begin(), good.begin() + 40), "end"));
  std::vector<uint8_t> cut = good;
  cut.erase(cut.end() - 30, cut.end() - 22);  // a central record shortened
  CHECK(refused(cut, "central"));
  std::vector<uint8_t> method = good;
  CHECK(vfs::read_zip(good, &dir, &error));
  // The first central record's method: 12 (bzip2) is refused at the directory, naming the entry.
  for (size_t at = 0; at + 4 <= method.size(); at++) {
    if (method[at] == 0x50 && method[at + 1] == 0x4b && method[at + 2] == 1 &&
        method[at + 3] == 2) {
      method[at + 10] = 12;
      break;
    }
  }
  CHECK(refused(method, "README.TXT"));
  std::vector<uint8_t> flipped = good;
  CHECK(vfs::read_zip(flipped, &dir, &error));
  flipped[dir.entries[0].data] ^= 0x20;
  std::vector<uint8_t> out;
  CHECK(!vfs::zip_unpack(flipped, dir.entries[0], &out, &error) &&
        error.find("CRC-32") != std::string::npos);
}
