// The game CLI's settings: the cvar catalog written and read back, the player's config changed
// through it (clamped, refused by tier, saved as the game saves it), and a dedicated server's
// server.cfg rewritten line by line.
//
// ge:summary Tests the game CLI's settings: the catalog, config.cfg set/get/reset, server.cfg
// ge:keywords test gamecli catalog catalog_text read_catalog settings-set settings-get
// ge:keywords settings-reset server-set server-settings config-cfg json-write
// ge:skill agent-interface testing
#include <filesystem>
#include <fstream>
#include <sstream>

#include "check.hpp"
#include "console/catalog.hpp"
#include "console/cfg.hpp"
#include "core/json_read.hpp"
#include "core/json_write.hpp"
#include "gamecli/gamecli.hpp"

namespace fs = std::filesystem;
using namespace ge::console;
namespace gc = ge::gamecli;

namespace {

fs::path fresh(const char* name) {
  const fs::path p = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

std::string slurp(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

// The game's settings as a game registers them.
void game_cvars(Cvars& v) {
  v.add({.name = "sensitivity",
         .fallback = "3",
         .flags = kArchive,
         .help = "mouse speed",
         .kind = CvarKind::number,
         .min = 0.1,
         .max = 20,
         .values = "0.1..20: that much (3 the default)"});
  v.add({.name = "name",
         .fallback = "player",
         .flags = kArchive | kUserInfo,
         .help = "your name\tin the game",
         .kind = CvarKind::text});
  v.add({.name = "cl_run",
         .fallback = "1",
         .flags = kArchive,
         .help = "always run",
         .kind = CvarKind::boolean});
  v.add({.name = "sv_gravity",
         .fallback = "800",
         .flags = kServer | kServerInfo,
         .help = "the fall",
         .kind = CvarKind::integer,
         .min = 0,
         .max = 4000});
}

fs::path g_settings;  // the toy game's settings folder, set by each test

bool toy_locate(const gc::Ctx&, gc::Places& out, std::string&) {
  out.install = g_settings.parent_path().string();
  out.settings = g_settings.string();
  out.logs = (g_settings.parent_path() / "logs").string();
  out.state = (g_settings.parent_path() / "state").string();
  return true;
}

gc::Spec toy() {
  gc::Spec s;
  s.game = "toy";
  s.title = "Toy";
  s.cli = "toy_cli";
  s.locate = toy_locate;
  return s;
}

int run(const gc::Spec& s, const std::vector<std::string>& args, std::string& out) {
  return gc::run(s, args, out, ".");
}

// A settings folder holding the toy game's catalog and a config.
fs::path with_catalog(const char* name) {
  const fs::path root = fresh(name);
  g_settings = root / "settings";
  Cvars v;
  game_cvars(v);
  write(g_settings / kCatalogFile, catalog_text(v));
  write(g_settings / kConfigFile,
        "unbindall\nbind w +forward\nseta sensitivity \"5\"\nseta name \"Ann\"\n");
  return root;
}

}  // namespace

TEST(catalog_round_trip) {
  Cvars a;
  game_cvars(a);
  a.set("made", "1", Source::local, true, kArchive);  // created: left out
  const std::string text = catalog_text(a);
  CHECK(text.find("made") == std::string::npos);
  std::vector<CatalogEntry> got;
  std::string error;
  CHECK_MSG(read_catalog(text, got, error), error);
  CHECK(got.size() + 1 == a.size());  // all but the created one
  Cvars b;
  add_catalog(b, got);
  for (const char* n : {"sensitivity", "name", "cl_run", "sv_gravity"}) {
    const Cvar* x = a.find(n);
    const Cvar* y = b.find(n);
    CHECK(y != nullptr);
    if (x == nullptr || y == nullptr) continue;
    CHECK(x->fallback == y->fallback);
    CHECK(x->flags == y->flags);
    CHECK(x->kind == y->kind);
    CHECK_BITS(y->min, x->min);
    CHECK_BITS(y->max, x->max);
  }
  CHECK(b.find("name")->help == "your name in the game");  // the tab kept on its line
  CHECK(b.find("sensitivity")->values == "0.1..20: that much (3 the default)");
  // A catalog written before the values (seven fields) still reads, with none.
  CHECK_MSG(read_catalog("cl_run\tboolean\tA.......\t1\t-\t-\talways run\n", got, error), error);
  CHECK(got.size() == 1 && got[0].values.empty());
  // A damaged line is refused with its number.
  CHECK(!read_catalog("// head\nsensitivity\tnumber\n", got, error));
  CHECK(error.find("line 2") != std::string::npos);
  CHECK(!read_catalog("x\tcolour\tA.......\t1\t-\t-\thelp\n", got, error));
}

TEST(json_write_escapes) {
  CHECK(ge::core::json_str("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"");
  CHECK(ge::core::json_obj({{"k", ge::core::json_num(3)}}) == "{\"k\":3}");
}

TEST(settings_set_get_reset) {
  const fs::path root = with_catalog("ge_gamecli_settings");
  const gc::Spec s = toy();
  std::string out;
  CHECK(run(s, {"settings", "set", "sensitivity", "4.5"}, out) == 0);
  std::string cfg = slurp(g_settings / kConfigFile);
  CHECK_MSG(cfg.find("seta sensitivity \"4.5\"") != std::string::npos, cfg);
  CHECK(cfg.find("bind w \"+forward\"") != std::string::npos ||
        cfg.find("bind w +forward") != std::string::npos);
  CHECK(cfg.find("seta name \"Ann\"") != std::string::npos);  // the rest kept
  // Clamped to the range, as the game would.
  CHECK(run(s, {"settings", "set", "sensitivity", "99", "--json"}, out) == 0);
  ge::core::FlatJson j;
  std::string error;
  CHECK_MSG(ge::core::read_flat(out, j, error), error + out);
  CHECK(j["result.value"].text == "20");
  CHECK(j["result.before"].text == "4.5");
  // get: the row.
  CHECK(run(s, {"settings", "get", "sensitivity", "--json"}, out) == 0);
  CHECK(ge::core::read_flat(out, j, error));
  CHECK(j["result.value"].text == "20");
  CHECK(j["result.default"].text == "3");
  CHECK(j["result.tier"].text == "player");
  CHECK(j["result.help"].text == "mouse speed");
  // Refusals: an unknown name (the nearest named), a server's setting, a usage error.
  CHECK(run(s, {"settings", "set", "sensitivty", "2"}, out) == 1);
  CHECK(out.find("sensitivity") != std::string::npos);
  CHECK(run(s, {"settings", "set", "sv_gravity", "100", "--json"}, out) == 1);
  CHECK(ge::core::read_flat(out, j, error));
  CHECK(j["ok"].boolean == false);
  CHECK(j["next"].text.find("server set sv_gravity") != std::string::npos);
  CHECK(run(s, {"settings", "set", "sensitivity"}, out) == 2);
  // list --changed: the two moved off their defaults.
  CHECK(run(s, {"settings", "list", "--changed", "--json"}, out) == 0);
  CHECK(ge::core::read_flat(out, j, error, true));
  CHECK(j.count("result.settings.1.name") == 1 && j.count("result.settings.2.name") == 0);
  // reset: back to the default, and autoexec.cfg named when it sets it too.
  write(g_settings / kAutoexecFile, "sensitivity 7\n");
  CHECK(run(s, {"settings", "reset", "sensitivity", "--json"}, out) == 0);
  CHECK(ge::core::read_flat(out, j, error));
  CHECK(j["result.value"].text == "3");
  CHECK(j["next"].text.find("autoexec.cfg") != std::string::npos);
  fs::remove_all(root);
}

TEST(settings_without_a_catalog) {
  const fs::path root = fresh("ge_gamecli_nocatalog");
  g_settings = root / "settings";
  const gc::Spec s = toy();
  std::string out;
  // No catalog and no config: the value is kept unchecked, and the answer says so.
  CHECK(run(s, {"settings", "set", "anything", "on", "--json"}, out) == 0);
  CHECK(out.find("\"checked\":false") != std::string::npos);
  CHECK(slurp(g_settings / kConfigFile).find("seta anything \"on\"") != std::string::npos);
  CHECK(run(s, {"settings", "get", "anything"}, out) == 0);
  fs::remove_all(root);
}

namespace {

void* no_create(const ge::server::ServerConfig&, std::string&) { return nullptr; }

constexpr ge::server::GameRule kRules[] = {{"fraglimit", "frags that end a match"}};

}  // namespace

TEST(server_settings_and_set) {
  const fs::path root = with_catalog("ge_gamecli_server");
  ge::server::ServerGame game;
  game.binary = "toy_server";
  game.title = "Toy";
  game.base_dir = "base";
  game.default_map = "start";
  game.rules = kRules;
  game.rule_count = 1;
  game.create = no_create;
  gc::Spec s = toy();
  s.server_game = &game;
  write(root / "base" / kServerFile,
        "// the base\nsv_hostname \"Old\"\nset fraglimit 30\nsv_maxclients 6\n");
  std::string out;
  CHECK_MSG(run(s, {"server", "settings", "sv_host", "--json"}, out) == 0, out);
  ge::core::FlatJson j;
  std::string error;
  CHECK(ge::core::read_flat(out, j, error, true));
  CHECK(j["result.settings.0.value"].text == "Old");
  CHECK(j["result.settings.0.from"].text.find("server.cfg:2") != std::string::npos);
  // Each line rewritten in place; a new one added at the end; the rest kept.
  CHECK(run(s, {"server", "set", "sv_hostname", "Friday frags"}, out) == 0);
  CHECK(run(s, {"server", "set", "fraglimit", "50"}, out) == 0);
  CHECK(run(s, {"server", "set", "sv_motd", "hi"}, out) == 0);
  const std::string cfg = slurp(root / "base" / kServerFile);
  CHECK_MSG(cfg ==
                "// the base\nsv_hostname \"Friday frags\"\nfraglimit \"50\"\nsv_maxclients 6\n"
                "sv_motd \"hi\"\n",
            cfg);
  CHECK(run(s, {"server", "set", "sensitivity", "2"}, out) == 1);  // a player's, not the server's
  CHECK(run(s, {"server", "set", "sv_motd", "a\"b"}, out) == 2);
  fs::remove_all(root);
}

TEST(settings_check_and_format) {
  const fs::path root = with_catalog("ge_gamecli_cfg_tidy");
  gc::Spec s = toy();
  write(g_settings / kConfigFile,
        "seta sensitivity \"50\"\nsensitvity 4\nbind w +forward\ncl_run 1\n");
  std::string out;
  CHECK_MSG(run(s, {"settings", "check", "--json"}, out) == 1, out);
  ge::core::FlatJson j;
  std::string error;
  CHECK_MSG(ge::core::read_flat(out, j, error, true), error);
  CHECK(j["result.findings.0.line"].text == "1");
  CHECK(j["result.findings.0.next"].text.find("0.1..20") != std::string::npos);
  CHECK(j["result.findings.1.next"].text.find("did you mean 'sensitivity'") != std::string::npos);
  // Formatted pretty and written: the same settings, in sections, a fixed order, commented.
  CHECK_MSG(run(s, {"settings", "format", "--pretty", "--write"}, out) == 0, out);
  const std::string cfg = slurp(g_settings / kConfigFile);
  CHECK_MSG(cfg.find("cl_run           \"1\"  // Always run. (Default: 1).\n") != std::string::npos,
            cfg);  // aligned, the help beside
  CHECK(cfg.find("seta sensitivity \"50\" // Mouse speed.") != std::string::npos);
  CHECK(cfg.find("\n\n// BINDS - ") != std::string::npos);
  CHECK_MSG(run(s, {"settings", "format", "--pretty", "--json"}, out) == 0, out);
  CHECK(out.find("\"changed\":false") != std::string::npos);
  // --defaults: the player's other settings too, each at its default; only with --pretty.
  CHECK_MSG(run(s, {"settings", "format", "--pretty", "--defaults"}, out) == 0, out);
  CHECK_MSG(out.find("seta sensitivity \"50\" ") != std::string::npos, out);
  CHECK(out.size() > cfg.size());
  CHECK(run(s, {"settings", "format", "--defaults"}, out) == 2);
  fs::remove_all(root);
}
