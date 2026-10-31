// engine/server/config: a server's settings in layers. On one fixture (a base game, two mods, a
// profile in two of them, the command line, the server's console): a mod's server.cfg overrides
// only what it sets, the higher mod over the lower, the profile's winning copy over both, the
// command line over all, the console last; each value's file and line kept, and what it
// overrode; a mistake named at its file and line; a missing profile refused with where it looked.
//
// ge:summary Test: server.cfg in layers: base, mods, a profile, the command line, --explain
// ge:keywords test server-cfg layers base mod profile command-line explain history
// ge:skill net testing
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "server/config.hpp"
#include "server/main.hpp"
#include "server/sample.hpp"

using namespace ge::server;

namespace {

using Files = std::map<std::string, std::string>;

const Files& fixture() {
  static const Files f = {
      {"base/server.cfg",
       "// the base game's\n"
       "sv_hostname \"Edge Base\"\n"
       "sv_maxclients 16\n"
       "sv_description \"the base game\"\n"
       "sv_motd \"welcome\"\n"},
      {"base/duel.cfg", "sv_maxclients 4\n"},
      {"arena/server.cfg", "sv_hostname \"Arena Night\"\nsv_map dm3\n"},
      {"arena/duel.cfg", "sv_maxclients 2\nsv_description \"one on one\"\n"},
      {"ctf/server.cfg", "sv_maxclients 16\nsv_maprotation \"ctf1 ctf2\"\n"},
  };
  return f;
}

void serve(ge::console::Console& con, const Files& files) {
  con.set_files(
      [&files](std::string_view name, std::string& out) {
        const auto it = files.find(std::string(name));
        if (it == files.end()) return false;
        out = it->second;
        return true;
      },
      [](std::string_view, std::string_view) { return false; });
}

// The `explain` line of `name` (its leading words up to the end of the line), or "".
std::string line_of(const std::string& text, const std::string& name) {
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find('\n', at);
    const std::string row = text.substr(at, end - at);
    if (row.compare(0, name.size() + 1, name + " ") == 0) return row;
    if (end == std::string::npos) break;
    at = end + 1;
  }
  return "";
}

// The row with its runs of spaces closed to one, so the test does not pin the column widths.
std::string squeezed(const std::string& row) {
  std::string out;
  for (char ch : row)
    if (ch != ' ' || out.empty() || out.back() != ' ') out += ch;
  return out;
}

struct Run {
  ge::console::Console con{true};
  ServerConfig config;
  ServerCfg cfg;
  std::string error;
  bool ok = false;
};

void configure(Run& r, const std::vector<std::string>& args, const Files& files = fixture()) {
  serve(r.con, files);
  r.ok = server_configure(r.con, args, r.config, r.cfg, sample_game(), r.error);
}

}  // namespace

TEST(server_cfg_layers_base_mods_profile_command_line) {
  Run r;
  configure(r, {"--mod=arena,ctf", "--cfg=duel", "--name=Friday"});
  CHECK(r.ok);
  CHECK(r.error.empty());
  CHECK(r.cfg.read == (std::vector<std::string>{"base/server.cfg", "arena/server.cfg",
                                                "ctf/server.cfg", "arena/duel.cfg"}));
  CHECK(r.config.hostname == "Friday");         // the command line over arena's
  CHECK(r.config.maxclients == 2);              // arena's profile over ctf and the base
  CHECK(r.config.description == "one on one");  // the profile over the base
  CHECK(r.config.motd == "welcome");            // only the base set it
  CHECK(r.config.map == "dm3");                 // the lower mod, untouched by the higher
  CHECK(r.config.maprotation == "ctf1 ctf2");   // the higher mod alone
  CHECK(r.config.mod == "arena,ctf");           // what it was started with
  CHECK(r.config.visibility == "lan");          // nobody set it
}

TEST(server_cfg_mod_overrides_only_what_it_sets) {
  Run r;
  configure(r, {"--mod=arena"});
  CHECK(r.ok);
  CHECK(r.config.hostname == "Arena Night");
  CHECK(r.config.maxclients == 16);
  CHECK(r.config.description == "the base game");
  CHECK(r.config.map == "dm3");
  Run base;
  configure(base, {});
  CHECK(base.config.hostname == "Edge Base");
  CHECK(base.config.map == "ring");  // the game's default
}

TEST(server_cfg_explain_names_file_line_and_what_it_overrode) {
  Run r;
  configure(r, {"--mod=arena,ctf", "--cfg=duel", "--name=Friday"});
  CHECK(r.ok);
  r.con.execute("sv_motd \"tonight: duels\"");
  const std::string text = explain(r.con, r.cfg);
  CHECK(squeezed(line_of(text, "sv_hostname")) ==
        "sv_hostname \"Friday\" command line (over arena/server.cfg:1 \"Arena Night\")");
  CHECK(squeezed(line_of(text, "sv_maxclients")) ==
        "sv_maxclients \"2\" arena/duel.cfg:1 (over ctf/server.cfg:1 \"16\")");
  CHECK(
      squeezed(line_of(text, "sv_description")) ==
      "sv_description \"one on one\" arena/duel.cfg:2 (over base/server.cfg:4 \"the base game\")");
  CHECK(squeezed(line_of(text, "sv_motd")) ==
        "sv_motd \"tonight: duels\" console (over base/server.cfg:5 \"welcome\")");
  CHECK(squeezed(line_of(text, "sv_map")) ==
        "sv_map \"dm3\" arena/server.cfg:2 (over the fallback \"ring\")");
  CHECK(squeezed(line_of(text, "sv_visibility")) == "sv_visibility \"lan\" fallback");
  CHECK(squeezed(line_of(text, "sv_mod")) == "sv_mod \"arena,ctf\" --mod (over the fallback \"\")");
  // The same value set again still moves the owner: ctf owns 16 over the base's 16.
  const std::vector<ValueSource>& h = r.cfg.history.at("sv_maxclients");
  CHECK(h.size() == 3);
  CHECK(h[0].layer == Layer::base && h[1].layer == Layer::mod && h[2].layer == Layer::profile);
  CHECK(h[1].file == "ctf/server.cfg" && h[1].value == "16");
  // A prefix narrows it.
  const std::string net = explain(r.con, r.cfg, "net_");
  CHECK(!line_of(net, "net_port").empty());
  CHECK(line_of(net, "sv_hostname").empty());
}

TEST(server_cfg_refusals_name_where_and_next) {
  {
    Files f = fixture();
    f["ctf/server.cfg"] = "sv_maxclients 16\nsv_hostnam \"typo\"\n";
    Run r;
    configure(r, {"--mod=arena,ctf"}, f);
    CHECK(!r.ok);
    CHECK(r.error.find("ctf/server.cfg:2") != std::string::npos);
  }
  {
    Run r;
    configure(r, {"--mod=ctf", "--cfg=sniper"});
    CHECK(!r.ok);
    CHECK(r.error.find("profile 'sniper': no sniper.cfg in ctf, base") != std::string::npos);
    CHECK(r.error.find("next:") != std::string::npos);
  }
  {
    // sv_mod is the command line's to say: a cfg cannot change which cfgs were read.
    Files f = fixture();
    f["base/server.cfg"] = "sv_mod other\n";
    Run r;
    configure(r, {}, f);
    CHECK(!r.ok);
    CHECK(r.error.find("base/server.cfg:1") != std::string::npos);
  }
}

TEST(server_cfg_missing_files_are_skipped) {
  Run r;
  configure(r, {"--mod=nothing_here"}, Files{});
  CHECK(r.ok);
  CHECK(r.cfg.read.empty());
  CHECK(r.config.hostname == "server");
  CHECK(squeezed(line_of(explain(r.con, r.cfg), "sv_hostname")) ==
        "sv_hostname \"server\" fallback");
}
