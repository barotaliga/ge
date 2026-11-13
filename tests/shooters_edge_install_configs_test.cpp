// A classic install's configs run in its own order (games/shooters_edge/client/install_configs.hpp)
// over an install written here, laid out as the bundle lays it out: default.cfg, then autoexec.cfg
// and the client folder's configs/config.cfg, autoexec.cfg again, the window's own saved config,
// the command line's +set pairs, then its other + commands; `exec configs/config.cfg` reaching the
// client folder; the settings this game lacks passed over and counted; and the engine's order
// (default, the window's config, autoexec) when the install has no main config. The sequel's:
// its config.cfg (q2config.cfg on Windows), autoexec.cfg of the base folder then the mod's,
// postexec.cfg, the window's config, the command line, then postinit.cfg.
//
// ge:summary Tests a classic install's configs run in its order, unknown settings counted
// ge:keywords test shooters-edge install-configs start_configs config_dirs autoexec config-order
// ge:skill shooter console testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
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

TEST(the_sequel_s_install_runs_its_client_s_order) {
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

TEST(the_sequel_s_default_cfg_comes_from_its_pack_keeping_the_console_keys) {
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
