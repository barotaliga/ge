// A DAMAGED ARCHIVE is refused or read whole, never read out of range: a pack, a zip and a wad of
// our own mutated thousands of ways from a seeded stream (bytes flipped, the file cut short, words
// written over headers and directories). Each mutant mounted on a search path is either refused
// with a reason naming the archive, or read into a directory whose every file lies inside it and
// reads (a zip entry unpacks or is refused on its checksum); a wad's every lump lies inside it and
// every picture's pixels are there. Every packed and archived form `unpack_one` peels (gzip, zstd,
// xz, tar.gz, 7z) mutated the same ways is refused with a reason or unpacked to a file. The
// sanitizer rows make an out-of-range read a failure here.
//
// ge:summary Test: thousands of mutated packs, zips and wads refused naming them or read in range
// ge:keywords test vfs fuzz mutate damage refuse bounds pak zip wad picture search-path sanitizer
// ge:keywords unpack gzip zstd xz tar 7z
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "vfs/lzma.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/wad.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace {

namespace core = ge::core;
namespace vfs = ge::vfs;

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

void mutate(core::Rng& rng, std::vector<uint8_t>& b) {
  if (b.size() < 8) return;
  switch (core::int_in(rng, 0, 3)) {
    case 0:  // a few bytes flipped anywhere
      for (int k = core::int_in(rng, 1, 6); k > 0; --k) {
        b[core::pick_index(rng, b.size())] ^= uint8_t(core::int_in(rng, 1, 255));
      }
      break;
    case 1:  // cut short
      b.resize(core::pick_index(rng, b.size()));
      break;
    case 2: {  // a word anywhere (a count, an offset, a size)
      const size_t at = core::pick_index(rng, b.size() - 3);
      const auto v = uint32_t(core::int_in(rng, -70000, 70000));
      for (int k = 0; k < 4; ++k) b[at + size_t(k)] = uint8_t(v >> (8 * k));
      break;
    }
    default:  // a byte at its extremes
      b[core::pick_index(rng, b.size())] = core::chance(rng, 0.5) ? 0xff : 0x00;
      break;
  }
}

std::vector<std::pair<std::string, std::vector<uint8_t>>> files() {
  return {{"a.txt", text("the first file")},
          {"maps/b.bsp", std::vector<uint8_t>(300, 7)},
          {"gfx/c.lmp", text("a picture's bytes")}};
}

}  // namespace

TEST(mutated_packs_and_zips_are_refused_naming_them_or_read_in_range) {
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(vfs::write_pak(files(), &pak, &error));
  std::vector<uint8_t> zip = vfs::write_stored_zip(files());
  core::Rng rng = core::make_rng(20261030u);
  for (const std::vector<uint8_t>* clean : std::vector<const std::vector<uint8_t>*>{&pak, &zip}) {
    int read = 0, refused = 0;
    for (int trial = 0; trial < 1500; ++trial) {
      std::vector<uint8_t> b = *clean;
      for (int k = core::int_in(rng, 1, 3); k > 0 && !b.empty(); --k) mutate(rng, b);
      vfs::SearchPath sp;
      if (!vfs::add_archive(&sp, "fuzzed.archive", b, "base", &error)) {
        ++refused;
        CHECK(error.find("fuzzed.archive") != std::string::npos);
        continue;
      }
      ++read;
      const vfs::Source& s = sp.sources[0];
      for (const vfs::Listed& l : vfs::list(sp)) {
        const std::span<const uint8_t> v = vfs::view(sp, l.ref);
        if (!v.empty()) {
          CHECK(v.data() >= s.bytes.data() &&
                v.data() + v.size() <= s.bytes.data() + s.bytes.size());
        }
        std::vector<uint8_t> out;
        if (vfs::read(sp, l.ref, &out, &error)) {
          CHECK(out.size() == l.ref.size);
        } else {
          CHECK(!error.empty());
        }
      }
    }
    CHECK(read > 50 && refused > 50);
  }
}

TEST(mutated_wads_are_refused_or_read_in_range) {
  vfs::IndexedPicture pic;
  pic.width = 8;
  pic.height = 4;
  pic.indices.assign(32, 9);
  std::vector<uint8_t> wad;
  std::string error;
  CHECK(vfs::write_wad({{"face", vfs::kPictureLump, vfs::write_picture(pic)},
                        {"conchars", vfs::kRawLump, std::vector<uint8_t>(64, 3)}},
                       &wad, &error));
  core::Rng rng = core::make_rng(20261031u);
  int read = 0, refused = 0;
  for (int trial = 0; trial < 1500; ++trial) {
    std::vector<uint8_t> b = wad;
    for (int k = core::int_in(rng, 1, 3); k > 0 && !b.empty(); --k) mutate(rng, b);
    vfs::Wad w;
    if (!vfs::read_wad(b, &w, &error)) {
      ++refused;
      CHECK(!error.empty());
      continue;
    }
    ++read;
    for (const vfs::WadEntry& e : w.entries) {
      const std::span<const uint8_t> lump = vfs::wad_lump(b, e);
      CHECK(lump.size() <= b.size());
      vfs::IndexedPicture p;
      if (e.type == vfs::kPictureLump && vfs::read_picture(lump, &p, &error)) {
        CHECK(p.indices.size() == size_t(p.width) * size_t(p.height));
      }
    }
  }
  CHECK(read > 50 && refused > 50);
}

TEST(mutated_packed_and_archived_files_are_refused_or_unpacked) {
  std::vector<uint8_t> plain;
  plain.reserve(6000);
  for (int i = 0; i < 6000; i++) plain.push_back(static_cast<uint8_t>((i * 7) ^ (i >> 5)));
  const std::vector<std::vector<uint8_t>> clean = {
      vfs::write_gzip(plain, "a.bin"), vfs::zstd_pack(plain, {3, 0, false}), vfs::xz_pack(plain, 1),
      vfs::write_gzip(vfs::write_tar({{"a.bin", plain}})), vfs::write_7z("a.bin", plain, 1)};
  core::Rng rng = core::make_rng(20261101u);
  for (const std::vector<uint8_t>& c : clean) {
    int read = 0, refused = 0;
    for (int trial = 0; trial < 300; ++trial) {
      std::vector<uint8_t> b = c;
      for (int k = core::int_in(rng, 1, 3); k > 0 && !b.empty(); --k) mutate(rng, b);
      vfs::Unpacked u;
      std::string error;
      if (vfs::unpack_one(std::move(b), u, error)) {
        ++read;
        CHECK(u.bytes.size() <= vfs::kUnpackLimit);
      } else {
        ++refused;
        CHECK(!error.empty());
      }
    }
    CHECK(refused > 50);
    CHECK(read + refused == 300);
  }
}
