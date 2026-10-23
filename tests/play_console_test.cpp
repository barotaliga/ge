// The play executable's cvar and cfg commands over a toy game's cvars: listing, getting, a cfg
// checked with its problems at file and line, a cfg run and what it changed, the dump.
//
// ge:summary Tests the play executable's cvar and cfg commands on a toy game
// ge:keywords test play cvar cfg check exec dump cvar_cmd cfg_cmd
// ge:skill console agent-interface testing
#include <filesystem>
#include <fstream>
#include <sstream>

#include "check.hpp"
#include "play/console_cmds.hpp"
#include "play/json_read.hpp"

namespace gp = ge::play;
namespace con = ge::console;

namespace {

void toy_cvars(con::Console& c) {
  c.cvars().add({.name = "r_glow",
                 .fallback = "0.5",
                 .flags = con::kArchive,
                 .help = "how much the glow shows",
                 .min = 0,
                 .max = 1});
  c.cvars().add({.name = "sv_speed", .fallback = "10", .flags = con::kServer | con::kServerInfo});
}

const gp::GameSpec kToy{"toy", "a toy", 60, 10, {}, {}, nullptr, {}, toy_cvars};

gp::FlatJson answer(int (*cmd)(const gp::GameSpec&, const std::vector<std::string>&,
                               const std::map<std::string, std::string>&, std::ostream&),
                    const std::vector<std::string>& words,
                    const std::map<std::string, std::string>& flags, int& code) {
  std::ostringstream out;
  code = cmd(kToy, words, flags, out);
  gp::FlatJson m;
  std::string why;
  gp::read_flat(out.str(), m, why, true);
  return m;
}

std::string cfg_file(const std::string& name, const std::string& text) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_console_test";
  std::filesystem::create_directories(dir);
  std::ofstream(dir / name) << text;
  return (dir / name).string();
}

}  // namespace

TEST(cvar_ls_and_get) {
  int code = 0;
  auto m = answer(gp::cvar_cmd, {"ls", "r_"}, {}, code);
  CHECK(code == 0 && m["count"].number == 1 && m["cvars.0.name"].text == "r_glow");
  CHECK(m["cvars.0.min"].text == "0" && m["cvars.0.flags"].text == "A.......");
  m = answer(gp::cvar_cmd, {"get", "r_glw"}, {}, code);
  CHECK(code == 1 && m["next"].text.find("r_glow") != std::string::npos);
}

TEST(cfg_check_names_file_and_line) {
  const std::string f = cfg_file("check.cfg", "r_glow 0.2\nr_glo 1\nsv_speed 20\nset r_gloww 3\n");
  int code = 0;
  auto m = answer(gp::cfg_cmd, {"check", f}, {}, code);
  CHECK(code == 1 && m["errors"].number == 2);
  CHECK(m["problems.0.file"].text == "check.cfg" && m["problems.0.line"].number == 2);
  CHECK(m["problems.1.line"].number == 3);  // a client refuses the server's cvar
  CHECK(m["problems.2.severity"].text == "warning" && m["problems.2.line"].number == 4);
  // A server's console takes sv_speed.
  m = answer(gp::cfg_cmd, {"check", f}, {{"server", ""}}, code);
  CHECK(m["errors"].number == 1);
}

TEST(cfg_exec_reports_what_changed) {
  const std::string f = cfg_file("exec.cfg", "r_glow 0.25\necho hi\n");
  int code = 0;
  auto m = answer(gp::cfg_cmd, {"exec", f}, {{"cmd", "echo $r_glow"}}, code);
  CHECK(code == 0 && m["changed.0.name"].text == "r_glow" && m["changed.0.value"].text == "0.25");
  CHECK(m["printed.0"].text == "hi" && m["printed.1"].text == "0.25");
}

TEST(cfg_dump_lists_the_players_settings) {
  int code = 0;
  auto m = answer(gp::cfg_cmd, {"dump"}, {}, code);
  CHECK(code == 0);
  CHECK(m["text"].text.find("seta r_glow \"0.5\"") != std::string::npos);
  CHECK(m["text"].text.find("sv_speed") == std::string::npos);
}
