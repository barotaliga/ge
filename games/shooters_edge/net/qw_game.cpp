// Shooter's Edge's playable dedicated server, defined: the mod's server made from the settings,
// its map loaded, the QuakeWorld endpoint started on the port the core hands it.
//
// ge:summary Shooter's Edge's playable dedicated server's definitions: create, serve, the frames
// ge:keywords qw_game serve net_frame create status
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#include "shooters_edge/net/qw_game.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "classic/content/downloads.hpp"
#include "classic/protocol/masters.hpp"
#include "classic/protocol/wire.hpp"
#include "core/num.hpp"
#include "mod/loader.hpp"
#include "net/packet.hpp"
#include "progs/file.hpp"
#include "server/homemap.hpp"
#include "server/overtime.hpp"
#include "shooter/midair.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/client/q2_party.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_admin.hpp"
#include "shooters_edge/net/qw_cheats.hpp"
#include "shooters_edge/net/qw_coop.hpp"
#include "shooters_edge/net/qw_coop_game.hpp"
#include "shooters_edge/net/qw_endpoint_cfg.hpp"
#include "shooters_edge/net/qw_game_tv.hpp"
#include "shooters_edge/net/qw_login.hpp"
#include "shooters_edge/net/qw_match_rules.hpp"
#include "shooters_edge/net/qw_qtv.hpp"
#include "shooters_edge/net/qw_report.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/dedicated.hpp"
#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"
#include "shooters_edge/server/qvm_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace srv = shooters_edge::server;

struct Game {
  const sv::RulesCommon* told_rules = nullptr;  // the rules whose console lines were printed
  size_t told = 0;                              // how many of them
  // The C++ mod packages named in sv_mod (docs/mods.md § On the server), open while any rules run.
  std::vector<std::unique_ptr<ge::mod::LoadedMod>> packages;
  ge::progs::Progs progs;
  std::string progs_crc;  // the program file's CRC-16 as a decimal (`*progs`; "": the baseline)
  const ge::progs::Progs* program = &progs;  // the one played: `progs`, or a listen's own
  std::vector<uint8_t> module;               // the mod's compiled module (a .qvm); empty: `program`
  // The mod's game library of Quake II's interface (server/q2_native.hpp), which plays in
  // place of both on any map, classic remix's too; empty: none.
  std::string native_library;
  std::shared_ptr<cl::LoadedMap> map;  // the spec's models point into it: outlives `rules`
  sv::RulesRunner rules;               // reads `progs`
  std::unique_ptr<QwServer> qw;        // serves `rules` once the core hands it the port
  ServerConfig cfg;
  gs::ServerConfig settings;  // what the server was opened with: every map is made from it
  cl::SearchPath files;       // the game folder the maps are read from (empty: the arena only)
  int changes = 0;            // maps changed to
  std::string failed;         // why the last change did not happen ("": it did)
  std::string held_map;       // the next map, while the intermission keeps the scores up
  // The lobby and the bots (sv_lobby, sv_bots; null: neither). Without sv_lobby it stays in its
  // match: the bots play every map as it comes.
  std::unique_ptr<cl::Room> room;
  // Quake II's companions outside deathmatch (`addbot` on its own rules; null: none ever).
  std::unique_ptr<cl::Q2Party> mates;
  std::array<const sv::RulesCommon*, gs::kLobbySeats> applied{};  // the rules a seat's team is on
  std::array<gs::Team, gs::kLobbySeats> shown{};                  // the team its user info says
  int said = 0;                                                   // the lobby's lines printed
  OvertimeRun overtime;  // a lobby's match tied at its time limit (qw_match_rules.hpp)
  gs::HomeWatch home;    // an empty server's wait before its home map (sv_homemap)
  std::array<double, gs::kLobbySeats> held_told{};  // when a held fire was last told, a seat
  // With a QuakeWorld game's packs in the folder: its item names and server words as local info
  // (profiles/items.hpp, profiles/words.hpp), set on every map before the rules start.
  std::vector<std::string> info_lines;
  // The masters it reports to (`setmaster`, `heartbeat`; classic/protocol/masters.hpp), and
  // what they said for the console.
  ge::classic::protocol::Masters masters;
  std::vector<std::string> masters_said;
  // A listen server (ServerConfig::host): the window's content and its view of the game; slot 0
  // its player, a local seat. Null on a dedicated server.
  cl::ListenHost* host = nullptr;
  // A dedicated server with `sv_coop 1`: the campaign in coop in place of the match (every call
  // below hands over to it; the match's own members stay empty).
  std::unique_ptr<CoopGame> coop;
  // The players' votes on the settings for this map (engine/server/vote.hpp): a passed one is
  // written into `settings.rules`, so a restart keeps it, and put back when the map changes.
  gs::VoteBook vote;
  // Timeouts in the match (engine/server/timeout.hpp): the rules paused while one is on, by the
  // server's clock (`now_s`, which runs on through the pause).
  gs::TimeoutBook timeout;
  bool timeout_held = false;  // the rules paused for it
  double now_s = 0;
  // The stream port for QuakeWorld relays (qw_qtv.hpp; qtv_streamport 0: none).
  int qtv_port = 0;
  QtvConfig qtv_cfg;
  std::unique_ptr<QtvPort> qtv;
  // The logins (sv_login, qw_login.hpp; null: none asked and no service handed in).
  std::unique_ptr<QwLogin> login;
  // The match being played, tallied each frame, and the report of the one that just ended (taken
  // once by the core: `match_report`).
  MatchTally tally;
  CheatWatch cheats;     // the speed check's cuts scored (qw_cheats.hpp), into the report
  StatsEyes stats_eyes;  // the weapon stats' last frame (qw_stats.hpp)
  ge::net::MatchReport report;
  bool report_ready = false;
  // The match as the core records it (qw_game_tv.hpp): a stream of the server's own.
  std::unique_ptr<QwTv> tv = std::make_unique<QwTv>();
  ~Game() {
    if (qtv && qw) qtv_close(*qtv, *qw);
    if (qw) qw_tv_stop(*tv, *qw);
    qtv.reset();
    coop.reset();
    qw.reset();
    rules = {};  // its views into the map go before the map does
    map.reset();
  }
};

void host_bind(Game& g);
// A line printed to a listen server's local seat: its own player's (slot 0) goes to the window.
void host_print(void* ctx, int slot, const std::string& text) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (slot == 0 && h.print != nullptr) h.print(h.ctx, text);
}

// A voice the endpoint relayed to a listen server's own player (slot 0), to the window.
void host_voice(void* ctx, int slot, const SvcMsg& v) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (slot == 0 && h.heard != nullptr && v.ints.size() >= 3)
    h.heard(h.ctx, v.ints[0], v.ints[1], v.ints[2], v.data);
}

// The rules' frame handed to a listen server's window (its player's prints, effects, blows).
void host_tap(void* ctx, const sv::RulesCommon& game) {
  const cl::ListenHost& h = *static_cast<const cl::ListenHost*>(ctx);
  if (h.tap != nullptr) h.tap(h.ctx, game);
}

// The mod's own rules a server owner sets (server.cfg, `+set fraglimit 20`); the mod server keeps
// each one's default (server/qc_server.cpp), so an unset rule is left to it. The move's and the
// loop's settings (plugins/shooter/settings.hpp: `sv_gravity`, `sv_mintic`, `pm_ktjump`) follow,
// then the endpoint's own, which the mod never sees.
constexpr gs::GameRule kModRules[] = {
    {"deathmatch", "The match's rules as the mod reads them.", nullptr,
     "0: no deathmatch, the mod's own game; 1..N: that deathmatch mode, as the mod numbers them"},
    {"teamplay", "The team rules as the mod reads them.", nullptr,
     "0: every player for himself; 1..N: that team mode, as the mod numbers them"},
    {"fraglimit", "The frags a player needs to end the match.", nullptr,
     "0: no limit; 1..N: that many frags"},
    {"timelimit", "The match's length in minutes.", nullptr,
     "0: no limit; 1..N: that many minutes"},
    {"samelevel", "Whether the map stays the same when the match ends.", nullptr,
     "0: the next map follows; 1: the same map again"},
    {"pausable", "Whether a player may pause the game.", nullptr,
     "0: only unpausing; 1: players may pause"},
    // The aim help's weapons (server/shot_help.hpp): names, spaces between, `*` any characters.
    {"sv_aimhelp_allow", "The weapons the aim help may bend, by name with spaces between.", nullptr,
     "\"\": every weapon but the denied (the default); TEXT: only these, * matching any "
     "characters"},
    {"sv_aimhelp_deny", "The weapons the aim help never bends, by name with spaces between.",
     nullptr, "\"\": none (the default); TEXT: these, such as rl lg"},
    // Quake II's rules only; a map without brushes plays by the mod's own and leaves it alone.
    {"dmflags", "Quake II's deathmatch rule bits, which Quake's maps ignore.", nullptr,
     "0: none; N: the sum of the bits, such as 8192 for infinite ammunition"},
    // The rocket jump helpers (server/rjump_rules.hpp): a mod's rule, live and voted per map.
    {"sv_rjump", "Whether players may use the rocket jump helper, +rjump.", nullptr,
     "0: not allowed (the default); 1: allowed"},
    {"sv_rrjump", "Whether players may use the reverse rocket jump helper, +rrjump.", nullptr,
     "0: not allowed (the default); 1: allowed"},
    {"k_midair", "Whether rockets count only on a player in the air.", nullptr,
     "0: every hit counts (the default); 1: midair only, the map restarts"},
    {"k_midair_minheight", "The least height a midair hit counts at.", nullptr,
     "0: 64 units; 1: 128 units (the default); 2: 256 units; 3: 512 units; 4: 1024 units"},
    // The server's own chat when the mod leaves it to the server (server/chat_rules.hpp).
    {"sv_spectalk", "Whether a spectator's public chat reaches the players during a match.",
     nullptr,
     "0: during a match only the other spectators hear it; 1: everyone hears it (the default)"},
    {"sv_sayteam_to_spec",
     "When spectators read the team messages of the team they follow (never a line typed by hand).",
     nullptr, "0: never; 1: during a match (the default); 2: outside a match; 3: always"},
    {"cheats", "Whether the server runs with cheats, which its info then says as *cheats ON.",
     nullptr, "0: no cheats (the default); 1: cheats, set at start"},
    {"sv_mapcheck", "Whether a player whose map file differs from the server's is turned away.",
     nullptr, "0: anyone joins; 1: only the server's map (the default)"},
    {"sv_highchars", "Whether a joining player's name and info may hold coloured characters.",
     nullptr, "0: only plain letters, digits and signs are kept; 1: any (the default)"},
};

// Every rule, in the order `--explain` lists them: built once.
const std::vector<gs::GameRule>& all_rules() {
  static const std::vector<gs::GameRule> rules = [] {
    std::vector<gs::GameRule> out(std::begin(kModRules), std::end(kModRules));
    int count = 0;
    const sh::Setting* table = sh::settings_table(count);
    for (int i = 0; i < count; i++)
      out.push_back({table[i].name, table[i].help, nullptr, table[i].values});
    for (const gs::GameRule& r : endpoint_rules()) out.push_back(r);
    out.insert(out.end(), std::begin(gs::kOvertimeRules), std::end(gs::kOvertimeRules));
    // The rest of what every classic mod's runner registers (server/rules_runner.hpp), so a
    // server.cfg may set them and the check knows them. Not `hostname`, which is the core's
    // sv_hostname by another name, nor `sv_progs`, which only a test match reads.
    for (const srv::ModSetting& m : srv::mod_settings()) {
      if (m.name == nullptr || std::string_view(m.name) == "hostname" ||
          std::string_view(m.name) == "sv_progs")
        continue;
      const auto same = [&m](const gs::GameRule& r) { return std::string_view(r.name) == m.name; };
      if (std::none_of(out.begin(), out.end(), same))
        out.push_back({m.name, m.help, nullptr, m.values, m.info});
    }
    return out;
  }();
  return rules;
}

// Each QuakeWorld game's own names for the server's settings (gs::SettingName): a server.cfg,
// `+set`, rcon or vote line by either name sets the one setting. Both games share the names they
// have in common, so one table serves both: Quake's alone (floodprot, floodprotmsg), the
// Quake II's alone (sv_maplist, the three flood words), and both (hostname, maxclients, ...).
constexpr gs::SettingName kSettingNames[] = {
    {"hostname", "sv_hostname"},
    {"maxclients", "sv_maxclients"},
    {"maxspectators", "sv_maxspectators"},
    {"password", "sv_password"},
    {"spectator_password", "sv_spectatorpassword"},
    {"timeout", "sv_timeout"},
    {"floodprot", "sv_floodprot"},
    {"floodprotmsg", "sv_floodprotmsg"},
    {"sv_maplist", "sv_maprotation"},
    {"flood_msgs", "sv_floodprot", 0},
    {"flood_persecond", "sv_floodprot", 1},
    {"flood_waitdelay", "sv_floodprot", 2},
    {"allow_download", "sv_download"},
    {"allow_download_skins", "sv_download_skins"},
    {"allow_download_models", "sv_download_models"},
    {"allow_download_sounds", "sv_download_sounds"},
    {"allow_download_maps", "sv_download_maps"},
    {"allow_download_other", "sv_download_other"},
    {"sv_downloadchunksperframe", "sv_downloadchunks"},
    {"sv_default_name", "sv_defaultname"},
    {"sv_forcenick", "sv_loginname"},
    {"sv_registrationinfo", "sv_loginwelcome"},
    {"sv_forcespec_onfull", "sv_fullspectate"},
    {"sv_reconnectlimit", "sv_reconnectwait"},
    {"sv_enable_cmd_minping", "sv_minpingcmd"},
    {"countrycode", "sv_country"},
    {"city", "sv_city"},
    {"coords", "sv_coords"},
    {"hostport", "sv_publicaddr"},
    {"sv_admininfo", "sv_admin"},
    {"watervis", "sv_watervis"},
    {"sv_kicktop", "sv_colorkick"},
    {"sv_kickuserinfospamcount", "sv_infoflood", 0},
    {"sv_kickuserinfospamtime", "sv_infoflood", 1},
    {"sv_antilag_no_pred", "sv_antilagexact"},
    {"sv_voip_echo", "sv_voipecho"},
    {"sv_crypt_rcon", "sv_rconcrypt"},
    {"sv_timestamplen", "sv_rcontime"},
    {"sv_rconlim", "sv_rconrate"},
    {"filterban", "sv_banmode"},
    {"vip_password", "sv_vippassword"},
    {"vip_values", "sv_vipvalues"},
    {"maxvip_spectators", "sv_maxvipspectators"},
};

// `*mods`: each package's name and version, ", " between, cut with "+N more" past what one
// serverinfo value should take of the whole string's 512 bytes.
std::string mods_info(const std::vector<std::unique_ptr<ge::mod::LoadedMod>>& packages) {
  constexpr size_t kMax = 120;
  std::string out;
  for (size_t k = 0; k < packages.size(); k++) {
    const ge::mod::Manifest& m = packages[k]->manifest;
    std::string one = m.name + " " + m.version;
    const std::string more = " +" + std::to_string(packages.size() - k) + " more";
    if (out.size() + 2 + one.size() > kMax - (k + 1 < packages.size() ? more.size() : 0)) {
      out += more;
      break;
    }
    if (!out.empty()) out += ", ";
    out += one;
  }
  return out;
}

// The server's settings a mod reads by its game's name (`cvar("maxclients")`), put in the rules'
// own table before they start: each mod sees the server it runs on, not a default of its own.
void game_names(sv::RulesCommon& rules, const gs::ServerConfig& c) {
  const struct {
    const char* name;
    std::string value;
    const char* help;
  } values[] = {
      {"hostname", c.hostname, "The server's name, which is sv_hostname."},
      {"maxclients", std::to_string(c.maxclients), "The most players, which is sv_maxclients."},
      {"maxspectators", std::to_string(c.max_spectators),
       "The most spectators, which is sv_maxspectators."},
  };
  for (const auto& v : values) {
    if (rules.cvars.find(v.name) == nullptr)
      rules.cvars.add({v.name, v.value, ge::console::kServer, v.help, ge::console::CvarKind::text});
    else
      rules.cvars.set(v.name, v.value, ge::console::Source::code);
  }
}

// A file of the game's search path, for a compiled module's reads.
bool read_game_file(void* ctx, const std::string& name, std::vector<uint8_t>& bytes) {
  const auto& files = *static_cast<const cl::SearchPath*>(ctx);
  std::vector<uint8_t> store;
  const std::span<const uint8_t> found = cl::find_file(files, name, store);
  if (found.empty()) return false;
  bytes.assign(found.begin(), found.end());
  return true;
}

// The loop's client slots: the players' and, when spectators have their own, theirs.
int slots(const gs::ServerConfig& c) {
  int vip = 0;
  if (const auto v = c.rules.find("sv_maxvipspectators"); v != c.rules.end())
    vip = std::max(0, std::atoi(v->second.c_str()));
  return std::min(sh::kMaxClients, c.maxclients + std::max(0, c.max_spectators) + vip);
}

// A save of Quake II's game the window asked to load (ListenHost::load): asked of `rules`, the
// next start loads it. False with `error` when the save is refused.
bool ask_load(const Game& g, const sv::RulesRunner& rules, std::string& error) {
  if (g.host == nullptr || g.host->load.empty()) return true;
  sv::Q2Server* sq = sv::q2_of(rules);
  const std::vector<uint8_t> bytes = std::move(g.host->load);
  g.host->load.clear();
  if (sv::NativeQ2* nq = sv::native_q2_of(rules); nq != nullptr)  // a mod's game library's game
    return !sv::native_q2_load_ask(*nq, bytes, g.host->load_name, error).empty();
  if (sq == nullptr) {
    error = g.host->load_name + ": only Quake II's maps load its saves";
    return false;
  }
  return !sv::q2_load_ask(*sq, bytes, g.host->load_name, error).empty();
}

// The mod's rules on the map `name`, set from the settings: into `map` and `rules`.
// A listen server's map is the one the window read (`supplied`), or the one playing again.
// A dedicated server's next map keeps what the mod kept (`carry_from`), not the server's settings.
bool open_map(const Game& g, const std::string& name, std::shared_ptr<cl::LoadedMap>& map,
              sv::RulesRunner& rules, std::string& error,
              std::shared_ptr<cl::LoadedMap> supplied = nullptr,
              const sv::RulesRunner* carry_from = nullptr) {
  const gs::ServerConfig& c = g.settings;
  if (supplied)
    map = std::move(supplied);
  else if (g.host != nullptr && g.map && g.map->name == name)
    map = g.map;
  else
    map = cl::load_named_map(g.files, name, error);
  if (!map) return false;
  const float speedjump = g.host != nullptr ? g.host->speedjump : 0.0f;  // pm_speedjump
  // A map of the family that carries brushes runs on the rules that trace them (Quake II's),
  // whatever the program: the programs' rules trace clip hulls (docs/goals/M18v.md, M18v2).
  // Classic remix (client/remix.hpp) plays such a map on the program's rules instead, its models
  // traced through their brushes (`program_rules`).
  const bool brushes = map->spec.brushes != nullptr && !map->program_rules;
  if (!g.native_library.empty()) {
    // The mod's own rules: its game library hosted unchanged, on the map's brushes or its clip
    // hulls (q2_native.hpp).
    sv::NativeQ2Options no;
    no.library = g.native_library;
    no.max_clients = slots(c);
    no.game = std::filesystem::path(g.native_library).parent_path().filename().string();
    no.settings.emplace_back("deathmatch", "1");
    no.settings.emplace_back("hostname", c.hostname);
    for (const auto& [rule, value] : c.rules)  // a password stays the server's own
      if (!value.empty() && rule != "lrcon_password") no.settings.emplace_back(rule, value);
    std::unique_ptr<sv::NativeQ2> ns = sv::load_native_q2(no, error);
    if (!ns) return false;
    if (!map->file.empty()) ns->map_checksum = sv::q2_map_checksum(map->file);
    rules = sv::native_q2_runner(std::move(ns));
  } else if (brushes) {
    sv::Q2ServerOptions so;
    so.seed = c.seed;
    so.clients_take_view = true;  // the QuakeWorld protocol's clients take the server's view
    so.speedjump = speedjump;
    std::unique_ptr<sv::Q2Server> sq = sv::make_q2_server(so);
    sq->sv.set.maxclients = slots(c);
    rules = sv::q2_runner(std::move(sq));
  } else if (!g.module.empty()) {
    sv::QvmServerOptions so;
    so.seed = c.seed;
    so.loop.maxclients = slots(c);
    so.loop.max_edicts = 1024;
    so.loop.move.speedjump = speedjump;
    so.read_file = &read_game_file;
    so.files_ctx = const_cast<cl::SearchPath*>(&g.files);
    so.list_dirs = {g.files.folder};
    if (!c.root.empty()) so.list_dirs.push_back(c.root + "/" + kGameFolder);
    so.write_dir = g.files.folder;
    std::unique_ptr<sv::QvmServer> qvm = sv::make_qvm_server(g.module, so, error);
    if (!qvm) return false;
    rules = sv::qvm_runner(std::move(qvm));
  } else {
    sv::QcServerOptions so;
    so.seed = c.seed;
    so.loop.maxclients = slots(c);
    so.loop.max_edicts = 1024;
    so.loop.move.speedjump = speedjump;
    std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(*g.program, so, error);
    if (!qc) return false;
    rules = sv::qc_runner(std::move(qc));
  }
  if (carry_from != nullptr && *carry_from && g.host == nullptr)
    sv::carry_server_state(**carry_from, carry_from->calls->form, *rules, rules.calls->form,
                           &c.rules);
  if (g.host != nullptr) {  // the window's own lines: the items' names, the arsenal, its settings
    for (const std::string& line : g.host->lines) rules.console(line);
  } else {
    rules.console("deathmatch 1");
    // A mod checks it runs from its own folder by the server's `*gamedir`.
    if (g.cfg.gamedir != decltype(g.cfg){}.gamedir)
      rules.console("serverinfo *gamedir \"" + g.cfg.gamedir + "\"");
    for (const std::string& line : g.info_lines) rules.console(line);
  }
  // The C++ mods it runs, for a browser and a hub to show (docs/mods.md § Distribution).
  if (const std::string mods = mods_info(g.packages); !mods.empty())
    rules.console("serverinfo *mods \"" + mods + "\"");
  game_names(*rules, c);
  // The settings a config made with `set` (a match mod's switches), before the map starts.
  if (!brushes && g.native_library.empty())
    for (const auto& [setting, value] : c.created)
      rules.console(std::string("set ").append(setting).append(" \"").append(value).append("\""));
  for (const auto& [rule, value] : c.rules) {
    // A module checks a client's rate against the endpoint's cap, so it is told the cap.
    if (rule == "sv_maxrate" && !g.module.empty() && !value.empty() && value != "0")
      rules.console("sv_maxrate \"" + value + "\"");
    if (value.empty() || is_endpoint_rule(rule)) continue;
    if (rule == "dmflags" && !brushes) continue;  // Quake II's alone
    sh::ServerSettings check;
    if (sh::find_setting(rule) && !sh::set_setting(check, rule, value, error)) {
      error.append(" (--explain=").append(rule).append(" shows where it was set)");
      return false;
    }
    char* end = nullptr;
    std::strtod(value.c_str(), &end);  // every rule is a number but the aim help's lists
    std::string line = rule;
    line.append(" \"").append(value).append("\"");
    // Quake II's rules keep their own few settings and leave the rest of a mod's alone.
    const bool taken = rules.console(line) || brushes;
    const bool text =
        rule == "sv_aimhelp_allow" || rule == "sv_aimhelp_deny" || rule == "sv_playermove";
    if ((!text && (end == value.c_str() || *end != '\0')) || !taken) {
      error = line;
      error.append(" is not a number (--explain=");
      error.append(rule).append(" shows where it was set)");
      return false;
    }
  }
  for (const auto& p : g.packages) {  // the C++ mods, laid over the rules as the map starts
    rules->mods.tables.push_back(p->table);
    rules->mods.packages.push_back(p.get());
    rules->mods.names.push_back(p->manifest.name);
  }
  rules->mods.seed = c.seed;
  if (!ask_load(g, rules, error) || !rules.start(map->spec, error)) {
    error = "the map " + name + " did not load: " + error;
    return false;
  }
  if (map->from_file) sv::use_map_sets(*rules, map->bsp);
  return true;
}

// Warmup's hold on the mod's limits: nothing ends a warmup but the lobby.
void hold_limits(const sv::RulesRunner& rules) {
  rules.console("fraglimit 0");
  rules.console("timelimit 0");
}

// The room: the lobby (a player needed before a countdown), the `sv_bots` fighters seated.
bool open_room(Game& g, std::string& error) {
  const gs::ServerConfig& c = g.settings;
  if (c.bots == 0 && c.lobby == 0) return true;
  g.room = std::make_unique<cl::Room>();
  cl::Room& r = *g.room;
  r.seed = c.seed;
  r.lobby.settings.humans = 1;  // bots alone warm up until a player joins
  r.lobby.settings.countdown_s = c.countdown_s;
  r.lobby.settings.intermission_s = c.intermission_s;
  const ge::console::Cvar* teamplay = g.rules->cvars.find("teamplay");
  lobby_rules(r.lobby.settings, c, teamplay != nullptr && teamplay->on());
  if (g.host != nullptr) {  // the window's own player has seat 0, the bots the ones after
    r.lobby.settings = g.host->lobby_settings;
    gs::lobby_join(r.lobby, 0, g.host->player, false, g.rules->sv.time);
  }
  if (c.lobby == 0) r.lobby.phase = gs::LobbyPhase::match;
  // The bots find their way on clip hulls; on a map of brushes they wait for their lift
  // (docs/goals/M18v.md, M18v8), and the match plays without them.
  const int bots = g.map->spec.brushes != nullptr ? 0 : c.bots;  // a remix's map of brushes too
  if (bots < c.bots)
    g.rules->console.push_back("The bots do not play this kind of map yet, so the match has none.");
  for (int i = 0; i < bots; i++)
    if (cl::room_add_bot(r, g.rules, g.map->spec, sb::AimSkill::average, error) < 0) {
      std::string why = "sv_bots " + std::to_string(c.bots) + ": ";
      why.append(error).append(" (keep sv_bots under sv_maxclients)");
      error = std::move(why);
      return false;
    }
  if (r.lobby.phase != gs::LobbyPhase::match) hold_limits(g.rules);
  return true;
}

// The mod on top: the last of `sv_mod`'s comma-separated folders ("" when none is named).
std::string top_mod(const std::string& mods) {
  const size_t comma = mods.find_last_of(',');
  std::string top = comma == std::string::npos ? mods : mods.substr(comma + 1);
  while (!top.empty() && top.back() == ' ') top.pop_back();
  while (!top.empty() && top.front() == ' ') top.erase(0, 1);
  return top;
}

// The C++ mod packages among `sv_mod`'s comma-separated folders (a folder holding a
// `mod.manifest`, under `--root` or as given), loaded in order into `g.packages`; the other folders
// are left in `folders` for the game folder. False with `error` when a package is refused.
bool load_packages(Game& g, const gs::ServerConfig& c, std::string& folders, std::string& error) {
  std::string rest;
  size_t at = 0;
  while (at <= c.mod.size()) {
    size_t comma = c.mod.find(',', at);
    if (comma == std::string::npos) comma = c.mod.size();
    std::string folder = c.mod.substr(at, comma - at);
    at = comma + 1;
    while (!folder.empty() && folder.back() == ' ') folder.pop_back();
    while (!folder.empty() && folder.front() == ' ') folder.erase(0, 1);
    if (folder.empty()) continue;
    std::filesystem::path dir = std::filesystem::path(c.root) / folder;
    std::error_code ec;
    if (!std::filesystem::exists(dir / "mod.manifest", ec)) dir = folder;
    if (!std::filesystem::exists(dir / "mod.manifest", ec)) {
      if (!rest.empty()) rest += ',';
      rest += folder;
      continue;
    }
    auto loaded = std::make_unique<ge::mod::LoadedMod>();
    const ge::mod::LoadResult r = ge::mod::load_mod(dir, {SE_MOD_GAME, SE_MOD_API}, *loaded);
    if (!r.ok()) {
      error = "the mod package " + folder + " was refused: " + r.errors.front();
      return false;
    }
    g.packages.push_back(std::move(loaded));
  }
  folders = std::move(rest);
  return true;
}

// The program: a named mod's own (its compiled module `qwprogs.qvm` when it has one, else its
// folder's program files through the game's adapter, the base game's under it), else the
// baseline mod.
bool open_program(Game& g, const std::string& mod, std::string& error) {
  if (mod.empty() || mod == kGameFolder) return cl::baseline_progs(g.progs, error);
  if (read_game_file(&g.files, "qwprogs.qvm", g.module)) return true;
  const cl::GameAdapter& game = cl::qw_adapter();
  cl::FoundProgram found;
  std::string why;
  if (game.find_program == nullptr || !game.find_program(game, g.files, false, found, why)) {
    error = "the mod folder " + mod + " holds no multiplayer program this server runs" +
            (why.empty() ? std::string() : " (" + why + ")") + ". Put the mod's qwprogs.dat in " +
            g.files.folder + ", or leave sv_mod empty for the baseline mod.";
    return false;
  }
  g.progs = std::move(found.progs);
  // `*progs`: the program file's 16-bit CRC, so a browser tells one build of a mod from another.
  std::vector<uint8_t> bytes;
  for (const char* name : {"qwprogs.dat", "progs.dat"})
    if (read_game_file(&g.files, name, bytes)) {
      g.progs_crc = std::to_string(ge::classic::protocol::crc16(bytes.data(), bytes.size()));
      break;
    }
  return true;
}

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  g->settings = c;
  std::string folders;  // sv_mod's game folders, the packages taken out
  if (!load_packages(*g, c, folders, error)) return nullptr;
  if (c.host == nullptr && coop_asked(c)) {
    if (!g->packages.empty()) {
      error =
          "C++ mods can't run in a coop campaign yet. Take the package out of sv_mod, or set "
          "sv_coop to 0.";
      return nullptr;
    }
    g->coop = coop_open(c, error);
    return g->coop ? g.release() : nullptr;
  }
  if (c.host != nullptr) {  // a listen server: the window's program, folder and first map
    g->host = static_cast<cl::ListenHost*>(c.host);
    g->program = g->host->progs;
    if (g->host->module != nullptr) g->module = *g->host->module;  // a mod's compiled module
    g->native_library = g->host->native_library;
    if (g->host->files != nullptr) g->files = *g->host->files;
    g->map = g->host->map;
  } else {
    const std::string mod = top_mod(folders);
    const std::string folder = mod.empty() ? std::string(kGameFolder) : mod;
    // The built-in arena needs no game folder; any other map, a rotation's or a mod's, is read
    // from one (the mod's over the base game's).
    if (!cl::open_game_dir(c.root, folder, g->files, error)) {
      if (c.map != "arena" || !mod.empty()) return nullptr;
      g->files = cl::SearchPath{};
      error.clear();
    }
    std::filesystem::path lib;
    if (!mod.empty() && sv::find_native_library(c.root, mod, mod, lib)) {
      g->native_library = lib.string();  // the mod's game library plays; the baseline stands by
      if (!cl::baseline_progs(g->progs, error)) return nullptr;
    } else if (!open_program(*g, mod, error)) {
      return nullptr;
    }
    if (!mod.empty()) g->cfg.gamedir = mod;  // serverdata names it: a client mounts the mod too
    if (!cl::archive_paths(g->files).empty()) {
      const profiles::GameProfile& game = profiles::q1_game();
      g->info_lines = profiles::item_name_lines(game.tables.names);
      for (std::string& line : profiles::word_lines(game.words)) g->info_lines.push_back(line);
      if (!game.arena_rules.empty())
        g->info_lines.push_back("localinfo rules " + std::string(game.arena_rules));
    }
  }
  if (!open_map(*g, c.map, g->map, g->rules, error)) return nullptr;
  if (!open_room(*g, error)) return nullptr;
  g->vote.settings = gs::vote_settings(c);
  g->timeout.settings = gs::timeout_settings(c);
  if (g->host != nullptr) host_bind(*g);
  g->cfg.hostname = c.hostname;
  g->cfg.info = {{"countrycode", c.country},
                 {"city", c.city},
                 {"coords", c.coords},
                 {"hostport", c.public_addr},
                 {"*admin", c.admin}};
  if (const auto w = c.rules.find("sv_watervis"); w != c.rules.end() && w->second == "1")
    g->cfg.info.emplace_back("watervis", "1");
  // A client predicts its own move, so it is told when the server holds its strafes apart.
  if (const auto w = c.rules.find("sv_safestrafe");
      w != c.rules.end() && !w->second.empty() && w->second != "0")
    g->cfg.info.emplace_back("sv_safestrafe", w->second);
  if (const auto w = c.rules.find("cheats"); w != c.rules.end() && w->second == "1")
    g->cfg.cheats = true;
  if (const auto w = c.rules.find("sv_highchars"); w != c.rules.end() && w->second == "0")
    g->cfg.high_chars = false;
  if (const auto w = c.rules.find("sv_mapcheck"); w != c.rules.end() && w->second == "0")
    g->cfg.map_check = false;
  g->cfg.progs_crc = g->progs_crc;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.rcon_password = c.rcon_password;
  g->cfg.unix_time = [] { return static_cast<int64_t>(std::time(nullptr)); };  // rcon, bans
  // The ban list beside the game's files, where a dedicated server keeps its own (writeip).
  if (g->host == nullptr && !g->files.folder.empty()) {
    g->cfg.ban_file = (std::filesystem::path(g->files.folder) / "listip.cfg").string();
    g->cfg.frag_dir = g->files.folder;  // fraglogfile's frag_<n>.log
    g->cfg.snap_dir = (std::filesystem::path(g->files.folder) / "snap").string();  // snap
    g->cfg.vip_file = (std::filesystem::path(g->files.folder) / "vip_ip.cfg").string();
  }
  g->cfg.seed = c.seed;
  g->cfg.timeout_s = c.timeout_s;
  g->cfg.flood_msg = c.floodprot_msg;
  if (!gs::parse_flood_rule(c.floodprot, g->cfg.flood, error)) return nullptr;
  if (!endpoint_settings(c, g->cfg, error)) return nullptr;
  if (!qtv_settings(c, g->qtv_port, g->qtv_cfg, error)) return nullptr;
  return g.release();
}

// The map after `current` when a match ends: the next in `sv_maprotation` (round to its first
// after its last; a map not in it goes to its first), else the one the mod asked for.
std::string next_map(const gs::ServerConfig& c, const std::string& current,
                     const std::string& asked) {
  std::vector<std::string> rotation;
  size_t at = 0;
  while (at < c.maprotation.size()) {
    const size_t space = c.maprotation.find(' ', at);
    const size_t stop = space == std::string::npos ? c.maprotation.size() : space;
    if (stop > at) rotation.push_back(c.maprotation.substr(at, stop - at));
    at = stop + 1;
  }
  if (rotation.empty()) return asked;
  for (size_t i = 0; i < rotation.size(); i++)
    if (rotation[i] == current) return rotation[(i + 1) % rotation.size()];
  return rotation.front();
}

// ---- The players' votes on the settings (engine/server/vote.hpp) ----
//
// What may be voted on here: the mod's rules (`fraglimit`, `teamplay`) and the move's and the
// loop's settings (`sv_gravity`, `pm_speedjump`), past the vote's lists; never the endpoint's own
// (read when the server starts). A passed change is written into `settings.rules`, which every map
// is made from: a mod's rule is set in the running rules at once (a limit the warmup holds waits
// for the match), and a setting the players' own moves are predicted with restarts the map, so
// every client is told it as it signs on again, as does midair (`k_midair`, which takes the items
// off the map); `k_midair_minheight` is told in the mode's words. A restart keeps the change; the
// next map puts it back.
//
// `vote <aid> <id|name> on|off|<value>` grants one player an aid for this map (`speedjump`,
// `aimhelp`, `rjump`: qw_admin.hpp's grants, engine/server/aids.hpp), or takes it back. The
// change is keyed `<aid>:<user id>`, so two votes on one player are one change and two players are
// two; its value names the player and the grant (`Ann on`, `Ann 0.5`, `Ann off`), its `before`
// what he had. The next map puts each back; a player who left is passed over.

// A rule the warmup holds at 0 (hold_limits) while the lobby waits for its match.
bool held_now(const Game& g, std::string_view name) {
  return g.room && g.settings.lobby != 0 && g.room->lobby.phase != gs::LobbyPhase::match &&
         (name == "fraglimit" || name == "timelimit");
}

// `vote <aid> <id|name> on|off|<value>`: the player named, and the grant asked for him.
bool aid_vote_check(const Game& g, const gs::AidKind& kind, std::string_view value,
                    gs::VoteCheck& out) {
  std::string who, grant;
  if (!g.qw) {
    out.error = std::string(kind.words) + " is granted only on a server players join.";
    return false;
  }
  if (!gs::aid_vote_split(kind, value, who, grant, out.error)) return false;
  const QwServer& cs = *g.qw;
  const int slot = qw_find_seat(cs, who);
  if (slot < 0) {
    out.error = "No player is called " + who + ". Type \"users\" to see the ids and names.";
    return false;
  }
  const QwClient& c = cs.clients[slot];
  out.key = gs::aid_vote_key(kind, c.userid);
  out.value = c.name + " " + gs::aid_vote_word(kind, grant);
  out.before = c.name + " " + gs::aid_vote_word(kind, qw_aid_granted(cs, slot, kind));
  return true;
}

// The aid a vote's change is about (its name), or null.
const gs::AidKind* vote_aid(std::string_view name) { return gs::aid_find(qw_aids(), name); }

// A passed or put-back aid change made: the grant set on the user id its key names, while he is on
// the server.
void aid_vote_apply(Game& g, const gs::AidKind& kind, const gs::VoteChange& c) {
  if (!g.qw) return;
  const int userid = std::atoi(c.key.c_str() + std::string_view(kind.name).size() + 1);
  const size_t space = c.value.find_last_of(' ');
  std::string grant, error;
  gs::aid_value(kind, space == std::string::npos ? c.value : c.value.substr(space + 1), grant,
                error);
  qw_grant(*g.qw, kind, userid, grant, error);
}

// `vote kick <player>` and `vote map <name>`: acts, not settings, so nothing is put back at the
// map's end.
bool act_vote_check(const Game& g, std::string_view name, std::string_view value,
                    gs::VoteCheck& out) {
  const std::string text(value);
  out.act = true;
  if (name == "map") {
    std::vector<uint8_t> store;
    if (text.empty() || text.find_first_of("/\\.") != std::string::npos ||
        (text != "arena" && cl::find_file(g.files, "maps/" + text + ".bsp", store).empty())) {
      out.error = "There is no map called " + text + " on this server.";
      return false;
    }
    out.before = g.map ? g.map->name : std::string();
    return true;
  }
  if (!g.qw) {
    out.error = "Players are kicked only on a server players join.";
    return false;
  }
  const QwServer& cs = *g.qw;
  const int slot = qw_find_seat(cs, text);
  if (slot < 0) {
    out.error = "No player is called " + text + ". Type \"users\" to see the ids and names.";
    return false;
  }
  const QwClient& c = cs.clients[slot];
  if (c.local) {
    out.error = c.name + (info_get(c.userinfo, "*bot").empty()
                              ? " is the host's own player and can't be kicked."
                              : " is a bot. The server's bots are set by its owner.");
    return false;
  }
  out.key = "kick " + std::to_string(c.userid);
  out.value = c.name;
  return true;
}

bool vote_check(void* ctx, std::string_view name, std::string_view value, gs::VoteCheck& out) {
  const Game& g = *static_cast<const Game*>(ctx);
  if (const gs::AidKind* aid = vote_aid(name)) return aid_vote_check(g, *aid, value, out);
  if (name == "kick" || name == "map") return act_vote_check(g, name, value, out);
  const std::string rule(name), text(value);
  bool known = false;
  for (const gs::GameRule& r : all_rules()) known = known || rule == r.name;
  if (!known) {
    out.error = rule +
                " is not a setting of this server. Type \"vote rules\" to see what may be "
                "voted on (map <name>, kick <player>, fraglimit, timelimit, teamplay, sv_gravity, "
                "pm_speedjump, sv_rjump, speedjump <player> on, aimhelp <player> on...).";
    return false;
  }
  if (is_endpoint_rule(rule)) {
    out.error = rule + " is set when the server starts, so it can't be voted on";
    return false;
  }
  if (const sh::Setting* setting = sh::find_setting(rule); setting != nullptr) {
    sh::ServerSettings check;
    std::string error;
    if (!sh::set_setting(check, rule, text, error)) {
      out.error = error;
      return false;
    }
  } else if (rule == "sv_aimhelp_allow" || rule == "sv_aimhelp_deny") {
    // A list of weapons' names: any text.
  } else {
    char* end = nullptr;
    std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
      out.error = rule + " \"" + text + "\" is not a number";
      return false;
    }
  }
  if (rule == "timelimit" || rule == "fraglimit") {  // the cap and the one limit left
    const ge::console::Cvar* frags = g.rules->cvars.find("fraglimit");
    const ge::console::Cvar* time = g.rules->cvars.find("timelimit");
    out.error =
        gs::vote_limit_refusal(rule, std::strtod(text.c_str(), nullptr), frags ? frags->number : 0,
                               time ? time->number : 0, g.settings.timelimit_max);
    if (!out.error.empty()) return false;
  }
  const auto it = g.settings.rules.find(rule);
  out.before = it == g.settings.rules.end() ? std::string() : it->second;
  return true;
}

// The vote's lines printed (each to everyone, or to the one player), and its passed changes made.
void vote_turn(Game& g) {
  gs::VoteBook& b = g.vote;
  if (g.qw)
    for (size_t i = 0; i < b.said.size(); i++) {
      const gs::VoteNote& n = b.notes[i];
      const std::string line = b.said[i] + "\n";
      if (n.to < 0)
        qw_print_all(*g.qw, line);
      else if (n.to < sh::kMaxClients)
        client_print(*g.qw, n.to, 2, line);
    }
  b.said.clear();
  b.notes.clear();
  bool restart = false;
  std::string to_map;  // a map vote passed
  const char* why = "The map restarts so everyone moves by the new setting\n";
  for (const gs::VoteChange& c : gs::vote_take(b)) {
    if (const gs::AidKind* aid = vote_aid(c.name)) {
      aid_vote_apply(g, *aid, c);
      continue;
    }
    if (c.name == "kick") {  // the player by the user id in the key, if he is still on
      std::string error;
      if (g.qw) qw_kick(*g.qw, qw_find_seat(*g.qw, c.key.substr(5)), error);
      continue;
    }
    if (c.name == "map") {
      to_map = c.value;
      continue;
    }
    g.settings.rules[c.name] = c.value;
    // The aim help's leeway is the server's alone: nobody predicts with it, so no restart.
    if (sh::find_setting(c.name) != nullptr && !c.name.starts_with("sv_aimhelp")) {
      restart = true;
    } else if (c.name == "k_midair") {
      // Midair takes the items off the map and arms every spawn: a new start.
      g.rules.console(c.name + " \"" + c.value + "\"");
      restart = true;
      why = c.value == "0" ? "The map restarts without midair\n" : "The map restarts for midair\n";
    } else if (!held_now(g, c.name)) {
      g.rules.console(c.name + " \"" + c.value + "\"");
    }
    if (c.name == "k_midair_minheight" && g.qw)
      qw_print_all(*g.qw, sh::midair_level_line(sh::midair_level(c.value)));
  }
  if (!to_map.empty()) {
    if (g.qw) qw_print_all(*g.qw, "The vote moves the server to " + to_map + "\n");
    g.rules->pending_map = to_map;
    g.rules->pending_change = sv::MapChange::change_level;
  } else if (restart && g.qw) {
    qw_print_all(*g.qw, why);
    sv::RulesCommon& now = *g.rules;
    now.pending_map = g.map->name;
    now.pending_change = sv::MapChange::restart;
  }
}

// The voters kept to the endpoint: every person on the server (players and spectators who have
// signed on, a listen server's own player), never a bot; an open vote's time checked.
void vote_sync(Game& g) {
  if (!g.qw) return;
  const double now = g.rules->sv.time;
  for (int i = 0; i < sh::kMaxClients && i < gs::kVoteSeats; i++) {
    const QwClient& c = g.qw->clients[i];
    const bool person =
        c.state == ConnState::spawned && (!c.local || (i == 0 && g.host != nullptr));
    gs::vote_seat(g.vote, i, person ? std::string_view(c.name) : std::string_view(), now);
  }
  gs::vote_tick(g.vote, now);
  vote_turn(g);
}

// ---- Timeouts in the match (engine/server/timeout.hpp) ----
//
// A side is a team in a team game (the player's `team`), else the player. The match is on when the
// lobby's phase is the match (always, with no lobby). While a timeout is on, a player's own
// `pause` is answered with the timeout's words, so nobody starts the game under it.

// The timeout's lines printed, and the rules paused or let go as it says.
void timeout_turn(Game& g) {
  gs::TimeoutBook& b = g.timeout;
  if (g.qw)
    for (size_t i = 0; i < b.said.size(); i++) {
      const std::string line = b.said[i] + "\n";
      if (b.to[i] < 0)
        qw_print_all(*g.qw, line);
      else if (b.to[i] < sh::kMaxClients)
        client_print(*g.qw, b.to[i], 2, line);
    }
  b.said.clear();
  b.to.clear();
  const bool want = gs::timeout_paused(b);
  if (want != g.timeout_held) {
    g.rules.pause(sv::kPauseByServer, want);
    g.timeout_held = want;
  }
}

// `timeout` and `timein` from a client; `pause` while one is on.
bool timeout_words(Game& g, int slot, const std::string& line) {
  if (!g.qw || slot < 0 || slot >= sh::kMaxClients) return false;
  const QwClient& c = g.qw->clients[slot];
  if (line == "pause" && gs::timeout_paused(g.timeout)) {
    client_print(*g.qw, slot, 2, "The game is in a timeout. Type timein to end yours early.\n");
    return true;
  }
  const ge::console::Cvar* teamplay = g.rules ? g.rules->cvars.find("teamplay") : nullptr;
  const bool teams = teamplay != nullptr && teamplay->number != 0;
  const std::string team = teams ? info_get(c.userinfo, "team") : std::string();
  gs::TimeoutCaller who;
  who.slot = slot;
  who.name = c.name;
  who.side = team.empty() ? std::string_view(c.name) : std::string_view(team);
  who.player = c.state == ConnState::spawned && !c.spectator &&
               (!c.local || (slot == 0 && g.host != nullptr));
  const bool in_match =
      !g.room || g.settings.lobby == 0 || g.room->lobby.phase == gs::LobbyPhase::match;
  if (!gs::timeout_command(g.timeout, who, in_match, line, g.now_s)) return false;
  timeout_turn(g);
  return true;
}

// A map change ends any timeout (the next map's rules start unpaused); a match's end gives every
// side its timeouts back.
void timeout_map_change(Game& g, bool match_over) {
  if (match_over) {
    gs::timeout_map_over(g.timeout);
  } else {
    g.timeout.phase = gs::TimeoutPhase::none;
    g.timeout.caller = -1;
  }
  g.timeout_held = false;
}

// A map change: every change a vote made goes back in the settings the next map is made from.
// Their lines go out at once, while the players are still on the map that is ending.
void vote_map_over(Game& g) {
  for (const gs::VoteChange& c : gs::vote_map_over(g.vote)) {
    if (const gs::AidKind* aid = vote_aid(c.name))
      aid_vote_apply(g, *aid, c);
    else
      g.settings.rules[c.name] = c.value;
  }
  vote_turn(g);
}

// The game moved onto the map `name` (`supplied`: a listen server's, read by its window): the
// next map's server made, the endpoint moved onto it, the old one let go. A map that does not load
// leaves the game on the one it was playing (`failed` says why, `error` too).
// Quake II's rules outside deathmatch keep the campaign (the bodies carried, the help computer,
// the cross-level bits: server/q2_campaign.hpp) in their own server, so a level change starts
// the next map on the same server rather than a new one; the players stay seated through it. A
// lobby's game and a map without brushes make new rules as before.
// Quake II's companions follow the game onto the next level of its campaign (the same rules):
// they stay seated and are made afresh on the new map's nav. New rules (a new game, deathmatch)
// send them away, as a map no companion can walk does.
void mates_follow(Game& g, bool same_rules) {
  if (!g.mates || cl::q2_party_size(*g.mates) == 0) return;
  cl::Q2Party& p = *g.mates;
  std::vector<int> gone;
  std::string error;
  if (same_rules) cl::q2_party_enter(p, g.rules, g.map->spec, false, gone, error);
  for (int i = cl::q2_party_last(p); !same_rules && i > 0; i = cl::q2_party_last(p)) {
    if (g.qw) g.qw->clients[i].rules_active = false;  // never on the new rules
    gone.push_back(i);
    cl::q2_party_forget(p, i);
  }
  for (const int i : gone)
    if (g.qw) qw_drop(*g.qw, i, "left the game");
  if (cl::q2_party_size(p) == 0) p.turned_coop = false;
  if (same_rules && !gone.empty() && g.qw)
    client_print(*g.qw, 0, 2, error + " Your companions leave.\n");
}

bool keeps_campaign(const Game& g, const cl::LoadedMap& next) {
  if (g.room || next.spec.brushes == nullptr) return false;
  const sv::Q2Server* sq = sv::q2_of(g.rules);
  return sq != nullptr && !sq->cvars.find("deathmatch")->on();
}

bool next_level_on_same_rules(Game& g, std::shared_ptr<cl::LoadedMap> map, std::string& error) {
  if (!ask_load(g, g.rules, error) || !g.rules.start(map->spec, error)) {
    error = "the map " + map->name + " did not load: " + error;
    g.failed = error;
    return false;
  }
  if (map->from_file) sv::use_map_sets(*g.rules, map->bsp);
  if (g.qw && !qw_change(*g.qw, g.rules, error)) {
    g.failed = error;
    return false;
  }
  g.map = std::move(map);
  g.changes++;
  g.failed.clear();
  mates_follow(g, true);
  host_bind(g);
  return true;
}

// A classic program's map change: a dedicated server's network players' spawn parms saved (its
// change parms hook), handed back when each begins on the next map.
void carry_parms(Game& g) {
  if (!g.qw || !g.rules || g.host != nullptr) return;
  const std::string_view form = g.rules.calls->form;
  if (form != "qc" && form != "qvm") return;
  g.rules.carry();
  for (int i = 0; i < sh::kMaxClients; i++) {
    QwClient& c = g.qw->clients[i];
    const sv::ClientInfo& r = g.rules->clients[i];
    c.carried = !c.local && c.rules_active && r.active && !r.spectator;
    if (c.carried) std::copy(std::begin(r.spawn_parms), std::end(r.spawn_parms), c.parms);
  }
}

bool move_to(Game& g, const std::string& name, bool match_over, std::string& error,
             std::shared_ptr<cl::LoadedMap> supplied = nullptr) {
  // A vote lasts the map: put back before the next map is made (a restart keeps it).
  if (match_over) vote_map_over(g);
  timeout_map_change(g, match_over);
  if (supplied && keeps_campaign(g, *supplied))
    return next_level_on_same_rules(g, std::move(supplied), error);
  if (!supplied && g.map && g.map->name == name && keeps_campaign(g, *g.map))
    return next_level_on_same_rules(g, g.map, error);  // a restart: as each player came in
  std::shared_ptr<cl::LoadedMap> map;
  sv::RulesRunner next;
  carry_parms(g);
  if (!open_map(g, name, map, next, error, std::move(supplied), &g.rules)) {
    g.failed = error;
    return false;
  }
  // The endpoint keeps the runner's address: it is moved to where it lives before it is handed on.
  sv::RulesRunner old = std::move(g.rules);
  g.rules = std::move(next);
  if (g.qw && !qw_change(*g.qw, g.rules, error)) {
    g.rules = std::move(old);
    g.failed = error;
    return false;
  }
  old = {};  // the old rules go before the map they view
  g.map = std::move(map);
  g.changes++;
  g.failed.clear();
  if (g.host != nullptr && g.qw)  // the player joins the next map's rules (sync_host)
    g.qw->clients[0].rules_active = false;
  if (g.room) {  // the seats onto the new rules: warmup again after a match the mod ended
    if (match_over && g.settings.lobby != 0) gs::lobby_back(g.room->lobby);
    cl::room_enter(*g.room, g.rules, g.map->spec);
    g.applied.fill(nullptr);
  }
  mates_follow(g, false);
  host_bind(g);
  return true;
}

// A number the rules hold (0 when they have no such setting).
double rule_number(const Game& g, const char* name) {
  const ge::console::Cvar* v = g.rules ? g.rules->cvars.find(name) : nullptr;
  return v ? v->number : 0;
}

// The match's tally after the rules' frame: it counts a deathmatch's players, in a lobby's match
// alone when the server has one (qw_report.hpp).
void tally(Game& g) {
  std::array<uint64_t, sh::kMaxClients> accounts{};
  if (g.login)
    for (int i = 0; i < sh::kMaxClients; i++) accounts[i] = gs::login_account(g.login->logins, i);
  const bool lobby = g.room && g.settings.lobby != 0;
  const bool counting =
      rule_number(g, "deathmatch") != 0 && (!lobby || g.room->lobby.phase == gs::LobbyPhase::match);
  tally_frame(g.tally, g.rules->world, counting, accounts);
  cheat_watch_frame(g.cheats, g.rules->sv);
  const std::array<sh::AimSpread*, sh::kMaxClients> aims = tally_aims(g.tally);
  sh::aim_spread_frame(g.rules->sv, aims);
  for (int i = 0; i < sh::kMaxClients; i++)
    if (cheat_watch_out(g.cheats, i)) tally_flag(g.tally, i, cheat_watch_verdict(g.cheats, i));
  if (const sv::QcServer* q = sv::qc_of(g.rules))
    stats_frame(g.stats_eyes, tally_stats(g.tally), *q);
}

// Overtime after the tally: a lobby's deathmatch tied at its time limit plays on.
void overtime_turn(Game& g) {
  overtime_frame(g.overtime, g.rules, g.tally, g.qw.get(),
                 g.room && g.settings.lobby != 0 && g.room->lobby.phase == gs::LobbyPhase::match &&
                     rule_number(g, "deathmatch") != 0);
}

// A dedicated server nobody is on goes back to its home map (sv_homemap,
// engine/server/homemap.hpp).
void home_turn(Game& g) {
  if (g.host != nullptr || !g.qw || !g.map) return;
  int people = 0;
  for (const QwClient& c : g.qw->clients) people += c.state != ConnState::free && !c.local ? 1 : 0;
  if (gs::home_due(g.home, g.settings.homemap, g.map->name, people, g.now_s) &&
      g.rules->pending_map.empty()) {
    g.rules->pending_map = g.settings.homemap;
    g.rules->pending_change = sv::MapChange::new_game;
  }
}

// The match is over: its report made, for the core to take.
void end_match(Game& g) {
  TallyFacts f;
  f.teams = rule_number(g, "teamplay") != 0;
  f.mode = qw_mode(f.teams, false, g.settings.maxclients);
  f.fraglimit = rule_number(g, "fraglimit");
  f.timelimit = rule_number(g, "timelimit");
  f.stopped = g.rules->map_stopped;
  f.stats = sv::qc_of(g.rules) != nullptr;
  ge::net::MatchReport r;
  if (!tally_end(g.tally, g.rules->world, f, r)) return;
  if (g.qw)
    for (const std::string& line : stats_lines(r)) qw_print_all(*g.qw, line + "\n");
  g.report = std::move(r);
  g.report_ready = true;
}

// The change the mod or the console asked for (`changelevel`, `map`, `restart`). A listen server's
// next map is its window's to read (`ListenHost::asked`, then `change`); a restart is the same map.
void change_if_asked(Game& g) {
  sv::RulesCommon& now = *g.rules;
  const std::string current = g.map->name;
  std::string name;
  bool match_over = true;
  gs::Lobby* lobby = g.room && g.settings.lobby != 0 ? &g.room->lobby : nullptr;
  if (lobby != nullptr && lobby->phase == gs::LobbyPhase::intermission) {
    // The scores stay up: the map the match's end asked for waits until the intermission is over
    // (what the mod asks meanwhile, its intermission's own, is dropped).
    now.pending_map.clear();
    if (!gs::lobby_next_due(*lobby, now.sv.time)) return;
    name = std::move(g.held_map);
    g.held_map.clear();
  } else {
    if (now.pending_map.empty()) return;
    name = now.pending_map;
    // The mod's end of a match asks for the next map, or for its own again: the rotation's turn.
    if (now.pending_change == sv::MapChange::restart)
      name = current;
    else if (now.pending_change == sv::MapChange::change_level || name == current)
      name = next_map(g.settings, current, name);
    match_over = now.pending_change != sv::MapChange::restart;
    now.pending_map.clear();
    if (match_over) end_match(g);
    if (match_over && lobby != nullptr && lobby->phase == gs::LobbyPhase::match &&
        lobby->settings.intermission_s > 0) {  // the intermission first, then this map
      gs::lobby_end(*lobby, now.sv.time);
      g.held_map = std::move(name);
      return;
    }
  }
  if (g.host != nullptr && name != current) {
    g.host->asked = name;
    return;
  }
  std::string error;
  move_to(g, name, match_over, error);
}

// A listen server's player (slot 0) on the rules while the lobby has it on the map (always without
// a lobby), off them on the lobby's screen.
void sync_host(Game& g) {
  if (g.host == nullptr || !g.qw) return;
  QwClient& c = g.qw->clients[0];
  const bool on = !g.room || gs::lobby_on_map(g.room->lobby, 0);
  if (on && !c.rules_active) {
    g.rules.connect(0, g.host->player, false, false);
    c.rules_active = true;
    g.applied[0] = nullptr;  // its team again
  } else if (!on && c.rules_active) {
    g.rules.drop(0);
    c.rules_active = false;
  }
}

// The room kept to the endpoint's seats: a bot seated is shown as a local seat and one gone is
// freed; a client who signed on takes a lobby seat and one who left frees it; a seat's team is
// put in its user info (every client sees the colours) and the mod's `.team`.
void sync_seats(Game& g) {
  if (!g.room || !g.qw) return;
  cl::Room& r = *g.room;
  QwServer& cs = *g.qw;
  const sv::RulesCommon& now_rules = *g.rules;
  const double now = now_rules.sv.time;
  for (int i = 0; i < gs::kLobbySeats && i < sh::kMaxClients; i++) {
    const auto at = static_cast<size_t>(i);
    const bool bot = r.bots[at] != nullptr;
    if (cs.clients[i].local && !bot && !(i == 0 && g.host != nullptr)) {
      cs.clients[i].rules_active = false;  // the room let it go from the rules
      qw_drop(cs, i, "left the room");
    }
    std::string error;
    if (bot && cs.clients[i].state == ConnState::free) qw_add_local(cs, i, r.bots[at]->name, error);
    const QwClient& c = cs.clients[i];
    gs::LobbySeat& seat = r.lobby.seats[at];
    if (!c.local && c.state == ConnState::free && seat.used && !seat.bot) {
      gs::lobby_leave(r.lobby, i, now);
      g.applied[at] = nullptr;
    }
    if (i == 0) sync_host(g);
    if (!c.rules_active) continue;
    if (!c.local && !seat.used) {
      gs::lobby_join(r.lobby, i, c.name, false, now);
      seat.warm = true;  // a client who joined over the network is on the map already
    }
    if (!seat.used || (g.applied[at] == &now_rules && g.shown[at] == seat.team)) continue;
    const std::string colour = std::to_string(sv::team_colour(seat.team));
    qw_set_info(cs, i, "team", gs::team_name(seat.team));
    qw_set_info(cs, i, "topcolor", colour);
    qw_set_info(cs, i, "bottomcolor", colour);
    sv::lobby_apply(g.rules, r.lobby, i);  // the mod's own `.team`
    g.applied[at] = &now_rules;
    g.shown[at] = seat.team;
  }
}

// The lobby's turn after a frame: its lines printed to everyone, and a countdown run out restarts
// the map with every seat on it, the match.
void lobby_turn(Game& g) {
  if (!g.room) return;
  cl::Room& r = *g.room;
  sv::RulesCommon& now = *g.rules;
  if (g.settings.lobby != 0 && gs::lobby_due(r.lobby, now.sv.time)) {
    now.pending_map = g.map->name;
    now.pending_change = sv::MapChange::restart;
  }
  gs::lobby_tick(r.lobby, now.sv.time);
  std::vector<int> kicked;  // the idle kick's: dropped once their lines are out
  for (const gs::LobbyNote& n : r.lobby.notes)
    if (n.kind == gs::LobbyNoteKind::kicked) kicked.push_back(n.slot);
  g.said += static_cast<int>(r.lobby.said.size());
  sv::MatchLook look;
  if (const ge::console::Cvar* c = now.cvars.find("deathmatch"); c != nullptr)
    look.deathmatch = static_cast<int>(c->number);
  if (const ge::console::Cvar* c = now.cvars.find("teamplay"); c != nullptr)
    look.teamplay = static_cast<int>(c->number);
  std::vector<std::string> lines;
  if (g.host != nullptr) lines = g.host->lines;
  for (const auto& [rule, value] : g.settings.rules)
    if (!value.empty()) {
      std::string line = rule;
      line += ' ';
      line += value;
      lines.push_back(std::move(line));
    }
  sv::announce(r.announcer, r.lobby, now.localinfo, sv::match_look(look, lines), now.sv.time);
  if (g.qw)
    for (int slot : kicked) qw_drop(*g.qw, slot, "was kicked for not getting ready");
}

// The lobby's words handed to the rules playing now (after a change, the new map's): they go out
// with the next frame's events, and into its recording; the phase as the server info's `status`.
void lobby_words(Game& g) {
  if (!g.room) return;
  sv::announce_phase(g.rules->localinfo, g.room->lobby.phase);
  if (g.qw != nullptr) {  // the match's `status` as other servers say it (a recording starts on it)
    const gs::LobbyPhase p = g.room->lobby.phase;
    qw_set_serverinfo(*g.qw, "status",
                      p == gs::LobbyPhase::warmup      ? "Standby"
                      : p == gs::LobbyPhase::countdown ? "Countdown"
                      : p == gs::LobbyPhase::match     ? "In progress"
                                                       : "End");
  }
  sv::announce_flush(g.room->announcer, g.rules->out, g.rules->sv.time);
}

// Quake II's companions outside deathmatch: `addbot [skill]` and `removebot` from the window's
// player (slot 0), the leader they escort. A single-player game turns cooperative for the first and
// back for the last one leaving. False when the line is not theirs.
bool mate_words(Game& g, int slot, const std::string& line) {
  const size_t space = line.find(' ');
  const std::string word = line.substr(0, space);
  if ((word != "addbot" && word != "removebot") || !g.qw || g.host == nullptr || slot != 0)
    return false;
  const ge::console::Cvar* dm = g.rules->cvars.find("deathmatch");
  if (sv::q2_of(g.rules) == nullptr || (dm != nullptr && dm->on())) return false;
  QwServer& cs = *g.qw;
  if (!g.mates) {
    g.mates = std::make_unique<cl::Q2Party>();
    g.mates->seed = g.settings.seed;
  }
  cl::Q2Party& p = *g.mates;
  const ge::console::Cvar* coop = g.rules->cvars.find("coop");
  if (word == "removebot") {
    const int last = cl::q2_party_last(p);
    if (last < 0) {
      client_print(cs, slot, 2, "There is no companion to send away.\n");
      return true;
    }
    qw_drop(cs, last, "left the game");  // off the rules too
    cl::q2_party_forget(p, last);
    if (p.turned_coop && cl::q2_party_size(p) == 0) {
      g.rules.console("coop 0");
      p.turned_coop = false;
      client_print(cs, slot, 2, "You play alone again.\n");
    }
    return true;
  }
  sb::AimSkill skill = sb::AimSkill::average;
  const std::string named = space == std::string::npos ? "" : line.substr(space + 1);
  if (!named.empty() && !cl::skill_named(named, skill)) {
    client_print(cs, slot, 2,
                 "addbot: unknown skill \"" + named + "\" (use novice, average or expert)\n");
    return true;
  }
  const bool turned = coop == nullptr || !coop->on();
  if (turned) g.rules.console("coop 1");
  uint32_t held = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (cs.clients[i].state != ConnState::free) held |= 1u << i;
  std::string error;
  const int at = cl::q2_party_add(p, g.rules, g.map->spec, skill, held, error);
  if (at < 0 || !qw_add_local(cs, at, p.mates[static_cast<size_t>(at)]->name, error)) {
    if (at > 0) {
      g.rules.drop(at);
      cl::q2_party_forget(p, at);
    }
    if (turned) g.rules.console("coop 0");
    client_print(cs, slot, 2, "addbot: " + error + "\n");
    return true;
  }
  if (turned) {
    p.turned_coop = true;
    client_print(cs, slot, 2, "The game is cooperative while you have company.\n");
  }
  const cl::Q2Mate& m = *p.mates[static_cast<size_t>(at)];
  client_print(cs, slot, 2, m.name + " (" + sb::aim_skill_name(skill) + ") joins the game\n");
  return true;
}

// A client's words the server takes before the mod: the vote's (`vote ...`, and `yes` and `no`
// while one is open) from anyone; with a room, `addbot [skill]` and `removebot` from the leader,
// and with sv_lobby the lobby's own (ready, break, team, start, lobby).
bool room_words(void* ctx, int slot, const std::string& line) {
  Game& g = *static_cast<Game*>(ctx);
  if (g.qw) {
    const gs::VoteTarget target{&g, vote_check};
    if (gs::vote_command(g.vote, target, slot, line, g.rules->sv.time)) {
      vote_turn(g);
      return true;
    }
    if (timeout_words(g, slot, line)) return true;
  }
  if (!g.room) return mate_words(g, slot, line);
  cl::Room& r = *g.room;
  QwServer& cs = *g.qw;
  const size_t space = line.find(' ');
  const std::string word = line.substr(0, space);
  if (word == "addbot" || word == "removebot") {
    const int leader = gs::lobby_leader(r.lobby);
    if (slot != leader) {
      const std::string who =
          leader >= 0 ? r.lobby.seats[static_cast<size_t>(leader)].name : std::string("nobody");
      client_print(cs, slot, 2, word + ": only the leader (" + who + ") changes the bots\n");
      return true;
    }
    std::string error;
    if (word == "removebot") {
      for (int i = gs::kLobbySeats - 1; i > 0; i--)
        if (r.bots[static_cast<size_t>(i)]) {
          cs.clients[i].rules_active = false;  // room_remove_bot lets it go from the rules
          break;
        }
      if (!cl::room_remove_bot(r, g.rules))
        client_print(cs, slot, 2, "There are no bots to remove.\n");
      sync_seats(g);
      return true;
    }
    sb::AimSkill skill = sb::AimSkill::average;
    const std::string named = space == std::string::npos ? "" : line.substr(space + 1);
    if (!named.empty() && !cl::skill_named(named, skill)) {
      client_print(cs, slot, 2,
                   "addbot: unknown skill \"" + named + "\" (use novice, average or expert)\n");
      return true;
    }
    r.held = 0;
    for (int i = 0; i < sh::kMaxClients; i++)
      if (cs.clients[i].state != ConnState::free) r.held |= 1u << i;
    if (cl::room_add_bot(r, g.rules, g.map->spec, skill, error) < 0)
      client_print(cs, slot, 2, "addbot: " + error + "\n");
    sync_seats(g);
    return true;
  }
  if (g.settings.lobby == 0) return false;
  return gs::lobby_command(r.lobby, slot, line, g.rules->sv.time);
}

namespace cp = ge::classic::protocol;

void masters_send(Game& game, const std::vector<cp::MasterSend>& out) {
  if (!game.qw || game.qw->transport == nullptr) return;
  for (const cp::MasterSend& m : out)
    ge::net::transport_send(*game.qw->transport, m.to, m.bytes.data(), m.bytes.size(),
                            static_cast<int64_t>(game.qw->now * 1e6));
}

// The masters told the server is going (Quake's `C`), then the game freed.
void destroy(void* g) {
  Game* game = static_cast<Game*>(g);
  if (game->qw) qw_shutdown(*game->qw, "server shutdown\n");
  std::vector<cp::MasterSend> out;
  std::vector<std::string> said;
  cp::masters_shutdown(game->masters, out, said);
  masters_send(*game, out);
  delete game;
}

double frame_seconds(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.coop ? sv::kCampaignStep : game.rules->sv.set.mintic;
}

// A bot's move length: the server's frame, ms.
int frame_msec(const Game& g) {
  return static_cast<int>(ge::core::num::round(g.rules->sv.set.mintic * 1000));
}

// Offline: the mod's frame with nobody's moves, its streams emptied as the endpoint would.
void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_frame(*game.coop);
  std::vector<sh::Packet> moves;
  if (game.room) cl::room_packets(*game.room, *game.rules, frame_msec(game), moves);
  if (game.mates) cl::q2_party_packets(*game.mates, game.rules, frame_msec(game), moves);
  game.rules.frame(moves.data(), static_cast<int>(moves.size()));
  tally(game);
  overtime_turn(game);
  sv::RulesCommon& now = *game.rules;
  now.out.all.clear();
  for (auto& one : now.out.one) one.clear();
  sv::clear_frame(now.out);
  lobby_turn(game);
  change_if_asked(game);
  lobby_words(game);
}

// The ban and VIP lists `writeip` and `vip_writeip` saved, read back at start (a line they do not
// take is left out).
void load_bans(QwServer& s) {
  gs::ban_file_read(s.bans, s.cfg.ban_file, qw_unix_time(s));
  std::string line, out;
  if (!s.cfg.vip_file.empty()) {
    std::ifstream f(s.cfg.vip_file);
    while (std::getline(f, line))
      if (line.rfind("vip_addip ", 0) == 0) gs::vip_command(s.vips, line, qw_unix_time(s), out);
  }
}

// A client's words: a login's first (qw_login.hpp), then the lobby's and the votes'.
// The lobby's lock and prewar on a client (qw_match_rules.hpp).
bool may_play(void* ctx, const std::string& name, std::string& why) {
  const Game& g = *static_cast<const Game*>(ctx);
  if (g.room && g.settings.lobby != 0) why = lock_refusal(g.room->lobby, name);
  return why.empty();
}
uint8_t move_buttons(void* ctx, int slot, uint8_t buttons) {
  Game& g = *static_cast<Game*>(ctx);
  if (!g.room || g.settings.lobby == 0 || slot < 0 || slot >= gs::kLobbySeats) return buttons;
  std::string tell;
  buttons = prewar_buttons(g.room->lobby, slot, buttons, g.rules->sv.time,
                           g.held_told[static_cast<size_t>(slot)], tell);
  if (!tell.empty() && g.qw) client_print(*g.qw, slot, 2, tell);
  return buttons;
}

bool game_words(void* ctx, int slot, const std::string& line) {
  Game& game = *static_cast<Game*>(ctx);
  if (game.login && qw_login_line(*game.login, slot, line)) return true;
  return room_words(ctx, slot, line);
}

// The masters' heartbeat at the frame (the first frame starts the list).
void masters_turn(Game& game, int64_t now_us) {
  const double now_s = static_cast<double>(now_us) / 1e6;
  if (!cp::masters_started(game.masters)) {
    cp::masters_start(game.masters, cp::MasterFamily::qw, now_s);
  }
  std::vector<cp::MasterSend> out;
  cp::masters_frame(game.masters, now_s, qw_count(*game.qw, ConnState::connected), "", true, out,
                    game.masters_said);
  masters_send(game, out);
}

// `setmaster` and `heartbeat` on the server's console; false for any other line.
bool masters_line(Game& game, const std::string& line) {
  std::vector<std::string> w;
  for (size_t at = 0; at < line.size();) {
    const size_t start = line.find_first_not_of(" \t", at);
    if (start == std::string::npos) break;
    const size_t end = std::min(line.find_first_of(" \t", start), line.size());
    w.push_back(line.substr(start, end - start));
    at = end;
  }
  if (w.empty() || (w[0] != "setmaster" && w[0] != "heartbeat")) return false;
  if (w[0] == "heartbeat") {
    cp::force_heartbeat(game.masters);
    return true;
  }
  std::vector<cp::MasterSend> out;
  cp::set_masters(game.masters, {w.begin() + 1, w.end()}, cp::resolve_master, game.masters_said,
                  out);
  masters_send(game, out);
  return true;
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_serve(*game.coop, port, error);
  game.qw = std::make_unique<QwServer>();
  // The game folder's files offered to a client that lacks one (a skin, a player figure), and
  // listed for `listskins`; the maps' folder, read as the maps are.
  game.cfg.read_file = [&game](const std::string& path, std::vector<uint8_t>* out) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> got = cl::find_file(game.files, path, store);
    out->assign(got.begin(), got.end());
    return !out->empty();
  };
  // The archive a file served comes out of, so a client keeps it there (engine/vfs/partial.hpp).
  game.cfg.file_origin = [&game](const std::string& path, ge::vfs::ArchiveOrigin* out) {
    return ge::classic::content::served_origin(game.files, path, out);
  };
  game.cfg.list_files = [&game] {
    std::vector<std::string> names;
    for (const ge::vfs::Listed& l : ge::vfs::list(game.files.vfs)) names.emplace_back(l.name);
    return names;
  };
  if (!qw_start(*game.qw, game.rules, port, game.cfg, error)) return false;
  load_bans(*game.qw);
  game.qw->game_command = game_words;
  game.qw->game_ctx = &game;
  game.qw->may_play = may_play;
  game.qw->buttons = move_buttons;
  // Logins, on a dedicated server and on the window's own match alike (its player is a local seat,
  // never asked).
  if (qw_login_wanted(game.settings)) {
    game.login = std::make_unique<QwLogin>();
    if (!qw_login_open(*game.login, *game.qw, game.settings, error)) return false;
  }
  if (game.host != nullptr) {  // the window's player: slot 0, a local seat the window moves
    if (!qw_add_local(*game.qw, 0, game.host->player, error, false)) return false;
    game.qw->clients[0].rules_active = false;  // sync_host puts it on the map
    game.qw->local_tap = host_tap;
    game.qw->local_print = host_print;
    game.qw->local_voice = host_voice;
    game.qw->local_ctx = game.host;
  }
  if (game.qtv_port > 0) {
    game.qtv = std::make_unique<QtvPort>();
    if (!qtv_open(*game.qtv, ge::net::ipv4(0, 0, 0, 0, static_cast<uint16_t>(game.qtv_port)),
                  game.qtv_cfg, error))
      return false;
  }
  sync_host(game);
  sync_seats(game);
  return true;
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop) return coop_net_frame(*game.coop, now_us);
  if (game.room) cl::room_packets(*game.room, *game.rules, frame_msec(game), game.qw->local_moves);
  if (game.mates)
    cl::q2_party_packets(*game.mates, game.rules, frame_msec(game), game.qw->local_moves);
  const gs::LobbyPhase phase = game.room ? game.room->lobby.phase : gs::LobbyPhase::warmup;
  game.qw->match_on = game.settings.lobby != 0 &&
                      (phase == gs::LobbyPhase::countdown || phase == gs::LobbyPhase::match);
  qw_frame(*game.qw, now_us);
  masters_turn(game, now_us);
  if (game.login) qw_login_frame(*game.login);
  tally(game);
  overtime_turn(game);
  if (game.qtv) qtv_update(*game.qtv, *game.qw);
  sync_host(game);
  sync_seats(game);
  vote_sync(game);
  game.now_s = static_cast<double>(now_us) / 1e6;
  gs::timeout_tick(game.timeout, game.now_s);
  home_turn(game);
  timeout_turn(game);
  lobby_turn(game);
  change_if_asked(game);
  lobby_words(game);
  sync_host(game);
  sync_seats(game);  // a new map's seats
}

// The rules' console lines since the last call (a new map's from its first).
void console_lines(void* g, std::vector<std::string>& out) {
  Game& game = *static_cast<Game*>(g);
  if (game.coop || !game.rules) return;
  const sv::RulesCommon& now = *game.rules;
  if (&now != game.told_rules || game.told > now.console.size()) {
    game.told_rules = &now;
    game.told = 0;
  }
  for (; game.told < now.console.size(); game.told++) out.push_back(now.console[game.told]);
  for (std::string& line : game.masters_said) out.push_back(std::move(line));
  game.masters_said.clear();
}

// A server console line the core does not know: the rules' own (`map`, a mod's words).
bool console_line(void* g, const std::string& line) {
  Game& game = *static_cast<Game*>(g);
  if (!game.coop && masters_line(game, line)) return true;
  if (game.coop || !game.rules) return false;
  return game.rules.console(line);
}

// The match that just ended, once (qw_report.hpp).
// The lobby's phase (sv_lobby), else the match: what the core's features act on (sv_autorecord).
gs::LobbyPhase phase(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.room && game.settings.lobby != 0 ? game.room->lobby.phase : gs::LobbyPhase::match;
}

bool match_report(void* g, ge::net::MatchReport& out) {
  Game& game = *static_cast<Game*>(g);
  if (!game.report_ready) return false;
  out = std::move(game.report);
  game.report_ready = false;
  return true;
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.coop) return coop_seated(*game.coop);
  return game.qw ? qw_count(*game.qw, ConnState::spawned) : 0;
}

uint64_t hash(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.coop ? coop_hash(*game.coop) : sv::rules_hash(*game.rules);
}

// The hub's record (docs/hub.md): the map, the mode the rules make, the phase, every client.
void fill_status(const void* g, ge::net::Status& st) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.map) st.map = game.map->name;
  auto number = [&](const char* name) {
    const ge::console::Cvar* v = game.rules ? game.rules->cvars.find(name) : nullptr;
    return v ? std::atoi(v->value.c_str()) : 0;
  };
  st.mode =
      qw_mode(number("teamplay") != 0, game.coop || number("coop") != 0, game.settings.maxclients);
  if (!game.held_map.empty()) st.phase = ge::net::StatusPhase::intermission;
  for (const char* name : {"fraglimit", "timelimit"}) {
    st.keys.emplace_back(name, std::to_string(number(name)));
  }
  if (const std::string mods = mods_info(game.packages); !mods.empty())
    st.keys.emplace_back("*mods", mods);
  if (game.qw) status_players(*game.qw, st.players);
}

// The rules the mod is playing by, then the endpoint's counts.
std::string status(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.coop) return coop_status(*game.coop);
  std::string out = "map=" + game.map->name;
  if (!game.failed.empty()) out += " (no change: " + game.failed + ")";
  for (const char* name : {"deathmatch", "fraglimit", "timelimit"}) {
    const ge::console::Cvar* v = game.rules->cvars.find(name);
    out.append(" ").append(name).append("=").append(v ? v->value : "?");
  }
  if (game.room) {
    const gs::Lobby& l = game.room->lobby;
    int bots = 0;
    for (const auto& b : game.room->bots) bots += b ? 1 : 0;
    out.append(" bots=").append(std::to_string(bots));
    if (game.settings.lobby != 0)
      out.append(" lobby=")
          .append(gs::phase_name(l.phase))
          .append(" ready=")
          .append(std::to_string(gs::lobby_ready_count(l)))
          .append("/")
          .append(std::to_string(gs::lobby_seated(l)))
          .append(" matches=")
          .append(std::to_string(l.matches));
  }
  if (gs::timeout_paused(game.timeout))
    out += game.timeout.phase == gs::TimeoutPhase::out ? " timeout=out" : " timeout=resuming";
  if (!game.qw) return out + " offline";
  const ServerCounts& n = game.qw->counts;
  char counts[128];
  std::snprintf(counts, sizeof counts, " connected=%d connects=%lld drops=%lld out=%lld B",
                qw_count(*game.qw, ConnState::connected), static_cast<long long>(n.connects),
                static_cast<long long>(n.drops), static_cast<long long>(n.bytes_out));
  return out + counts;
}

// ---- A listen server's calls (cl::ListenHost) ----

bool host_words(void* g, const std::string& line) {
  Game& game = *static_cast<Game*>(g);
  const bool taken = room_words(&game, 0, line);
  sync_host(game);
  sync_seats(game);
  lobby_turn(game);  // the answer goes out with the next frame
  lobby_words(game);
  return taken;
}

bool host_admin(void* g, const std::string& line, std::string& out) {
  Game& game = *static_cast<Game*>(g);
  return game.qw && qw_admin(*game.qw, line, out);
}

void host_move(void* g, const sh::Packet& p) {
  Game& game = *static_cast<Game*>(g);
  if (game.qw) game.qw->local_moves.push_back(p);
}

bool host_change(void* g, std::shared_ptr<cl::LoadedMap> map, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  const std::string name = map->name;
  if (!move_to(game, name, true, error, std::move(map))) return false;
  sync_host(game);
  sync_seats(game);
  return true;
}

void host_info(void* g, const std::string& key, const std::string& value) {
  Game& game = *static_cast<Game*>(g);
  if (game.qw) qw_set_info(*game.qw, 0, key, value);
}

// The window's player talking: his voice message relayed by the endpoint as a client's.
void host_speak(void* g, int gen, int seq, const std::vector<uint8_t>& data) {
  Game& game = *static_cast<Game*>(g);
  if (!game.qw) return;
  ClcMsg m;
  m.id = clc::voice;
  m.gen = gen;
  m.seq = seq;
  m.data = data;
  qw_local_voice(*game.qw, 0, m);
}

// The window's player's string command (a chat line, his voice's target), as a client's.
void host_command(void* g, const std::string& line) {
  Game& game = *static_cast<Game*>(g);
  if (game.qw) string_command(*game.qw, 0, line);
}

void host_bind(Game& g) {
  if (g.host == nullptr) return;
  cl::ListenHost& h = *g.host;
  h.game = &g;
  h.rules = &g.rules;
  h.room = g.room.get();
  h.map = g.map;
  h.changes = g.changes;
  h.words = host_words;
  h.move = host_move;
  h.change = host_change;
  h.set_info = host_info;
  h.speak = host_speak;
  h.admin = host_admin;
  h.command = host_command;
}

// The broadcast and a player's view (qw_game_tv.hpp): the QuakeWorld endpoint's own stream; a
// campaign in coop is not recorded.
void tv_frame(const void* g, std::vector<uint8_t>& out) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.qw && !game.coop) qw_tv_frame(*game.tv, *game.qw, out);
}

void tv_keyframe(const void* g, std::vector<uint8_t>& out) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.qw && !game.coop) qw_tv_keyframe(*game.tv, *game.qw, out);
}

void player_view(const void* g, int player, std::vector<uint8_t>& out) {
  const Game& game = *static_cast<const Game*>(g);
  if (game.qw && !game.coop) return mvd_player_view(*game.qw, player, out);
  out.assign(1, 0);
}

gs::ServerGame make() {
  gs::ServerGame g;
  g.binary = "sesv";
  g.title = "Shooter's Edge";
  g.version_text = kProgramVersion;
  g.protocol = ge::net::protocol_id("shooters_edge", 1);
  g.default_port = server::kServerPort;
  g.discovery_port = 0;  // QuakeWorld browsers ask the port itself (status, ping)
  g.max_clients = 32;
  g.default_map = "arena";
  g.base_dir = kGameFolder;
  g.rules = all_rules().data();
  g.rule_count = static_cast<int>(all_rules().size());
  g.names = kSettingNames;
  g.name_count = static_cast<int>(std::size(kSettingNames));
  g.create = create;
  g.destroy = destroy;
  g.frame_seconds = frame_seconds;
  g.frame = frame;
  g.seated = seated;
  g.hash = hash;
  g.status = status;
  g.console_lines = console_lines;
  g.console = console_line;
  g.fill_status = fill_status;
  g.version = 1;
  g.serve = serve;
  g.net_frame = net_frame;
  g.match_report = match_report;
  g.phase = phase;
  g.tv_frame = tv_frame;
  g.tv_keyframe = tv_keyframe;
  g.player_view = player_view;
  qw_watch_hooks(g);
  return g;
}

}  // namespace

const gs::ServerGame& qw_game() {
  static const gs::ServerGame g = make();
  return g;
}

cl::ListenOptions listen_options() {
  cl::ListenOptions o;
  o.game = &qw_game();
  o.door = &qw_coop();
  return o;
}

}  // namespace shooters_edge::net
