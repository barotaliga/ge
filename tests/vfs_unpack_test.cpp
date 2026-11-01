// The one file inside (engine/vfs/unpack.hpp, tar.hpp, lzma.hpp): a file packed as gzip, zstd
// and xz, archived as tar, zip (stored, zstd and LZMA entries) and 7z, and stacked (tar.gz,
// tar.xz, tar.zst, a zip in a gzip) comes back the same with its layers and name; plain bytes pass
// through; an archive of several files is refused naming them unless `pick` leaves one; and
// damage, a nest past four layers and a cut-off zstd frame are each answered as documented.
//
// ge:summary Tests unpack_one over every layer and stack, the pick, and the refusals
// ge:keywords test vfs unpack gzip zstd xz tar tgz zip 7z seven-zip archive pick layers refuse
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "vfs/lzma.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace vfs = ge::vfs;
using W = vfs::Wrapping;

namespace {

std::vector<uint8_t> demo() {
  std::vector<uint8_t> out;
  for (int i = 0; i < 20000; i++) {
    out.push_back(static_cast<uint8_t>(i % 7));
    out.push_back(static_cast<uint8_t>(i >> 4));
    out.push_back(static_cast<uint8_t>((i * 37) >> 3));
  }
  return out;
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// `plain` as a zip entry of `method` holding `packed` (both headers patched from a stored zip).
std::vector<uint8_t> zip_entry(const std::string& name, const std::vector<uint8_t>& plain,
                               const std::vector<uint8_t>& packed, uint16_t method) {
  auto zip = vfs::write_stored_zip({{name, packed}});
  const uint32_t crc = vfs::crc32_of(plain);
  const auto size = static_cast<uint32_t>(plain.size());
  const auto put32 = [&](size_t at, uint32_t v) {
    for (int i = 0; i < 4; i++) zip[at + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  for (size_t h = 0; h + 4 <= zip.size(); h++) {
    if (zip[h] != 'P' || zip[h + 1] != 'K') continue;
    if (zip[h + 2] == 3 && zip[h + 3] == 4) {  // local: method 8, crc 14, size 22
      zip[h + 8] = static_cast<uint8_t>(method);
      put32(h + 14, crc);
      put32(h + 22, size);
    } else if (zip[h + 2] == 1 && zip[h + 3] == 2) {  // central: method 10, crc 16, size 24
      zip[h + 10] = static_cast<uint8_t>(method);
      put32(h + 16, crc);
      put32(h + 24, size);
    }
  }
  return zip;
}

void expect(const std::vector<uint8_t>& file, const std::vector<W>& layers,
            const std::string& name) {
  vfs::Unpacked u;
  std::string error;
  CHECK_MSG(vfs::unpack_one(file, u, error), error);
  CHECK(u.bytes == demo());
  CHECK(u.layers == layers);
  CHECK_MSG(u.name == name, u.name);
  CHECK(!u.truncated);
}

}  // namespace

TEST(unpack_passes_plain_bytes_through) {
  vfs::Unpacked u;
  std::string error;
  CHECK(vfs::unpack_one(demo(), u, error));
  CHECK(u.bytes == demo() && u.layers.empty() && u.name.empty());
  CHECK(vfs::unpack_one({}, u, error) && u.bytes.empty());
  CHECK(vfs::wrapping_of(demo()) == W::none);
}

TEST(unpack_peels_each_packing) {
  expect(vfs::write_gzip(demo(), "match.mvd"), {W::gzip}, "match.mvd");
  expect(vfs::write_gzip(demo()), {W::gzip}, "");
  expect(vfs::zstd_pack(demo()), {W::zstd}, "");
  expect(vfs::xz_pack(demo(), 6), {W::xz}, "");
}

TEST(unpack_opens_each_archive_of_one_file) {
  expect(vfs::write_tar({{"demos/match.mvd", demo()}}), {W::tar}, "demos/match.mvd");
  expect(vfs::write_stored_zip({{"match.rec", demo()}}), {W::zip}, "match.rec");
  expect(zip_entry("z.rec", demo(), vfs::zstd_pack(demo()), 93), {W::zip}, "z.rec");
  expect(vfs::write_7z("match.mvd", demo()), {W::seven}, "match.mvd");
  // LZMA in a zip: the version, the properties' size, the properties, the raw stream. The 7z
  // writer's stream is the same raw LZMA, so its coder properties and data make the entry.
  const auto seven = vfs::write_7z("x", demo(), 5);
  const size_t packed = static_cast<size_t>(seven[12]) | (static_cast<size_t>(seven[13]) << 8) |
                        (static_cast<size_t>(seven[14]) << 16);
  std::vector<uint8_t> lz = {9, 20, 5, 0};
  // The props follow the coder id 03 01 01 and its size byte 05 in the header.
  for (size_t at = 32 + packed; at + 9 < seven.size(); at++)
    if (seven[at] == 3 && seven[at + 1] == 1 && seven[at + 2] == 1 && seven[at + 3] == 5) {
      lz.insert(lz.end(), seven.begin() + static_cast<long>(at + 4),
                seven.begin() + static_cast<long>(at + 9));
      break;
    }
  lz.insert(lz.end(), seven.begin() + 32, seven.begin() + static_cast<long>(32 + packed));
  expect(zip_entry("l.mvd", demo(), lz, 14), {W::zip}, "l.mvd");
}

TEST(unpack_peels_stacked_layers) {
  const auto tar = vfs::write_tar({{"match.mvdv", demo()}});
  expect(vfs::write_gzip(tar), {W::gzip, W::tar}, "match.mvdv");
  expect(vfs::xz_pack(tar, 3), {W::xz, W::tar}, "match.mvdv");
  expect(vfs::zstd_pack(tar), {W::zstd, W::tar}, "match.mvdv");
  const auto zip = vfs::write_stored_zip({{"a.qwdv", vfs::zstd_pack(demo())}});
  expect(vfs::write_gzip(zip), {W::gzip, W::zip, W::zstd}, "a.qwdv");
}

TEST(unpack_refuses_several_files_unless_pick_leaves_one) {
  const auto tar = vfs::write_tar({{"readme.txt", text("notes")}, {"match.mvd", demo()}});
  vfs::Unpacked u;
  std::string error;
  CHECK(!vfs::unpack_one(tar, u, error));
  CHECK_MSG(error.find("readme.txt, match.mvd") != std::string::npos, error);
  const auto is_demo = [](std::string_view n) { return n.ends_with(".mvd"); };
  CHECK_MSG(vfs::unpack_one(tar, u, error, is_demo), error);
  CHECK(u.bytes == demo() && u.name == "match.mvd");
  const auto none = vfs::write_7z("readme.txt", text("notes"));
  CHECK(!vfs::unpack_one(none, u, error, is_demo));
  CHECK_MSG(error.find("none of a kind") != std::string::npos, error);
  const auto two = vfs::write_stored_zip({{"a.mvd", demo()}, {"dir/", {}}, {"b.mvd", demo()}});
  CHECK(!vfs::unpack_one(two, u, error, is_demo));
  CHECK_MSG(error.find("2 files") != std::string::npos, error);
}

TEST(unpack_refuses_damage_and_deep_nests) {
  vfs::Unpacked u;
  std::string error;
  auto seven = vfs::write_7z("m.mvd", demo());
  seven[40] ^= 0xFF;
  CHECK(!vfs::unpack_one(seven, u, error));
  CHECK_MSG(error.find("7z") != std::string::npos, error);
  auto xz = vfs::xz_pack(demo());
  xz[xz.size() / 2] ^= 0xFF;
  CHECK(!vfs::unpack_one(xz, u, error));
  CHECK_MSG(error.find("xz") != std::string::npos, error);
  auto gz = vfs::write_gzip(demo());
  gz[gz.size() - 6] ^= 1;  // the CRC
  CHECK(!vfs::unpack_one(gz, u, error));
  CHECK_MSG(error.find("gzip") != std::string::npos, error);
  auto tar = vfs::write_tar({{"m.mvd", demo()}});
  tar.resize(2000);  // cut inside the file
  CHECK(!vfs::unpack_one(tar, u, error));
  CHECK_MSG(error.find("past the archive") != std::string::npos, error);
  std::vector<uint8_t> nest = demo();
  for (int i = 0; i < 5; i++) nest = vfs::write_gzip(nest);
  CHECK(!vfs::unpack_one(nest, u, error));
  CHECK_MSG(error.find("layers deep") != std::string::npos, error);
}

TEST(unpack_keeps_what_a_cut_zstd_frame_holds) {
  vfs::ZstdStream s;
  vfs::zstd_begin(s, {3, 0, false});
  vfs::zstd_write(s, demo());
  vfs::zstd_flush(s);  // never ended: a recorder that crashed after its last flush
  vfs::Unpacked u;
  std::string error;
  CHECK_MSG(vfs::unpack_one(s.out, u, error), error);
  CHECK(u.truncated);
  CHECK(u.bytes == demo());
}

TEST(read_7z_lists_names_and_sizes) {
  std::vector<vfs::SevenEntry> list;
  std::string error;
  CHECK_MSG(vfs::read_7z(vfs::write_7z("demos/match.mvd", demo()), list, error), error);
  CHECK(list.size() == 1 && list[0].name == "demos/match.mvd" && list[0].size == demo().size());
  CHECK(!vfs::read_7z(demo(), list, error));
}
