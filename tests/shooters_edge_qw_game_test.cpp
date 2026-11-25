// games/shooters_edge/net/qw_game: Shooter's Edge's playable dedicated server on the engine's
// server core. Offline and on the network with nobody connected it is one digest; a classic client
// over the simulated network connects to the core's server, follows the signon and plays, and the
// core counts it seated; the settings reach the endpoint (the name in its status, the password at
// the connect); too many bots are refused with the next step; a whole session replays to the bit;
// the mod's own rules come in as settings (`+set fraglimit 20`) and reach the mod; a map read from
// a pack; a mod folder's own program (from a pack of any name) runs over the base game, its
// name in serverdata and the serverinfo's `*gamedir`, and a mod folder with no program is refused;
// the mod's changelevel takes the players to the next map (the rotation's when it has one); bots
// are players every client sees, with their teams; the lobby warms up (its limits held, a bot
// alone never counts down) until the client is ready, then restarts the map into the match; the
// window's own match (the listen server) is the same server, and a classic client joins it; on
// it the window's player hears the visitor talk and is heard by him while talking; a player's vote
// changes a rule for the map (and the next map puts it back), a vote on the move restarts the map
// and is kept, and the owner's settings are refused.
//
// ge:summary Test: Shooter's Edge's dedicated server on the core: one digest, a client plays on it
// ge:keywords test classic-game dedicated-server classic-client simulated-network serve net_frame
// ge:keywords game-rules fraglimit pack-map changelevel map-change maprotation reconnect
// ge:keywords bots sv_bots sv_lobby lobby warmup ready countdown match-restart local-seat
// ge:keywords listen-server listen_options join-the-window listen_mic local-voice sv_voip
// ge:keywords sv_mod mod-folder gamedir qwprogs misc-pak vote sv_vote per-map-vote
// ge:skill net shooter testing
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "console/cfg_tidy.hpp"
#include "console/console.hpp"
#include "console/words.hpp"
#include "net/sim_network.hpp"
#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "server/config.hpp"
#include "server/main.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/server/chat_rules.hpp"
#include "shooters_edge/server/rjump_rules.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/shot_help.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "vfs/pak.hpp"

using namespace ge::server;
namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

constexpr uint16_t kPort = 27500, kClientPort = 27001;

ServerConfig arena(uint32_t seed = 1) {
  ServerConfig c;
  c.hostname = "Arena Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = seed;
  return c;
}

struct Session {
  gn::SimNetwork net;
  gn::Transport server_port, client_port;
  Server s;
  se::QwClientEnd client;
  std::string error;
  bool opened = false;
  uint64_t digest = 1469598103934665603ull;
  int look_at = -1;    // a frame whose status is kept
  std::string looked;  // its status
};

// The core's server on the simulated network, one classic client joining at once, `frames`
// frames; the digest folds every frame's hash and the client's end.
// The client says `ready` at frame `ready_at` (-1: never), and each of `lines` at its frame.
void run_session(Session& r, const ServerConfig& c, int frames, const std::string& password = "",
                 int ready_at = -1, const std::vector<std::pair<int, std::string>>& lines = {}) {
  gn::LinkConditions lan;
  r.net = gn::make_sim_network(c.seed, lan);
  gn::open_sim_transport(r.server_port, r.net, gn::sim_address(0, kPort));
  gn::open_sim_transport(r.client_port, r.net, gn::sim_address(1, kClientPort));
  r.opened = server_open(r.s, se::qw_game(), c, &r.server_port, nullptr, r.error);
  if (!r.opened) return;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  cc.password = password;
  se::client_start(r.client, r.client_port, gn::sim_address(0, kPort), cc, 0);
  for (int f = 0; f < frames; f++) {
    const int64_t now = r.s.now_us;
    gn::sim_advance(r.net, now);
    if (f == ready_at) se::client_command(r.client, "ready");
    for (const auto& [at, line] : lines)
      if (f == at) se::client_command(r.client, line);
    se::client_tick(r.client, now);
    server_frame(r.s);
    if (f == r.look_at) r.looked = server_status(r.s);
    r.digest = (r.digest ^ server_digest(r.s)) * 1099511628211ull;
  }
  r.digest = (r.digest ^ static_cast<uint64_t>(r.client.stage)) * 1099511628211ull;
}

}  // namespace

TEST(classic_game_offline_and_networked_alone_are_one_digest) {
  Server off;
  std::string error;
  CHECK(server_open(off, se::qw_game(), arena(), nullptr, nullptr, error));
  CHECK(error.empty());
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  Server on;
  CHECK(server_open(on, se::qw_game(), arena(), &port, nullptr, error));
  CHECK(on.own_protocol);
  CHECK(off.frame_s > 0.012 && off.frame_s < 0.014);  // the mod's 13 ms
  for (int f = 0; f < 600; f++) {
    gn::sim_advance(net, on.now_us);
    server_frame(off);
    server_frame(on);
    if (server_digest(off) != server_digest(on)) {
      CHECK(false);
      break;
    }
  }
  CHECK(server_banner(on).find("the game's own protocol") != std::string::npos);
}

TEST(classic_game_a_client_connects_and_plays) {
  Session r;
  run_session(r, arena(), 600);  // 7.8 s
  CHECK(r.opened);
  CHECK(r.error.empty());
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK(server_status(r.s).find("seated=1") != std::string::npos);
  CHECK(server_status(r.s).find("connects=1") != std::string::npos);
  const auto* game_state = r.s.state;
  CHECK(game_state != nullptr);
}

TEST(classic_game_the_settings_reach_the_endpoint) {
  {
    ServerConfig c = arena();
    c.password = "secret";
    Session wrong;
    run_session(wrong, c, 300, "guess");
    CHECK(wrong.client.stage != se::ClientStage::active);
    Session right;
    run_session(right, c, 600, "secret");
    CHECK(right.client.stage == se::ClientStage::active);
  }
  {
    ServerConfig c = arena();
    c.bots = 8;  // every slot: none left for a player
    Server s;
    std::string error;
    CHECK(!server_open(s, se::qw_game(), c, nullptr, nullptr, error));
    CHECK_MSG(error.find("keep sv_bots under sv_maxclients") != std::string::npos, error);
  }
  {
    ServerConfig c = arena();
    c.map = "nowhere";
    c.root = "no-such-folder";
    Server s;
    std::string error;
    CHECK(!server_open(s, se::qw_game(), c, nullptr, nullptr, error));
    CHECK_MSG(!error.empty() && error.find("next:") == std::string::npos, error);
  }
}

TEST(classic_game_a_session_replays_to_the_bit) {
  Session a, b;
  run_session(a, arena(7), 500);
  run_session(b, arena(7), 500);
  CHECK(a.client.stage == se::ClientStage::active);
  CHECK(a.digest == b.digest);
}

TEST(classic_game_the_mod_s_rules_are_settings) {
  ge::console::Console con{true};
  ServerConfig c;
  ServerCfg cfg;
  std::string error;
  CHECK_MSG(
      server_configure(con, {"--map=arena", "+set", "fraglimit", "20", "+set", "timelimit", "10"},
                       c, cfg, se::qw_game(), error),
      error);
  CHECK(c.rules["fraglimit"] == "20");
  CHECK(c.rules["teamplay"].empty());  // unset: the mod's own default
  CHECK(explain(con, cfg, "fraglimit").find("command line") != std::string::npos);
  Server s;
  CHECK_MSG(server_open(s, se::qw_game(), c, nullptr, nullptr, error), error);
  const std::string status = server_status(s);
  CHECK_MSG(status.find("deathmatch=1 fraglimit=20 timelimit=10") != std::string::npos, status);
  ServerConfig bad = arena();
  bad.rules["fraglimit"] = "lots";
  Server refused;
  CHECK(!server_open(refused, se::qw_game(), bad, nullptr, nullptr, error));
  CHECK_MSG(error.find("fraglimit \"lots\" is not a number") != std::string::npos, error);
}

// Every setting a server owner can set says what it does in one sentence and what each value
// does (console/words.hpp): the server's own, the game's rules and the mod's table.
TEST(classic_game_every_server_setting_explains_itself) {
  ge::console::Console con{true};
  ServerConfig c;
  ServerCfg cfg;
  std::string error;
  CHECK_MSG(server_configure(con, {"--map=arena"}, c, cfg, se::qw_game(), error), error);
  int count = 0;
  for (const ge::console::Cvar* v : con.cvars().sorted()) {
    CHECK_MSG(ge::console::words_fault(*v).empty(), v->name + ": " + ge::console::words_fault(*v));
    CHECK_MSG(!v->values.empty() || v->kind == ge::console::CvarKind::text, v->name);
    count++;
  }
  CHECK(count > 100);
  auto mod = std::make_unique<shooters_edge::server::RulesCommon>();
  shooters_edge::server::add_mod_settings(*mod, {});
  shooters_edge::server::add_rjump_settings(mod->cvars);
  shooters_edge::server::add_chat_settings(mod->cvars);
  shooters_edge::server::add_aim_help(*mod);
  for (const ge::console::Cvar* v : mod->cvars.sorted()) {
    CHECK_MSG(ge::console::words_fault(*v).empty(), v->name + ": " + ge::console::words_fault(*v));
    CHECK_MSG(!v->values.empty() || v->kind == ge::console::CvarKind::text, v->name);
  }
  // A server.cfg setting any of the mod's own settings to its default checks clean: the server
  // knows every one (but sv_progs, which only a test match reads).
  std::string cfg_text;
  for (const ge::console::Cvar* v : mod->cvars.sorted())
    if (v->name != "sv_progs") cfg_text += v->name + " \"" + v->fallback + "\"\n";
  for (const ge::console::CfgFinding& f : ge::console::check_cfg(con, cfg_text))
    CHECK_MSG(false, std::to_string(f.line) + ": " + f.what);
}

TEST(classic_game_the_shooter_s_and_the_endpoint_s_settings) {
  ge::console::Console con{true};
  ServerConfig c;
  ServerCfg cfg;
  std::string error;
  CHECK_MSG(server_configure(con,
                             {"--map=arena", "+set", "pm_ktjump", "0", "+set", "sv_maxrate", "8000",
                              "+set", "sv_floodprot", "2 5 3", "+set", "sv_floodprotmsg", "hush",
                              "+set", "maxfps", "100"},
                             c, cfg, se::qw_game(), error),
            error);
  CHECK(c.rules["pm_ktjump"] == "0");
  CHECK(c.floodprot == "2 5 3");
  CHECK(explain(con, cfg, "pm_ktjump").find("command line") != std::string::npos);
  // The client is told the changed switch and the move rate; its third line in a burst is
  // refused (to it alone) and the silence holds the fourth.
  Session r;
  run_session(r, c, 700, "", -1,
              {{500, "say one"}, {502, "say two"}, {504, "say three"}, {506, "say four"}});
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK_MSG(r.client.serverinfo.find("\\pm_ktjump\\0") != std::string::npos, r.client.serverinfo);
  CHECK_MSG(r.client.serverinfo.find("\\maxfps\\100") != std::string::npos, r.client.serverinfo);
  int said = 0, hushed = 0, silenced = 0;
  for (const std::string& p : r.client.prints) {
    said += p.find(": one") != std::string::npos || p.find(": two") != std::string::npos;
    said += p.find(": three") != std::string::npos ? 100 : 0;
    hushed += p == "FloodProt: hush\n";
    silenced += p.find("You can't talk for") != std::string::npos;
  }
  CHECK_MSG(said == 2 && hushed == 1 && silenced == 1, std::to_string(said));
  // The client's rate of 25000 is past sv_maxrate's 8000: it is told, once, at its begin.
  int too_high = 0;
  for (const std::string& p : r.client.prints)
    too_high += p == "\nYour rate setting is too high for this server.\nRate set to 8000\n";
  CHECK_MSG(too_high == 1, std::to_string(too_high));
  // Out of range: refused with the range and the next step.
  for (const auto& [name, value] : {std::pair<std::string, std::string>{"sv_mintic", "0.5"},
                                    {"pm_airstep", "2"},
                                    {"sv_downloadchunks", "99"}}) {
    ServerConfig bad = arena();
    bad.rules[name] = value;
    Server refused;
    CHECK(!server_open(refused, se::qw_game(), bad, nullptr, nullptr, error));
    CHECK_MSG(error.find(name) != std::string::npos && error.find("--explain") != std::string::npos,
              error);
  }
  ServerConfig bad = arena();
  bad.floodprot = "lots";
  Server refused;
  CHECK(!server_open(refused, se::qw_game(), bad, nullptr, nullptr, error));
  CHECK_MSG(error.find("sv_floodprot") != std::string::npos, error);
}

// Each classic game's names for the server's settings: a server.cfg in either game's words sets
// ours, and Quake II's own rule is left alone on a classic map.
TEST(classic_game_each_game_s_names_for_the_server_s_settings) {
  ge::console::Console con{true};
  ServerConfig c;
  ServerCfg cfg;
  std::string error;
  CHECK_MSG(server_configure(con, {"--map=arena", "+set", "maxclients",
                                   "12",          "+set", "hostname",
                                   "Old Names",   "+set", "flood_persecond",
                                   "7",           "+set", "sv_maplist",
                                   "dm2 dm3",     "+set", "spectator_password",
                                   "look",        "+set", "maxspectators",
                                   "3",           "+set", "dmflags",
                                   "16"},
                             c, cfg, se::qw_game(), error),
            error);
  CHECK(c.maxclients == 12);
  CHECK(c.hostname == "Old Names");
  CHECK_MSG(c.floodprot == "10 7 1", c.floodprot);
  CHECK(c.maprotation == "dm2 dm3");
  CHECK(c.spectator_password == "look");
  CHECK(c.max_spectators == 3);
  CHECK(c.rules["dmflags"] == "16");
  // Our names set the same settings, read back by the game's.
  CHECK(con.cvars().find("maxclients") == con.cvars().find("sv_maxclients"));
  CHECK(con.cvars().find("maxplayers") == con.cvars().find("sv_maxclients"));  // Quake's words
  CHECK(con.cvars().find("net_messagetimeout") == con.cvars().find("sv_timeout"));
  // Every game name is a synonym, none a setting of its own: the two never disagree.
  for (const ge::console::Synonym& n : con.cvars().synonyms())
    CHECK_MSG(n.target != nullptr && n.target->name != n.name, n.name);
  CHECK(con.cvars().synonyms().size() == 46);  // sv_port among them (server.cpp)
  Server s;                                    // a classic map leaves Quake II's dmflags alone
  CHECK_MSG(server_open(s, se::qw_game(), c, nullptr, nullptr, error), error);
}

// The name and chat guards: a public line's carriage returns turn into '#', a client changing its
// top colour too often is kicked, as is one changing its user info past sv_infoflood; each one's
// classic name sets it, and a bad value is refused naming the setting.
// Where the server is, by its classic names too, reaches the server info a browser lists.
TEST(classic_game_the_server_s_place_is_in_its_info) {
  ge::console::Console con{true};
  ServerConfig c;
  ServerCfg cfg;
  std::string error;
  CHECK_MSG(server_configure(con,
                             {"--map=arena",
                              "+set",
                              "countrycode",
                              "se",
                              "+set",
                              "sv_city",
                              "Lund",
                              "+set",
                              "coords",
                              "55.7,13.2",
                              "+set",
                              "hostport",
                              "edge.example:27500",
                              "+set",
                              "watervis",
                              "1",
                              "+set",
                              "sv_safestrafe",
                              "2",
                              "+set",
                              "sv_antilag_no_pred",
                              "1"},
                             c, cfg, se::qw_game(), error),
            error);
  CHECK(c.country == "se" && c.city == "Lund" && c.public_addr == "edge.example:27500");
  CHECK(c.rules["sv_antilagexact"] == "1");  // by its classic name
  Session r;
  run_session(r, c, 400);
  const std::string& info = r.client.serverinfo;
  CHECK_MSG(info.find("\\countrycode\\se") != std::string::npos, info);
  CHECK_MSG(info.find("\\city\\Lund") != std::string::npos, info);
  CHECK_MSG(info.find("\\coords\\55.7,13.2") != std::string::npos, info);
  CHECK_MSG(info.find("\\hostport\\edge.example:27500") != std::string::npos, info);
  CHECK_MSG(info.find("\\watervis\\1") != std::string::npos, info);
  CHECK_MSG(info.find("\\sv_safestrafe\\2") != std::string::npos, info);  // the client predicts
}

// The slots and the pings: a player joining a full server watches from a spectator slot; the
// minimum ping holds a player's packets until its ping reaches it, and players may change it
// with `minping` (in range); a player over the most ping watches instead; each by its classic
// name too.
TEST(classic_game_spectator_slots_and_the_pings) {
  {
    ServerConfig c = arena();
    c.maxclients = 1;
    c.max_spectators = 1;
    c.bots = 1;  // the one player slot taken
    c.rules["sv_fullspectate"] = "1";
    Session r;
    run_session(r, c, 500);
    CHECK(r.client.stage == se::ClientStage::active);
    bool told = false;
    for (const std::string& p : r.client.oob_prints)
      told |= p.find("connecting as spectator") != std::string::npos;
    for (const std::string& p : r.client.prints)
      told |= p.find("connecting as spectator") != std::string::npos;
    CHECK(told);
    c.rules["sv_fullspectate"] = "0";
    Session refused;
    run_session(refused, c, 300);
    CHECK(refused.client.stage != se::ClientStage::active);
  }
  {  // A VIP slot (sv_maxvipspectators): a VIP word seats the visitor beside the full server.
    ServerConfig c = arena();
    c.maxclients = 1;
    c.bots = 1;
    c.rules["sv_maxvipspectators"] = "1";
    c.rules["sv_vippassword"] = "gold";
    c.rules["sv_fullspectate"] = "1";
    Session vip;
    run_session(vip, c, 500, "gold");
    CHECK(vip.client.stage == se::ClientStage::active);
    Session plain;
    run_session(plain, c, 300);
    CHECK(plain.client.stage != se::ClientStage::active);
  }
  {
    ge::console::Console con{true};
    ServerConfig c;
    ServerCfg cfg;
    std::string error;
    CHECK_MSG(server_configure(con,
                               {"--map=arena", "+set", "sv_minping", "80", "+set",
                                "sv_enable_cmd_minping", "1", "+set", "sv_reconnectlimit", "5"},
                               c, cfg, se::qw_game(), error),
              error);
    CHECK(c.rules["sv_minpingcmd"] == "1" && c.rules["sv_reconnectwait"] == "5");
    Session r;
    run_session(r, c, 1500, "", -1, {{1300, "minping 500"}, {1310, "minping 120"}});
    const ge::net::Status st = server_public_status(r.s);
    int ping = -1;
    for (const ge::net::StatusPlayer& p : st.players)
      if (p.name == "visitor") ping = p.ping_ms;
    CHECK_MSG(ping >= 70, std::to_string(ping));  // held toward 80 ms on a fast network
    bool range = false, set = false;
    for (const std::string& p : r.client.prints) {
      range |= p.find("must be 0 to 300") != std::string::npos;
      set |= p.find("Minimum ping set to 120 ms") != std::string::npos;
    }
    CHECK(range && set);
  }
  {
    ServerConfig c = arena();
    c.rules["sv_maxping"] = "1";
    Session r;
    run_session(r, c, 600);
    bool watching = false;
    for (const std::string& p : r.client.prints)
      watching |= p.find("join as a spectator") != std::string::npos;
    CHECK(watching);  // even a fast network's ping is over 1 ms
  }
}

TEST(classic_game_the_name_and_chat_guards) {
  auto kicked = [](const Session& r, const std::string& why) {
    for (const std::string& p : r.client.prints)
      if (p.find("You were kicked from the game for " + why) != std::string::npos) return true;
    return false;
  };
  {
    Session r;
    run_session(r, arena(), 700, "", -1, {{500, "say \"one\rtwo\""}});
    bool unfaked = false;
    for (const std::string& p : r.client.prints) unfaked |= p.find("one#two") != std::string::npos;
    CHECK(unfaked);
  }
  {
    std::vector<std::pair<int, std::string>> lines;
    lines.reserve(8);
    for (int i = 0; i < 8; i++)
      lines.push_back({500 + i * 4, "setinfo topcolor " + std::to_string(i + 1)});
    Session r;
    run_session(r, arena(), 700, "", -1, lines);
    CHECK(kicked(r, "topcolor spam"));
    ServerConfig off = arena();
    off.rules["sv_colorkick"] = "0";
    Session spared;
    run_session(spared, off, 700, "", -1, lines);
    CHECK(!kicked(spared, "topcolor spam"));
  }
  {
    ge::console::Console con{true};
    ServerConfig c;
    ServerCfg cfg;
    std::string error;
    CHECK_MSG(
        server_configure(con,
                         {"--map=arena", "+set", "sv_kickuserinfospamcount", "3", "+set",
                          "sv_kickuserinfospamtime", "10", "+set", "sv_default_name", "guest"},
                         c, cfg, se::qw_game(), error),
        error);
    CHECK_MSG(c.rules["sv_infoflood"] == "3 10", c.rules["sv_infoflood"]);
    CHECK(c.rules["sv_defaultname"] == "guest");
    std::vector<std::pair<int, std::string>> lines;
    lines.reserve(5);
    for (int i = 0; i < 5; i++)
      lines.push_back({500 + i * 4, "setinfo skin s" + std::to_string(i)});
    Session r;
    run_session(r, c, 700, "", -1, lines);
    CHECK(kicked(r, "userinfo spam"));
  }
  for (const auto& [name, value] : {std::pair<std::string, std::string>{"sv_infoflood", "lots"},
                                    {"sv_defaultname", "a$b"},
                                    {"sv_unfake", "2"}}) {
    ServerConfig bad = arena();
    bad.rules[name] = value;
    Server refused;
    std::string error;
    CHECK(!server_open(refused, se::qw_game(), bad, nullptr, nullptr, error));
    CHECK_MSG(error.find(name) != std::string::npos, error);
  }
}

TEST(classic_game_a_map_from_the_game_folder_s_pack) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "ge_se_classic_game_pack";
  fs::remove_all(root);
  fs::create_directories(root / se::kGameFolder);
  std::string error;
  std::vector<uint8_t> bsp, pak;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error),
            error);
  CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &pak, &error), error);
  {
    std::ofstream out(root / se::kGameFolder / "pak0.pak", std::ios::binary);
    out.write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  }
  ServerConfig c = arena();
  c.map = "halls";
  c.root = root.string();
  c.maprotation = "halls arena";
  c.rules["timelimit"] = "0.05";  // 3 s, then the mod's 5 s intermission, then its changelevel
  Server s;
  CHECK_MSG(server_open(s, se::qw_game(), c, nullptr, nullptr, error), error);
  CHECK(server_status(s).find("map=halls") != std::string::npos);
  for (int f = 0; f < 800; f++) server_frame(s);  // 10.4 s
  CHECK_MSG(server_status(s).find("map=arena") != std::string::npos, server_status(s));
  fs::remove_all(root);
}

TEST(classic_game_a_mod_folder_runs_its_own_program) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "ge_se_classic_game_mod";
  fs::remove_all(root);
  fs::create_directories(root / se::kGameFolder);
  fs::create_directories(root / "mymod");
  std::string error;
  std::vector<uint8_t> bsp, pak, misc;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error),
            error);
  CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &pak, &error), error);
  std::vector<int32_t> lines;
  CHECK_MSG(ge::vfs::write_pak({{"qwprogs.dat", shooters_edge::server::test_mod_bytes(lines)}},
                               &misc, &error),
            error);
  auto put = [](const fs::path& at, const std::vector<uint8_t>& bytes) {
    std::ofstream out(at, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  };
  put(root / se::kGameFolder / "pak0.pak", pak);
  ServerConfig c = arena();
  c.map = "halls";
  c.root = root.string();
  c.mod = "mymod";
  {
    Server s;  // no program in the mod's folder: refused, naming it
    CHECK(!server_open(s, se::qw_game(), c, nullptr, nullptr, error));
    CHECK_MSG(error.find("the mod folder mymod holds no multiplayer program") == 0, error);
  }
  put(root / "mymod" / "misc.pak", misc);  // a mod's program in its unnumbered pack
  Session r;
  run_session(r, c, 300);
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK(r.client.gamedir == "mymod");
  CHECK_MSG(r.client.serverinfo.find("\\*gamedir\\mymod") != std::string::npos,
            r.client.serverinfo);
  fs::remove_all(root);
}

// A mission pack's folder holds its campaign's single-player program and its maps: the server
// plays the pack's maps for deathmatch on the baseline mod rather than refusing the folder.
TEST(classic_game_a_mission_pack_s_folder_runs_its_maps_on_the_baseline_mod) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "ge_se_classic_game_pack";
  fs::remove_all(root);
  fs::create_directories(root / se::kGameFolder);
  fs::create_directories(root / "pack");
  std::string error;
  std::vector<uint8_t> bsp, pak;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error),
            error);
  const std::vector<uint8_t> campaign =
      ge::progs::Assembler(ge::progs::kSinglePlayerLayout).bytes();
  CHECK_MSG(ge::vfs::write_pak({{"maps/packdm.bsp", bsp}, {"progs.dat", campaign}}, &pak, &error),
            error);
  {
    std::ofstream out(root / "pack" / "pak0.pak", std::ios::binary);
    out.write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  }
  ServerConfig c = arena();
  c.map = "packdm";
  c.root = root.string();
  c.mod = "pack";
  Session r;
  run_session(r, c, 300);
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK(r.client.gamedir == "pack");
  fs::remove_all(root);
}

TEST(classic_game_the_mod_s_changelevel_takes_the_players_along) {
  ServerConfig c = arena();
  c.rules["timelimit"] = "0.05";
  Session r;
  run_session(r, c, 900);  // 11.7 s: the match ends at 3 s, the map changes at about 8 s
  CHECK(r.opened);
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK_MSG(server_status(r.s).find("seated=1") != std::string::npos, server_status(r.s));
  Session again;
  run_session(again, c, 900);
  CHECK(r.digest == again.digest);
}

TEST(classic_game_bots_are_players_every_client_sees) {
  ServerConfig c = arena();
  c.bots = 2;
  Session r;
  run_session(r, c, 600);
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.stage == se::ClientStage::active);
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("seated=3") != std::string::npos, status);
  CHECK_MSG(status.find("bots=2") != std::string::npos, status);
  int bots = 0, teamed = 0;
  for (const std::string& info : r.client.userinfos) {
    bots += info.find("\\*bot\\1") != std::string::npos ? 1 : 0;
    teamed += info.find("\\team\\") != std::string::npos ? 1 : 0;
  }
  CHECK_MSG(bots == 2, std::to_string(bots) + " bots in the client's scoreboard");
  CHECK_MSG(teamed == 3, std::to_string(teamed) + " seats with a team");
  Session again;
  run_session(again, c, 600);
  CHECK(r.digest == again.digest);
}

TEST(classic_game_the_lobby_warms_up_until_everyone_is_ready) {
  ServerConfig c = arena();
  c.lobby = 1;
  c.bots = 1;
  c.rules["fraglimit"] = "20";
  {
    Server alone;  // a bot alone never counts down
    std::string error;
    CHECK_MSG(server_open(alone, se::qw_game(), c, nullptr, nullptr, error), error);
    for (int f = 0; f < 800; f++) server_frame(alone);
    CHECK_MSG(server_status(alone).find("lobby=warmup") != std::string::npos, server_status(alone));
  }
  Session r;
  r.look_at = 399;
  run_session(r, c, 1100, "", 400);  // ready at 5.2 s, the countdown's 5 s, the restart
  CHECK_MSG(r.looked.find("lobby=warmup ready=1/2") != std::string::npos, r.looked);
  CHECK_MSG(r.looked.find("fraglimit=0") != std::string::npos, r.looked);  // warmup holds it
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("lobby=match") != std::string::npos, status);
  CHECK_MSG(status.find("matches=1") != std::string::npos, status);
  CHECK_MSG(status.find("fraglimit=20") != std::string::npos, status);
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  // The phase reaches the client as the server info's `status` (a match recording starts on it).
  CHECK_MSG(se::info_get(r.client.serverinfo, "status") == "In progress", r.client.serverinfo);
  Session again;
  again.look_at = 399;
  run_session(again, c, 1100, "", 400);
  CHECK(r.digest == again.digest);
}

namespace {

bool printed(const se::QwClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(classic_game_a_timeout_stops_the_match_and_play_resumes) {
  ServerConfig c = arena();
  c.timeout_length_s = 10;
  c.timeout_resume_s = 2;
  Session r;
  r.look_at = 500;
  run_session(r, c, 1200, "", -1, {{100, "timein"}, {120, "timeout"}, {200, "pause"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "There is no timeout to end."));
  CHECK(printed(r.client, "visitor calls a timeout. The game stops for 10 seconds."));
  CHECK(printed(r.client, "The game is in a timeout."));
  CHECK_MSG(r.looked.find("timeout=out") != std::string::npos, r.looked);
  CHECK(printed(r.client, "Play resumes in 2 seconds."));
  CHECK(printed(r.client, "Play!"));
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("timeout=") == std::string::npos, status);
}

TEST(classic_game_a_vote_changes_a_rule_for_the_map) {
  ServerConfig c = arena();
  c.rules["timelimit"] = "0.05";  // the match ends at 3 s, the map changes at about 8 s
  Session r;
  r.look_at = 200;
  run_session(r, c, 900, "", -1, {{120, "vote hostname mine"}, {140, "vote fraglimit 30"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "hostname may not be voted on here"));
  CHECK(printed(r.client, "visitor calls fraglimit 30: vote yes/no"));
  CHECK(printed(r.client, "Vote passed: fraglimit 30 for this map"));
  CHECK_MSG(r.looked.find("fraglimit=30") != std::string::npos, r.looked);
  // The map changed: the vote's change went back to the server's own (the mod's 0).
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK(printed(r.client, "fraglimit back to default for the new map"));
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("fraglimit=0") != std::string::npos, status);
  Session again;
  again.look_at = 200;
  run_session(again, c, 900, "", -1, {{120, "vote hostname mine"}, {140, "vote fraglimit 30"}});
  CHECK(r.digest == again.digest);
}

TEST(classic_game_a_vote_on_the_move_restarts_the_map_and_is_kept) {
  ServerConfig c = arena();
  Session r;
  run_session(r, c, 400, "", -1, {{140, "vote sv_gravity 400"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "Vote passed: sv_gravity 400"));
  CHECK(printed(r.client, "The map restarts so everyone moves by the new setting"));
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK_MSG(r.client.movevars[0] == 400.0f, std::to_string(r.client.movevars[0]));  // gravity
  CHECK(!printed(r.client, "back to"));  // a restart keeps it
  Session refused;
  run_session(refused, c, 300, "", -1, {{140, "vote sv_gravity heavy"}, {160, "vote sv_coop 1"}});
  CHECK(refused.client.spawncount == 1);
  CHECK(printed(refused.client, "sv_coop may not be voted on"));
}

TEST(classic_game_a_vote_moves_the_server_to_a_map) {
  Session r;
  run_session(r, arena(), 400, "", -1, {{120, "vote map nowhere"}, {140, "vote map arena"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "There is no map called nowhere on this server."));
  CHECK(printed(r.client, "visitor calls map arena (now arena): vote yes/no"));
  CHECK(printed(r.client, "Vote passed: map arena"));
  CHECK(!printed(r.client, "Vote passed: map arena for this map"));  // an act, not a setting
  CHECK(printed(r.client, "The vote moves the server to arena"));
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK(r.client.stage == se::ClientStage::active);
  CHECK(!printed(r.client, "back to"));
}

TEST(classic_game_a_vote_kicks_a_player) {
  Session r;
  run_session(r, arena(), 300, "", -1, {{120, "vote kick nobody"}, {140, "vote kick visitor"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "No player is called nobody. Type \"users\" to see the ids and names."));
  CHECK(printed(r.client, "visitor calls kick visitor: vote yes/no"));
  CHECK(r.client.stage != se::ClientStage::active);  // the server let him go
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("visitor") == std::string::npos, status);
}

TEST(classic_game_a_vote_grants_one_player_the_easy_jump_for_the_map) {
  ServerConfig c = arena();
  c.rules["timelimit"] = "0.05";  // the match ends at 3 s, the map changes at about 8 s
  Session r;
  run_session(r, c, 900, "", -1,
              {{120, "vote speedjump nobody on"},
               {130, "vote speedjump visitor sometimes"},
               {140, "vote speedjump visitor on"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "No player is called nobody. Type \"users\" to see the ids and names."));
  CHECK(printed(r.client,
                "easy jump is turned \"on\" or \"off\" or given a share from 0 to 1, "
                "not \"sometimes\"."));
  CHECK(printed(r.client, "visitor calls speedjump visitor on (now visitor off): vote yes/no"));
  CHECK(printed(r.client, "Vote passed: speedjump visitor on for this map"));
  CHECK(printed(r.client, "visitor is granted the easy jump (share 0.8, cl_speedjump 1)"));
  // The map changed: the grant went back with it.
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK(printed(r.client, "speedjump back to visitor off for the new map"));
  CHECK(printed(r.client, "visitor's easy jump is no longer granted"));
}

TEST(classic_game_a_vote_grants_one_player_the_aim_help_for_the_map) {
  ServerConfig c = arena();
  c.rules["timelimit"] = "0.05";  // the match ends at 3 s, the map changes at about 8 s
  Session r;
  run_session(r, c, 900, "", -1,
              {{120, "vote aimhelp visitor 0.5"}, {140, "vote aimhelp visitor on"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "aim help is turned \"on\" or \"off\", not \"0.5\"."));
  CHECK(printed(r.client, "visitor calls aimhelp visitor on (now visitor off): vote yes/no"));
  CHECK(printed(r.client, "Vote passed: aimhelp visitor on for this map"));
  CHECK(printed(r.client, "visitor is granted the aim help (24 across, 48 up and down)"));
  CHECK_MSG(r.client.spawncount == 2, "spawncount " + std::to_string(r.client.spawncount));
  CHECK(printed(r.client, "aimhelp back to visitor off for the new map"));
  CHECK(printed(r.client, "visitor's aim help is no longer granted"));
}

TEST(classic_game_sv_intermission_holds_the_next_map) {
  ServerConfig c = arena();
  c.lobby = 1;
  c.countdown_s = 1;
  c.intermission_s = 4;
  c.rules["timelimit"] = "0.05";  // the match's 3 s
  Session r;
  r.look_at = 1149;  // 15 s
  // Ready at 3.9 s: the match from about 5 s to 8 s, the mod's own end until about 13 s, then
  // the next map held 4 s more.
  run_session(r, c, 1700, "", 300);
  CHECK_MSG(r.opened, r.error);
  CHECK_MSG(r.looked.find("lobby=intermission") != std::string::npos, r.looked);
  CHECK_MSG(r.client.spawncount == 3, "spawncount " + std::to_string(r.client.spawncount));
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("lobby=warmup") != std::string::npos, status);  // the next map
  Session again;
  again.look_at = 1149;
  run_session(again, c, 1700, "", 300);
  CHECK(r.digest == again.digest);
}

TEST(classic_game_a_client_joins_the_window_s_own_match) {
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(shooters_edge::client::baseline_progs(progs, error), error);
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport port, client_port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gn::open_sim_transport(client_port, net, gn::sim_address(1, kClientPort));
  shooters_edge::client::Listen l;
  shooters_edge::client::ListenOptions opt = se::listen_options();
  opt.transport = &port;
  opt.player = "host";
  shooters_edge::client::SearchPath none;
  auto map = shooters_edge::client::load_named_map(none, "arena", error);
  CHECK_MSG(map != nullptr, error);
  CHECK_MSG(shooters_edge::client::listen_start(l, progs, std::move(map), opt, error), error);
  CHECK(l.match != nullptr && l.match->own_protocol);  // the core's server, on the window's side
  se::QwClientEnd visitor;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  se::client_start(visitor, client_port, gn::sim_address(0, kPort), cc, 0);
  ge::shooter::MoveCmd idle;
  for (int f = 0; f < 400; f++) {
    const int64_t now = l.match->now_us;
    gn::sim_advance(net, now);
    se::client_tick(visitor, now);
    shooters_edge::client::listen_frame(l, idle);
  }
  CHECK(visitor.stage == se::ClientStage::active);
  CHECK(l.server->clients[0].active && l.server->clients[1].active);  // the host and the visitor
  // The visitor sees the window's player as a player, not a bot.
  CHECK(se::info_get(visitor.userinfos[0], "name") == "host");
  CHECK(se::info_get(visitor.userinfos[0], "*bot").empty());
  // A mod reads the server's settings by its game's names (`cvar("maxclients")`).
  const ge::console::Cvar* most = l.server->cvars.find("maxclients");
  CHECK(most != nullptr && std::atoi(most->value.c_str()) >= 2);
  CHECK(l.server->cvars.find("hostname") != nullptr);
  shooters_edge::client::listen_stop(l);
}

namespace {
bool always_talks(int, int) { return true; }
// `sv_modern 1`: a modern server in each game's own terms (engine/server's GameMeta). Quake II's
// rewinds every shot, rocket and bolt to what its shooter drew; QuakeWorld's every shot's line and
// its projectiles, and at 2 lifts the rate cap on a LAN. A setting named anywhere wins over it, and
// turning it off puts back what it set.
TEST(sv_modern_turns_on_each_games_own_modern_network) {
  using ge::console::Source;
  {
    ge::console::Cvars cvars;
    ge::server::ServerConfig c;
    ge::server::add_server_cvars(cvars, c, se::q2_game());
    CHECK(c.metas["sv_modern"] == "0" && c.rules["sv_antilag"].empty());
    cvars.set("sv_modern", "1", Source::local);
    CHECK(c.rules["sv_antilag"] == "2" && c.rules["sv_antilag_projectiles"] == "1" &&
          c.rules["sv_antilag_interp"] == "1");
    cvars.set("sv_antilag_interp", "0", Source::local);  // the operator's own line wins
    cvars.set("sv_modern", "0", Source::local);
    cvars.set("sv_modern", "1", Source::local);
    CHECK(c.rules["sv_antilag_interp"] == "0" && c.rules["sv_antilag"] == "2");
    cvars.set("sv_modern", "0", Source::local);
    CHECK(c.rules["sv_antilag"].empty());  // put back
  }
  {
    ge::console::Cvars cvars;
    ge::server::ServerConfig c;
    ge::server::add_server_cvars(cvars, c, se::qw_game());
    cvars.set("sv_modern", "2", Source::local);
    CHECK(c.rules["sv_antilag"] == "2" && c.rules["sv_antilag_projectiles"] == "1");
    CHECK(c.rules["sv_lanrate"] == "1");
    const ge::console::Cvar* m = cvars.find("sv_modern");
    CHECK(m != nullptr && !m->help.empty());
  }
}

}  // namespace

TEST(classic_game_the_window_s_player_hears_a_visitor_and_is_heard_on_its_own_match) {
  namespace cl = shooters_edge::client;
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(cl::baseline_progs(progs, error), error);
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport port, client_port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gn::open_sim_transport(client_port, net, gn::sim_address(1, kClientPort));
  cl::Listen l;
  cl::ListenOptions opt = se::listen_options();
  opt.transport = &port;
  opt.player = "host";
  cl::SearchPath none;
  auto map = cl::load_named_map(none, "arena", error);
  CHECK_MSG(map != nullptr, error);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  CHECK(cl::listen_voice(l));
  se::QwClientEnd visitor;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.voice = true;
  cc.talk = always_talks;
  se::client_start(visitor, client_port, gn::sim_address(0, kPort), cc, 0);
  std::vector<float> tone(48000 / 77);  // a frame's worth of a microphone
  for (size_t i = 0; i < tone.size(); i++) tone[i] = (i / 24) % 2 == 0 ? 0.25f : -0.25f;
  ge::shooter::MoveCmd idle;
  size_t heard = 0, from_visitor = 0;
  const auto run = [&](int frames, bool talking) {
    for (int f = 0; f < frames; f++) {
      const int64_t now = l.match->now_us;
      gn::sim_advance(net, now);
      se::client_tick(visitor, now);
      cl::listen_mic(l, tone, talking);
      cl::listen_frame(l, idle);
      for (const cl::DemoVoice& v : l.voices) {
        heard++;
        if (v.sender == visitor.playernum) from_visitor++;
      }
      l.voices.clear();
    }
  };
  run(400, false);
  CHECK(visitor.stage == se::ClientStage::active);
  CHECK(visitor.ext.has(se::ExtFamily::x2, se::kX2Voice));
  CHECK_MSG(
      from_visitor > 0 && from_visitor == heard,
      std::to_string(from_visitor) + " of " + std::to_string(heard) + " voices the visitor's");
  CHECK_MSG(visitor.voice_heard[0] == 0, "the host was heard without talking");
  run(200, true);
  CHECK_MSG(visitor.voice_heard[0] > 0, "the visitor heard nothing of the host");
  CHECK(l.voice.sent > 0);
  cl::listen_stop(l);
}

TEST(classic_game_a_tied_match_plays_overtime) {
  ServerConfig c = arena();
  c.lobby = 1;
  c.bots = 1;
  c.rules["timelimit"] = "0.05";  // the match's 3 s, tied at nothing each
  c.rules["sv_overtime"] = "1";
  c.rules["sv_overtime_minutes"] = "1";
  Session r;
  r.look_at = 1149;  // the match's 10th s: in extra time
  run_session(r, c, 1200, "", 300);
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "Time is up and the game is a draw."));
  CHECK(printed(r.client, "1 minute of overtime follows."));
  CHECK_MSG(r.looked.find("lobby=match") != std::string::npos, r.looked);
  CHECK_MSG(r.looked.find("timelimit=1.05") != std::string::npos, r.looked);
  c.rules["sv_overtime"] = "2";
  Session sudden;
  run_session(sudden, c, 900, "", 300);
  CHECK(printed(sudden.client, "Sudden death overtime begins."));
  c.rules["sv_overtime"] = "0";
  Session off;
  run_session(off, c, 900, "", 300);
  CHECK(!printed(off.client, "overtime"));
}

TEST(classic_game_prewar_holds_the_fire_of_the_warmup) {
  ServerConfig c = arena();
  c.lobby = 1;
  c.bots = 1;
  c.prewar = 0;  // the client's script fires now and then: held, and told
  Session r;
  run_session(r, c, 400);
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.client, "Nobody may fire before the match."));
  c.prewar = 1;
  Session free;
  run_session(free, c, 400);
  CHECK(!printed(free.client, "may fire"));
}

// Quake's rules (a game folder's pack loaded) tell everyone when a spectator comes.
TEST(classic_game_a_spectator_is_announced_under_quakes_rules) {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "ge_se_classic_game_spectator";
  fs::remove_all(root);
  fs::create_directories(root / se::kGameFolder);
  std::string error;
  std::vector<uint8_t> bsp, pak;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error),
            error);
  CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &pak, &error), error);
  {
    std::ofstream out(root / se::kGameFolder / "pak0.pak", std::ios::binary);
    out.write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
  }
  ServerConfig c = arena();
  c.map = "halls";
  c.root = root.string();
  c.maxclients = 1;
  c.max_spectators = 1;
  c.bots = 1;  // the one player slot taken: the visitor watches
  c.rules["sv_fullspectate"] = "1";
  Session r;
  run_session(r, c, 500);
  CHECK(r.client.stage == se::ClientStage::active);
  bool told = false;
  for (const std::string& p : r.client.prints)
    told |= p.find("Spectator visitor entered the game") != std::string::npos;
  CHECK(told);
  fs::remove_all(root);
}

// `setmaster` on a classic server: the master is pinged, then hears a heartbeat at once; the
// quit tells it the server is gone.
TEST(classic_game_setmaster_pings_and_beats_to_the_master) {
  ServerConfig c = arena();
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(c.seed, lan);
  gn::Transport port, master;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gn::open_sim_transport(master, net, gn::sim_address(7, 27000));
  Server s;
  std::string error;
  CHECK_MSG(server_open(s, se::qw_game(), c, &port, nullptr, error), error);
  CHECK(se::qw_game().console(s.state, "setmaster sim:7"));
  std::vector<std::string> heard;
  const auto listen = [&] {
    uint8_t buf[256];
    gn::Address from;
    for (int n; (n = gn::transport_receive(master, buf, sizeof buf, from, s.now_us)) >= 0;)
      heard.emplace_back(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
  };
  for (int f = 0; f < 20; f++) {
    gn::sim_advance(net, s.now_us);
    server_frame(s);
    listen();
  }
  std::vector<std::string> lines;
  se::qw_game().console_lines(s.state, lines);
  std::string told;
  for (const std::string& l : lines) told += l + " | ";
  CHECK_MSG(told.find("Master server at sim:7:27000") != std::string::npos, told);
  CHECK_MSG(heard.size() >= 2, told);
  if (heard.size() >= 2) {
    CHECK(heard[0] == std::string("k\0", 2));
    CHECK(heard[1].starts_with("a\n") && heard[1].ends_with("\n0\n"));
  }
  server_close(s);
  gn::sim_advance(net, s.now_us + 1000000);
  listen();
  CHECK(!heard.empty() && heard.back() == "C\n");
}

// The move's server settings reach a joined client as the server set them (its server data's move
// values: gravity, stop speed, top speed, a spectator's speed, the accelerations on the ground and
// in water, friction and water friction), so its prediction moves as the server does.
TEST(classic_game_the_moves_settings_reach_the_joined_client) {
  ServerConfig c = arena();
  c.rules["sv_gravity"] = "700";
  c.rules["sv_stopspeed"] = "50";
  c.rules["sv_maxspeed"] = "300";
  c.rules["sv_spectatormaxspeed"] = "400";
  c.rules["sv_accelerate"] = "7";
  c.rules["sv_wateraccelerate"] = "6";
  c.rules["sv_friction"] = "3";
  c.rules["sv_waterfriction"] = "2";
  Session r;
  run_session(r, c, 200);
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.stage == se::ClientStage::active);
  const float want[10] = {700, 50, 300, 400, 7, 0, 6, 3, 2, 0};
  for (int i = 0; i < 10; i++)
    if (i != 5 && i != 9)  // the air's acceleration and the entity gravity are not set here
      CHECK_MSG(r.client.movevars[i] == want[i],
                "move value " + std::to_string(i) + " is " + std::to_string(r.client.movevars[i]));
}

// The match's stats words are the server's own: each is answered once, and never also called a
// bad command after its answer.
TEST(classic_game_the_stats_words_answer_and_are_not_bad_commands) {
  Session r;
  run_session(r, arena(), 700, "", -1,
              {{500, "stats"}, {520, "teamstats"}, {540, "laststats"}, {560, "nosuchword"}});
  bool bad_stats = false, bad_other = false;
  std::string all;
  for (const std::string& p : r.client.prints) {
    all += p;
    bad_stats |= p.find("Bad user command: stats") != std::string::npos ||
                 p.find("Bad user command: teamstats") != std::string::npos ||
                 p.find("Bad user command: laststats") != std::string::npos;
    bad_other |= p.find("Bad user command: nosuchword") != std::string::npos;
  }
  CHECK_MSG(!bad_stats, all);
  CHECK_MSG(bad_other, all);  // an unknown word is still refused
}
