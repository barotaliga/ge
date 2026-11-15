// Games taken into the data folder (games/shooters_edge/client/take_in.hpp), from fixtures only: a
// game in a folder elsewhere is copied into data/classic and that copy is what opens; pointing at
// the copy opens it as it is; the data folder is beside the game when that can be written; a
// folder with no game is refused in the game's words.
//
// ge:summary Tests taking a pointed-at game into the data folder and opening the copy
// ge:keywords test import take-in data-folder copy basedir
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/take_in.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

TEST(take_in_copies_a_game_from_elsewhere_and_opens_the_copy) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_test";
  fs::remove_all(top);
  fs::create_directories(top / "placed");
  fs::create_directories(top / "elsewhere/id1");
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"gfx/palette.lmp", std::vector<uint8_t>(768, 3)}}, &pak, &error));
  std::ofstream(top / "elsewhere/id1/pak0.pak", std::ios::binary)
      .write(reinterpret_cast<const char*>(pak.data()), std::streamsize(pak.size()));

  const std::string root = cl::data_root((top / "placed").string(), (top / "prefs").string());
  CHECK(root == (top / "placed/data").string());
  std::string basedir, said;
  CHECK_MSG(cl::take_in((top / "elsewhere").string(), root, &basedir, &said, &error), error);
  CHECK(basedir == (top / "placed/data/classic").string());
  CHECK(fs::exists(top / "placed/data/classic/id1/pak0.pak"));
  CHECK(said.find("1 file copied") != std::string::npos);
  CHECK(cl::inside(basedir, root));

  std::string again;
  CHECK(cl::take_in(basedir, root, &again, &said, &error));
  CHECK(again == basedir && said.empty());  // the copy opens as it is

  CHECK(!cl::take_in((top / "prefs").string(), root, &again, &said, &error));
  CHECK(error.find("Could not take the game in") == 0);
}
