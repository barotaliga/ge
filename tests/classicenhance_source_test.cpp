// Where the classic content comes from (plugins/classicenhance/source.hpp, lha.hpp): an LHA
// archive written here (level-0 headers, `-lh5-` blocks with fixed but complete codes, matches
// and literals) found behind a self-extractor's program and unpacked; the pack found through a
// release zip's installer archive, in a zip directly, as a file and in an installed folder in
// any case; and every unknown or damaged pack refused, with its digest, writing nothing.
//
// ge:summary Tests the LHA reader and the source search: every route, and refusing unknown packs
// ge:keywords test classicenhance lha lh5 source zip folder pack verify refuse sha256
// ge:skill classicenhance testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/sha256.hpp"
#include "check.hpp"
#include "classicenhance/lha.hpp"
#include "classicenhance/source.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ce = ge::classicenhance;
namespace fs = std::filesystem;

namespace {

struct BitOut {
  std::vector<uint8_t> bytes;
  uint32_t acc = 0;
  int have = 0;
  void put(uint32_t v, int n) {
    for (int i = n - 1; i >= 0; i--) {
      acc = (acc << 1) | ((v >> i) & 1);
      if (++have == 8) {
        bytes.push_back(uint8_t(acc));
        acc = 0;
        have = 0;
      }
    }
  }
  void flush() {
    if (have > 0) put(0, 8 - have);
  }
};

// `-lh5-` in one block: literals and matches (greedy, the window's 8 KB), with complete codes
// chosen once: symbols 0 and 1 of the literal/length code 8 bits and the rest 9; distance bit
// counts 0 and 1 3 bits and the rest 4; the lengths' own code two 1-bit codes (for 10 and 11).
std::vector<uint8_t> lh5(const std::vector<uint8_t>& in) {
  struct Sym {
    int c, p;  // p < 0: a literal
  };
  std::vector<Sym> syms;
  for (size_t i = 0; i < in.size();) {
    size_t best = 0, dist = 0;
    for (size_t back = 1; back <= std::min<size_t>(i, 8192); back++) {
      size_t n = 0;
      while (n < 256 && i + n < in.size() && in[i + n] == in[i - back + n]) n++;
      if (n > best) {
        best = n;
        dist = back;
      }
    }
    if (best >= 3) {
      syms.push_back({int(best) + 253, int(dist) - 1});
      i += best;
    } else {
      syms.push_back({in[i], -1});
      i++;
    }
  }
  BitOut b;
  b.put(uint32_t(syms.size()), 16);
  // The lengths' code: 12 lengths sent, 0 for 0..9 (a run of three zeros after the third), 1
  // for 10 and 11.
  b.put(12, 5);
  for (int i = 0; i < 3; i++) b.put(0, 3);
  b.put(3, 2);
  for (int i = 6; i < 10; i++) b.put(0, 3);
  b.put(1, 3);
  b.put(1, 3);
  // The literal/length code's 510 lengths through it: 8 is sent as 10 (code 0), 9 as 11 (code 1).
  b.put(510, 9);
  for (int i = 0; i < 510; i++) b.put(i < 2 ? 0 : 1, 1);
  // The distance code's 14 lengths: 3 as "011", 4 as "100".
  b.put(14, 4);
  for (int i = 0; i < 14; i++) b.put(i < 2 ? 3 : 4, 3);
  for (const Sym& s : syms) {
    if (s.c < 2) {
      b.put(uint32_t(s.c), 8);
    } else {
      b.put(uint32_t(4 + s.c - 2), 9);
    }
    if (s.p < 0) continue;
    int bits = 0;
    while ((s.p >> bits) != 0) bits++;
    if (bits < 2) {
      b.put(uint32_t(bits), 3);
    } else {
      b.put(uint32_t(4 + bits - 2), 4);
    }
    if (bits > 1) b.put(uint32_t(s.p - (1 << (bits - 1))), bits - 1);
  }
  b.flush();
  return b.bytes;
}

void put32(std::vector<uint8_t>& o, uint32_t v) {
  for (int i = 0; i < 4; i++) o.push_back(uint8_t(v >> (8 * i)));
}

// A self-extracting LHA archive: a program's bytes, then level-0 headers, then the end marker.
std::vector<uint8_t> sfx(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
                         bool compress = true) {
  std::vector<uint8_t> o = {'M', 'Z', 1, 2, 3, 4, 5, 6, 7, 8, '-', 'l', 'h', 'x'};
  for (const auto& [name, data] : files) {
    const std::vector<uint8_t> packed = compress ? lh5(data) : data;
    std::vector<uint8_t> h;
    for (const char c : std::string(compress ? "-lh5-" : "-lh0-")) h.push_back(uint8_t(c));
    put32(h, uint32_t(packed.size()));
    put32(h, uint32_t(data.size()));
    put32(h, 0);
    h.push_back(0x20);
    h.push_back(0);
    h.push_back(uint8_t(name.size()));
    h.insert(h.end(), name.begin(), name.end());
    const uint16_t crc = ce::crc16(data);
    h.push_back(uint8_t(crc));
    h.push_back(uint8_t(crc >> 8));
    uint8_t sum = 0;
    for (const uint8_t v : h) sum = uint8_t(sum + v);
    o.push_back(uint8_t(h.size()));
    o.push_back(sum);
    o.insert(o.end(), h.begin(), h.end());
    o.insert(o.end(), packed.begin(), packed.end());
  }
  o.push_back(0);
  return o;
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// A small pack standing in for the real one (whose bytes are never in the tree).
std::vector<uint8_t> fixture_pack() {
  std::vector<uint8_t> pal(768);
  for (size_t i = 0; i < pal.size(); i++) pal[i] = uint8_t(i * 7);
  std::vector<uint8_t> out;
  std::string error;
  ge::vfs::write_pak({{"gfx/palette.lmp", pal}, {"readme.txt", text("a fixture pack")}}, &out,
                     &error);
  return out;
}

fs::path scratch(const std::string& name) {
  const fs::path p = fs::temp_directory_path() / ("ge_classicenhance_" + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

}  // namespace

TEST(lha_reads_lh5_and_lh0_behind_a_program) {
  std::vector<uint8_t> big;
  // Runs, repeats from near and far back, and bytes that do not repeat: every kind of symbol.
  uint32_t s = 1;
  for (int i = 0; i < 40000; i++) {
    s = s * 1103515245u + 12345u;
    const int kind = (i / 997) % 3;
    big.push_back(kind == 0   ? uint8_t(s >> 24)
                  : kind == 1 ? uint8_t(i % 7)
                              : uint8_t((i / 3) % 251));
  }
  const std::vector<uint8_t> words = text("the pack, the pack, the pack and its maps and its maps");
  for (const bool compress : {true, false}) {
    const auto archive = sfx({{"ID1\\BIG.DAT", big}, {"README.TXT", words}}, compress);
    std::vector<ce::LhaEntry> entries;
    std::string error;
    CHECK_MSG(ce::read_lha(archive, &entries, &error), error);
    CHECK(entries.size() == 2);
    std::vector<uint8_t> out;
    CHECK_MSG(ce::lha_unpack(archive, entries[0], &out, &error), error);
    CHECK(out == big);
    CHECK_MSG(ce::lha_unpack(archive, entries[1], &out, &error), error);
    CHECK(out == words);
    if (compress) CHECK(entries[0].packed < big.size());
  }
}

TEST(lha_refuses_damage) {
  std::vector<uint8_t> words;
  for (int i = 0; i < 300; i++) words.push_back(uint8_t('a' + (i * 7) % 26));
  auto archive = sfx({{"A.TXT", words}});
  std::vector<ce::LhaEntry> entries;
  std::string error;
  CHECK(ce::read_lha(archive, &entries, &error));
  // A byte in the middle of the coded symbols changed: the CRC-16 (or the code) catches it.
  archive[size_t(entries[0].data) + entries[0].packed - 20] ^= 0x5a;
  std::vector<uint8_t> out;
  CHECK(!ce::lha_unpack(archive, entries[0], &out, &error));
  CHECK(!ce::read_lha(text("no archive here at all, only words and more words"), &entries, &error));
  CHECK(error.find("no archive header") != std::string::npos);
}

TEST(source_finds_the_pack_by_every_route) {
  const std::vector<uint8_t> pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(pack.size()), "the fixture pack"}};
  const fs::path dir = scratch("routes");
  std::string error;
  ce::Source s;

  // The release zip: the pack inside the installer's archive inside the zip.
  const auto release = ge::vfs::write_stored_zip(
      {{"resource.1", sfx({{"HELP.TXT", text("help")}, {"ID1\\PAK0.PAK", pack}})},
       {"install.bat", text("@echo off")}});
  write(dir / "release.zip", release);
  CHECK_MSG(ce::open_source((dir / "release.zip").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::release_zip && s.pack == pack && s.known == &known[0]);
  CHECK(s.zip == nullptr && s.zip_sha256 == ge::assets::sha256_hex(release));

  // The installer's archive on its own.
  write(dir / "resource.1", sfx({{"ID1\\PAK0.PAK", pack}}));
  CHECK_MSG(ce::open_source((dir / "resource.1").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::installer && s.pack == pack);

  // A zip holding the pack itself.
  write(dir / "plain.zip", ge::vfs::write_stored_zip({{"GAME/ID1/PAK0.PAK", pack}}));
  CHECK_MSG(ce::open_source((dir / "plain.zip").string(), &s, &error, known), error);
  CHECK(s.pack == pack);

  // The pack as a file, and an installed folder with its names in capitals.
  write(dir / "game" / "ID1" / "PAK0.PAK", pack);
  CHECK_MSG(ce::open_source((dir / "game" / "ID1" / "PAK0.PAK").string(), &s, &error, known),
            error);
  CHECK(s.route == ce::Route::pack_file);
  CHECK_MSG(ce::open_source((dir / "game").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::folder && s.pack == pack);

  // A folder with only the release zip in it.
  fs::create_directories(dir / "downloads");
  fs::copy_file(dir / "release.zip", dir / "downloads" / "shareware.zip");
  CHECK_MSG(ce::open_source((dir / "downloads").string(), &s, &error, known), error);
  CHECK(s.pack == pack);
  fs::remove_all(dir);
}

TEST(source_refuses_an_unknown_pack) {
  std::vector<uint8_t> pack = fixture_pack();
  const fs::path dir = scratch("refuse");
  write(dir / "pak0.pak", pack);
  ce::Source s;
  std::string error;
  // The real pack's list: the fixture is not on it.
  CHECK(!ce::open_source((dir / "pak0.pak").string(), &s, &error));
  CHECK(error.find(ge::assets::sha256_hex(pack)) != std::string::npos);
  CHECK(error.find("not a known unchanged release") != std::string::npos);
  // One byte changed is another pack.
  const std::string sha = ge::assets::sha256_hex(pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(pack.size()), "the fixture pack"}};
  pack[20] ^= 1;
  write(dir / "pak0.pak", pack);
  CHECK(!ce::open_source((dir / "pak0.pak").string(), &s, &error, known));
  CHECK(!ce::open_source((dir / "nothing-here").string(), &s, &error, known));
  write(dir / "notes.txt", text("not a pack"));
  CHECK(!ce::open_source((dir / "notes.txt").string(), &s, &error, known));
  CHECK(error.find("neither a zip, an installer archive nor a pack") != std::string::npos);
  fs::remove_all(dir);
}
