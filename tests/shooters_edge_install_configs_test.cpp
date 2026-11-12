// A classic install's configs run in its own order (games/shooters_edge/client/install_configs.hpp)
// over an install written here, laid out as the bundle lays it out: default.cfg, then autoexec.cfg
// and the client folder's configs/config.cfg, autoexec.cfg again, the window's own saved config,
// the command line's +set pairs, then its other + commands; `exec configs/config.cfg` reaching the
// client folder; the settings this game lacks passed over and counted; and the engine's order
// (default, the window's config, autoexec) when the install has no main config.
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
