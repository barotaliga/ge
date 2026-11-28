// A classic install's configs run in its own order (games/shooters_edge/client/install_configs.hpp)
// over an install written here, laid out as the bundle lays it out: default.cfg, then autoexec.cfg
// and the client folder's configs/config.cfg, autoexec.cfg again, the window's own saved config,
// the command line's +set pairs, then its other + commands; `exec configs/config.cfg` reaching the
// client folder; the settings this game lacks passed over and counted; and the engine's order
// (default, the window's config, autoexec) when the install has no main config. Quake II's:
// its config.cfg (q2config.cfg on Windows), autoexec.cfg of the base folder then the mod's,
// postexec.cfg, the window's config, the command line, then postinit.cfg.
//
// ge:summary Tests a classic install's configs run in its order, unknown settings counted
// ge:keywords test shooters-edge install-configs start_configs config_dirs autoexec config-order
// ge:skill shooter console testing
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "shooter/controls.hpp"
#include "shooters_edge/client/install_configs.hpp"
#include "shooters_edge/client/settings.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f << text;
}

const ge::classicenhance::Profile& first() { return ge::classicenhance::kProfiles[0]; }
const std::string kGame(first().multiplayer_dir);
const std::string kClient = std::string("ezqu") + "ake";

// A console whose `mark <word>` keeps the words in order.
struct Marks {
  ge::console::Console con;
  std::vector<std::string> seen;
  Marks() {
    con.add_command("mark", "", [this](ge::console::Console&, const ge::console::Args& a) {
      seen.push_back(a.size() > 1 ? a[1] : "");
    });
  }
};

std::string joined(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& w : v) s += (s.empty() ? "" : ",") + w;
  return s;
}

}  // namespace

TEST(an_install_s_configs_run_in_its_bundled_client_s_order) {
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs";
  fs::remove_all(root);
  write(root / "id1" / "pak0.pak", "");
  write(root / kGame / "default.cfg", "mark default\n");
  write(root / kGame / "autoexec.cfg", "mark autoexec\n");
  write(root / kClient / "configs" / "config.cfg", "mark main\nhud_a_setting_we_lack 1\n");
  write(cl::settings_dir(root.string(), kGame) / "config.cfg", "mark own\n");
  Marks m;
  m.con.set_search_path(cl::config_dirs(root.string(), kGame, first()));
  const cl::ConfigsRun ran = cl::start_configs(m.con, root.string(), kGame, first(),
                                               {"+mark", "plus", "+set", "seen_set", "1"});
  CHECK_MSG(joined(m.seen) == "default,autoexec,main,autoexec,own,plus", joined(m.seen));
  CHECK(ran.passed_over == 1);
  CHECK(ran.files.size() == 5);
  // The install's own autoexec reaches the client folder's configs through `exec`.
  write(root / kGame / "autoexec.cfg", "exec configs/preset.cfg\n");
  write(root / kClient / "configs" / "preset.cfg", "mark preset\n");
  Marks again;
  again.con.set_search_path(cl::config_dirs(root.string(), kGame, first()));
  cl::start_configs(again.con, root.string(), kGame, first(), {});
  CHECK_MSG(joined(again.seen) == "default,preset,main,preset,own", joined(again.seen));
}

TEST(q2_s_install_runs_its_client_s_order) {
  const ge::classicenhance::Profile& two = ge::classicenhance::kProfiles[1];
  const std::string base(two.base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_two";
  fs::remove_all(root);
  write(root / base / "pak0.pak", "");
#ifdef _WIN32
  write(root / base / "q2config.cfg", "mark main\n");
  write(root / base / "config.cfg", "mark other\n");
#else
  write(root / base / "config.cfg", "mark main\n");
  write(root / base / "q2config.cfg", "mark other\n");
#endif
  write(root / base / "autoexec.cfg", "mark autoexec\n");
  write(root / base / "postexec.cfg", "mark postexec\n");
  write(root / base / "postinit.cfg", "mark postinit\n");
  write(root / "ctf" / "autoexec.cfg", "mark mod\n");
  write(cl::settings_dir(root.string(), base) / "config.cfg", "mark own\n");
  Marks m;
  m.con.set_search_path(cl::config_dirs(root.string(), base, two));
  cl::start_configs(m.con, root.string(), base, two, {"+mark", "plus"});
  CHECK_MSG(joined(m.seen) == "main,autoexec,postexec,own,plus,postinit", joined(m.seen));
  // A mod: its main config before the base folder's, the base folder's autoexec.cfg then its own.
  write(root / "ctf" / "config.cfg", "mark modmain\n");
  Marks mod;
  mod.con.set_search_path(cl::config_dirs(root.string(), "ctf", two));
  cl::start_configs(mod.con, root.string(), "ctf", two, {});
  CHECK_MSG(joined(mod.seen) == "modmain,autoexec,mod,postexec,postinit", joined(mod.seen));
}

TEST(a_game_change_runs_quake_ii_s_autoexecs_after_its_settings_when_fs_autoexec_is_on) {
  const ge::classicenhance::Profile& two = ge::classicenhance::kProfiles[1];
  const std::string base(two.base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_change";
  fs::remove_all(root);
  write(root / base / "pak0.pak", "");
  write(root / base / "default.cfg", "mark default\n");
  write(root / base / "autoexec.cfg", "mark autoexec\n");
  write(root / base / "postexec.cfg", "mark postexec\n");
  write(root / "ctf" / "autoexec.cfg", "mark mod\n");
  write(cl::settings_dir(root.string(), "ctf") / "config.cfg", "mark own\n");
  // A game without `fs_autoexec` (Quake's): its default.cfg and its own settings.
  Marks q1;
  q1.con.set_search_path(cl::config_dirs(root.string(), "ctf", two));
  CHECK(cl::run_game_settings(q1.con, root.string(), "ctf", two));
  CHECK_MSG(joined(q1.seen) == "default,own", joined(q1.seen));
  // Quake II's, on: its settings, the base folder's autoexec then the mod's, postexec.
  Marks on;
  on.con.cvars().add({.name = "fs_autoexec", .fallback = "1"});
  on.con.set_search_path(cl::config_dirs(root.string(), "ctf", two));
  cl::run_game_settings(on.con, root.string(), "ctf", two);
  CHECK_MSG(joined(on.seen) == "default,own,autoexec,mod,postexec", joined(on.seen));
  // Off: its settings alone again.
  Marks off;
  off.con.cvars().add({.name = "fs_autoexec", .fallback = "0"});
  off.con.set_search_path(cl::config_dirs(root.string(), "ctf", two));
  cl::run_game_settings(off.con, root.string(), "ctf", two);
  CHECK_MSG(joined(off.seen) == "default,own", joined(off.seen));
  // fs_default 0: no default.cfg, on a game change and at start-up.
  Marks bare;
  bare.con.cvars().add({.name = "fs_default", .fallback = "0"});
  bare.con.set_search_path(cl::config_dirs(root.string(), "ctf", two));
  cl::run_game_settings(bare.con, root.string(), "ctf", two);
  CHECK_MSG(joined(bare.seen) == "own", joined(bare.seen));
}

TEST(fs_default_on_the_command_line_keeps_default_cfg_from_the_start) {
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_nodefault";
  fs::remove_all(root);
  write(root / "id1" / "pak0.pak", "");
  write(root / kGame / "default.cfg", "mark default\n");
  write(root / kGame / "autoexec.cfg", "mark autoexec\n");
  write(root / kClient / "configs" / "config.cfg", "mark main\n");
  Marks m;
  m.con.cvars().add({.name = "fs_default", .fallback = "1"});
  m.con.set_search_path(cl::config_dirs(root.string(), kGame, first()));
  cl::start_configs(m.con, root.string(), kGame, first(), {"+set", "fs_default", "0"});
  CHECK_MSG(joined(m.seen) == "autoexec,main,autoexec", joined(m.seen));
}

TEST(q2_s_default_cfg_comes_from_its_pack_keeping_the_console_keys) {
  const ge::classicenhance::Profile& two = ge::classicenhance::kProfiles[1];
  const std::string base(two.base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_pack";
  fs::remove_all(root);
  std::vector<uint8_t> pack;
  std::string error;
  const std::string defaults = "unbindall\nmark packdefault\n";
  CHECK_MSG(
      ge::vfs::write_pak({{"default.cfg", {defaults.begin(), defaults.end()}}}, &pack, &error),
      error);
  fs::create_directories(root / base);
  std::ofstream(root / base / "pak0.pak", std::ios::binary)
      .write(reinterpret_cast<const char*>(pack.data()), std::streamsize(pack.size()));
  Marks m;
  m.con.binds().set(ge::console::BindMode::game, "nonus", "toggleconsole");
  m.con.binds().set(ge::console::BindMode::game, "x", "mark x");
  m.con.set_search_path(cl::config_dirs(root.string(), base, two));
  const cl::ConfigsRun ran = cl::start_configs(m.con, root.string(), base, two, {});
  CHECK_MSG(joined(m.seen) == "packdefault", joined(m.seen));
  CHECK(!ran.files.empty() && ran.files[0].find("pak0.pak") != std::string::npos);
  CHECK(m.con.binds().get(ge::console::BindMode::game, "nonus") == "toggleconsole");  // kept
  CHECK(m.con.binds().get(ge::console::BindMode::game, "x").empty());  // the pack's unbindall
}

// A pack's default.cfg that moves on the arrows (A looks up, the right button sidesteps) leaves
// the window's move keys and its lookspring as they were; its other keys and its mouse speed stand.
TEST(q2_s_pack_defaults_keep_the_window_s_move_keys) {
  const ge::classicenhance::Profile& two = ge::classicenhance::kProfiles[1];
  const std::string base(two.base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_moves";
  fs::remove_all(root);
  std::vector<uint8_t> pack;
  std::string error;
  const std::string defaults =
      "unbindall\nbind a +lookup\nbind mouse2 +strafe\nbind up +forward\nset sensitivity 3\n"
      "set lookspring 1\n";
  CHECK_MSG(
      ge::vfs::write_pak({{"default.cfg", {defaults.begin(), defaults.end()}}}, &pack, &error),
      error);
  fs::create_directories(root / base);
  std::ofstream(root / base / "pak0.pak", std::ios::binary)
      .write(reinterpret_cast<const char*>(pack.data()), std::streamsize(pack.size()));
  ge::console::Console con;
  ge::shooter::Controls c;
  ge::shooter::add_control_cvars(con, c);
  con.binds().set(ge::console::BindMode::game, "a", "+moveleft");
  con.binds().set(ge::console::BindMode::game, "mouse2", "+jump");
  con.set_search_path(cl::config_dirs(root.string(), base, two));
  (void)cl::start_configs(con, root.string(), base, two, {});
  CHECK(con.binds().get(ge::console::BindMode::game, "a") == "+moveleft");
  CHECK(con.binds().get(ge::console::BindMode::game, "mouse2") == "+jump");
  CHECK(con.binds().get(ge::console::BindMode::game, "up") == "+forward");
  CHECK(!c.lookspring);  // the window's own default kept over the pack's lookspring 1
  // Its mouse speed is a classic config's number: 3 times 0.022 degrees a count.
  CHECK(std::fabs(ge::shooter::yaw_per_count(c) - 0.066) < 1e-9);
  ge::input::InputState in;
  in.mouse.dx = 10;
  ge::shooter::Vec3f view{};
  const ge::shooter::Moves m = ge::shooter::button_moves(c);
  ge::shooter::turn_view(in, m, c, 0.01, view);
  CHECK(!m.strafe && std::fabs(view.y - (360 - 0.66)) < 1e-3);
  CHECK(ge::shooter::read_controls(m, in, c).side == 0);
}

TEST(with_no_main_config_the_engine_s_order_runs) {
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_bare";
  fs::remove_all(root);
  write(root / "id1" / "pak0.pak", "");
  write(root / "id1" / "autoexec.cfg", "mark autoexec\n");
  write(cl::settings_dir(root.string(), "id1") / "config.cfg", "mark own\nno_such_setting 2\n");
  Marks m;
  m.con.set_search_path(cl::config_dirs(root.string(), "id1", first()));
  const cl::ConfigsRun ran = cl::start_configs(m.con, root.string(), "id1", first(), {});
  CHECK_MSG(joined(m.seen) == "own,autoexec", joined(m.seen));
  CHECK(ran.passed_over == 1);
}

TEST(the_host_s_own_words_come_back_after_the_configs_run) {
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_host";
  fs::remove_all(root);
  write(root / "id1" / "pak0.pak", "");
  write(root / kGame / "autoexec.cfg", "a_word_nobody_knows\n");
  Marks m;
  int host = 0;
  m.con.set_unknown([&host](const ge::console::Args&) { return ++host > 0; });
  m.con.set_search_path(cl::config_dirs(root.string(), kGame, first()));
  const cl::ConfigsRun ran = cl::start_configs(m.con, root.string(), kGame, first(), {});
  CHECK(ran.passed_over == 1 && host == 0);  // counted while the files ran
  m.con.execute("another_word");
  CHECK(host == 1);  // the host's handler is back
}

namespace {

void write_pack(const fs::path& file,
                const std::vector<std::pair<std::string, std::string>>& files) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> in;
  in.reserve(files.size());
  for (const auto& [name, text] : files)
    in.emplace_back(name, std::vector<uint8_t>(text.begin(), text.end()));
  std::vector<uint8_t> pack;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(in, &pack, &error), error);
  fs::create_directories(file.parent_path());
  std::ofstream(file, std::ios::binary)
      .write(reinterpret_cast<const char*>(pack.data()), std::streamsize(pack.size()));
}

}  // namespace

// A Quake mission pack's own default.cfg, inside its pack, runs as what it adds to the base game's
// (its keys for the new weapons, its aliases); the lines both share do not run again, and the
// window's own layout stands.
TEST(a_quake_mod_s_pack_default_cfg_runs_what_it_adds_to_the_base_s) {
  const std::string base(first().base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_mod";
  fs::remove_all(root);
  write_pack(root / base / "pak0.pak",
             {{"default.cfg", "unbindall\nmark base\nbind 0 \"impulse 0\"\n"}});
  write_pack(root / "hipnotic" / "pak0.pak",
             {{"default.cfg",
               "unbindall\nmark  base\nbind 9 \"impulse 225\"\nbind 0 \"impulse "
               "226\"\nalias zoom \"fov 10\"\n"}});
  Marks m;
  m.con.binds().set(ge::console::BindMode::game, "w", "+forward");
  m.con.set_search_path(cl::config_dirs(root.string(), "hipnotic", first()));
  cl::start_configs(m.con, root.string(), "hipnotic", first(), {});
  CHECK_MSG(m.seen.empty(), joined(m.seen));  // the shared lines did not run
  CHECK(m.con.binds().get(ge::console::BindMode::game, "9") == "impulse 225");
  CHECK(m.con.binds().get(ge::console::BindMode::game, "0") == "impulse 226");
  CHECK(m.con.binds().get(ge::console::BindMode::game, "w") == "+forward");  // no unbindall
  // The base folder played: nothing of the mod's.
  Marks b;
  b.con.set_search_path(cl::config_dirs(root.string(), base, first()));
  cl::start_configs(b.con, root.string(), base, first(), {});
  CHECK(b.con.binds().get(ge::console::BindMode::game, "9").empty());
  fs::remove_all(root);
}

// A mission pack's own quake.rc names the recordings its title plays; the base folder's list is
// the game's own.
TEST(a_mod_s_quake_rc_names_the_title_s_demos) {
  const std::string base(first().base_dir);
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_demos";
  fs::remove_all(root);
  write_pack(root / base / "pak0.pak", {{"quake.rc", "startdemos demo1 demo2 demo3\n"}});
  write_pack(root / "hipnotic" / "pak0.pak",
             {{"quake.rc",
               "// Included\nteamplay 0?Hipnotic\nexec default.cfg\n"
               "startdemos hipdemo1 hipdemo2 hipdemo3 hipdemo4\n"}});
  fs::create_directories(root / "plain");
  CHECK_MSG(cl::mod_startdemos(root.string(), "hipnotic", first()) ==
                "hipdemo1 hipdemo2 hipdemo3 hipdemo4",
            cl::mod_startdemos(root.string(), "hipnotic", first()));
  CHECK(cl::mod_startdemos(root.string(), base, first()).empty());
  CHECK(cl::mod_startdemos(root.string(), "plain", first()).empty());
  write(root / "plain" / "quake.rc", "startdemos own1\n");
  CHECK(cl::mod_startdemos(root.string(), "plain", first()) == "own1");
  fs::remove_all(root);
}

// A Quake II mission pack names its title's recordings in its own default.cfg's demo aliases,
// the cinematics between them skipped.
TEST(a_quake_ii_pack_s_default_cfg_names_the_title_s_demos) {
  const ge::classicenhance::Profile& q2 = *ge::classicenhance::find_profile("q2");
  const fs::path root = fs::temp_directory_path() / "ge_se_install_configs_q2_demos";
  fs::remove_all(root);
  write_pack(root / std::string(q2.base_dir) / "pak0.pak", {{"default.cfg", "bind w +forward\n"}});
  write_pack(root / "xatrix" / "pak0.pak",
             {{"default.cfg",
               "//\tDEMO STUFF\r\nalias   d1      \"demomap idlog.cin ; set nextserver d2\"\r\n"
               "alias   d2      \"demomap xdemo1.dm2 ; set nextserver d3\"\r\n"
               "alias\td4\t\"demomap xdemo2.dm2 ; set nextserver d1\"\r\n"}});
  CHECK_MSG(cl::mod_startdemos(root.string(), "xatrix", q2, "q2demo1") == "xdemo1 xdemo2",
            cl::mod_startdemos(root.string(), "xatrix", q2, "q2demo1"));
  fs::create_directories(root / "plain");
  CHECK(cl::mod_startdemos(root.string(), "plain", q2, "q2demo1") == "q2demo1");
  fs::remove_all(root);
}
