// Classic remix on a dedicated server (M18u13c): `sesv` reads the cr_ settings as its rules and
// the other game's install from its fs_basedir_<id>, plays the other game's map on its own rules
// wearing the other game's things, and tells every client the remix in its server info.
//
// ge:summary Tests classic remix on a dedicated server: cr_ rules, fs_basedir_<id>, the server info
// ge:keywords test remix dedicated-server sesv fs_basedir fs_basegame remix_rules open_server_remix
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "classicenhance/profile.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "server/main.hpp"
#include "server/server.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/remix_dedicated.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace se = shooters_edge::net;
namespace br = ge::brush;
namespace ce = ge::classicenhance;
namespace fs = std::filesystem;
namespace gs = ge::server;
namespace gn = ge::net;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

void write_pack(const fs::path& p, const Files& files) {
  std::vector<uint8_t> b;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &b, &error), error);
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

// A floor with a start on it, in the family of brushes.
std::vector<uint8_t> brush_hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  world.models[0].push_back(br::box_brush(lo, hi, ge::shooter::kSqSolid, "world/floor"));
  br::Entity start;
  start.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}};
  world.entities.push_back(start);
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

// The classic family's sample map.
std::vector<uint8_t> hull_halls() {
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bsp, &error), error);
  return bsp;
}

// Two full installs side by side: Quake's (`halls` its own map) and Quake II's (`q2hall`).
struct Installs {
  fs::path root = fs::temp_directory_path() / "ge_se_remix_dedicated";
  fs::path q1 = root / "quake", q2 = root / "quake2";
  Installs() {
    fs::remove_all(root);
    const ce::Profile& one = ce::q1_profile();
    const ce::Profile* two = ce::find_profile("q2");
    CHECK(two != nullptr);
    write_pack(q1 / one.base_dir / "pak0.pak", {{"maps/halls.bsp", hull_halls()}});
    write_pack(q1 / one.base_dir / "pak1.pak", {{std::string(one.full_game), {1}}});
    if (two == nullptr) return;
    write_pack(q2 / two->base_dir / "pak0.pak",
               {{"maps/q2hall.bsp", brush_hall()}, {std::string(two->full_game), brush_hall()}});
  }
  ~Installs() { fs::remove_all(root); }
};

}  // namespace

// Every cr_ setting a server takes explains itself, the player's own cr_menu left out, with the
// game served and each game's install; the window's defaults hold for a rule left unset.
TEST(the_server_takes_every_cr_setting_but_the_menu) {
  bool basegame = false, basedir = false, items = false, menu = false;
  for (const gs::GameRule& r : cl::remix_rules()) {
    CHECK_MSG(r.help != nullptr && r.values != nullptr, r.name);
    basegame = basegame || std::string(r.name) == "fs_basegame";
    basedir = basedir || std::string(r.name) == "fs_basedir_q2";
    items = items || std::string(r.name) == "cr_items";
    menu = menu || std::string(r.name) == "cr_menu";
  }
  CHECK(basegame && basedir && items && !menu);
  CHECK(cl::is_remix_rule("cr_physics") && !cl::is_remix_rule("fraglimit"));
  const cl::RemixSettings s = cl::remix_of_rules({{"cr_enabled", "1"}, {"cr_items", "q2"}});
  CHECK(s.enabled && s.items == "q2" && s.expansions && s.balance.strong_min == 1);
  CHECK(!cl::remix_of_rules({}).enabled);
}

// Off, with the reason in the game's words, until the other game's fs_basedir_<id> names its full
// game; the served game's own setting is not the other's.
TEST(the_server_s_remix_says_why_it_stays_off) {
  const Installs in;
  cl::RemixFiles r;
  std::map<std::string, std::string> rules = {{"cr_enabled", "1"}};
  const std::string q1 = in.q1.string(), base(ce::q1_profile().base_dir);
  CHECK(cl::open_server_remix(r, {}, "q1", q1, base, false).empty());
  std::string line = cl::open_server_remix(r, rules, "q1", q1, base, false);
  CHECK_MSG(!r.state.on && line.find("fs_basedir") != std::string::npos, line);
  rules["fs_basedir_q1"] = q1;  // the served game's own install: still no other game
  line = cl::open_server_remix(r, rules, "q1", q1, base, false);
  CHECK_MSG(!r.state.on && line.find("other classic game") != std::string::npos, line);
  rules["fs_basedir_q2"] = in.q1.string();  // not Quake II
  line = cl::open_server_remix(r, rules, "q1", q1, base, false);
  CHECK_MSG(!r.state.on && line.find("fs_basedir_q2") != std::string::npos, line);
  rules["fs_basedir_q2"] = in.q2.string();
  rules["cr_items"] = "q2";
  line = cl::open_server_remix(r, rules, "q1", q1, base, false);
  CHECK_MSG(r.state.on && r.state.other == "q2" && r.worn != nullptr, line);
  const std::vector<std::string> lines = cl::server_remix_lines(r);
  bool told = false;
  for (const std::string& l : lines) told = told || l == "serverinfo cr_items q2";
  CHECK(told);
}

// QuakeWorld's server plays Quake II's map on Quake's rules with Quake II's weapons, and a client
// joining reads the remix from the server info.
TEST(sesv_plays_the_other_game_s_map_and_tells_its_remix) {
  const Installs in;
  gs::ServerConfig c;
  c.hostname = "Remix Night";
  c.map = "q2hall";
  c.maxclients = 8;
  c.seed = 1;
  c.root = in.q1.string();
  c.rules = {{"cr_enabled", "1"}, {"cr_items", "q2"}, {"fs_basedir_q2", in.q2.string()}};
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(c.seed, lan);
  gn::Transport server_port, client_port;
  gn::open_sim_transport(server_port, net, gn::sim_address(0, 27500));
  gn::open_sim_transport(client_port, net, gn::sim_address(1, 27001));
  gs::Server s;
  std::string error;
  CHECK_MSG(gs::server_open(s, se::qw_game(), c, &server_port, nullptr, error), error);
  if (s.state == nullptr) return;
  CHECK_MSG(gs::server_status(s).find("map=q2hall") != std::string::npos, gs::server_status(s));
  std::vector<std::string> said;
  s.game->console_lines(s.state, said);
  bool on = false;
  for (const std::string& l : said) on = on || l.find("Classic remix is on") != std::string::npos;
  CHECK(on);
  se::QwClientEnd client;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  se::client_start(client, client_port, gn::sim_address(0, 27500), cc, 0);
  for (int f = 0; f < 400 && client.serverinfo.empty(); f++) {
    gn::sim_advance(net, s.now_us);
    se::client_tick(client, s.now_us);
    gs::server_frame(s);
  }
  CHECK_MSG(client.serverinfo.find("\\cr_enabled\\1") != std::string::npos, client.serverinfo);
  CHECK_MSG(client.serverinfo.find("\\cr_items\\q2") != std::string::npos, client.serverinfo);
  gs::server_close(s);
}

// Quake II's server plays Quake's map on Quake II's rules, its cells made brushes; the remix is
// told, and without it the map is none of Quake II's.
TEST(sesv_q2_plays_quake_s_map_on_quake_ii_s_rules) {
  const Installs in;
  gs::ServerConfig c;
  c.map = "halls";
  c.maxclients = 4;
  c.seed = 1;
  c.root = in.q2.string();
  gs::Server off;
  std::string error;
  CHECK(!gs::server_open(off, se::q2_game(), c, nullptr, nullptr, error));
  c.rules = {{"cr_enabled", "1"}, {"cr_items", "q1"}, {"fs_basedir_q1", in.q1.string()}};
  gs::Server s;
  error.clear();
  CHECK_MSG(gs::server_open(s, se::q2_game(), c, nullptr, nullptr, error), error);
  if (s.state == nullptr) return;
  gn::Status st;
  s.game->fill_status(s.state, st);
  CHECK_MSG(st.map == "halls", st.map);
  for (int f = 0; f < 20; f++) gs::server_frame(s);
  gs::server_close(s);
}
