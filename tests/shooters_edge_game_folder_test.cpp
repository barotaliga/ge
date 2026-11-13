// The game folder a player loads (games/shooters_edge/client/game_folder.hpp) over installs
// written here: an install with its first pack in any case taken, its base folder picked taken as
// its parent, a folder with no base folder or no pack refused with what it lacks; the multiplayer
// folder played over the base when the install has one; the folder kept and read back, and chosen
// at start only when -basedir names none; the programs of a folder with no game (the baseline mod,
// no campaign, the fallback map) and of one whose packs hold the start map; the campaign's
// commands taken away with no campaign; `coopgame` asked for in a campaign already running starts
// a new one in coop, where `addbot` seats companions.
//
// ge:summary Tests the game folder a player loads: the install checked, kept, its programs
// ge:keywords test shooters-edge game-folder load-game check_game_folder game_of remember_game
// ge:keywords choose_folder load_programs campaign_commands coopgame addbot new_game
// ge:skill shooter classicenhance testing
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/coop_party.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
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
  // An install the game was started inside wins over the game kept: it is played at once.
  const fs::path here = fresh("here");
  write_pack(here / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  cl::Args in_install;
  in_install.basedir = here.string();
  cl::choose_folder(in_install, prefs.string());
  CHECK(fs::equivalent(in_install.basedir, here));
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

// A game whose own rules keep the campaign (the sequel's runner) and no single-player program:
// NEW GAME and COOPERATIVE start its game with deathmatch 0, coop off or on, from its first map;
// there is nothing to save yet, and `coopgame` seats its companions (1 by default).
TEST(a_game_whose_rules_keep_the_campaign_has_new_game_and_coopgame) {
  ge::console::Console con;
  cl::Listen listen;
  cl::SearchPath files;
  cl::GameContent c;
  std::string next_map, load_ask;
  c.rules_campaign = true;
  c.first_map = "demo1";
  cl::campaign_commands(con, listen, files, c, next_map, load_ask);
  for (const char* name : {"newgame", "coopgame"}) CHECK_MSG(con.has_command(name), name);
  for (const char* name : {"save", "load"}) CHECK_MSG(!con.has_command(name), name);
  con.execute("newgame");
  CHECK(next_map == "demo1" && listen.opt.new_game);
  CHECK(cl::wants_campaign(listen.opt) && listen.opt.console.back() == "coop 0");
  next_map.clear();
  listen.opt.new_game = false;
  con.execute("coopgame 2 expert demo2");
  CHECK(next_map == "demo2" && listen.opt.new_game && listen.opt.companions == 2);
  CHECK(listen.opt.companion_skill == ge::shooter::AimSkill::expert);
  CHECK(listen.opt.console.back() == "coop 1");
  next_map.clear();
  con.execute("coopgame");  // one companion by default, from the first map
  CHECK(next_map == "demo1" && listen.opt.companions == 1);
}

namespace {

std::unique_ptr<cl::LoadedMap> stub_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = shooters_edge::server::arena_models();
  m->spec.entities = shooters_edge::server::kStubEntities;
  return m;
}

}  // namespace

// The campaign running as a game without coop (MULTIPLAYER > NEW GAME's COOPERATIVE once ran it
// so): `addbot` seats a companion and turns it cooperative, `removebot` makes it single player
// again, and `coopgame` starts a new game in coop, not a change of map, so the companions it asks
// for sit beside the player and `addbot` seats more.
TEST(coopgame_in_a_running_campaign_starts_a_coop_game_that_takes_bots) {
  ge::console::Console con;
  cl::Listen listen;
  cl::SearchPath files;
  cl::GameContent c;
  std::string next_map, load_ask, error;
  CHECK_MSG(shooters_edge::server::build_campaign_stub(c.progs, error), error);
  c.campaign = &c.progs;
  c.start_map = "start";
  cl::campaign_commands(con, listen, files, c, next_map, load_ask);
  listen.opt.console = {"deathmatch 0"};
  CHECK_MSG(cl::start_map(listen, c, stub_map("first"), error), error);
  CHECK(listen.campaign_host != nullptr);
  CHECK(cl::listen_lobby(listen, "addbot"));
  CHECK(listen.party != nullptr && cl::party_size(*listen.party) == 1);
  CHECK(listen.server->cvars.find("coop")->on());
  CHECK(cl::listen_lobby(listen, "removebot"));
  CHECK(cl::party_size(*listen.party) == 0 && !listen.server->cvars.find("coop")->on());

  con.execute("coopgame 2 first");
  CHECK(next_map == "first" && listen.opt.new_game);
  CHECK_MSG(cl::start_map(listen, c, stub_map("first"), error), error);
  CHECK(!listen.opt.new_game);
  CHECK(listen.party != nullptr && cl::party_size(*listen.party) == 2);
  CHECK(listen.server->clients[1].active && listen.server->clients[2].active);
  CHECK(cl::listen_lobby(listen, "addbot expert"));
  CHECK(cl::party_size(*listen.party) == 3);

  // A map asked for in the coop game is a change of map: the party comes along.
  CHECK_MSG(cl::start_map(listen, c, stub_map("second"), error), error);
  CHECK(cl::party_size(*listen.party) == 3);

  // `newgame` is a new single-player game: coop off, the companions let go; `addbot` then seats
  // one in it as in any single-player game.
  con.execute("newgame");
  CHECK_MSG(cl::start_map(listen, c, stub_map("start"), error), error);
  CHECK(!listen.party || cl::party_size(*listen.party) == 0);
  CHECK(!listen.server->cvars.find("coop")->on());
  CHECK(cl::listen_lobby(listen, "addbot"));
  CHECK(cl::party_size(*listen.party) == 1 && listen.companion_coop);
}

TEST(each_game_keeps_its_own_install_and_reads_as_its_edition) {
  const fs::path prefs = fresh("prefs-installs");
  const fs::path shareware = fresh("shareware");
  const fs::path full = fresh("full");
  const ge::classicenhance::Profile& classic = ge::classicenhance::classic_profile();
  write_pack(shareware / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  write_pack(full / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  write_pack(full / kBase / "pak1.pak", {{std::string(classic.full_game), {1}}});
  CHECK(cl::edition_of(shareware.string(), classic) == cl::Edition::shareware);
  CHECK(cl::edition_of(full.string(), classic) == cl::Edition::full);
  CHECK(cl::edition_of((shareware / "gone").string(), classic) == cl::Edition::unknown);
  CHECK(cl::edition_words(cl::Edition::shareware, classic) == "Shareware");
  CHECK(cl::edition_words(cl::Edition::full, classic) == "Registered");

  // Nothing kept yet; then one install a game, the last one kept for each.
  CHECK(cl::remembered_installs(prefs.string()).empty());
  CHECK(cl::remember_install(prefs.string(), "classic", shareware.string()));
  CHECK(cl::remember_install(prefs.string(), "sequel", "/games/the sequel"));
  CHECK(cl::remember_install(prefs.string(), "classic", full.string()));
  const auto kept = cl::remembered_installs(prefs.string());
  CHECK(kept.size() == 2);
  CHECK(kept.at("classic") == full.string());
  CHECK(kept.at("sequel") == "/games/the sequel");  // a folder with a space kept whole
  // No preferences folder: nothing kept, nothing read.
  CHECK(!cl::remember_install("", "classic", full.string()));
  CHECK(cl::remembered_installs("").empty());
}
