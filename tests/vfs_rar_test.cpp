// The RAR archive (engine/vfs/rar.hpp): archives written by the stored RAR 5 fixture writer read
// back through the unRAR library (names, sizes, a directory, the bytes), the one file peeled out
// by `unpack_one` beside a readme, and damage refused (a flipped byte under the checksum, a cut
// archive, a file past the limit).
//
// ge:summary Tests the RAR reader: entries, bytes, unpack_one's pick, damage and the limit refused
// ge:keywords test vfs rar rar5 unrar refuse crc
// ge:skill vfs testing
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "vfs/rar.hpp"
#include "vfs/unpack.hpp"

namespace vfs = ge::vfs;

namespace {

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

std::vector<uint8_t> big() {
  std::vector<uint8_t> b(100000);
  for (size_t i = 0; i < b.size(); i++) b[i] = uint8_t(i * 7 + i / 300);
  return b;
}

}  // namespace

TEST(rar_lists_and_unpacks_every_entry) {
  const auto data = big();
  const auto archive =
      vfs::write_rar({{"demos/", {}}, {"demos/match.dem", data}, {"readme.txt", text("hi")}});
  CHECK(vfs::is_rar(archive));
  CHECK(!vfs::is_rar(text("Rar! is not enough")));
  std::vector<vfs::RarEntry> entries;
  std::string error;
  CHECK_MSG(vfs::read_rar(archive, entries, error), error);
  CHECK(entries.size() == 3);
  CHECK(entries[0].dir && entries[0].name == "demos");
  CHECK(entries[1].name == "demos/match.dem" && entries[1].size == data.size());
  std::vector<uint8_t> out;
  CHECK_MSG(vfs::rar_unpack(archive, 1, out, error), error);
  CHECK(out == data);
  CHECK_MSG(vfs::rar_unpack(archive, 2, out, error), error);
  CHECK(out == text("hi"));
}

TEST(rar_unpack_one_takes_the_recording_beside_a_readme) {
  const auto data = big();
  const auto archive = vfs::write_rar({{"readme.txt", text("read me")}, {"match.dem", data}});
  vfs::Unpacked u;
  std::string error;
  const auto pick = [](std::string_view n) { return n.ends_with(".dem"); };
  CHECK_MSG(vfs::unpack_one(archive, u, error, pick), error);
  CHECK(u.bytes == data && u.name == "match.dem");
  CHECK(u.layers.size() == 1 && u.layers[0] == vfs::Wrapping::rar);
  // Without the pick two files are there, and both are named.
  CHECK(!vfs::unpack_one(archive, u, error));
  CHECK_MSG(error.find("readme.txt") != std::string::npos, error);
}

TEST(rar_refuses_damage_and_a_file_past_the_limit) {
  const auto data = big();
  auto archive = vfs::write_rar({{"match.dem", data}});
  std::vector<uint8_t> out;
  std::string error;
  CHECK(!vfs::rar_unpack(archive, 0, out, error, 1000));
  CHECK_MSG(error.find("more than 1000") != std::string::npos, error);
  // A byte of the stored file changed: its CRC-32 no longer holds.
  auto bad = archive;
  bad[bad.size() - 5000] ^= 0x5a;
  CHECK(!vfs::rar_unpack(bad, 0, out, error));
  // Cut in half: the list or the file does not come back whole.
  std::vector<uint8_t> cut(archive.begin(), archive.begin() + archive.size() / 2);
  std::vector<vfs::RarEntry> entries;
  CHECK(!vfs::read_rar(cut, entries, error) || !vfs::rar_unpack(cut, 0, out, error));
}
