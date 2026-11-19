// The sequel's dedicated server game (sequel_game.hpp).
//
// ge:summary The sequel's dedicated server game: its maps read, the rules started and moved on
// ge:keywords sequel-dedicated-server sequel_game protocol-34 servergame sv_maxrate sv_minrate
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_game.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "classic/content/downloads.hpp"
#include "classicenhance/profile.hpp"
#include "core/num.hpp"
#include "server/admission.hpp"
#include "server/idle.hpp"
#include "server/limit.hpp"
#include "server/lrcon.hpp"
#include "server/names.hpp"
#include "server/rate.hpp"
#include "server/skew.hpp"
#include "server/status.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/sequel_game_tv.hpp"
#include "shooters_edge/net/sequel_login.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"
#include "shooters_edge/server/sequel_native.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;
namespace gs = ge::server;

// The rules' settings a server's cfg, `+set` or the console may set (the rules' own cvars).
constexpr gs::GameRule kRules[] = {
    {"dmflags", "Deathmatch's rule bits.", nullptr,
     "0: none (the default); N: the sum of the bits, such as 8192 for infinite ammunition"},
    {"fraglimit", "The frags that end the level.", nullptr,
     "0: no limit (the default); 1..N: that many frags"},
    {"timelimit", "The minutes that end the level.", nullptr,
     "0: no limit (the default); 1..N: that many minutes"},
    {"sv_gravity", "The pull down on players, in units a second a second.", nullptr,
     "N: that pull (800 the default)", false},
    {"sv_download", "Whether a client that lacks a file is offered it.", nullptr,
     "0: no file is offered; 1: files are offered (the default)", false},
    {"sv_download_players", "Whether files in players/ (skins and figures) are offered.", nullptr,
     "0: not offered; 1: offered (the default)", false},
    {"sv_download_models", "Whether files in models/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)", false},
    {"sv_download_sounds", "Whether files in sound/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)", false},
    {"sv_download_maps", "Whether files in maps/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)", false},
    {"sv_speedcheck", "Whether a player whose moves add up to more time than passed is held back.",
     nullptr, "0: not checked; 1: a move past his time budget is not run (the default)"},
    {"sv_fps",
     "The rules' frames a second, from the next map: the doors and lifts, the shots and what "
     "falls step this often; monsters, items and the players' hands keep ten.",
     nullptr,
     "10: the classic beat (the default); 20, 30, 40, 50 or 60: that many (another number is "
     "taken down to its ten, 10 to 60)"},
    {"sv_zombietime",
     "The seconds a dropped player's slot stays held, resending the goodbye, before it is free.",
     nullptr, "0..60: that many seconds after the player's last packet (2 the default)"},
    {"sv_ghosttime",
     "The seconds a slot taken by a connect may wait for the player's first packet before it is "
     "free again.",
     nullptr, "0: it waits for sv_timeout; 1..60: that many seconds (6 the default)"},
    {"sv_nodelta",
     "Whether a player's game may keep asking for whole frames, which cost the most bytes.",
     nullptr,
     "1: whenever it asks (the default); 0: a player who asks for more than 64 is dropped"},
    {"sv_pingmethod", "How the ping shown in the scores and the status is worked out.", nullptr,
     "0: none shown; 1: the average of the last 16 round trips; 2: the least of them (the "
     "default)"},
    {"sv_maxents",
     "The most entities one frame sends a player; the rest wait for a frame with room.", nullptr,
     "0: as many as the packet holds (the default); 1..1024: at most that many"},
    {"sv_entpriority", "Which entities a frame keeps when sv_maxents is reached.", nullptr,
     "0: the lowest numbers (the default); 1: players, doors and lifts and anything alive first, "
     "pieces and effects last, the nearest first"},
    {"sv_novis", "Whether every player is sent every entity, behind walls too.", nullptr,
     "0: only what the player may see or hear (the default); 1: everything"},
    {"sv_dropquad",
     "Whether a dead player leaves the Quad Damage they held, with the time it had left.", nullptr,
     "0: it is lost unless dmflags has 16384 (the default); 1: left for whoever takes it"},
    {"sv_droppent",
     "Whether a dead player leaves the Invulnerability they held, with the time it had left.",
     nullptr, "0: it is lost (the default); 1: left for whoever takes it first"},
    {"sv_dropsuit",
     "Whether a dead player leaves the Environment Suit they held, with the time it had left.",
     nullptr, "0: it is lost (the default); 1: left for whoever takes it first"},
    {"sv_dropbreather",
     "Whether a dead player leaves the Rebreather they held, with the time it had left.", nullptr,
     "0: it is lost (the default); 1: left for whoever takes it first"},
    {"sv_dropsilencer",
     "Whether a dead player leaves the Silencer they held, with the shots it had left.", nullptr,
     "0: it is lost (the default); 1: left for whoever takes it first"},
    {"sv_landjump", "Whether a player may jump again at once after landing from a fast fall.",
     nullptr,
     "0: a landing faster than 200 units a second holds the next jump for 144 or 200 ms; 1: no "
     "hold for players whose game predicts it (the default); 2: no hold for anyone"},
    {"sv_waterjump", "Whether jump held under water lifts a swimmer at a fixed speed.", nullptr,
     "0: the fixed lift (100 in water, 80 in slime, 50 in lava); 1: only the swim's own up move "
     "for players whose game predicts it (the default); 2: the same for anyone"},
    {"sv_banmode", "How the ban list made by addip is read.", nullptr,
     "0: only the addresses on it may join; 1: the addresses on it are refused (the default)"},
};

// The rules a server may set: the engine's rate bounds and per-address limit (server/rate.hpp,
// admission.hpp), then the sequel's own.
std::span<const gs::GameRule> all_rules() {
  static const std::vector<gs::GameRule> rules = [] {
    std::vector<gs::GameRule> out;  // row by row: GCC's release build misreads a range insert
    for (std::span<const gs::GameRule> t :
         {std::span<const gs::GameRule>(gs::kRateRules),
          std::span<const gs::GameRule>(gs::kAdmitRules),
          std::span<const gs::GameRule>(gs::kIdleRules),
          std::span<const gs::GameRule>(gs::kSkewRules),
          std::span<const gs::GameRule>(gs::kNameRules),
          std::span<const gs::GameRule>(gs::kLimitRules),
          std::span<const gs::GameRule>(gs::kStatusRules),
          std::span<const gs::GameRule>(gs::kLrconRules), std::span<const gs::GameRule>(kRules)})
      for (const gs::GameRule& r : t) out.push_back(r);
    return out;
  }();
  return rules;
}

// A whole-number rule of the endpoint's (unset: `out` kept); false with `error` when not one.
bool endpoint_number(const gs::ServerConfig& c, const char* name, int most, int& out,
                     std::string& error) {
  const auto r = c.rules.find(name);
  if (r == c.rules.end() || r->second.empty()) return true;
  char* end = nullptr;
  const long v = std::strtol(r->second.c_str(), &end, 10);
  if (end == r->second.c_str() || *end != '\0' || v < 0 || v > most) {
    error = std::string(name) + " \"" + r->second + "\" is not a number from 0 to " +
            std::to_string(most) + " (--explain=" + name + " shows where it was set)";
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

// The sequel's own names for the download settings, as its configs and admins write them.
constexpr gs::SettingName kSettingNames[] = {
    {"allow_download", "sv_download"},
    {"allow_download_players", "sv_download_players"},
    {"allow_download_models", "sv_download_models"},
    {"allow_download_sounds", "sv_download_sounds"},
    {"allow_download_maps", "sv_download_maps"},
    {"filterban", "sv_banmode"},
    {"maxspectators", "sv_maxspectators"},
    {"spectator_password", "sv_spectatorpassword"},
    {"sv_allow_nodelta", "sv_nodelta"},
    {"sv_auth_limit", "sv_passwordrate"},
    {"sv_calcpings_method", "sv_pingmethod"},
    {"sv_changemapcmd", "sv_mapcmd"},
    {"sv_enforcetime", "sv_speedcheck"},
    {"sv_ghostime", "sv_ghosttime"},
    {"sv_lan_force_rate", "sv_lanrate"},
    {"sv_max_packet_entities", "sv_maxents"},
    {"sv_max_rate", "sv_maxrate"},
    {"sv_min_rate", "sv_minrate"},
    {"sv_namechange_limit", "sv_namelimit"},
    {"sv_prioritize_entities", "sv_entpriority"},
    {"sv_rcon_limit", "sv_rconrate"},
    {"sv_redirect_address", "sv_redirect"},
    {"sv_reserved_password", "sv_vippassword"},
    {"sv_reserved_slots", "sv_vipslots"},
    {"sv_show_name_changes", "sv_shownames"},
    {"sv_status_limit", "sv_statusrate"},
    {"sv_status_show", "sv_status"},
    {"sv_strafejump_hack", "sv_landjump"},
    {"sv_timescale_kick", "sv_speedkick"},
    {"sv_timescale_time", "sv_speedwindow"},
    {"sv_timescale_warn", "sv_speedwarn"},
    {"sv_waterjump_hack", "sv_waterjump"},
    {"zombietime", "sv_zombietime"},
};

// A download switch from the rules (`sv_download...`): "0" off, anything else (or unset) on.
bool switch_on(const gs::ServerConfig& c, const char* name) {
  const auto r = c.rules.find(name);
  return r == c.rules.end() || r->second.empty() || r->second != "0";
}

// The first of `sv_mod`'s comma-separated folders ("" none): the sequel's mod folder.
std::string first_mod(const std::string& mods) {
  std::string m = mods.substr(0, mods.find(','));
  while (!m.empty() && m.back() == ' ') m.pop_back();
  while (!m.empty() && m.front() == ' ') m.erase(0, 1);
  return m;
}

// The rules' slots: the players' and the spectators' own (sequel_net_server.hpp).
int slots(const gs::ServerConfig& c) {
  return std::min(sh::kMaxClients, c.maxclients + std::max(0, c.max_spectators));
}

struct Game {
  cl::SearchPath files;
  std::unique_ptr<cl::LoadedMap> map;
  sv::RulesRunner rules;
  SequelNetConfig cfg;
  std::unique_ptr<SequelNetServer> net;  // on the network only
  // The logins (sequel_login.hpp), when sv_login asks for them or the server has accounts.
  gs::LoginSettings login_set;
  ge::account::Service* accounts = nullptr;
  ge::net::Transport* account_link = nullptr;
  int64_t (*unix_us)() = nullptr;
  const ge::net::SignKeys* signer = nullptr;  // signs the seats `sv_ranked` offers a master
  std::string base;                           // the ladder's game: the sequel's folder
  bool logins_wanted = false;
  std::unique_ptr<SequelLogin> login;
};

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  const ge::classicenhance::Profile* sequel = ge::classicenhance::find_profile("sequel");
  if (sequel == nullptr) {
    error = "this build has no content row for the sequel, so its game folder is unknown";
    return nullptr;
  }
  const std::string base(sequel->base_dir);
  const std::string mod = first_mod(c.mod);
  if (!cl::open_game_dir(c.root, mod.empty() ? base : mod, g->files, error, *sequel))
    return nullptr;
  const std::string name = c.map.empty() ? std::string("q2dm1") : c.map;
  g->map = cl::load_named_map(g->files, name, error);
  if (!g->map) return nullptr;
  if (g->map->spec.brushes == nullptr) {
    error = "maps/" + name + ".bsp is not a map of the sequel's family; name one from " + c.root +
            "/" + base;
    return nullptr;
  }
  std::filesystem::path library;
  if (!mod.empty() && sv::find_native_library(c.root, mod, base, library)) {
    // The mod's own rules: its game library hosted unchanged (sequel_native.hpp).
    sv::NativeSequelOptions no;
    no.library = library;
    no.max_clients = slots(c);
    no.game = mod;
    no.settings.emplace_back("hostname", c.hostname);
    for (const auto& [rule, value] : c.rules)
      if (!value.empty() && rule != "lrcon_password") no.settings.emplace_back(rule, value);
    std::unique_ptr<sv::NativeSequel> ns = sv::load_native_sequel(no, error);
    if (!ns) return nullptr;
    ns->map_checksum = sv::sequel_map_checksum(g->map->file);
    g->rules = sv::native_sequel_runner(std::move(ns));
  } else {
    sv::SequelServerOptions so;
    so.seed = c.seed;
    std::unique_ptr<sv::SequelServer> sq = sv::make_sequel_server(so);
    sq->sv.set.maxclients = slots(c);
    g->rules = sv::sequel_runner(std::move(sq));
  }
  g->rules.console("deathmatch 1");
  g->rules.console("sv_speedcheck 1");  // a server's default (the rules' alone: 0); a rule may undo
  if (!c.maprotation.empty()) {  // the rules' own list: this map, then the rotation's, wrapping
    g->rules->cvars.set("sv_maplist", name + " " + c.maprotation, ge::console::Source::local);
  }
  for (const auto& [rule, value] : c.rules) {
    if (value.empty()) continue;  // unset: the rules' own default
    if (rule.rfind("sv_download", 0) == 0 || rule == "sv_banmode" || rule == "sv_maxrate" ||
        rule == "sv_minrate" || rule == "sv_zombietime" || rule == "lrcon_password" ||
        rule == "sv_ghosttime" || rule == "sv_nodelta" || rule == "sv_pingmethod" ||
        rule == "sv_maxents" || rule == "sv_entpriority" || rule == "sv_novis")
      continue;  // the endpoint's
    std::string line = rule;
    line += ' ';
    line += value;
    g->rules.console(line);
  }
  if (!g->rules.start(g->map->spec, error)) return nullptr;
  g->cfg.hostname = c.hostname;
  g->cfg.max_clients = c.maxclients;
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.max_spectators = c.max_spectators;
  g->cfg.rcon_password = c.rcon_password;
  if (const auto it = c.rules.find("lrcon_password"); it != c.rules.end())
    g->cfg.lrcon.password = it->second;
  if (const auto it = c.rules.find("sv_redirect"); it != c.rules.end())
    g->cfg.redirect = it->second;
  if (const auto v = c.rules.find("sv_vippassword"); v != c.rules.end()) {  // its words
    std::string w;
    for (const char ch : v->second + ' ')
      if (ch != ' ')
        w += ch;
      else if (!w.empty())
        g->cfg.vip_passwords.push_back(std::exchange(w, {}));
  }
  g->cfg.unix_time = [] { return static_cast<int64_t>(std::time(nullptr)); };  // timed bans
  if (const auto b = c.rules.find("sv_banmode"); b != c.rules.end() && !b->second.empty()) {
    if (b->second != "0" && b->second != "1") {
      error = "sv_banmode \"" + b->second +
              "\" is not 0 or 1 (--explain=sv_banmode shows where it "
              "was set)";
      return nullptr;
    }
    g->cfg.ban_mode = b->second == "1" ? 1 : 0;
  }
  int lan_rate = 0;
  if (!endpoint_number(c, "sv_maxrate", 1000000, g->cfg.max_rate, error) ||
      !endpoint_number(c, "sv_minrate", 100000, g->cfg.min_rate, error) ||
      !endpoint_number(c, "sv_lanrate", 1, lan_rate, error))
    return nullptr;
  int zombie = 2, locked = 0;
  if (!endpoint_number(c, "sv_zombietime", 60, zombie, error) ||
      !endpoint_number(c, "sv_iplimit", 32, g->cfg.ip_limit, error) ||
      !endpoint_number(c, "sv_locked", 1, locked, error) ||
      !endpoint_number(c, "sv_vipslots", 32, g->cfg.vip_slots, error))
    return nullptr;
  int idle = 0;
  if (!endpoint_number(c, "sv_idletime", 86400, idle, error)) return nullptr;
  int ghost = 6, nodelta = 1, ping_method = 2;
  if (!endpoint_number(c, "sv_ghosttime", 60, ghost, error) ||
      !endpoint_number(c, "sv_nodelta", 1, nodelta, error) ||
      !endpoint_number(c, "sv_pingmethod", 2, ping_method, error))
    return nullptr;
  g->cfg.ghost_time = ghost;
  g->cfg.allow_nodelta = nodelta != 0;
  g->cfg.ping_method = ping_method;
  int priority = 0, novis = 0;
  if (!endpoint_number(c, "sv_maxents", 1024, g->cfg.wire.max_entities, error) ||
      !endpoint_number(c, "sv_entpriority", 1, priority, error) ||
      !endpoint_number(c, "sv_novis", 1, novis, error))
    return nullptr;
  g->cfg.wire.prioritize = priority != 0;
  g->cfg.cut_to_view = novis == 0;
  g->cfg.idle_time = idle;
  if (!gs::skew_settings(c, g->cfg.skew, error) || !gs::name_settings(c, g->cfg.names, error) ||
      !gs::limit_settings(c, g->cfg.rcon_rate, g->cfg.password_rate, error) ||
      !gs::status_settings(c, g->cfg.status, error))
    return nullptr;
  // Voice, as on the first game's server: off unless sv_voip is 1.
  int voip = 0, echo = 0;
  if (!endpoint_number(c, "sv_voip", 1, voip, error) ||
      !endpoint_number(c, "sv_voipecho", 1, echo, error) ||
      !endpoint_number(c, "sv_earshot", 65536, g->cfg.earshot, error))
    return nullptr;
  int antilag = 0, exact = 0;  // lag compensation, as on the first game's server: off unless 1
  if (!endpoint_number(c, "sv_antilag", 1, antilag, error) ||
      !endpoint_number(c, "sv_antilagexact", 1, exact, error))
    return nullptr;
  g->cfg.antilag = antilag == 1;
  g->cfg.antilag_exact = exact == 1;
  g->cfg.voice = voip == 1;
  g->cfg.voice_echo = echo == 1;
  g->cfg.zombie_time = zombie;
  g->cfg.locked = locked != 0;
  g->cfg.lan_rate = lan_rate != 0;
  // The ban list beside the game's files, where `writeip` keeps it.
  if (!g->files.folder.empty())
    g->cfg.ban_file = (std::filesystem::path(g->files.folder) / "listip.cfg").string();
  // Downloads: a client's missing file read from the game's folders, its archive told to ours.
  g->cfg.allow_download = switch_on(c, "sv_download");
  g->cfg.allow_players = switch_on(c, "sv_download_players");
  g->cfg.allow_models = switch_on(c, "sv_download_models");
  g->cfg.allow_sounds = switch_on(c, "sv_download_sounds");
  g->cfg.allow_maps = switch_on(c, "sv_download_maps");
  Game* gp = g.get();
  g->cfg.read_file = [gp](const std::string& path, std::vector<uint8_t>* out) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> got = cl::find_file(gp->files, path, store);
    out->assign(got.begin(), got.end());
    return !out->empty();
  };
  g->cfg.file_origin = [gp](const std::string& path, ge::vfs::ArchiveOrigin* out) {
    return ge::classic::content::served_origin(gp->files, path, out);
  };
  g->cfg.wire.map_checksum = sv::sequel_map_checksum(g->map->file);
  sv::sequel_sky(*g->map->spec.brushes, g->cfg.wire);
  g->login_set = c.login;
  g->accounts = c.accounts;
  g->account_link = c.account_link;
  g->unix_us = c.unix_us;
  g->signer = c.account_signer;
  g->base = base;
  g->logins_wanted = c.login.login > 0 || c.accounts != nullptr || c.account_link != nullptr;
  return g.release();
}

// The map the rules asked for (an intermission's end, `map`, `changelevel`, `restart`) loaded and
// the rules started on it, the clients told; a map that cannot be read starts this one again.
void change_level(Game& g) {
  sv::RulesCommon& s = *g.rules;
  if (s.pending_map.empty()) return;
  const std::string asked = s.pending_map;
  std::string error;
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(g.files, asked, error);
  if (next && next->spec.brushes == nullptr) {
    error = "maps/" + asked + ".bsp is not a map of the sequel's family";
    next.reset();
  }
  g.rules.carry();
  if (sv::NativeSequel* ns = sv::native_sequel_of(g.rules); ns != nullptr && next)
    ns->map_checksum = sv::sequel_map_checksum(next->file);
  if (!next || !g.rules.start(next->spec, error)) {
    if (g.net) g.net->log.push_back("the level stays on " + g.map->name + ": " + error);
    if (sv::NativeSequel* ns = sv::native_sequel_of(g.rules); ns != nullptr)
      ns->map_checksum = sv::sequel_map_checksum(g.map->file);
    if (!g.rules.start(g.map->spec, error)) {
      if (g.net) g.net->log.push_back("the level could not start again: " + error);
      s.pending_map.clear();
      return;
    }
  } else {
    g.map = std::move(next);
  }
  g.cfg.wire.map_checksum = sv::sequel_map_checksum(g.map->file);
  sv::sequel_sky(*g.map->spec.brushes, g.cfg.wire);
  if (g.net) sequel_net_level(*g.net, g.cfg.wire);
}

void destroy(void* g) {
  Game* game = static_cast<Game*>(g);
  if (game->net) sequel_net_stop(*game->net);
  delete game;
}

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  game.rules.frame(nullptr, 0);
  change_level(game);
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.net ? sequel_net_clients(*game.net) : 0;
}

uint64_t hash(const void* g) {
  const sv::RulesRunner& rules = static_cast<const Game*>(g)->rules;
  if (const sv::NativeSequel* ns = sv::native_sequel_of(rules)) return sv::native_sequel_hash(*ns);
  return sv::rules_hash(*rules);
}

void fill_status(const void* g, ge::net::Status& st) {
  const Game& game = *static_cast<const Game*>(g);
  st.map = game.map->name;
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.net = std::make_unique<SequelNetServer>();
  if (!sequel_net_start(*game.net, game.rules, port, game.cfg, error)) return false;
  gs::ban_file_read(game.net->bans, game.cfg.ban_file, game.cfg.unix_time());
  if (!game.logins_wanted) return true;
  game.login = std::make_unique<SequelLogin>();
  game.login->game = game.base;
  if (!sequel_login_open(*game.login, *game.net, game.login_set, game.accounts, game.account_link,
                         game.unix_us, error))
    return false;
  game.login->logins.signer = game.signer;
  return true;
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  sequel_net_frame(*game.net, now_us);
  change_level(game);
  if (game.login) sequel_login_frame(*game.login);
}

// The broadcast and a player's view (sequel_game_tv.hpp): every player's state whole, a frame
// and a keyframe alike.
void tv_state(const void* g, std::vector<uint8_t>& out) {
  sequel_tv_state(*static_cast<const Game*>(g)->rules, out);
}

void player_view(const void* g, int player, std::vector<uint8_t>& out) {
  out.clear();
  sequel_player_view(*static_cast<const Game*>(g)->rules, player, out);
}

const gs::ServerGame& make() {
  static gs::ServerGame g = [] {
    gs::ServerGame s;
    s.binary = "sesv";
    s.title = "Shooter's Edge on the sequel's protocol";
    s.default_port = kPortSequel;
    s.max_clients = 32;
    s.default_map = "q2dm1";
    s.base_dir = "baseq2";
    s.rules = all_rules().data();
    s.rule_count = static_cast<int>(all_rules().size());
    s.names = kSettingNames;
    s.name_count = static_cast<int>(std::size(kSettingNames));
    s.create = create;
    s.destroy = destroy;
    s.frame_seconds = frame_seconds;
    s.frame = frame;
    s.seated = seated;
    s.hash = hash;
    s.fill_status = fill_status;
    s.serve = serve;
    s.net_frame = net_frame;
    s.tv_frame = tv_state;
    s.tv_keyframe = tv_state;
    s.player_view = player_view;
    sequel_watch_hooks(s);
    return s;
  }();
  return g;
}

}  // namespace

const gs::ServerGame& sequel_game() { return make(); }

}  // namespace shooters_edge::net
