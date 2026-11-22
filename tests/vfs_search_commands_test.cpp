// engine/vfs/search_commands.hpp: path, dir, fdir and whereis over two packs mounted one over the
// other, and the wildcard's rules.
//
// ge:summary Test: the search path's console words over two packs, and the wildcard
// ge:keywords test path dir fdir whereis glob_match search-path
// ge:skill vfs testing
#include "vfs/search_commands.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

namespace vfs = ge::vfs;

namespace {

std::vector<uint8_t> pak_of(const std::vector<std::string>& names) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  files.reserve(names.size());
  for (const std::string& n : names) files.emplace_back(n, std::vector<uint8_t>{1, 2, 3});
  std::vector<uint8_t> b;
  std::string error;
  CHECK_MSG(vfs::write_pak(files, &b, &error), error);
  return b;
}

}  // namespace

TEST(wildcards_fit_runs_and_single_characters) {
  CHECK(vfs::glob_match("maps/*.bsp", "maps/dm2.bsp"));
  CHECK(vfs::glob_match("MAPS/DM?.BSP", "maps/dm2.bsp"));
  CHECK(!vfs::glob_match("maps/*.bsp", "maps/dm2.lit"));
  CHECK(vfs::glob_match("*", "a/b"));
  CHECK(!vfs::glob_match("dm?", "dm"));
}

TEST(the_four_words_read_the_mounted_packs) {
  vfs::SearchPath sp;
  std::string error;
  CHECK_MSG(vfs::add_archive(&sp, "pak0.pak", pak_of({"maps/e1m1.bsp", "maps/dm2.bsp", "a.txt"}),
                             "id1", &error),
            error);
  CHECK_MSG(
      vfs::add_archive(&sp, "pak1.pak", pak_of({"maps/dm2.bsp", "maps/dm2.lit"}), "id1", &error),
      error);
  ge::console::Console con{false};
  vfs::add_search_commands(con, sp);
  con.execute("path");
  const auto& out = con.output();
  CHECK(out[out.size() - 3] == "Current search path:");
  CHECK(out[out.size() - 2] == "pak1.pak (2 files)");
  CHECK(out.back() == "pak0.pak (3 files)");
  con.execute("dir maps bsp");
  CHECK(out[out.size() - 3] == "dm2.bsp" && out[out.size() - 2] == "e1m1.bsp");
  CHECK(out.back() == "2 files listed.");
  con.execute("fdir maps/dm2.*");
  CHECK(out.back() == "2 files listed.");
  con.execute("whereis maps/dm2.bsp all");
  CHECK(out[out.size() - 2] == "maps/dm2.bsp is in pak1.pak");
  CHECK(out.back() == "  and under it in pak0.pak");
  con.execute("whereis maps/none.bsp");
  CHECK(out.back() == "maps/none.bsp was not found.");
}
