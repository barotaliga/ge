// Games taken into the data folder (games/shooters_edge/client/take_in.hpp), from fixtures only: a
// game in a folder elsewhere is copied into data/classic and that copy is what opens; pointing at
// the copy opens it as it is, and a second launch at the same unchanged game copies nothing; the
// data folder is the player's own (beside the game only when that cannot be written), with a
// config beside the program read over the player's; the copy's share said for its bar and run on
// its own thread; a folder with no game is refused in the game's words.
//
// ge:summary Tests taking a pointed-at game into the data folder and opening the copy
// ge:keywords test import take-in data-folder copy basedir series_settings_files share
// ge:keywords take-in-thread cfg_save_global
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/settings.hpp"
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

TEST(take_in_names_the_mission_packs_and_waits_for_their_game) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_packs_test";
  fs::remove_all(top);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"gfx/palette.lmp", std::vector<uint8_t>(768, 3)}}, &pak, &error));
  const auto put = [&](const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), std::streamsize(pak.size()));
  };
  const std::string root = (top / "data").string();
  std::string basedir, said;
  // The pack alone: kept, but the game playing stays until Quake itself is added.
  put(top / "disc/hipnotic/pak0.pak");
  CHECK(!cl::take_in((top / "disc").string(), root, &basedir, &said, &error));
  CHECK_MSG(error.find("Took Scourge of Armagon in") == 0, error);
  CHECK(error.find("add Quake too") != std::string::npos && basedir.empty());
  CHECK(fs::exists(top / "data/q1/hipnotic/pak0.pak"));
  // The game with both packs: named, and how to start them said.
  put(top / "install/id1/pak0.pak");
  put(top / "install/hipnotic/pak0.pak");
  put(top / "install/rogue/pak0.pak");
  CHECK_MSG(cl::take_in((top / "install").string(), root, &basedir, &said, &error), error);
  CHECK_MSG(said.find("Took Quake with Scourge of Armagon and Dissolution of Eternity in") == 0,
            said);
  CHECK(said.find("Start -game hipnotic or -game rogue to play a mission pack.") !=
        std::string::npos);
  CHECK(basedir == (top / "data/q1").string());
}

TEST(take_in_falls_back_beside_the_game_and_reads_its_own_config_over_the_players) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_own_config_test";
  fs::remove_all(top);
  const std::string placed = (top / "placed").string();
  fs::create_directories(top / "placed");
  CHECK(cl::data_root(placed, "") == (top / "placed/data").string());
  const std::string root = cl::data_root(placed, (top / "prefs").string());
  // No config beside the program: the player's alone, written there.
  std::vector<fs::path> files = cl::series_settings_files(root, placed);
  CHECK(files.size() == 1 && files[0] == top / "prefs/data/settings/config.cfg");
  // One beside it: run after the player's, its lines winning, and the one written.
  fs::create_directories(top / "placed/data/settings");
  std::ofstream(top / "placed/data/settings/config.cfg") << "sensitivity 4\n";
  files = cl::series_settings_files(root, placed);
  CHECK(files.size() == 2 && files[1] == top / "placed/data/settings/config.cfg");
  // The data folder beside the program: the one file, never read twice.
  CHECK(cl::series_settings_files((top / "placed/data").string(), placed).size() == 1);
  // cfg_save_global: written to the player's own, not the program's.
  ge::console::Console con;
  fs::create_directories(top / "game");
  con.set_search_path({top / "game"});
  cl::add_save_global_command(con, files);
  con.execute("cfg_save_global");
  con.run();
  CHECK(fs::exists(top / "prefs/data/settings/config.cfg"));
}

TEST(a_game_taken_in_on_its_own_thread_says_its_share_done) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_thread_test";
  fs::remove_all(top);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"gfx/palette.lmp", std::vector<uint8_t>(768, 3)}}, &pak, &error));
  for (const char* name : {"id1/pak0.pak", "id1/pak1.pak"}) {
    fs::create_directories((top / "elsewhere" / name).parent_path());
    std::ofstream(top / "elsewhere" / name, std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), std::streamsize(pak.size()));
  }
  std::vector<std::pair<std::string, double>> shares;
  std::string basedir, said;
  CHECK_MSG(
      cl::take_in((top / "elsewhere").string(), (top / "data").string(), &basedir, &said, &error,
                  {}, [&](const std::string& what, double v) { shares.emplace_back(what, v); }),
      error);
  // The bar's words and its share by bytes: none, half (one of two packs the same size), all.
  CHECK_MSG(shares.size() == 3, std::to_string(shares.size()));
  if (shares.size() == 3) {
    CHECK(shares[0].first == "Copying Quake" && shares[0].second == 0);
    CHECK(shares[1].second == 0.5 && shares[2].second == 1);
  }
  // On its own thread: the lines said in the console, the copy's folder once it has ended.
  fs::remove_all(top / "data");
  ge::console::Console con;
  cl::TakeIn job;
  std::string ask = (top / "elsewhere").string(), folder;
  CHECK(job.start(&ask, (top / "data").string(), true) && ask.empty() && job.running());
  std::string empty;
  CHECK(!job.start(&empty, (top / "data").string(), false));
  while (!job.finished(con, &folder, &error)) {
  }
  CHECK_MSG(folder == (top / "data/q1").string(), folder + " " + error);
  CHECK(job.from_menu() && !job.running());
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

// A Quake II mission pack's level asks its disc track; with `ogg_remap_tracks` on, the window's
// music finds the digital release's file for that pack, and with it off the disc's number.
TEST(a_mission_packs_level_plays_the_digital_releases_track) {
  const fs::path top = fs::temp_directory_path() / "ge_take_in_remap_test";
  fs::remove_all(top);
  fs::create_directories(top / "music");
  for (const char* name : {"track02.flac", "track09.flac", "track12.flac"})
    std::ofstream(top / "music" / name, std::ios::binary) << "fLaC";
  ge::console::Console con;
  ge::audio::MusicConsole music;
  std::string ask, basedir = top.string(), game = "xatrix";
  bool remap = true;
  cl::add_take_in_commands(con, &ask, &basedir, &music, &game, &remap);
  CHECK(music.track_of(2) == (top / "music/track09.flac").string());
  game = "rogue";
  CHECK(music.track_of(2) == (top / "music/track12.flac").string());
  game = "XATRIX";  // as a player may type it
  CHECK(music.track_of(2) == (top / "music/track09.flac").string());
  remap = false;
  CHECK(music.track_of(2) == (top / "music/track02.flac").string());
  game = "baseq2";
  remap = true;
  CHECK(music.track_of(2) == (top / "music/track02.flac").string());
  fs::remove_all(top);
}
