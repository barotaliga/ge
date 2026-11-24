// The QuakeWorld server's settings by the games' own names, defined: qw_game_names.hpp.
//
// ge:summary The QuakeWorld server's setting names, sv_modern's meaning, *mods and the slots
// ge:keywords qw-setting-names sv_modern setting-synonyms qw_setting_names qw_slots
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#include "shooters_edge/net/qw_game_names.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>

#include "shooter/server.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

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

}  // namespace

std::span<const gs::SettingName> qw_setting_names() { return kSettingNames; }
std::span<const gs::GameMeta> qw_game_metas() { return kMetas; }

// `*mods`: each package's name and version, ", " between, cut with "+N more" past what one
// serverinfo value should take of the whole string's 512 bytes.
std::string qw_mods_info(const std::vector<std::unique_ptr<ge::mod::LoadedMod>>& packages) {
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
void qw_game_names(sv::RulesCommon& rules, const gs::ServerConfig& c) {
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

// The loop's client slots: the players' and, when spectators have their own, theirs.
int qw_slots(const gs::ServerConfig& c) {
  int vip = 0;
  if (const auto v = c.rules.find("sv_maxvipspectators"); v != c.rules.end())
    vip = std::max(0, std::atoi(v->second.c_str()));
  return std::min(sh::kMaxClients, c.maxclients + std::max(0, c.max_spectators) + vip);
}

}  // namespace shooters_edge::net
