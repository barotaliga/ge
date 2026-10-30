// THE SEARCH PATH: a base game directory and a mod over it, built on disk from our own fixture
// archives. The packs mount in number order (pak10 after pak2), then pk3s and pk3dirs in name
// order, then the loose files over every archive, and the mod over the base; names are found
// whatever their case and separators; a pack's file is a view, a loose one is read from disk; a
// lookup allocates nothing; a damaged archive is refused naming it; and the whole table (each
// name, the source that wins it, its size and a digest of its bytes) is held golden
// (tests/golden/vfs_search.txt).
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/vfs_search_test` and say why in the PR.
//
// ge:summary Test: game directories, archive order, loose files over archives, the mod over the
// ge:summary base, case-blind lookup without allocation, a damaged archive, the golden file table
// ge:keywords test vfs search-path gamedir pak pk3 pk3dir loose override find golden allocation
// ge:skill vfs testing
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "prof/memory.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"
#include "vfs/zip.hpp"

namespace {

namespace fs = std::filesystem;
namespace vfs = ge::vfs;

constexpr const char* kGolden = "tests/golden/vfs_search.txt";
using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

void put(const fs::path& p, const std::vector<uint8_t>& bytes) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}

void put_pak(const fs::path& p, const Files& files) {
  std::vector<uint8_t> b;
  std::string error;
  CHECK_MSG(vfs::write_pak(files, &b, &error), error);
  put(p, b);
}

// The fixture: <tmp>/base and <tmp>/mod.
fs::path fixture() {
  const fs::path root = fs::temp_directory_path() / "ge_vfs_search_test";
  fs::remove_all(root);
  const fs::path base = root / "base", mod = root / "mod";
  put_pak(base / "pak0.pak",
          {{"a.txt", text("pak0")}, {"b.txt", text("pak0")}, {"gfx/palette.lmp", text("palette")}});
  put_pak(base / "pak1.pak", {{"b.txt", text("pak1")}, {"maps/C.bsp", text("pak1 map")}});
  put_pak(base / "pak2.pak", {{"e.txt", text("pak2")}});
  put_pak(base / "pak10.pak", {{"e.txt", text("pak10")}});
  put(base / "b.pk3", vfs::write_stored_zip({{"d.txt", text("b.pk3")}, {"f.txt", text("b.pk3")}}));
  put(base / "a.pk3dir" / "d.txt", text("a.pk3dir"));
  put(base / "a.pk3dir" / "g" / "h.txt", text("a.pk3dir deep"));
  put(base / "A.txt", text("loose"));
  put(base / "sound" / "Door.wav", text("loose wav"));
  put_pak(mod / "pak0.pak", {{"b.txt", text("mod")}});
  put(mod / "progs.dat", text("mod progs"));
  return root;
}

vfs::SearchPath mounted(const fs::path& root) {
  vfs::SearchPath sp;
  std::string error;
  CHECK_MSG(vfs::add_game(&sp, root.string(), "base", &error), error);
  CHECK_MSG(vfs::add_game(&sp, root.string(), "mod", &error), error);
  return sp;
}

std::string contents(const vfs::SearchPath& sp, const char* path) {
  const vfs::FileRef* f = vfs::find(sp, path);
  if (!f) return "<none>";
  std::vector<uint8_t> b;
  std::string error;
  if (!vfs::read(sp, *f, &b, &error)) return "<" + error + ">";
  return {b.begin(), b.end()};
}

uint64_t fnv(const std::vector<uint8_t>& b) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (uint8_t x : b) h = (h ^ x) * 0x100000001b3ull;
  return h;
}

}  // namespace

TEST(each_name_is_won_by_the_last_source_mounted) {
  const fs::path root = fixture();
  const vfs::SearchPath sp = mounted(root);
  CHECK(contents(sp, "a.txt") == "loose");  // loose over pak0
  CHECK(contents(sp, "b.txt") == "mod");    // the mod over the base
  CHECK(contents(sp, "e.txt") == "pak10");  // pak10 after pak2: numbers, not names
  CHECK(contents(sp, "d.txt") == "b.pk3");  // b.pk3 after a.pk3dir: name order
  CHECK(contents(sp, "g/h.txt") == "a.pk3dir deep");
  CHECK(contents(sp, "f.txt") == "b.pk3");
  CHECK(contents(sp, "progs.dat") == "mod progs");
  CHECK(contents(sp, "missing.txt") == "<none>");
  // The archives themselves and the pk3dir's own files are not loose files.
  CHECK(vfs::find(sp, "pak0.pak") == nullptr);
  CHECK(vfs::find(sp, "a.pk3dir/d.txt") == nullptr);
}

TEST(names_are_found_whatever_their_case_and_separators) {
  const vfs::SearchPath sp = mounted(fixture());
  CHECK(contents(sp, "MAPS\\c.BSP") == "pak1 map");
  CHECK(contents(sp, "sound/door.wav") == "loose wav");
  const vfs::FileRef* f = vfs::find(sp, "maps/c.bsp");
  CHECK(f != nullptr);
  if (f) CHECK(vfs::stored_name(sp, *f) == "maps/C.bsp");
}

TEST(a_pack_file_is_a_view_and_a_loose_one_is_read_from_disk) {
  const vfs::SearchPath sp = mounted(fixture());
  const vfs::FileRef* pal = vfs::find(sp, "gfx/palette.lmp");
  CHECK(pal != nullptr);
  if (pal) {
    const std::span<const uint8_t> v = vfs::view(sp, *pal);
    CHECK(std::string(v.begin(), v.end()) == "palette");
    CHECK(pal->size == 7);
  }
  const vfs::FileRef* loose = vfs::find(sp, "a.txt");
  CHECK(loose != nullptr);
  if (loose) CHECK(vfs::view(sp, *loose).empty());
}

TEST(a_lookup_allocates_nothing) {
  ge::prof::count_allocations();
  const vfs::SearchPath sp = mounted(fixture());
  const char* names[] = {"a.txt", "MAPS/C.BSP", "g/h.txt", "missing", "sound\\Door.wav"};
  CHECK(ge::prof::allocations().counting);
  const int64_t before = ge::prof::allocations().count;
  size_t found = 0;
  for (int k = 0; k < 1000; ++k) {
    for (const char* n : names) found += vfs::find(sp, n) != nullptr;
  }
  CHECK(ge::prof::allocations().count == before);
  CHECK(found == 4000);
}

TEST(a_damaged_archive_is_refused_naming_it) {
  const fs::path root = fixture();
  put(root / "base" / "pak3.pak", text("PACK but nothing after"));
  vfs::SearchPath sp;
  std::string error;
  CHECK(!vfs::add_game(&sp, root.string(), "base", &error));
  CHECK(error.find("pak3.pak") != std::string::npos);
  CHECK(error.find("next") != std::string::npos || error.find("skill") != std::string::npos);
  CHECK(!vfs::add_game(&sp, root.string(), "nowhere", &error));
  CHECK(error.find("nowhere") != std::string::npos);
}

TEST(the_file_table_holds_golden) {
  const fs::path root = fixture();
  const vfs::SearchPath sp = mounted(root);
  std::string table = "# name source size digest (vfs_search_test)\n";
  for (const vfs::Listed& l : vfs::list(sp)) {
    std::vector<uint8_t> b;
    std::string error;
    CHECK(vfs::read(sp, l.ref, &b, &error));
    const fs::path src = fs::path(sp.sources[l.ref.source].path).lexically_relative(root);
    char row[64];
    std::snprintf(row, sizeof row, " %llu %016llx\n", static_cast<unsigned long long>(l.ref.size),
                  static_cast<unsigned long long>(fnv(b)));
    table += std::string(l.name) + " " + src.generic_string() + row;
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& w : ge::test::golden_words(kGolden)) rows.push_back(w);
  std::vector<std::vector<std::string>> now;
  size_t at = table.find('\n') + 1;
  while (at < table.size()) {
    const size_t end = table.find('\n', at);
    std::vector<std::string> words;
    size_t w = at;
    while (w < end) {
      const size_t sp_at = std::min(table.find(' ', w), end);
      words.push_back(table.substr(w, sp_at - w));
      w = sp_at + 1;
    }
    now.push_back(words);
    at = end + 1;
  }
  CHECK(rows == now);
}
