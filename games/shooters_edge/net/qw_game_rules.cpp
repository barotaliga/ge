// The QuakeWorld server's settings for the core; qw_game_rules.hpp says what they are.
//
// ge:summary The QuakeWorld server's settings, their other names and sv_modern, for the core
// ge:keywords qw_game_rules qw_setting_names qw_game_metas
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#include "shooters_edge/net/qw_game_rules.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>

#include "server/overtime.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/net/qw_endpoint_cfg.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace sh = ge::shooter;
namespace srv = shooters_edge::server;

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
    {"zombietime",
     "Seconds a dropped player's packets are still answered with the goodbye, so his game learns "
     "it "
     "was dropped.",
     nullptr, "0: none; 2: two seconds (the default)"},
};

// Each QuakeWorld game's own names for the server's settings (gs::SettingName): a server.cfg,
// `+set`, rcon or vote line by either name sets the one setting. Both games share the names they
// have in common, so one table serves both: Quake's alone (floodprot, floodprotmsg), the
// Quake II's alone (sv_maplist, the three flood words), and both (hostname, maxclients, ...).
// `sv_modern` (engine/server's GameMeta): what a modern QuakeWorld server is. Every shot's line and
// every rocket, grenade and nail meet the others where the shooter saw them; on a LAN (2) the
// rate is not held to sv_maxrate either. The game's feel is never part of it: only how the
// network judges it. A setting any layer names wins over it.
constexpr gs::GameMeta kMetas[] = {
    {"sv_modern", "0", "",
     "Whether the server plays as a modern one: every shot judged where its shooter saw the "
     "others.",
     "0: as each setting says (the default); 1: sv_antilag 2 and sv_antilag_projectiles 1; 2: "
     "the same and sv_lanrate 1, for a LAN"},
    {"sv_modern", "1", "sv_antilag 2; sv_antilag_projectiles 1"},
    {"sv_modern", "2", "sv_modern 1; sv_lanrate 1"},
};

constexpr gs::SettingName kSettingNames[] = {
    {"hostname", "sv_hostname"},
    {"maxclients", "sv_maxclients"},
    {"maxplayers", "sv_maxclients"},  // Quake's word for the next map's player count
    {"maxspectators", "sv_maxspectators"},
    {"password", "sv_password"},
    {"spectator_password", "sv_spectatorpassword"},
    {"timeout", "sv_timeout"},
    {"net_messagetimeout", "sv_timeout"},  // Quake's: seconds of silence before a drop
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

}  // namespace

// Every rule, in the order `--explain` lists them: built once.
const std::vector<gs::GameRule>& qw_game_rules() {
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

std::span<const gs::SettingName> qw_setting_names() { return kSettingNames; }

std::span<const gs::GameMeta> qw_game_metas() { return kMetas; }

}  // namespace shooters_edge::net
