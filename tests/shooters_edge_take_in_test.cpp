// Games taken into the data folder (games/shooters_edge/client/take_in.hpp), from fixtures only: a
// game in a folder elsewhere is copied into data/classic and that copy is what opens; pointing at
// the copy opens it as it is, and a second launch at the same unchanged game copies nothing; the
// data folder is the player's own (beside the game only when that cannot be written), and the
// settings an older build kept beside the game are carried over; a folder with no game is refused
// in the game's words.
//
// ge:summary Tests taking a pointed-at game into the data folder and opening the copy
// ge:keywords test import take-in data-folder copy basedir
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
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
  CHECK(root == (top / "prefs/data").string());  // the player's own folder outlives the program
  std::string basedir, said;
  CHECK_MSG(cl::take_in((top / "elsewhere").string(), root, &basedir, &said, &error), error);
  CHECK(basedir == (top / "prefs/data/q1").string());
  CHECK(fs::exists(top / "prefs/data/q1/id1/pak0.pak"));
  CHECK(said.find("1 file copied") != std::string::npos);
  CHECK(cl::inside(basedir, root));

  std::string again;
  CHECK(cl::take_in(basedir, root, &again, &said, &error));
  CHECK(again == basedir && said.empty());  // the copy opens as it is

  // A second launch pointed at the same unchanged game copies nothing.
  std::string second;
  CHECK(cl::take_in((top / "elsewhere").string(), root, &second, &said, &error));
  CHECK(second == basedir);
  CHECK_MSG(said.find("nothing there has changed") != std::string::npos, said);

  CHECK(!cl::take_in((top / "placed").string(), root, &again, &said, &error));
  CHECK(error.find("Could not take the game in") == 0);
}

TEST(take_in_falls_back_beside_the_game_and_adopts_its_old_settings) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_adopt_test";
  fs::remove_all(top);
  fs::create_directories(top / "placed/data/settings");
  CHECK(cl::data_root((top / "placed").string(), "") == (top / "placed/data").string());
  std::ofstream(top / "placed/data/settings/config.cfg") << "sensitivity 4\n";
  const std::string root = cl::data_root((top / "placed").string(), (top / "prefs").string());
  CHECK(cl::adopt_placed_settings((top / "placed").string(), root));
  CHECK(fs::exists(top / "prefs/data/settings/config.cfg"));
  CHECK(!cl::adopt_placed_settings((top / "placed").string(), root));  // never over its own
}

TEST(homedir_on_the_command_line_is_the_data_folder_and_is_read_only_after) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_home_test";
  fs::remove_all(top);
  std::vector<std::string> plus{"+set", "skill", "2", "+SET", "HomeDir", (top / "mine").string(),
                                "+map", "base1"};
  const std::string home = cl::take_home_dir(&plus);
  CHECK(home == (top / "mine").string());
  CHECK((plus == std::vector<std::string>{"+set", "skill", "2", "+map", "base1"}));
  CHECK(cl::data_root((top / "placed").string(), (top / "prefs").string(), home) == home);
  CHECK(fs::is_directory(top / "mine"));  // made when it was missing
  std::vector<std::string> none{"+map", "base1"};
  CHECK(cl::take_home_dir(&none).empty() && none.size() == 2);
  CHECK(cl::data_root((top / "placed").string(), (top / "prefs").string(), "") ==
        (top / "prefs/data").string());
  ge::console::Console con;
  cl::add_home_dir(con, home);
  CHECK(con.cvars().find("homedir") != nullptr && con.cvars().find("homedir")->value == home);
  CHECK(con.cvars().set("homedir", "/elsewhere", ge::console::Source::local) ==
        ge::console::SetResult::read_only);
  fs::remove_all(top);
}
