// The game folder a player loads (games/shooters_edge/client/game_folder.hpp) over installs
// written here: an install with its first pack in any case taken, its base folder picked taken as
// its parent, a folder with no base folder or no pack refused with what it lacks; the multiplayer
// folder played over the base when the install has one; the game opened from the install its
// setting names (fs_basegame, fs_basedir_<id>), asked for when unset or gone, the settings read,
// bound and asked for; the programs of a folder with no game (the baseline mod,
// no campaign, the fallback map) and of one whose packs hold the start map; the campaign's
// commands taken away with no campaign; `coopgame` asked for in a campaign already running starts
// a new one in coop, where `addbot` seats companions.
//
// ge:summary Tests the game folder a player loads: the install checked, kept, its programs
// ge:keywords test shooters-edge game-folder load-game check_game_folder game_of fs_basegame
// ge:keywords fs_basedir read_base_game bind_base_game base_game_asked
// ge:keywords fs_artifacts_basedir own_files own_game_folder
// ge:keywords choose_folder load_programs campaign_commands coopgame addbot new_game
// ge:skill shooter classicenhance testing
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/coop_party.hpp"
#include "shooters_edge/client/files.hpp"
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

const std::string kBase(ge::classicenhance::q1_profile().base_dir);
const std::string kMulti(ge::classicenhance::q1_profile().multiplayer_dir);

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

TEST(the_game_opens_from_the_install_its_setting_names) {
  const fs::path root = fresh("kept");
  write_pack(root / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  fs::create_directories(root / kMulti);
  const std::string q1(ge::classicenhance::q1_profile().id);
  // fs_basegame q1, its fs_basedir_q1 set: that install, the multiplayer folder over it.
  cl::BaseGame base;
  base.game = q1;
  base.dirs[q1] = root.string();
  cl::Args a;
  cl::choose_folder(a, base, "own");
  CHECK(!a.ask_install && fs::equivalent(a.basedir, root));
  CHECK(a.game == kMulti && a.profile == q1);
  // -game given: kept as it is.
  cl::Args named;
  named.game = "mod";
  named.game_set = true;
  cl::choose_folder(named, base, "own");
  CHECK(fs::equivalent(named.basedir, root) && named.game == "mod");
  // fs_basegame "": Shooter's Edge's own game, whatever installs are set.
  cl::BaseGame own = base;
  own.game.clear();
  cl::Args b;
  cl::choose_folder(b, own, "own");
  CHECK(!b.ask_install && b.basedir == "own");
  // Its own folder is the data folder's se/, beside the classic games' q1/ and q2/: its saves,
  // screenshots and recordings are never written in the folder the game was started in.
  CHECK(fs::path(cl::own_game_folder("data")) == fs::path("data") / "se");
  CHECK(cl::own_game_folder("") == ".");
}

TEST(a_game_whose_install_is_unset_gone_or_unknown_is_asked_for_or_passed_over) {
  const std::string q1(ge::classicenhance::q1_profile().id);
  // Set to a folder that no longer holds the game: asked for.
  cl::BaseGame base;
  base.game = q1;
  base.dirs[q1] = (fresh("gone") / "nothing").string();
  cl::Args a;
  cl::choose_folder(a, base, "own");
  CHECK(a.ask_install && a.profile == q1);
  // Not set at all: asked for.
  base.dirs[q1].clear();
  cl::Args b;
  cl::choose_folder(b, base, "own");
  CHECK(b.ask_install);
  // A game this build does not know: Shooter's Edge's own game, nothing asked.
  base.game = "no-such-game";
  cl::Args c;
  cl::choose_folder(c, base, "own");
  CHECK(!c.ask_install && c.basedir == "own");
}

TEST(a_relative_install_starts_from_the_programs_folder) {
  CHECK(cl::resolve_basedir("", "/games/se").empty());
  CHECK(fs::path(cl::resolve_basedir("quake", "/games/se")) == fs::path("/games/se/quake"));
  CHECK(fs::path(cl::resolve_basedir("../quake", "/games/se")) == fs::path("/games/quake"));
  const std::string absolute = fs::absolute("/q").string();
  CHECK(cl::resolve_basedir(absolute, "/games/se") == absolute);
  cl::BaseGame base;
  base.from = "/games/se";
  base.dirs["q1"] = "id";
  CHECK(fs::path(base.dir("q1")) == fs::path("/games/se/id"));
}

TEST(the_players_files_go_in_the_data_folder_or_with_fs_artifacts_basedir_in_the_install) {
  cl::BaseGame base;
  base.from = "/games/se";
  base.dirs["q1"] = "quake";
  // 0, the default: under the game folder played (the copy in the data folder).
  CHECK(!base.artifacts);
  CHECK(fs::path(base.own_files("q1", "/data/q1", "id1")) == fs::path("/data/q1/id1"));
  // 1: under the same game folder in the install, a relative one from the program's folder.
  base = cl::read_base_game(std::string_view("seta fs_artifacts_basedir \"1\"\n"),
                            {"+set", "fs_basedir_q1", "quake"});
  base.from = "/games/se";
  CHECK(base.artifacts);
  CHECK(fs::path(base.own_files("q1", "/data/q1", "id1")) == fs::path("/games/se/quake/id1"));
  // A game with no install set, and Shooter's Edge's own game: the data folder still.
  CHECK(fs::path(base.own_files("q2", "/data/q2", "baseq2")) == fs::path("/data/q2/baseq2"));
  CHECK(fs::path(base.own_files("", "/data/se", "id1")) == fs::path("/data/se/id1"));
  // The command line turns it off again; the saves follow the folder they are given.
  base = cl::read_base_game(std::string_view("fs_artifacts_basedir 1\n"),
                            {"+set", "fs_artifacts_basedir", "0"});
  CHECK(!base.artifacts);
  cl::SearchPath files;
  files.folder = fresh("own-game").string();
  files.own = fresh("own-files").string();
  const std::string text = "5\nthe hall\n";
  std::string error;
  CHECK_MSG(cl::write_save(files, "s1",
                           {reinterpret_cast<const uint8_t*>(text.data()), text.size()}, error),
            error);
  CHECK(fs::is_regular_file(fs::path(files.own) / "saves" / "s1.save"));
  CHECK(!fs::exists(fs::path(files.folder) / "saves"));
  std::vector<uint8_t> back;
  CHECK_MSG(cl::read_save(files, "s1", back, error), error);
  CHECK(std::string(back.begin(), back.end()) == text);
}

TEST(the_settings_are_read_from_the_config_then_the_command_line) {
  const std::string config =
      "seta fs_basegame \"q1\"\nfs_basedir_q1 \"/games/Quake\"\n"
      "set fs_basedir_q2 \"/games/Quake II\"\r\nvid_mode \"3\"\n";
  cl::BaseGame base = cl::read_base_game(std::string_view(config), {});
  CHECK(base.game == "q1" && base.dir("q1") == "/games/Quake");
  CHECK(base.dir("q2") == "/games/Quake II");  // a folder with a space kept whole
  // The command line's +set words after the config, either spelling.
  base = cl::read_base_game(std::string_view(config),
                            {"+set", "fs_basegame", "q2", "+fs_basedir_q1", "/q1"});
  CHECK(base.game == "q2" && base.dir("q1") == "/q1");
  CHECK(base.dir("q3").empty());
  // No config file: the command line alone.
  base = cl::read_base_game(std::vector<fs::path>{fresh("no-config") / "config.cfg"},
                            {"+set", "fs_basegame", "q1"});
  CHECK(base.game == "q1" && base.dir("q1").empty());
}

TEST(the_settings_are_bound_kept_and_their_change_asks_for_the_game) {
  ge::console::Console con;
  cl::BaseGame held;
  held.game = "q1";
  held.dirs["q1"] = "/games/Quake";
  cl::BaseGame bound = held;
  const cl::BaseGame chosen = cl::bind_base_game(con.cvars(), &bound);
  CHECK(chosen.game == "q1" && bound.game == "q1" && bound.dir("q1") == "/games/Quake");
  CHECK(con.cvars().find("fs_basegame") != nullptr && con.cvars().find("fs_basedir_q2") != nullptr);
  std::string seen = cl::set_base_game(con.cvars(), chosen);
  std::string why;
  CHECK(cl::base_game_asked(bound, &seen, &why).empty() && why.empty());  // nothing changed
  // The window opens Quake II from a folder: both settings kept, nothing asked after.
  cl::keep_base_game(con.cvars(), bound, "q2", "/games/Quake II", &seen);
  CHECK(bound.game == "q2" && bound.dir("q2") == "/games/Quake II");
  CHECK(cl::base_game_asked(bound, &seen, &why).empty() && why.empty());
  // Set in the console: the game named asked for from its install.
  con.cvars().set("fs_basegame", "q1", ge::console::Source::code);
  CHECK(cl::base_game_asked(bound, &seen, &why) == "/games/Quake");
  CHECK(cl::base_game_asked(bound, &seen, &why).empty() && why.empty());  // asked once
  // A game with no install, and Shooter's Edge's own: nothing opens, the reason said.
  con.cvars().set("fs_basedir_q1", "", ge::console::Source::code);
  CHECK(cl::base_game_asked(bound, &seen, &why).empty());
  CHECK_MSG(why.find("fs_basedir_q1") != std::string::npos, why);
  con.cvars().set("fs_basegame", "", ge::console::Source::code);
  CHECK(cl::base_game_asked(bound, &seen, &why).empty());
  CHECK_MSG(why.find("next start") != std::string::npos, why);
}

TEST(a_folder_with_no_game_loads_the_baseline_mod_and_no_map) {
  const cl::GameAdapter& adapter = cl::adapter_of(shooters_edge::profiles::q1_game());
  cl::SearchPath files;
  cl::GameContent c;
  std::string error;
  CHECK_MSG(cl::load_programs(adapter, files, "", c, error), error);
  CHECK(c.campaign == nullptr);
  CHECK(!c.registered);
  CHECK(c.first_map.empty());  // nothing loads: the title and its menu
  CHECK(c.start_map == ge::classicenhance::q1_profile().start_map);
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

// A game whose own rules keep the campaign (Quake II's runner) and no single-player program:
// NEW GAME and COOPERATIVE start its game with deathmatch 0, coop off or on, from its first map;
// `save` and `load` (Quake II's own saves), and `coopgame` seats its companions (1 by default).
TEST(a_game_whose_rules_keep_the_campaign_has_new_game_and_coopgame) {
  ge::console::Console con;
  cl::Listen listen;
  cl::SearchPath files;
  cl::GameContent c;
  std::string next_map, load_ask;
  c.rules_campaign = true;
  c.first_map = "demo1";
  cl::campaign_commands(con, listen, files, c, next_map, load_ask);
  for (const char* name : {"newgame", "coopgame", "save", "load", "savegame", "loadgame"})
    CHECK_MSG(con.has_command(name), name);
  con.execute("newgame");
  CHECK(next_map == "demo1" && listen.opt.new_game);
  CHECK(cl::wants_campaign(listen.opt) && listen.opt.console.back() == "coop 0");
  next_map.clear();
  listen.opt.new_game = false;
  con.execute("coopgame 2 expert demo2");
  CHECK(next_map == "demo2" && listen.opt.new_game && listen.opt.companions == 2);
  CHECK(listen.opt.companion_skill == ge::shooterbots::AimSkill::expert);
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
  const fs::path shareware = fresh("shareware");
  const fs::path full = fresh("full");
  const ge::classicenhance::Profile& classic = ge::classicenhance::q1_profile();
  write_pack(shareware / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  write_pack(full / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  write_pack(full / kBase / "pak1.pak", {{std::string(classic.full_game), {1}}});
  CHECK(cl::edition_of(shareware.string(), classic) == cl::Edition::shareware);
  CHECK(cl::edition_of(full.string(), classic) == cl::Edition::full);
  CHECK(cl::edition_of((shareware / "gone").string(), classic) == cl::Edition::unknown);
  CHECK(cl::edition_words(cl::Edition::shareware, classic) == "Shareware");
  CHECK(cl::edition_words(cl::Edition::full, classic) == "Registered");
}

TEST(the_file_words_link_over_the_game_s_files) {
  ge::console::Console con;
  cl::SearchPath files;
  std::vector<uint8_t> pack;
  std::string error;
  CHECK(ge::vfs::write_pak({{"maps/a.bsp", {1, 2}}}, &pack, &error));
  CHECK(cl::add_archive(files, "pak0.pak", std::move(pack), error));
  cl::add_path_command(con, files);
  con.execute("link maps/b.bsp maps/a.bsp");
  con.run();
  con.execute("link");
  con.run();
  bool listed = false;
  for (const std::string& line : con.output())
    listed = listed || (line.find("maps/b.bsp") != std::string::npos &&
                        line.find("maps/a.bsp") != std::string::npos);
  CHECK(listed);
  con.execute("unlink -a");
  con.run();
  con.execute("fs_restart");
  con.run();
  bool banner = false;
  for (const std::string& line : con.output()) banner = banner || line == "----- FS_Restart -----";
  CHECK(banner);
}

TEST(select_game_lists_each_mission_pack_under_its_game) {
  const fs::path install = fresh("packs-install");
  write_pack(install / kBase / "pak0.pak", {{"maps/start.bsp", {1}}});
  write_pack(install / "rogue" / "pak0.pak", {{"maps/r1m1.bsp", {1}}});
  write_pack(install / "hipnotic" / "pak0.pak", {{"maps/hip1m1.bsp", {1}}});
  fs::create_directories(install / "xatrix");  // another game's pack's name, empty
  cl::BaseGame base;
  base.dirs["q1"] = install.string();
  const auto& q1 = ge::classicenhance::q1_profile();
  CHECK(cl::packs_of(install.string(), q1).size() == 2);
  // Playing the game itself: its row in colour, a row a pack under it.
  auto rows = cl::game_rows(base, "q1", install.string(), {}, kMulti);
  CHECK_MSG(rows.size() == 3, std::to_string(rows.size()));
  if (rows.size() != 3) return;
  CHECK(rows[0].pack.empty() && rows[0].playing);
  CHECK(rows[1].pack == "hipnotic" && rows[1].pack_name == "Scourge of Armagon" &&
        !rows[1].playing && rows[1].install == install.string());
  CHECK(rows[2].pack == "rogue" && rows[2].pack_name == "Dissolution of Eternity");
  // Playing a pack: its row in colour, the game's not.
  rows = cl::game_rows(base, "q1", install.string(), {}, "rogue");
  CHECK(!rows[0].playing && !rows[1].playing && rows[2].playing);
}

// NEW GAME as each Quake II game's default.cfg names it: the pack's own first map, its movies
// first; a cfg with no `newgame` alias, or one that starts no map, names none.
TEST(new_game_takes_the_map_and_movies_the_default_cfg_names) {
  std::string map, shows;
  CHECK(cl::new_game_level("bind ESCAPE togglemenu\nalias newgame \"map *ntro.cin+base1\"\n", map,
                           shows));
  CHECK(map == "base1" && shows == "ntro.cin");
  CHECK(cl::new_game_level("  ALIAS NewGame \"map *logo.cin+xin.cin+xswamp\"\r\n", map, shows));
  CHECK(map == "xswamp" && shows == "logo.cin+xin.cin");
  CHECK(cl::new_game_level(
      "// newgame command\nalias\tnewgame \" killserver ; maxclients 1 ; deathmatch 0 ; map "
      "*rintro.cin+rmine1\"\n",
      map, shows));
  CHECK(map == "rmine1" && shows == "rintro.cin");
  CHECK(!cl::new_game_level("alias newgamex \"map e1m1\"\nbind x y\n", map, shows));
  CHECK(cl::new_game_level("alias newgamex \"map e1m1\"\nalias newgame \"map e1m2\"", map, shows));
  CHECK(map == "e1m2" && shows.empty());
  CHECK(!cl::new_game_level("alias newgame \"echo hi\"", map, shows));
  CHECK(!cl::new_game_level("alias newgame \"map end.cin+victory.pcx\"", map, shows));
}
