// The game folder a player loads (games/shooters_edge/client/game_folder.hpp) over installs
// written here: an install with its first pack in any case taken, its base folder picked taken as
// its parent, a folder with no base folder or no pack refused with what it lacks; the multiplayer
// folder played over the base when the install has one; the folder kept and read back, and chosen
// at start only when -basedir names none; the programs of a folder with no game (the baseline mod,
// no campaign, the fallback map) and of one whose packs hold the start map; the campaign's
// commands taken away with no campaign.
//
// ge:summary Tests the game folder a player loads: the install checked, kept, its programs
// ge:keywords test shooters-edge game-folder load-game check_game_folder game_of remember_game
// ge:keywords choose_folder load_programs campaign_commands
// ge:skill shooter classicenhance testing
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

void write_pack(const fs::path& p, const Files& files) {
  std::vector<uint8_t> b;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &b, &error), error);
  write(p, b);
}

fs::path fresh(const std::string& name) {
  const fs::path root = fs::temp_directory_path() / ("ge_se_game_folder_" + name);
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

const std::string kBase(ge::classicenhance::classic_profile().base_dir);
const std::string kMulti(ge::classicenhance::classic_profile().multiplayer_dir);

}  // namespace

TEST(an_install_is_taken_by_its_first_pack_in_any_case) {
  const fs::path root = fresh("install");
  write_pack(root / kBase / "PAK0.PAK", {{"maps/start.bsp", {1}}});
  std::string basedir, error;
  CHECK_MSG(cl::check_game_folder(root.string(), &basedir, &error), error);
  CHECK(fs::equivalent(basedir, root));
  // The base folder picked: the install is its parent.
  CHECK_MSG(cl::check_game_folder((root / kBase).string(), &basedir, &error), error);
  CHECK(fs::equivalent(basedir, root));
  // A trailing separator is the same folder.
  CHECK_MSG(cl::check_game_folder(root.string() + "/", &basedir, &error), error);
  CHECK(fs::equivalent(basedir, root));
}

TEST(a_folder_that_is_no_install_is_refused_with_what_it_lacks) {
  const fs::path root = fresh("refused");
  std::string basedir, error;
  CHECK(!cl::check_game_folder(root.string(), &basedir, &error));
  CHECK_MSG(error.find("holds no " + kBase + " folder") != std::string::npos, error);
  CHECK(basedir.empty());
  fs::create_directories(root / kBase);
  write(root / kBase / "readme.txt", {1});
  CHECK(!cl::check_game_folder(root.string(), &basedir, &error));
  CHECK_MSG(error.find("holds no pak0.pak") != std::string::npos, error);
  CHECK(!cl::check_game_folder((root / "missing").string(), &basedir, &error));
  CHECK_MSG(error.find("is not a folder") != std::string::npos, error);
}

TEST(the_multiplayer_folder_is_played_over_the_base_when_the_install_has_one) {
  const fs::path root = fresh("multi");
  write_pack(root / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  CHECK(cl::game_of(root.string()) == kBase);
  fs::create_directories(root / kMulti / "skins");
  CHECK(cl::game_of(root.string()) == kMulti);
}

TEST(the_game_loaded_is_kept_and_chosen_unless_basedir_names_another) {
  const fs::path prefs = fresh("prefs");
  CHECK(cl::remembered_game(prefs.string()).basedir.empty());
  CHECK(cl::remembered_game("").basedir.empty());
  CHECK(!cl::remember_game("", "/x"));
  const fs::path root = fresh("kept");
  write_pack(root / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  fs::create_directories(root / kMulti);
  CHECK(cl::remember_game(prefs.string(), root.string()));
  const cl::KeptGame kept = cl::remembered_game(prefs.string());
  CHECK(kept.basedir == root.string());
  CHECK(kept.profile == ge::classicenhance::classic_profile().id);
  // No -basedir: the game kept, the multiplayer folder over it.
  cl::Args a;
  a.basedir = fresh("cwd").string();
  cl::choose_folder(a, prefs.string());
  CHECK(fs::equivalent(a.basedir, root));
  CHECK(a.game == kMulti);
  // The game kept wins over an install where the game runs: it stays until another is loaded.
  const fs::path here = fresh("here");
  write_pack(here / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  cl::Args in_install;
  in_install.basedir = here.string();
  cl::choose_folder(in_install, prefs.string());
  CHECK(fs::equivalent(in_install.basedir, root));
  // -basedir and -game given: kept as they are.
  cl::Args named;
  named.basedir = "/elsewhere";
  named.basedir_set = true;
  named.game = "mod";
  named.game_set = true;
  cl::choose_folder(named, prefs.string());
  CHECK(named.basedir == "/elsewhere" && named.game == "mod");
}

TEST(a_kept_game_that_no_longer_opens_falls_back_to_the_folder_the_game_runs_in) {
  const fs::path prefs = fresh("prefs-gone");
  const fs::path here = fresh("here-gone");
  write_pack(here / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  CHECK(cl::remember_game(prefs.string(), (fresh("gone") / "nothing").string()));
  cl::Args a;
  a.basedir = here.string();
  cl::choose_folder(a, prefs.string());
  CHECK(a.basedir == here.string());
  CHECK(a.game == kBase);
  // A game this build does not know is passed over the same way.
  {
    std::ofstream f(prefs / "game_folder.txt", std::ios::trunc);
    f << "profile no-such-game\nbasedir " << here.string() << "\n";
  }
  CHECK(cl::remembered_game(prefs.string()).profile == "no-such-game");
  cl::Args b;
  b.basedir = fresh("cwd-gone").string();
  cl::choose_folder(b, prefs.string());
  CHECK(b.basedir != here.string());
}

TEST(the_older_kept_file_of_a_folder_alone_reads_as_the_first_profile) {
  const fs::path prefs = fresh("prefs-old");
  {
    std::ofstream f(prefs / "game_folder.txt", std::ios::trunc);
    f << "/games/classic\r\n";
  }
  const cl::KeptGame kept = cl::remembered_game(prefs.string());
  CHECK(kept.basedir == "/games/classic");
  CHECK(kept.profile == ge::classicenhance::classic_profile().id);
}

TEST(a_folder_with_no_game_plays_the_baseline_mod_on_the_fallback_map) {
  const cl::GameAdapter& adapter = cl::adapter_of(shooters_edge::profiles::classic_game());
  cl::SearchPath files;
  cl::GameContent c;
  std::string error;
  CHECK_MSG(cl::load_programs(adapter, files, "", c, error), error);
  CHECK(c.campaign == nullptr);
  CHECK(!c.registered);
  CHECK(c.first_map == shooters_edge::profiles::classic_game().fallback_map);
  CHECK(c.start_map == ge::classicenhance::classic_profile().start_map);
  // Packs holding the start map start on it.
  std::vector<uint8_t> pack;
  CHECK(ge::vfs::write_pak({{"maps/start.bsp", {1, 2}}}, &pack, &error));
  CHECK(cl::add_archive(files, "pak0.pak", std::move(pack), error));
  CHECK_MSG(cl::load_programs(adapter, files, "", c, error), error);
  CHECK(c.first_map == c.start_map);
}

TEST(the_campaign_commands_go_with_the_campaign) {
  ge::console::Console con;
  cl::Listen listen;
  cl::SearchPath files;
  cl::GameContent c;
  std::string next_map, load_ask;
  c.campaign = &c.progs;
  c.start_map = "start";
  cl::campaign_commands(con, listen, files, c, next_map, load_ask);
  for (const char* name : {"newgame", "save", "load"}) CHECK_MSG(con.has_command(name), name);
  con.execute("newgame");
  CHECK(next_map == "start");
  con.execute("load s0");
  CHECK(load_ask == "s0");
  c.campaign = nullptr;
  cl::campaign_commands(con, listen, files, c, next_map, load_ask);
  for (const char* name : {"newgame", "save", "load"}) CHECK_MSG(!con.has_command(name), name);
}
