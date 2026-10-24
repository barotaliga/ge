// Cfg files: the startup order, the server's, the dump of every player setting.
//
// ge:summary Tests cfg files: default, config, autoexec, then +commands; server.cfg; the dump
// ge:keywords test cfg start start_server autoexec config dump_text describe
// ge:skill console testing
#include <map>

#include "check.hpp"
#include "console/cfg.hpp"

using namespace ge::console;

namespace {

void use_files(Console& c, const std::map<std::string, std::string>& files) {
  c.set_files(
      [files](std::string_view name, std::string& text) {
        const auto it = files.find(std::string(name));
        if (it == files.end()) return false;
        text = it->second;
        return true;
      },
      [](std::string_view, std::string_view) { return true; });
}

}  // namespace

TEST(startup_order) {
  Console c;
  c.cvars().add({.name = "cl_fov", .fallback = "90", .flags = kArchive});
  use_files(c, {{"default.cfg", "cl_fov 80\necho default"},
                {"config.cfg", "cl_fov 100\necho config"},
                {"autoexec.cfg", "echo autoexec $cl_fov\nalias t \"wait; echo late\"; t"}});
  start(c, {"+echo", "cmdline", "+cl_fov", "110"});
  CHECK(c.output().size() >= 4);
  CHECK(c.output()[0] == "default" && c.output()[1] == "config");
  CHECK(c.output()[2] == "autoexec 100");
  CHECK(c.cvars().find("cl_fov")->number == 110);
  CHECK(c.problems().empty());
}

TEST(missing_files_are_skipped_quietly) {
  Console c;
  use_files(c, {});
  start(c, {});
  CHECK(c.output().empty() && c.problems().empty());
}

TEST(server_reads_server_cfg) {
  Console c(true);
  c.cvars().add({.name = "sv_fps", .fallback = "77", .flags = kServer | kServerInfo});
  use_files(c, {{"server.cfg", "sv_fps 60\nserverinfo hostname \"a test\""}});
  start_server(c, {});
  CHECK(c.cvars().find("sv_fps")->number == 60);
  CHECK(c.cvars().info_string(kServerInfo) == "\\hostname\\a test\\sv_cheats\\0\\sv_fps\\60");
}

TEST(dump_lists_only_what_a_player_may_set) {
  Console c;
  c.cvars().add({.name = "cl_fov",
                 .fallback = "90",
                 .flags = kArchive,
                 .help = "field of view",
                 .min = 10,
                 .max = 140});
  c.cvars().add({.name = "sv_fps", .fallback = "77", .flags = kServer});
  c.cvars().add({.name = "net_fakelag", .fallback = "0", .flags = kCheat});
  const std::string d = dump_text(c);
  CHECK(d.find("// field of view [a number 10 to 140]\nseta cl_fov \"90\"") != std::string::npos);
  CHECK(d.find("sv_fps") == std::string::npos && d.find("net_fakelag") == std::string::npos);
}
