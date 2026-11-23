// Quake II's dedicated server game (q2_game.hpp).
//
// ge:summary Quake II's dedicated server game: its maps read, the rules started and moved on
// ge:keywords q2-dedicated-server q2_game protocol-34 servergame sv_maxrate sv_minrate
// ge:keywords sv_public public classic-master-announce
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_game.hpp"

#include "classic/protocol/masters.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "classic/content/downloads.hpp"
#include "classic/content/fs_words.hpp"
#include "classic/content/load_order.hpp"
#include "classic/content/web_downloads.hpp"
#include "classic/content/web_serve.hpp"
#include "classicenhance/profile.hpp"
#include "core/num.hpp"
#include "net/tcp_socket.hpp"
#include "server/admission.hpp"
#include "server/idle.hpp"
#include "server/limit.hpp"
#include "server/lrcon.hpp"
#include "server/names.hpp"
#include "server/rate.hpp"
#include "server/skew.hpp"
#include "server/status.hpp"
#include "server/web_files.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_game_tv.hpp"
#include "shooters_edge/net/q2_login.hpp"
#include "shooters_edge/net/q2_match.hpp"
#include "shooters_edge/net/q2_mvd_relay.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_download.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/q2_votes.hpp"
#include "shooters_edge/net/qw_report.hpp"
#include "shooters_edge/net/server_words.hpp"
#include "shooters_edge/server/q2_mapfix.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"

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
    {"sv_download_textures", "Whether files in textures/ and env/ (the skies) are offered.",
     nullptr, "0: not offered; 1: offered (the default)", false},
    {"sv_download_pics", "Whether files in pics/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)", false},
    {"sv_download_others", "Whether files in any other folder are offered.", "0",
     "0: not offered (the default); 1: offered", false},
    {"sv_max_download_size", "The largest file offered, in bytes.", "8388608",
     "N: that many bytes (8388608 the default); 0: the most, 128 MiB and 4 KiB", false},
    {"sv_downloadserver", "A web address clients may fetch the server's files from instead.",
     nullptr, "empty: none (the default); a URL ending in /", false},
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
    {"sv_vispatch",
     "Whether four stock maps' visibility is mended, so players at a few spots are not hidden "
     "from those who can see them.",
     nullptr, "1: mended (the default); 0: as the maps were made"},
    {"sv_entpath",
     "A folder of entity files that replace a map's own: <map>@<crc>.ent for one build of the "
     "map, else <map>.ent.",
     nullptr, "\"\": none (the default); TEXT: the folder, in the game's files"},
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
    {"sv_public", "Whether the server announces itself to the classic masters setmaster names.",
     nullptr,
     "0: it does not until setmaster is used (the default); 1: from the map's start; read when "
     "a map starts"},
    {"sv_banmode", "How the ban list made by addip is read.", nullptr,
     "0: only the addresses on it may join; 1: the addresses on it are refused (the default)"},
    {"sv_antilag", "Whether a shot sees the others where the shooter saw them.", nullptr,
     "0: off (the default); 1: bullets, pellets and slugs; 2: the same"},
    {"sv_antilag_projectiles",
     "Whether, with sv_antilag 2, a player's rockets, grenades and bolts meet the others where he "
     "saw them.",
     nullptr, "0: off (the default); 1: on"},
    {"sv_antilag_interp", "Whether a shot sees the others one frame further back, as drawn.",
     nullptr, "0: off (the default); 1: a frame (1 / sv_fps) further back"},
    {"sv_antilag_max", "How far back a shot may see the others, in milliseconds.", nullptr,
     "0: as far as the history goes; 1..1000: that many (250 the default)"},
    {"sv_partialnames",
     "Whether the operator's kick, kickban, dumpuser, stuff and stuffcvar also find a player by "
     "his name in any case or a unique part of it.",
     nullptr, "0: a slot number or the exact name (the default); 1: a part of a name too", false},
    {"sv_packetdup", "The most extra copies of each frame a player may ask for with packetdup.",
     nullptr, "0: none (the default); 1..8: that many", false},
    {"sv_forcereconnect",
     "An address every newcomer is first sent to and must come back from, with a setting it must "
     "bring back; from the next map.",
     nullptr, "empty: none (the default); an address: clients reconnect to it first", false},
    {"sv_maxmsglen",
     "The largest packet a player whose game speaks the newer protocols may ask for, in bytes.",
     nullptr, "0: what the player asks; 512..4086: at most that many (1390 the default)", false},
    // The multiview recorder and its relay stream (q2_mvd_server.hpp).
    {"sv_mvd_enable", "Whether the server keeps a multiview recorder, and serves it to relays.",
     nullptr, "0: none (the default); 1: recordings (mvdrecord); 2: relays over TCP too", false},
    {"sv_mvd_maxclients", "How many relays may watch over TCP at once.", nullptr,
     "1..256 (8 the default)", false},
    {"sv_mvd_bufsize", "How much a relay may fall behind before it is dropped, in 32 KiB.", nullptr,
     "1..4 (2 the default)", false},
    // The relay's password under the name every classic game's server shares (QuakeWorld's
    // relays too: net/qw_endpoint_cfg.cpp); `sv_mvd_password` its spelling here.
    {"qtv_password", "The password a relay must give.", nullptr,
     "\"\": none (the default); TEXT: that password", false},
    {"sv_mvd_maxsize", "The size a multiview recording stops at, in kilobytes.", nullptr,
     "0: no limit (the default); 1..2000000", false},
    {"sv_mvd_maxtime", "The length a multiview recording stops at, in minutes.", nullptr,
     "0: no limit (the default); a count of minutes", false},
    {"sv_mvd_maxmaps", "The level changes a multiview recording stops at.", nullptr,
     "0: never; 1 the default (it stops at the first change)", false},
    {"sv_mvd_noblend", "Whether the multiview stream leaves the players' screen blends out.",
     nullptr, "0: kept (the default); 1: left out", false},
    {"sv_mvd_nogun", "Whether the multiview stream leaves the players' held weapons out.", nullptr,
     "0: kept (the default); 1: left out", false},
    {"sv_mvd_suspend_time",
     "The minutes with no player in the game after which the multiview stream pauses.", nullptr,
     "0: never; 5 the default", false},
    {"sv_mvd_disconnect_time",
     "The minutes with no recording and no relay after which the multiview recorder stops.",
     nullptr, "0: never; 15 the default", false},
    {"sv_cinematics",
     "Whether a movie an exit names before its map is shown, as a level of its own.", nullptr,
     "0: skipped when something follows it; 1: shown (the default)", false},
    {"sv_mvd_spawn_dummy",
     "Whether the multiview recorder keeps its own spectator in the game for the scoreboard.",
     nullptr, "0: never; 1: when the rules know it (the default; ours do); 2: always", false},
    {"sv_mvd_begincmd", "The commands the recorder's spectator runs as it enters a level.", nullptr,
     "wait 50; putaway; wait 10; help; the default", false},
    {"sv_mvd_scorecmd",
     "The commands the recorder's spectator runs when its scoreboard has been gone 9 seconds.",
     nullptr, "putaway; wait 10; help; the default; empty: none", false},
    {"sv_mvd_capture_flags", "Which players the multiview stream records, as bits.", nullptr,
     "1: only players with a connection; 2: not the unseen; 4: every seen one (5 the default)",
     false},
    {"sv_mvd_nomsgs",
     "Whether the multiview stream keeps only the recorder's spectator's own messages.", nullptr,
     "0: everyone's; 1: the spectator's alone (the default)", false},
    {"sv_mvd_allow_stufftext",
     "Whether the recorder's spectator runs what the game sends it, or what relays send it.",
     nullptr, "-1: the relays' commands; 0: neither (the default); 1: the game's lines", false},
    {"sv_mvd_autorecord",
     "Whether the recorder's spectator is made at every level, so the game can record with it.",
     nullptr, "0: no (the default); 1: yes", false},
    // The relay (q2_mvd_relay.hpp): another server's stream watched, or a recording played.
    {"mvd_timeout", "The seconds a relay waits on a silent game server before it drops it.",
     nullptr, "90 the default", false},
    {"mvd_suspend_time",
     "The minutes with no viewer doing anything after which a relay stops its streams.", nullptr,
     "0: never; 5 the default", false},
    {"mvd_wait_delay", "The seconds of the match a relay holds before it plays a channel.", nullptr,
     "20 the default", false},
    {"mvd_wait_percent", "How full a relay's buffer may get before it plays a channel anyway.",
     nullptr, "50 the default (percent)", false},
    {"mvd_buffer_size", "The size of a relay's buffer for each channel, in 32 KiB.", nullptr,
     "8 the default", false},
    {"mvd_username", "The name a relay gives the game server.", nullptr, "unnamed the default",
     false},
    {"mvd_password", "The password a relay gives the game server.", nullptr, "empty the default",
     false},
    {"mvd_snaps", "The seconds between the snapshots a played recording seeks back to.", nullptr,
     "0: none; 10 the default", false},
    {"mvd_admin_password", "The password a relay's viewer becomes its admin with.", nullptr,
     "empty: nobody (the default)", false},
    {"mvd_stats_score", "What a relay's viewer sees as the frags on the status bar.", nullptr,
     "0: nothing (the default); 1: the channel's number; 2: the followed player's frags", false},
    {"mvd_filter_version", "Whether a relay hides the version trigger in the players' chat.",
     nullptr, "0: chat as said (the default); 1: each !version shown as !versi0n", false},
    {"mvd_stats_hack",
     "Which numbers of a relay's chased player's status bar come from the recorder's spectator.",
     nullptr, "0: none (the default); a mask of the stats 0 to 31", false},
    {"mvd_freeze_hack",
     "Whether a relay takes the recorder's spectator frozen as the intermission and shows it.",
     nullptr, "0: no; 1: yes (the default)", false},
    {"mvd_part_filter", "Whose comings and goings a relay's channel is told of.", nullptr,
     "0: everyone's (the default); below 0: nobody's; N: who talked within N s", false},
    {"mvd_default_map", "The map of a relay's Waiting Room.", nullptr, "q2dm1 the default", false},
    {"mvd_chase_prefix", "Where a relay's chase line is drawn, as a layout's position.", nullptr,
     "xv 0 yb -64 the default", false},
    {"sys_allow_unsafe_savegames",
     "Lets a game library that cannot be saved safely be saved and loaded anyway; set at start.",
     "0", "0: such a game is never saved (the default); 1: it is", false},
};

// The rules a server may set: the engine's rate bounds and per-address limit (server/rate.hpp,
// admission.hpp), then Quake II's own.
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
          std::span<const gs::GameRule>(gs::kLrconRules),
          std::span<const gs::GameRule>(gs::kWebFilesRules), std::span<const gs::GameRule>(kRules)})
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

// Quake II's own names for the download settings, as its configs and admins write them.
constexpr gs::SettingName kSettingNames[] = {
    {"allow_download", "sv_download"},
    {"allow_download_players", "sv_download_players"},
    {"allow_download_models", "sv_download_models"},
    {"allow_download_sounds", "sv_download_sounds"},
    {"allow_download_maps", "sv_download_maps"},
    {"allow_download_textures", "sv_download_textures"},
    {"allow_download_pics", "sv_download_pics"},
    {"allow_download_others", "sv_download_others"},
    {"filterban", "sv_banmode"},
    {"public", "sv_public"},
    {"map_override_path", "sv_entpath"},
    {"map_visibility_patch", "sv_vispatch"},
    {"maxspectators", "sv_maxspectators"},
    {"spectator_password", "sv_spectatorpassword"},
    {"sv_allow_nodelta", "sv_nodelta"},
    {"sv_auth_limit", "sv_passwordrate"},
    {"sv_calcpings_method", "sv_pingmethod"},
    {"sv_changemapcmd", "sv_mapcmd"},
    {"sv_enforcetime", "sv_speedcheck"},
    {"sv_ghostime", "sv_ghosttime"},
    {"sv_idlekick", "sv_idletime", -1, true},  // the idle drop here, not the lobby's unready kick
    {"sv_lan_force_rate", "sv_lanrate"},
    {"sv_max_packet_entities", "sv_maxents"},
    {"sv_max_rate", "sv_maxrate"},
    {"sv_min_rate", "sv_minrate"},
    {"sv_mvd_password", "qtv_password"},
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
    {"sv_enhanced_setplayer", "sv_partialnames"},
    {"sv_packetdup_hack", "sv_packetdup"},
    {"sv_force_reconnect", "sv_forcereconnect"},
    {"net_maxmsglen", "sv_maxmsglen"},
    {"sys_history", "con_history"},
    {"com_completion_mode", "con_completion_mode"},
    {"com_completion_treshold", "con_completion_threshold"},
    {"logfile", "con_logfile"},
    {"logfile_flush", "con_logfile_flush"},
    {"logfile_name", "con_logfile_name"},
    {"logfile_prefix", "con_logfile_prefix"},
    {"net_log_enable", "net_log"},
};

// A download switch from the rules (`sv_download...`): "0" off, anything else (or unset) on.
bool switch_on(const gs::ServerConfig& c, const char* name) {
  const auto r = c.rules.find(name);
  return r == c.rules.end() || r->second.empty() || r->second != "0";
}

// The first of `sv_mod`'s comma-separated folders ("" none): Quake II's mod folder.
std::string first_mod(const std::string& mods) {
  std::string m = mods.substr(0, mods.find(','));
  while (!m.empty() && m.back() == ' ') m.pop_back();
  while (!m.empty() && m.front() == ' ') m.erase(0, 1);
  return m;
}

// The players' slots: a deathmatch server (the one a dedicated server starts) of one slot or
// fewer is given 8, as Quake II's server does.
int players(const gs::ServerConfig& c) { return c.maxclients <= 1 ? 8 : c.maxclients; }

// The rules' slots: the players' and the spectators' own (q2_net_server.hpp).
int slots(const gs::ServerConfig& c) {
  return std::min(sh::kMaxClients, players(c) + std::max(0, c.max_spectators));
}

struct Game {
  // The install's root and the game folder the files were opened from (`fs_restart` opens them
  // again).
  std::string root, folder;
  // The masters it reports to (`setmaster`, `heartbeat`; classic/protocol/masters.hpp).
  ge::classic::protocol::Masters masters;
  cl::SearchPath files;
  std::unique_ptr<cl::LoadedMap> map;
  sv::RulesRunner rules;
  Q2NetConfig cfg;
  gs::WebFiles web;                  // the server's own web files (sv_http_port)
  std::unique_ptr<Q2NetServer> net;  // on the network only
  // The logins (q2_login.hpp), when sv_login asks for them or the server has accounts.
  gs::LoginSettings login_set;
  ge::account::Service* accounts = nullptr;
  ge::net::Transport* account_link = nullptr;
  int64_t (*unix_us)() = nullptr;
  const ge::net::SignKeys* signer = nullptr;  // signs the seats `sv_ranked` offers a master
  std::string base;                           // the ladder's game: Quake II's folder
  bool logins_wanted = false;
  std::unique_ptr<Q2Login> login;
  // The relay (q2_mvd_relay.hpp): once relay mode starts it takes the port from the game.
  Q2Relay relay;
  ge::net::Transport* port = nullptr;
  bool relayed = false;  // the game's clients were sent away to the relay
  double now_s = 0;
  // The maps mended as they load (server/q2_mapfix.hpp): sv_vispatch, sv_entpath; what that said,
  // for the log once the network is up.
  bool vis_patch = true;
  std::string ent_path;
  std::vector<std::string> said;
  std::string mode;  // `gamemode`'s choice for the next map ("": as it is)
  // CINEMATIC AND PICTURE LEVELS (show_level): the files still to show after the one on now, the
  // map that follows them ("" none), the `nextserver` text we set (run as ours when unchanged),
  // whether this show's `nextserver` was taken; `sv_cinematics` (0: a movie with something after it
  // is skipped).
  std::vector<std::string> shows;
  std::string show_map, show_next;
  // The map a new game starts on: the game folder's default.cfg's `newgame` (a mission pack's own
  // first map), else base1.
  std::string first_map = "base1";
  bool show_taken = false, show_new_game = false, cinematics = true;
  bool drop_told = false;  // the stopped map's error handed to the core (drop_error)
  // What the console printed and was handed on (console_lines): the rules' lines told so far.
  const sv::RulesCommon* told_rules = nullptr;
  size_t told = 0;
  Q2Votes votes;  // the players' votes and the settings sheet (q2_votes.hpp)
  // The match: the lobby, timeouts, overtime (q2_match.hpp), opened with the network from the
  // settings the server started with.
  Q2Match match;
  gs::ServerConfig match_cfg;
};

// `m` mended as it loads (server/q2_mapfix.hpp): the visibility patch, then an entity file.
void mend(Game& g, cl::LoadedMap& m) {
  const auto say = [&g](std::string line) {
    if (g.net)
      g.net->log.push_back(std::move(line));
    else
      g.said.push_back(std::move(line));
  };
  if (g.vis_patch && sv::q2_vis_patch(m.bsp.map, sv::q2_map_checksum(m.file)))
    say("maps/" + m.name + ".bsp: its visibility mended (sv_vispatch)");
  if (g.ent_path.empty()) return;
  for (const std::string& file :
       {g.ent_path + "/" + sv::q2_entity_file_name(m.name, m.bsp.map.entity_text),
        g.ent_path + "/" + m.name + ".ent"}) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> got = cl::find_file(g.files, file, store);
    if (got.empty()) continue;
    if (got.size() < 2) {
      say("Couldn't load entity string from " + file + ": File too small");
      return;
    }
    // The rules spawn from the map's parsed entities, a hosted library from its text: both.
    std::string text(got.begin(), got.end());
    std::vector<ge::brush::Entity> parsed;
    std::string why;
    if (!ge::brush::parse_entities(text, &parsed, &why)) {
      std::string line = "Couldn't load entity string from " + file;
      line += ": ";
      line += why;
      say(std::move(line));
      return;
    }
    m.bsp.map.entities = std::move(parsed);
    m.bsp.map.entity_text = text;
    m.spec.entities = std::move(text);
    say("Loaded entity string from " + file);
    return;
  }
}

void* create(const gs::ServerConfig& c, std::string& error) {
  auto g = std::make_unique<Game>();
  const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
  if (q2 == nullptr) {
    error = "this build has no content row for Quake II, so its game folder is unknown";
    return nullptr;
  }
  const std::string base(q2->base_dir);
  std::string mod = first_mod(c.mod);
  std::string said;  // the game folder's name checked: a bad one is the base folder's
  ge::classic::content::game_folder_check(mod, base, said);
  if (!said.empty()) g->said.push_back(said.substr(0, said.size() - 1));
  g->root = c.root;
  g->folder = mod.empty() ? base : mod;
  if (!cl::open_game_dir(c.root, g->folder, g->files, error, *q2)) return nullptr;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> cfg = cl::find_file(g->files, "default.cfg", store);
    const std::string own = ge::classic::content::new_game_map(
        std::string_view(reinterpret_cast<const char*>(cfg.data()), cfg.size()));
    if (!own.empty()) g->first_map = own;
  }
  const std::string name = c.map.empty() ? std::string("q2dm1") : c.map;
  if (const auto v = c.rules.find("sv_vispatch"); v != c.rules.end() && !v->second.empty())
    g->vis_patch = v->second != "0";
  if (const auto v = c.rules.find("sv_cinematics"); v != c.rules.end() && !v->second.empty())
    g->cinematics = v->second != "0";
  if (const auto p = c.rules.find("sv_entpath"); p != c.rules.end()) g->ent_path = p->second;
  while (!g->ent_path.empty() && g->ent_path.back() == '/') g->ent_path.pop_back();
  g->map = cl::load_named_map(g->files, name, error);
  if (!g->map) return nullptr;
  mend(*g, *g->map);
  if (g->map->spec.brushes == nullptr) {
    error = "maps/" + name + ".bsp is not a map of Quake II's family; name one from " + c.root +
            "/" + base;
    return nullptr;
  }
  std::filesystem::path library;
  if (!mod.empty() && sv::find_native_library(c.root, mod, base, library)) {
    // The mod's own rules: its game library hosted unchanged (q2_native.hpp).
    sv::NativeQ2Options no;
    no.library = library;
    no.max_clients = slots(c);
    no.game = mod;
    no.settings.emplace_back("hostname", c.hostname);
    for (const auto& [rule, value] : c.rules)
      if (!value.empty() && rule != "lrcon_password") no.settings.emplace_back(rule, value);
    std::unique_ptr<sv::NativeQ2> ns = sv::load_native_q2(no, error);
    if (!ns) return nullptr;
    ns->map_checksum = sv::q2_map_checksum(g->map->file);
    g->rules = sv::native_q2_runner(std::move(ns));
  } else {
    sv::Q2ServerOptions so;
    so.seed = c.seed;
    so.pack = sv::q2_pack_of_folder(g->folder);  // a mission pack's folder plays its rules
    std::unique_ptr<sv::Q2Server> sq = sv::make_q2_server(so);
    sq->sv.set.maxclients = slots(c);
    g->rules = sv::q2_runner(std::move(sq));
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
        rule == "sv_maxents" || rule == "sv_entpriority" || rule == "sv_novis" ||
        rule == "sv_vispatch" || rule == "sv_entpath" || rule == "sv_partialnames" ||
        rule == "sv_packetdup" || rule == "sv_forcereconnect" || rule == "sv_maxmsglen" ||
        rule == "sv_cinematics" || rule == "sv_public" || rule.rfind("sv_mvd_", 0) == 0 ||
        rule.rfind("mvd_", 0) == 0)
      continue;  // the endpoint's
    std::string line = rule;
    line += ' ';
    line += value;
    g->rules.console(line);
  }
  if (!g->rules.start(g->map->spec, error)) return nullptr;
  q2_votes_rules(g->votes, g->rules, gs::vote_settings(c));
  g->match_cfg = c;
  Game* game = g.get();
  g->votes.map = [game] { return game->map ? game->map->name : std::string(); };
  g->votes.has_map = [game](const std::string& wanted) {
    std::string why;
    return cl::load_named_map(game->files, wanted, why) != nullptr;
  };
  g->cfg.hostname = c.hostname;
  g->cfg.version = kProgramVersion;
  g->cfg.game_dir = mod;
  g->cfg.admin_info = c.admin_info;
  g->cfg.max_clients = players(c);
  g->cfg.password = c.password;
  g->cfg.spectator_password = c.spectator_password;
  g->cfg.max_spectators = c.max_spectators;
  g->cfg.rcon_password = c.rcon_password;
  if (const auto it = c.rules.find("lrcon_password"); it != c.rules.end())
    g->cfg.lrcon.password = it->second;
  if (const auto it = c.rules.find("sv_redirect"); it != c.rules.end())
    g->cfg.redirect = it->second;
  if (const auto it = c.rules.find("sv_ruleset"); it != c.rules.end()) g->cfg.ruleset = it->second;
  if (const auto v = c.rules.find("sv_vippassword"); v != c.rules.end()) {  // its words
    std::string w;
    for (const char ch : v->second + ' ')
      if (ch != ' ')
        w += ch;
      else if (!w.empty())
        g->cfg.vip_passwords.push_back(std::exchange(w, {}));
  }
  g->cfg.unix_time = [] { return static_cast<int64_t>(std::time(nullptr)); };  // timed bans
  // A public server beats to the classic masters from the start (setmaster also makes it one).
  if (const auto p = c.rules.find("sv_public"); p != c.rules.end())
    g->masters.is_public = p->second == "1";
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
  int idle = 0, partial = 0;
  if (!endpoint_number(c, "sv_idletime", 86400, idle, error) ||
      !endpoint_number(c, "sv_partialnames", 1, partial, error) ||
      !endpoint_number(c, "sv_packetdup", 8, g->cfg.packetdup_most, error) ||
      !endpoint_number(c, "sv_maxmsglen", 4086, g->cfg.net_maxmsglen, error))
    return nullptr;
  if (g->cfg.net_maxmsglen > 0 && g->cfg.net_maxmsglen < 512) g->cfg.net_maxmsglen = 512;
  {  // the multiview recorder
    Q2MvdConfig& m = g->cfg.mvd;
    int noblend = 0, nogun = 0, maxsize = 0, maxtime = 0, nomsgs = 1, autorecord = 0;
    if (!endpoint_number(c, "sv_mvd_enable", 2, m.enable, error) ||
        !endpoint_number(c, "sv_mvd_maxclients", 256, m.maxclients, error) ||
        !endpoint_number(c, "sv_mvd_bufsize", 4, m.bufsize, error) ||
        !endpoint_number(c, "sv_mvd_maxsize", 2000000, maxsize, error) ||
        !endpoint_number(c, "sv_mvd_maxtime", 3456000, maxtime, error) ||
        !endpoint_number(c, "sv_mvd_maxmaps", 1000000, m.maxmaps, error) ||
        !endpoint_number(c, "sv_mvd_noblend", 1, noblend, error) ||
        !endpoint_number(c, "sv_mvd_nogun", 1, nogun, error) ||
        !endpoint_number(c, "sv_mvd_suspend_time", 34560, m.suspend_min, error) ||
        !endpoint_number(c, "sv_mvd_disconnect_time", 34560, m.disconnect_min, error) ||
        !endpoint_number(c, "sv_mvd_spawn_dummy", 2, m.spawn_dummy, error) ||
        !endpoint_number(c, "sv_mvd_capture_flags", 7, m.capture_flags, error) ||
        !endpoint_number(c, "sv_mvd_nomsgs", 1, nomsgs, error) ||
        !endpoint_number(c, "sv_mvd_autorecord", 1, autorecord, error))
      return nullptr;
    m.nomsgs = nomsgs != 0;
    m.autorecord = autorecord != 0;
    if (const auto it = c.rules.find("sv_mvd_allow_stufftext"); it != c.rules.end())
      m.allow_stufftext = std::clamp(std::atoi(it->second.c_str()), -1, 1);
    if (const auto it = c.rules.find("sv_mvd_begincmd"); it != c.rules.end())
      m.begincmd = it->second;
    if (const auto it = c.rules.find("sv_mvd_scorecmd"); it != c.rules.end())
      m.scorecmd = it->second;
    m.maxclients = std::max(m.maxclients, 1);
    m.bufsize = std::max(m.bufsize, 1);
    m.maxsize_kb = maxsize;
    m.maxtime_min = maxtime;
    m.noblend = noblend != 0;
    m.nogun = nogun != 0;
    if (const auto it = c.rules.find("qtv_password"); it != c.rules.end()) m.password = it->second;
    m.demos = (std::filesystem::path(c.root) / base / "demos").string();
  }
  {  // the relay
    Q2RelayConfig& rc = g->relay.cfg;
    int timeout = 90;
    if (!endpoint_number(c, "mvd_timeout", 2073600, timeout, error) ||
        !endpoint_number(c, "mvd_suspend_time", 34560, rc.suspend_min, error) ||
        !endpoint_number(c, "mvd_wait_delay", 3600, rc.wait_delay, error) ||
        !endpoint_number(c, "mvd_wait_percent", 1000000, rc.wait_percent, error) ||
        !endpoint_number(c, "mvd_buffer_size", 256, rc.buffer_size, error) ||
        !endpoint_number(c, "mvd_snaps", 1000000, rc.snaps, error))
      return nullptr;
    rc.timeout = timeout;
    const auto text = [&c](const char* rule, std::string& out) {
      if (const auto it = c.rules.find(rule); it != c.rules.end() && !it->second.empty())
        out = it->second;
    };
    text("mvd_username", rc.username);
    text("mvd_password", rc.upstream_password);
    text("mvd_admin_password", rc.admin_password);
    text("mvd_default_map", rc.default_map);
    text("mvd_chase_prefix", rc.chase_prefix);
    if (const auto it = c.rules.find("mvd_part_filter"); it != c.rules.end() && !it->second.empty())
      rc.part_filter = std::atoi(it->second.c_str());
    if (const auto it = c.rules.find("mvd_stats_score"); it != c.rules.end() && !it->second.empty())
      rc.stats_score = std::atoi(it->second.c_str());
    if (const auto it = c.rules.find("mvd_filter_version"); it != c.rules.end())
      rc.filter_version = std::atoi(it->second.c_str()) != 0;
    if (const auto it = c.rules.find("mvd_stats_hack"); it != c.rules.end())
      rc.stats_hack = static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 0));
    if (const auto it = c.rules.find("mvd_freeze_hack"); it != c.rules.end())
      rc.freeze_hack = std::atoi(it->second.c_str()) != 0;
    // The chat flood rule's settings, the rules' own names, read by the relay's chat too.
    if (const auto it = c.rules.find("flood_msgs"); it != c.rules.end() && !it->second.empty())
      rc.flood_msgs = std::atoi(it->second.c_str());
    if (const auto it = c.rules.find("flood_persecond"); it != c.rules.end() && !it->second.empty())
      rc.flood_persecond = std::atof(it->second.c_str());
    if (const auto it = c.rules.find("flood_waitdelay"); it != c.rules.end() && !it->second.empty())
      rc.flood_waitdelay = std::atof(it->second.c_str());
    if (const auto it = c.rules.find("flood_mute"); it != c.rules.end() && !it->second.empty())
      rc.flood_mute = std::atoi(it->second.c_str()) != 0;
    rc.demos = g->cfg.mvd.demos;
    rc.hostname = c.hostname.empty() ? rc.hostname : c.hostname;
  }
  g->cfg.enhanced_setplayer = partial != 0;
  if (const auto it = c.rules.find("sv_forcereconnect"); it != c.rules.end())
    g->cfg.force_reconnect = it->second;
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
  // Voice, as on Quake's server: off unless sv_voip is 1.
  int voip = 0, echo = 0;
  if (!endpoint_number(c, "sv_voip", 1, voip, error) ||
      !endpoint_number(c, "sv_voipecho", 1, echo, error) ||
      !endpoint_number(c, "sv_earshot", 65536, g->cfg.earshot, error))
    return nullptr;
  // Lag compensation, as on Quake's server: off unless 1 or 2; at most 250 ms back.
  int antilag = 0, exact = 0, projectiles = 0, interp = 0, most = 250;
  if (!endpoint_number(c, "sv_antilag", 2, antilag, error) ||
      !endpoint_number(c, "sv_antilagexact", 1, exact, error) ||
      !endpoint_number(c, "sv_antilag_projectiles", 1, projectiles, error) ||
      !endpoint_number(c, "sv_antilag_interp", 1, interp, error) ||
      !endpoint_number(c, "sv_antilag_max", 1000, most, error))
    return nullptr;
  g->cfg.antilag = antilag;
  g->cfg.antilag_exact = exact == 1;
  g->cfg.antilag_projectiles = projectiles == 1;
  g->cfg.antilag_interp = interp == 1;
  g->cfg.antilag_max_ms = most;
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
  g->cfg.allow_textures = switch_on(c, "sv_download_textures");
  g->cfg.allow_pics = switch_on(c, "sv_download_pics");
  g->cfg.allow_others = switch_on(c, "sv_download_others");
  if (const auto it = c.rules.find("sv_max_download_size"); it != c.rules.end())
    g->cfg.max_download_size = std::atoll(it->second.c_str());
  if (const auto it = c.rules.find("sv_downloadserver"); it != c.rules.end())
    g->cfg.download_server = it->second;
  if (!gs::web_files_config(c, g->cfg.web, error)) return nullptr;
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
  g->cfg.read_deflated = [gp](const std::string& path, std::vector<uint8_t>* out) {
    return ge::classic::content::served_deflated(gp->files, path, out);
  };
  g->cfg.wire.map_checksum = sv::q2_map_checksum(g->map->file);
  sv::q2_sky(*g->map->spec.brushes, g->cfg.wire);
  g->login_set = c.login;
  g->accounts = c.accounts;
  g->account_link = c.account_link;
  g->unix_us = c.unix_us;
  g->signer = c.account_signer;
  g->base = base;
  g->logins_wanted = c.login.login > 0 || c.accounts != nullptr || c.account_link != nullptr;
  return g.release();
}

// A SHOW (a `.cin` or `.pcx` an exit names before its map, or the game's end): on the network the
// level changes to the file (no world: the server data names it, Q2WireOptions::show) and the
// clients play it. Its `nextserver` setting is `gamemap "!<what follows>"`, or `gamemap "!*<first
// map>"` after co-op's victory picture (the game's `newgame` map, `first_map`), or empty. A
// client's `nextserver <count>` is taken once a show, with the level's count, on a movie or (in
// co-op) a picture: the setting run and emptied, an empty one stopping the server (`killserver`).
bool is_movie(const std::string& f) { return f.ends_with(".cin"); }

bool coop_on(const Game& g) {
  const ge::console::Cvar* v = (*g.rules).cvars.find("coop");
  return v != nullptr && std::atoi(v->value.c_str()) != 0;
}

void start_show(Game& g) {
  const std::string show = g.shows.front();
  g.shows.erase(g.shows.begin());
  std::string rest;
  for (const std::string& f : g.shows) rest += (rest.empty() ? "" : "+") + f;
  if (!g.show_map.empty()) rest += (rest.empty() ? "" : "+") + g.show_map;
  const bool coop = coop_on(g);
  g.show_new_game = rest.empty() && coop && show == "victory.pcx";
  g.show_next = g.show_new_game ? "gamemap \"!*" + g.first_map + "\""
                : rest.empty()  ? std::string()
                                : "gamemap \"!" + rest + "\"";
  g.rules->cvars.set("nextserver", g.show_next, ge::console::Source::local, true);
  g.show_taken = false;
  g.cfg.wire.show = show;
  q2_net_level(*g.net, g.cfg.wire);
  g.net->log.push_back("------- Server Initialization -------");
  g.net->log.push_back("SpawnServer: " + show);
  g.net->log.push_back("-------------------------------------");
}

// The shows the rules' exit named taken before its map (true: one is on).
bool take_shows(Game& g) {
  sv::Q2Server* q = sv::q2_of(g.rules);
  if (q == nullptr || q->cinematic.empty() || !g.net) return false;
  std::vector<std::string> files;
  for (size_t from = 0; from <= q->cinematic.size();) {
    const size_t plus = std::min(q->cinematic.find('+', from), q->cinematic.size());
    if (plus > from) files.push_back(q->cinematic.substr(from, plus - from));
    from = plus + 1;
  }
  g.show_map = (*g.rules).pending_map;
  // sv_cinematics 0: a movie with something after it is skipped.
  for (size_t i = 0; i < files.size();)
    if (!g.cinematics && is_movie(files[i]) && (i + 1 < files.size() || !g.show_map.empty()))
      files.erase(files.begin() + static_cast<std::ptrdiff_t>(i));
    else
      i++;
  q->cinematic.clear();
  if (files.empty()) return false;
  g.shows = files;
  (*g.rules).pending_map.clear();
  start_show(g);
  return true;
}

// A client's `nextserver <count>` (above).
void next_server(Game& g, int32_t count) {
  if (g.cfg.wire.show.empty() || g.show_taken || count != g.net->count) return;
  const bool coop = coop_on(g);
  if (!is_movie(g.cfg.wire.show) && !coop) return;  // a picture outside co-op stays up
  g.show_taken = true;
  const ge::console::Cvar* v = (*g.rules).cvars.find("nextserver");
  const std::string text = v != nullptr ? v->value : std::string();
  g.rules->cvars.set("nextserver", "", ge::console::Source::local, true);
  std::string out;
  if (text.empty()) {
    g.cfg.wire.show.clear();
    q2_admin(*g.net, "killserver", out);
  } else if (text != g.show_next) {  // an operator's own: run as he wrote it
    g.cfg.wire.show.clear();
    q2_admin(*g.net, text, out);
  } else if (!g.shows.empty()) {
    start_show(g);
  } else {
    g.cfg.wire.show.clear();
    (*g.rules).pending_map = g.show_new_game ? g.first_map : g.show_map;
    if (g.show_new_game) (*g.rules).pending_change = sv::MapChange::new_game;
  }
}

// The autosave the rules wrote as a level of a unit started (not in deathmatch), kept as `save0`.
void keep_autosave(Game& g) {
  const sv::RunnerCalls* c = g.rules.calls;
  if (c == nullptr || c->take_autosave == nullptr) return;
  const std::vector<uint8_t> bytes = c->take_autosave(g.rules.rules);
  std::string why;
  if (!bytes.empty() && !cl::write_save(g.files, "save0", bytes, why) && g.net)
    g.net->log.push_back("The autosave was not written: " + why);
}

// THE SAVE WORDS on a dedicated server (`save <dir>`, `autosave` into `save1`, `load <dir>`): the
// game kept as `saves/<dir>.save` in the game's folder, as the window keeps it; a load starts the
// save's map with the game read back. True when `w[0]` was one, its answer in `out`.
bool save_words(Game& g, const std::vector<std::string>& w, std::string& out) {
  const bool autosave = w[0] == "autosave";
  if (w[0] != "save" && w[0] != "load" && !autosave) return false;
  if (!autosave && w.size() != 2) {
    out = "Usage: " + w[0] + " <directory>\n";
    return true;
  }
  const std::string dir = autosave ? "save1" : w[1];
  bool name_ok = !dir.empty() && dir.size() <= 64;
  for (const char ch : dir)
    name_ok = name_ok && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-');
  if (!name_ok) {
    out = "Bad savedir.\n";
    return true;
  }
  const sv::RunnerCalls* c = g.rules.calls;
  if (w[0] == "load") {
    std::vector<uint8_t> bytes;
    std::string why;
    if (!cl::read_save(g.files, dir, bytes, why)) {
      out = "No such savegame: " + dir + "\n";
      return true;
    }
    std::string map;
    if (sv::NativeQ2* nq = sv::native_q2_of(g.rules); nq != nullptr)
      map = sv::native_q2_load_ask(*nq, bytes, dir, why);
    else if (sv::Q2Server* sq = sv::q2_of(g.rules); sq != nullptr)
      map = sv::q2_load_ask(*sq, bytes, dir, why);
    else
      why = "this game cannot read saves";
    if (map.empty()) {
      out = "Couldn't load " + dir + ": " + why + "\n";
      return true;
    }
    g.rules.console("deathmatch 0");
    (*g.rules).pending_map = map;
    return true;
  }
  if (c == nullptr || c->save == nullptr || c->save_problem == nullptr) {
    out = "Game does not support enhanced savegames.\n";
    return true;
  }
  const std::string problem = c->save_problem(g.rules.rules);
  if (!problem.empty()) {
    out = problem + "\n";
    return true;
  }
  std::vector<uint8_t> bytes;
  std::string why;
  if (!c->save(g.rules.rules, bytes, why, static_cast<int64_t>(std::time(nullptr))) ||
      !cl::write_save(g.files, dir, bytes, why)) {
    out = "Couldn't save " + dir + ": " + why + "\n";
    return true;
  }
  out = "Game saved.\n";
  return true;
}

// The map the rules asked for (an intermission's end, `map`, `changelevel`, `restart`) loaded and
// the rules started on it, the clients told; a map that cannot be read starts this one again.
// An exit's shows come first (take_shows).
void change_level(Game& g) {
  sv::RulesCommon& s = *g.rules;
  if (take_shows(g)) return;
  if (s.pending_map.empty()) return;
  g.cfg.wire.show.clear();
  const std::string asked = s.pending_map;
  std::string error;
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(g.files, asked, error);
  if (next) mend(g, *next);
  if (!g.mode.empty()) {  // gamemode: the three modes set together for the map that starts
    g.rules.console(std::string("deathmatch ") + (g.mode == "dm" ? "1" : "0"));
    g.rules.console(std::string("coop ") + (g.mode == "coop" ? "1" : "0"));
    g.mode.clear();
  }
  if (next && next->spec.brushes == nullptr) {
    error = "maps/" + asked + ".bsp is not a map of Quake II's family";
    next.reset();
  }
  if (s.pending_change != sv::MapChange::restart)
    q2_votes_map_over(g.votes);  // a vote's restart keeps them
  q2_match_map_change(g.match, s.pending_change == sv::MapChange::restart, g.now_s);
  g.rules.carry();
  if (sv::NativeQ2* ns = sv::native_q2_of(g.rules); ns != nullptr && next)
    ns->map_checksum = sv::q2_map_checksum(next->file);
  if (!next || !g.rules.start(next->spec, error)) {
    if (g.net) g.net->log.push_back("the level stays on " + g.map->name + ": " + error);
    if (sv::NativeQ2* ns = sv::native_q2_of(g.rules); ns != nullptr)
      ns->map_checksum = sv::q2_map_checksum(g.map->file);
    if (!g.rules.start(g.map->spec, error)) {
      if (g.net) g.net->log.push_back("the level could not start again: " + error);
      s.pending_map.clear();
      return;
    }
  } else {
    g.map = std::move(next);
  }
  g.cfg.wire.map_checksum = sv::q2_map_checksum(g.map->file);
  sv::q2_sky(*g.map->spec.brushes, g.cfg.wire);
  if (g.net) q2_net_level(*g.net, g.cfg.wire);
  keep_autosave(g);
}

namespace cp = ge::classic::protocol;

void masters_send(Game& game, const std::vector<cp::MasterSend>& out) {
  if (!game.net || game.net->transport == nullptr) return;
  for (const cp::MasterSend& m : out)
    ge::net::transport_send(*game.net->transport, m.to, m.bytes.data(), m.bytes.size(),
                            static_cast<int64_t>(game.net->now * 1e6));
}

void destroy(void* g) {
  Game* game = static_cast<Game*>(g);
  std::vector<cp::MasterSend> out;
  std::vector<std::string> said;
  cp::masters_shutdown(game->masters, out, said);  // a public server's `shutdown`
  masters_send(*game, out);
  if (game->net) q2_net_stop(*game->net);
  delete game;
}

// `quit [reason]` and `recycle [reason]`: every player (or relay viewer) told "Server quit" or
// "Server restarted", with the reason when one was given, and the disconnect or the reconnect;
// the multiview streams' relays the same; the masters then hear `shutdown` from `destroy`.
void quit(void* g, const std::string& reason, bool restart) {
  Game& game = *static_cast<Game*>(g);
  std::string text = restart ? "Server restarted" : "Server quit";
  if (!reason.empty()) text += ": " + reason;
  text = text.substr(0, 1022) + "\n";
  if (game.relay.spawned) {
    q2_relay_farewell(game.relay, text, restart);
  } else {
    if (!game.relay.conns.empty()) q2_relay_stop(game.relay);
    if (game.net && !game.net->killed) q2_net_shutdown(*game.net, text, restart);
  }
  // Our rules end with the game's closing line (a hosted library says its own when unloaded).
  if (sv::Q2Server* q = sv::q2_of(game.rules); q != nullptr && !game.relay.spawned)
    q->console.emplace_back("==== ShutdownGame ====");
}

// `$com_maplist`'s maps: those the search path holds.
void maps(const void* g, std::vector<std::string>& out) {
  const Game& game = *static_cast<const Game*>(g);
  const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
  if (q2 != nullptr) out = cl::map_names(game.files, *q2);
}

// `showip`'s line for the multiview streams' TCP listener, while it is open.
void sockets(const void* g, std::vector<std::pair<std::string, ge::net::Address>>& out) {
  const Game& game = *static_cast<const Game*>(g);
  if (!game.net || !game.net->mvd.listening) return;
  const ge::net::Address& at = game.net->mvd.listener.local;
  out.emplace_back(at.family == ge::net::Family::ipv6 ? "Server TCP6" : "Server TCP", at);
}

// `net_restart` opened the game's socket again: the multiview streams' TCP listener too, at its
// address.
void reopened(void* g, ge::net::Transport& port) {
  Game& game = *static_cast<Game*>(g);
  if (!game.net) return;
  Q2Mvd& m = game.net->mvd;
  if (m.listening) {
    ge::net::tcp_close_listener(m.listener);
    m.listening = false;
  }
  if (port.kind == ge::net::TransportKind::udp) q2_mvd_listen(m, port.local);
}

double frame_seconds(const void* g) { return static_cast<const Game*>(g)->rules->sv.set.mintic; }

// A RECOVERABLE ERROR (ServerGame::drop_error): the rules stopped the map (a hosted library's
// error call: "Game Error: <its text>"), told once a map.
std::string drop_error(void* g) {
  Game& game = *static_cast<Game*>(g);
  const sv::RulesCommon& r = *game.rules;
  if (!r.map_stopped) {
    game.drop_told = false;
    return {};
  }
  if (game.drop_told || (game.net && game.net->killed)) return {};
  game.drop_told = true;
  if (const sv::NativeQ2* nq = sv::native_q2_of(game.rules); nq != nullptr) {
    const std::string e = sv::native_q2_error(*nq);
    if (!e.empty()) return "Game Error: " + e;
  }
  return r.stop_reason;
}

// Every client told `text` and dropped; the server answers nothing but rcon until the next map.
void crash(void* g, const std::string& text) {
  Game& game = *static_cast<Game*>(g);
  if (!game.net || game.net->killed) return;
  q2_net_shutdown(*game.net, text, false);
  game.net->killed = true;
}

void frame(void* g) {
  Game& game = *static_cast<Game*>(g);
  if (game.relay.spawned) return;  // no game while it relays
  game.rules.frame(nullptr, 0);
  change_level(game);
}

int seated(const void* g) {
  const Game& game = *static_cast<const Game*>(g);
  return game.net ? q2_net_clients(*game.net) : 0;
}

uint64_t hash(const void* g) {
  const sv::RulesRunner& rules = static_cast<const Game*>(g)->rules;
  if (const sv::NativeQ2* ns = sv::native_q2_of(rules)) return sv::native_q2_hash(*ns);
  return sv::rules_hash(*rules);
}

void fill_status(const void* g, ge::net::Status& st) {
  const Game& game = *static_cast<const Game*>(g);
  st.map = game.map->name;
  // The game (the hub draws Quake II's map and board: hub/cards.hpp), the mode by the seats, and
  // every client held with his frags and ping, as Quake II's own status answer lists them.
  st.keys.emplace_back("*game", "q2");
  if (!game.net) return;
  const Q2NetServer& s = *game.net;
  st.mode = qw_mode(false, false, q2_public_slots(s));
  st.max_players = q2_public_slots(s);
  for (int i = 0; i < q2_net_slots(s); i++) {
    const Q2NetClient& c = s.clients[i];
    if (!q2_held(c)) continue;
    ge::net::StatusPlayer p;
    p.name = c.name;
    p.score = static_cast<int32_t>(game.rules->world.clients[i].numbers.frags);
    p.ping_ms = static_cast<uint16_t>(std::clamp(c.ping_ms, 0, 9999));
    p.flags = c.spectator ? ge::net::kPlayerSpectator : 0;
    st.players.push_back(std::move(p));
  }
}

bool serve(void* g, ge::net::Transport& port, std::string& error) {
  Game& game = *static_cast<Game*>(g);
  game.masters.family = cp::MasterFamily::q2;  // setmaster may come before the first frame
  game.net = std::make_unique<Q2NetServer>();
  // The web files opened first, so the connect answer names their address. Their gate is the
  // channel's, and a map out of an archive stays off the web as it stays off the channel for a
  // client the server cannot tell its archive to.
  if (game.cfg.web.port > 0) {
    const Q2NetConfig* cfg = &game.net->cfg;
    ge::classic::content::WebServe ws;
    ws.game_folder = game.folder;
    ws.base_folder = "baseq2";
    ws.kinds = ge::classic::content::WebConfig{}.kinds;
    ws.allowed = [cfg](const std::string& path) {
      ge::vfs::ArchiveOrigin origin;
      return q2_download_allowed_path(*cfg, path) &&
             !(path.rfind("maps/", 0) == 0 && cfg->file_origin && cfg->file_origin(path, &origin));
    };
    if (!gs::web_files_open(game.web, game.cfg.web, ge::net::ipv4(0, 0, 0, 0, 0),
                            ge::classic::content::web_serve_lookup(game.files, std::move(ws)),
                            error))
      return false;
    game.cfg.web_url = game.web.url;
    game.cfg.web_pin = game.web.pin;
  }
  if (!q2_net_start(*game.net, game.rules, port, game.cfg, error)) return false;
  for (std::string& line : game.said) game.net->log.push_back(std::move(line));
  game.said.clear();
  gs::ban_file_read(game.net->bans, game.cfg.ban_file, game.cfg.unix_time(),
                    &game.net->cfg.ban_mode);
  Q2MvdConfig& mc = game.net->mvd.cfg;  // the endpoint's own limits, as the relays' too
  mc.iplimit = game.cfg.ip_limit;
  mc.timeout = game.cfg.timeout;
  mc.zombie = game.cfg.zombie_time;
  mc.ghost = game.cfg.ghost_time > 0 ? game.cfg.ghost_time : 6;
  game.net->mvd.log = [&game](const std::string& line) { game.net->log.push_back(line); };
  game.net->nextserver = [&game](int32_t count) { next_server(game, count); };
  if (port.kind == ge::net::TransportKind::udp) q2_mvd_listen(game.net->mvd, port.local);
  game.port = &port;
  game.relay.log = [&game](const std::string& line) { game.net->log.push_back(line); };
  game.relay.spot = [&game](const std::string& name, float origin[3], float angles[3],
                            int32_t& checksum, std::string& why) {
    std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(game.files, name, why);
    if (!m) return false;
    checksum = sv::q2_map_checksum(m->file);
    const ge::brush::Entity* best = nullptr;
    for (const char* kind :
         {"info_player_intermission", "info_player_deathmatch", "info_player_start"}) {
      for (const ge::brush::Entity& e : m->bsp.map.entities)
        if (e.get("classname") == kind) {
          best = &e;
          break;
        }
      if (best != nullptr) break;
    }
    if (best != nullptr) {
      std::sscanf(best->get("origin").c_str(), "%f %f %f", &origin[0], &origin[1], &origin[2]);
      if (!best->get("angles").empty())
        std::sscanf(best->get("angles").c_str(), "%f %f %f", &angles[0], &angles[1], &angles[2]);
      else
        angles[1] = static_cast<float>(std::atof(best->get("angle").c_str()));
    }
    return true;
  };
  if (game.logins_wanted) {
    game.login = std::make_unique<Q2Login>();
    game.login->game = game.base;
    if (!q2_login_open(*game.login, *game.net, game.login_set, game.accounts, game.account_link,
                       game.unix_us, error))
      return false;
    game.login->logins.signer = game.signer;
  }
  q2_votes_open(game.votes, *game.net);  // after the logins: their hook runs first
  q2_match_open(game.match, *game.net, game.rules, game.match_cfg);  // its words before the votes'
  return true;
}

void net_frame(void* g, int64_t now_us) {
  Game& game = *static_cast<Game*>(g);
  game.now_s = static_cast<double>(now_us) / 1e6;
  if (game.relay.spawned) {  // relay mode: the port is the relay's
    if (!game.relayed) {
      q2_net_shutdown(*game.net, "Server restarted\n", true);
      game.relayed = true;
    }
    q2_relay_frame(game.relay, now_us);
    return;
  }
  q2_net_frame(*game.net, now_us);
  gs::web_files_frame(game.web, game.now_s);
  if (!game.relay.conns.empty()) q2_relay_frame(game.relay, now_us);
  const double now_s = static_cast<double>(now_us) / 1e6;
  for (const ge::net::Address& from : game.net->acks) cp::masters_heard(game.masters, from, now_s);
  game.net->acks.clear();
  if (!cp::masters_started(game.masters)) {  // the first round 5 s on
    cp::masters_start(game.masters, cp::MasterFamily::q2, now_s);
  }
  if (!game.net->killed) {  // `killserver`: no heartbeats until the next map
    std::vector<cp::MasterSend> out;
    cp::masters_frame(game.masters, now_s, 0, q2_status_text(*game.net), true, out, game.net->log);
    masters_send(game, out);
  }
  q2_votes_frame(game.votes);
  q2_match_frame(game.match, game.map ? game.map->name : std::string(), now_s);
  change_level(game);
  if (game.login) q2_login_frame(*game.login);
}

// The broadcast and a player's view (q2_game_tv.hpp): every player's state whole, a frame
// and a keyframe alike.
void tv_state(const void* g, std::vector<uint8_t>& out) {
  q2_tv_state(*static_cast<const Game*>(g)->rules, out);
}

// A name `dumpents` may write under entdumps/: `\\` as `/`, `.` and `..` parts and stray slashes
// gone, anything not printable ASCII as `_`.
std::string dump_name(const std::string& typed) {
  std::string path;
  for (char ch : typed) path += ch == '\\' ? '/' : ch;
  std::string out, part;
  for (size_t i = 0; i <= path.size(); i++) {
    if (i == path.size() || path[i] == '/') {
      if (!part.empty() && part != "." && part != "..") out += (out.empty() ? "" : "/") + part;
      part.clear();
      continue;
    }
    const auto c = static_cast<unsigned char>(path[i]);
    part += c < 32 || c > 126 ? '_' : path[i];
  }
  return out;
}

bool iequal_tail(const std::string& s, const char* tail) {
  const size_t n = std::strlen(tail);
  if (s.size() < n) return false;
  for (size_t i = 0; i < n; i++) {
    char a = s[s.size() - n + i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (a != tail[i]) return false;
  }
  return true;
}

// Lines the server's console prints, from a multi-line answer.
void print_lines(Game& game, const std::string& text) {
  std::vector<std::string>& to = game.net ? game.net->log : game.said;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find('\n', at);
    to.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
    if (end == std::string::npos) break;
    at = end + 1;
  }
}

// A console line the server core does not know: the endpoint's admin words (status, kick, the
// lists), `listmaps`, `gamemode`, then the rules' own (map, gamemap, say, settings).
bool console_line(void* g, const std::string& line) {
  Game& game = *static_cast<Game*>(g);
  std::string answer;
  {  // the relay's words (its own mvdrecord and mvdstop once relay mode runs)
    const std::vector<std::string> rw = q2_words(line);
    if (game.net && !rw.empty() && rw[0].rfind("mvd", 0) == 0 &&
        (game.relay.spawned ||
         (rw[0] != "mvdrecord" && rw[0] != "mvdstop" && rw[0] != "mvdstuff")) &&
        q2_relay_command(game.relay, rw, line, game.port, game.now_s, answer)) {
      print_lines(game, answer);
      return true;
    }
  }
  const bool was_up = game.net && !game.net->killed;
  if (game.net && q2_admin(*game.net, line, answer)) {
    if (was_up && game.net->killed) {  // killserver: a public server's masters hear `shutdown`
      std::vector<cp::MasterSend> out;
      std::vector<std::string> said;
      cp::masters_shutdown(game.masters, out, said);
      masters_send(game, out);
    }
    print_lines(game, answer);
    return true;
  }
  const std::vector<std::string> w = q2_words(line);
  if (w.empty()) return true;
  // The file words (classic/content/fs_words.hpp).
  if (w[0] == "path") {
    print_lines(game, ge::classic::content::search_path_text(game.files, true));
    return true;
  }
  if (w[0] == "fs_restart") {
    const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
    std::string why;
    if (q2 == nullptr || !cl::open_game_dir(game.root, game.folder, game.files, why, *q2)) {
      print_lines(game, "The game's files could not be opened again: " + why + "\n");
      return true;
    }
    print_lines(game, ge::classic::content::fs_restart_text(game.files));
    return true;
  }
  if (std::string out; save_words(game, w, out)) {
    print_lines(game, out);
    return true;
  }
  if (std::string out; ge::classic::content::fs_command(game.files, w, out)) {
    print_lines(game, out);
    return true;
  }
  if (w[0] == "setmaster" || w[0] == "heartbeat") {
    if (!game.net) {  // only a server on the network has masters
      print_lines(game, "Only dedicated servers use masters.\n");
      return true;
    }
    if (w[0] == "heartbeat") {
      cp::force_heartbeat(game.masters);
      return true;
    }
    std::vector<cp::MasterSend> out;
    cp::set_masters(game.masters, {w.begin() + 1, w.end()}, cp::resolve_master, game.net->log, out);
    masters_send(game, out);
    return true;
  }
  if (w[0] == "listmasters") {
    print_lines(game, cp::masters_list_text(game.masters, game.net ? game.net->now : 0,
                                            game.net && !game.net->killed));
    return true;
  }
  if (w[0] == "sv_running" || w[0] == "sv_paused") {  // the server's state, read-only
    const bool running = game.net == nullptr || !game.net->killed;
    print_lines(game, w.size() > 1 ? w[0] + " is read-only: only the game sets it\n"
                                   : "\"" + w[0] + "\" is \"" +
                                         (w[0] == "sv_paused" ? "0"
                                          : running           ? "2"
                                                              : "0") +
                                         "\"\n");
    return true;
  }
  if (w[0] == "dumpents") {  // the map's entity text, as loaded, to entdumps/<name>.ent
    if (!game.map || game.map->bsp.map.entity_text.empty()) {
      print_lines(game, "No map loaded.\n");
      return true;
    }
    if (w.size() != 2) {
      print_lines(game, "Usage: dumpents <filename>\n");
      return true;
    }
    std::string name = dump_name(w[1]);
    if (name.empty()) {
      print_lines(game, "Couldn't open " + w[1] + ": Invalid quake path\n");
      return true;
    }
    if (name.size() < 4 || !iequal_tail(name, ".ent")) name += ".ent";
    const std::filesystem::path path = std::filesystem::path(game.files.folder) / "entdumps" / name;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    const std::string& text = game.map->bsp.map.entity_text;
    if (f == nullptr) {
      print_lines(game, "Couldn't open entdumps/" + name + ": " + std::strerror(errno) + "\n");
      return true;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    print_lines(game, ok ? "Dumped entity string to entdumps/" + name + "\n"
                         : "Couldn't write entdumps/" + name + "\n");
    return true;
  }
  if (w[0] == "bsplist") {  // the map held: its size, and with any word its counts
    if (!game.map || !game.map->from_file || (game.net && game.net->killed)) {
      print_lines(game, "BSP cache is empty\n");
      return true;
    }
    const ge::brush::Map& m = game.map->bsp.map;
    const std::string name = "maps/" + game.map->name + ".bsp";
    const size_t bytes = game.map->file.size();
    char row[160];
    std::string out = "------------------\n";
    std::snprintf(row, sizeof row, "%8zu : %s (1 refs)\n", bytes, name.c_str());
    out += row;
    if (w.size() > 1) {
      const std::pair<size_t, const char*> counts[] = {{m.brush_sides.size(), "brushsides"},
                                                       {m.planes.size(), "planes"},
                                                       {m.nodes.size(), "nodes"},
                                                       {m.leaves.size(), "leafs"},
                                                       {m.leaf_brushes.size(), "leafbrushes"},
                                                       {m.models.size(), "models"},
                                                       {m.brushes.size(), "brushes"},
                                                       {m.entity_text.size(), "entitychars"},
                                                       {m.areas.size(), "areas"},
                                                       {m.area_portals.size(), "areaportals"}};
      for (const auto& [n, label] : counts) {
        std::snprintf(row, sizeof row, "%8zu : %s\n", n, label);
        out += row;
      }
      if (m.clusters > 0) {
        std::snprintf(row, sizeof row, "%8d : clusters\n", m.clusters);
        out += row;
      }
      const uint32_t sum = sv::q2_map_checksum(game.map->file);
      std::snprintf(row, sizeof row, "Checksum : %s%x\n", sum != 0 ? "0x" : "", sum);
      out += row;
      out += "------------------\n";
    }
    std::snprintf(row, sizeof row, "Total resident: %zu\n", bytes);
    print_lines(game, out + row);
    return true;
  }
  if (w[0] == "listmaps") {
    const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
    std::string list;
    for (const std::string& m : cl::map_names(game.files, *q2)) list += m + "\n";
    print_lines(game, list.empty() ? "No maps found.\n" : list);
    return true;
  }
  if (w[0] == "gamemode") {  // the mode the next map is played in
    static const char* const kModes[][2] = {
        {"dm", "deathmatch"}, {"coop", "coop"}, {"sp", "single player"}};
    if (w.size() < 2 || w[1] == "?") {
      const std::string now = game.rules->cvars.find("deathmatch")->on() ? "dm"
                              : game.rules->cvars.find("coop")->on()     ? "coop"
                                                                         : "sp";
      print_lines(game, "gamemode: " + now + (game.mode.empty() ? "" : ", next map " + game.mode) +
                            "\nUsage: gamemode <dm|coop|sp|?>\n");
      return true;
    }
    for (const auto& [word, name] : kModes)
      if (w[1] == word) {
        game.mode = word;
        print_lines(game, std::string("gamemode: the next map is played in ") + name + "\n");
        return true;
      }
    print_lines(game, "Usage: gamemode <dm|coop|sp|?>\n");
    return true;
  }
  return game.rules.console(line);
}

// What the server printed since the last call: the endpoint's lines (joins, drops, refusals, bans,
// the maps mended), then the rules' (a new map's from its first). The dedicated server prints them
// and adds them to `sv_logfile`.
void console_lines(void* g, std::vector<std::string>& out) {
  Game& game = *static_cast<Game*>(g);
  std::vector<std::string>& endpoint = game.net ? game.net->log : game.said;
  for (std::string& line : endpoint) out.push_back(std::move(line));
  endpoint.clear();
  if (!game.rules) return;
  const sv::RulesCommon& now = *game.rules;
  if (&now != game.told_rules || game.told > now.console.size()) {
    game.told_rules = &now;
    game.told = 0;
  }
  for (; game.told < now.console.size(); game.told++) out.push_back(now.console[game.told]);
}

void player_view(const void* g, int player, std::vector<uint8_t>& out) {
  out.clear();
  q2_player_view(*static_cast<const Game*>(g)->rules, player, out);
}

const gs::ServerGame& make() {
  static gs::ServerGame g = [] {
    gs::ServerGame s;
    s.binary = "sesv";
    s.title = "Shooter's Edge on Quake II's protocol";
    s.version_text = kProgramVersion;
    // The game's own id on the hub's masters, as a Quake server's: a hub lists both (sv_master).
    s.protocol = ge::net::protocol_id("shooters_edge", 1);
    s.default_port = kPortQ2;
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
    s.reopened = reopened;
    s.tv_frame = tv_state;
    s.tv_keyframe = tv_state;
    s.player_view = player_view;
    s.console_lines = console_lines;
    s.console = console_line;
    s.words = kQ2ServerWords;
    s.word_count = static_cast<int>(std::size(kQ2ServerWords));
    s.drop_error = drop_error;
    s.crash = crash;
    s.quit = quit;
    s.sockets = sockets;
    s.maps = maps;
    s.sheet = [](const void* game, int, gs::SettingsSheet& out) {
      q2_votes_sheet(static_cast<const Game*>(game)->votes, out);
    };
    q2_watch_hooks(s);
    return s;
  }();
  return g;
}

}  // namespace

const gs::ServerGame& q2_game() { return make(); }

}  // namespace shooters_edge::net
