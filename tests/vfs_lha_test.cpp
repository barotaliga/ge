// The LHA archive (engine/vfs/lha.hpp): archives of every header level and method written by the
// fixture writer (one block of fixed but complete codes, matches and literals) read back behind a
// self-extractor's program and on their own; a directory's name joined to the file's; a stored
// entry; damage refused (a flipped byte under the CRC-16, no header anywhere, a method not read).
//
// ge:summary Tests the LHA reader: levels 0-2, -lh0- and -lh4- to -lh7-, behind a program, damage
// ge:keywords test vfs lha lzh lh5 lh6 lh7 sfx self-extracting level-2 crc16 refuse
// ge:skill vfs testing
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "vfs/lha.hpp"

namespace vfs = ge::vfs;

namespace {

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// Runs, repeats from near and far back, and bytes that do not repeat: every kind of symbol.
std::vector<uint8_t> mixed() {
  std::vector<uint8_t> big;
  uint32_t s = 1;
  for (int i = 0; i < 40000; i++) {
    s = s * 1103515245u + 12345u;
    const int kind = (i / 997) % 3;
    big.push_back(kind == 0   ? uint8_t(s >> 24)
                  : kind == 1 ? uint8_t(i % 7)
                              : uint8_t((i / 3) % 251));
  }
  return big;
}

const std::vector<uint8_t> kProgram = {'M', 'Z', 1, 2, 3, 4, 5, 6, 7, 8, '-', 'l', 'h', 'x'};

}  // namespace

TEST(lha_reads_every_level_and_method_behind_a_program) {
  const std::vector<uint8_t> big = mixed();
  const std::vector<uint8_t> words = text("the pack, the pack, the pack and its maps and its maps");
  for (const char method : {'0', '4', '5', '6', '7'}) {
    for (const int level : {0, 1, 2}) {
      const std::string what = std::string("-lh") + method + "- level " + std::to_string(level);
      const auto archive =
          vfs::write_lha({{"ID1\\BIG.DAT", big}, {"README.TXT", words}}, method, level, kProgram);
      std::vector<vfs::LhaEntry> entries;
      std::string error;
      CHECK_MSG(vfs::read_lha(archive, &entries, &error),
                std::string(what).append(": ").append(error));
      CHECK_MSG(entries.size() == 2, what);
      if (entries.size() != 2) continue;
      // Level 0 keeps the name as stored; levels 1 and 2 join the directory's with '/'.
      CHECK_MSG(entries[0].name == (level == 0 ? "ID1\\BIG.DAT" : "ID1/BIG.DAT"),
                what + ": " + entries[0].name);
      CHECK(entries[1].name == "README.TXT");
      std::vector<uint8_t> out;
      CHECK_MSG(vfs::lha_unpack(archive, entries[0], &out, &error),
                std::string(what).append(": ").append(error));
      CHECK_MSG(out == big, what);
      CHECK_MSG(vfs::lha_unpack(archive, entries[1], &out, &error),
                std::string(what).append(": ").append(error));
      CHECK_MSG(out == words, what);
      if (method != '0') CHECK_MSG(entries[0].packed < big.size(), what);
      // Behind a program it is found by the scan, but only a bare archive opens with a header.
      CHECK(!vfs::is_lha(archive));
      CHECK(vfs::is_lha(vfs::write_lha({{"A", words}}, method, level)));
    }
  }
}

TEST(lha_refuses_damage) {
  std::vector<uint8_t> words;
  words.reserve(300);
  for (int i = 0; i < 300; i++) words.push_back(uint8_t('a' + (i * 7) % 26));
  auto archive = vfs::write_lha({{"A.TXT", words}}, '5', 0, kProgram);
  std::vector<vfs::LhaEntry> entries;
  std::string error;
  CHECK(vfs::read_lha(archive, &entries, &error));
  // A byte in the middle of the coded symbols changed: the CRC-16 (or the code) catches it.
  archive[size_t(entries[0].data) + entries[0].packed - 20] ^= 0x5a;
  std::vector<uint8_t> out;
  CHECK(!vfs::lha_unpack(archive, entries[0], &out, &error));
  CHECK(
      !vfs::read_lha(text("no archive here at all, only words and more words"), &entries, &error));
  CHECK(error.find("no archive header") != std::string::npos);
  // A method this reader does not take is named.
  auto old = vfs::write_lha({{"B.TXT", words}}, '5', 1);
  CHECK(vfs::read_lha(old, &entries, &error));
  vfs::LhaEntry one = entries[0];
  one.method = '1';
  CHECK(!vfs::lha_unpack(old, one, &out, &error));
  CHECK_MSG(error.find("-lh1-") != std::string::npos, error);
}
