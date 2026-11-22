// The QuakeWorld endpoint's own settings (qw_endpoint_cfg.hpp states them).
//
// ge:summary The QuakeWorld endpoint's settings table and the reading of each into its config
// ge:keywords endpoint_rules endpoint_settings endpoint_int endpoint_flag info_flood vip_words
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#include "shooters_edge/net/qw_endpoint_cfg.hpp"

#include <cstdlib>
#include <iterator>
#include <vector>

#include "server/admission.hpp"
#include "server/idle.hpp"
#include "server/limit.hpp"
#include "server/lrcon.hpp"
#include "server/names.hpp"
#include "server/rate.hpp"
#include "server/skew.hpp"
#include "server/status.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;

namespace {

constexpr gs::GameRule kEndpointRules[] = {
    {"sv_antilag", "Whether a shot sees the others where the shooter saw them.", nullptr,
     "0: off (the default); 1: the shots the game marks; 2: every shot's line"},
    {"sv_antilag_projectiles",
     "Whether, with sv_antilag 2, a player's rockets, grenades and nails meet the others where he "
     "saw them.",
     nullptr, "0: off (the default); 1: on"},
    {"sv_antilag_max", "How far back a shot may see the others, in milliseconds.", nullptr,
     "0: as far as the history goes (the default); 1..1000: that many"},
    {"sv_antilagexact",
     "Whether the rewind goes to exactly the frame the shooter saw, not a little further.", nullptr,
     "0: a little further (the default); 1: exactly"},
    {"sv_safestrafe", "Commands of no sideways move put between a player's reversed strafes.",
     nullptr, "0: off (the default); 1..N: that many commands"},
    {"sv_voip", "Whether players' voices are relayed to those they talk to.", nullptr,
     "0: off (the default); 1: on"},
    {"sv_voipecho", "Whether a speaker hears his own voice back, to test it.", nullptr,
     "0: off (the default); 1: on"},
    {"sv_earshot", "How near a player must stand to read nearby chat and hear a nearby voice.",
     nullptr, "0: no nearby chat or voice; 1..65536: that many map units (1000 the default)"},
    {"sv_rconcrypt", "Whether the remote console takes only a hashed password.", nullptr,
     "0: the password itself or a hashed one (the default); 1: only a hashed one"},
    {"sv_rcontime", "The seconds a hashed remote console line's time may be off by.", nullptr,
     "0: any time; 1..3600: that many seconds (60 the default)"},
    {"sv_vipvalues", "The VIP level each password of sv_vippassword gives, spaces between.",
     nullptr, "\"\": 1, 2 and so on, in order (the default); TEXT: these levels"},
    {"sv_maxvipspectators", "The spectator slots only a VIP may take.", nullptr,
     "0: none (the default); 1..32: that many"},
    {"sv_banmode", "How the ban list made by addip is read.", nullptr,
     "0: only the addresses on it may join; 1: the addresses on it are refused (the default)"},
    {"maxfps", "The most moves a second a client sends, told in the server info.", nullptr,
     "N: that many (77 the default)"},
    {"sv_download", "Whether a client that lacks a file is offered it.", nullptr,
     "0: no file is offered; 1: files are offered (the default)"},
    {"sv_download_skins", "Whether files in skins/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)"},
    {"sv_download_models", "Whether models in progs/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)"},
    {"sv_download_sounds", "Whether files in sound/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)"},
    {"sv_download_maps", "Whether files in maps/ are offered.", nullptr,
     "0: not offered; 1: offered (the default)"},
    {"sv_download_other", "Whether files outside those folders are offered too.", nullptr,
     "0: not offered (the default); 1: offered"},
    {"sv_downloadchunks", "The 1 KB chunks a downloading client gets a frame.", nullptr,
     "1..30: that many (30 the default)"},
    {"sv_downloadserver", "A web address clients may fetch the server's files from instead.",
     nullptr, "\"\": none (the default); TEXT: a URL ending in /, or github:<owner>/<repo>"},
    {"sv_watervis", "Whether clients are told they may see through water.", nullptr,
     "0: not told (the default); 1: told"},
    {"sv_fullspectate",
     "What happens to a player joining a full server while a spectator slot is free.", nullptr,
     "0: refused; 1: seated as a spectator unless its user info's svf has bit 2; 2: seated as a "
     "spectator only if its svf has bit 1 (the default)"},
    {"sv_reconnectwait",
     "The seconds a slot must have been connected before its address may connect again.", nullptr,
     "0: no wait (the default); 1..N: that many seconds"},
    {"sv_maxping", "The ping above which a player entering plays as a spectator, in ms.", nullptr,
     "0: no limit (the default); 1..10000: that many ms"},
    {"sv_minping", "The ping each player's packets are held to at least, in ms.", nullptr,
     "0: off (the default); 1..N: that many ms"},
    {"sv_minpingcmd", "Whether players may set the minimum ping with minping before a match.",
     nullptr, "0: not allowed; 1: allowed (the default)"},
    {"sv_defaultname", "The name a client with an empty or unusable name gets.", nullptr,
     "TEXT: that name (player the default)"},
    {"sv_unfake",
     "Whether line breaks in a public chat line are shown as #, so none fakes another.", nullptr,
     "0: kept; 1: shown as # (the default)"},
    {"sv_colorkick",
     "Whether a client changing its top colour more than 6 times in 8 seconds is kicked.", nullptr,
     "0: never kicked; 1: kicked (the default)"},
    {"sv_infoflood", "The user info changes a client may make before it is kicked.", "300 3",
     "N N: more changes than the first in the second's seconds is kicked, 0 in either no limit "
     "(300 3 the default)"},
    // The stream port for QuakeWorld relays (qw_qtv.hpp).
    {"qtv_streamport", "The TCP port QuakeWorld relays take the match from.", nullptr,
     "0: none (the default); 1..65534: that port"},
    {"qtv_maxstreams", "The relays streamed at once.", nullptr,
     "N: that many, 0 or less no limit (1 the default)"},
    {"qtv_password", "The password a relay must give.", nullptr,
     "\"\": none (the default); TEXT: that password", false},
    {"qtv_pendingtimeout", "The seconds a relay has to finish its request.", nullptr,
     "1..60: that many seconds (5 the default)"},
    {"qtv_streamtimeout", "The seconds a relay may send nothing before it is dropped.", nullptr,
     "5..600: that many seconds (45 the default)"},
    {"qtv_sayenabled", "Whether a relay's viewers' chat reaches the players.", nullptr,
     "0: only the spectators (the default); 1: the players too"},
    // The coop campaign in place of a match (qw_coop_game.hpp): the endpoint's, never a mod's.
    {"sv_coop", "Whether the server plays the game folder's campaign in coop, not a match.",
     nullptr, "0: a match (the default); 1: the campaign in coop"},
    {"sv_coopbots", "The companion bots playing beside the players in coop.", nullptr,
     "0: none (the default); 1..7: that many"},
    {"skill", "The monsters' skill for everyone in coop.", nullptr,
     "0: easy; 1: normal (the default); 2: hard; 3: nightmare"},
    {"sv_friendlyfire", "Whether one player's shots harm another in coop.", nullptr,
     "0: spared (the default); 1: harmed"},
    {"sv_friendlypush", "Whether a spared teammate's blow pushes you in coop, harmlessly.", nullptr,
     "0: no push (the default); 1: pushed"},
    {"sv_episodes_in_order", "Whether the start map opens the episodes one at a time.", nullptr,
     "0: all open, as the original (the default); 1: one at a time"},
    {"sv_episodes_done", "The episodes done in coop, a bit each, so the start map opens the next.",
     nullptr, "N: the bits (0 the default)"},
};

}  // namespace

// The rate rules and the per-address limit (the engine's, server/rate.hpp and admission.hpp), and
// the endpoint's own.
std::span<const gs::GameRule> endpoint_rules() {
  static const std::vector<gs::GameRule> rules = [] {
    // One by one: GCC's release build reads a range insert of the one-row table as out of bounds.
    std::vector<gs::GameRule> out;
    for (std::span<const gs::GameRule> t : {std::span<const gs::GameRule>(gs::kRateRules),
                                            std::span<const gs::GameRule>(gs::kAdmitRules),
                                            std::span<const gs::GameRule>(gs::kIdleRules),
                                            std::span<const gs::GameRule>(gs::kSkewRules),
                                            std::span<const gs::GameRule>(gs::kNameRules),
                                            std::span<const gs::GameRule>(gs::kLimitRules),
                                            std::span<const gs::GameRule>(gs::kStatusRules),
                                            std::span<const gs::GameRule>(gs::kLrconRules),
                                            std::span<const gs::GameRule>(kEndpointRules)})
      for (const gs::GameRule& r : t) out.push_back(r);
    return out;
  }();
  return rules;
}

bool is_endpoint_rule(const std::string& name) {
  for (const gs::GameRule& r : endpoint_rules())
    if (name == r.name) return true;
  return false;
}

namespace {

// An endpoint rule as a whole number in `low..high`; unset leaves `out`. False with `error`.
bool endpoint_int(const gs::ServerConfig& c, const char* name, int low, int high, int& out,
                  std::string& error) {
  const auto it = c.rules.find(name);
  if (it == c.rules.end() || it->second.empty()) return true;
  char* end = nullptr;
  const long v = std::strtol(it->second.c_str(), &end, 10);
  if (end == it->second.c_str() || *end != '\0' || v < low || v > high) {
    error = std::string(name) + " \"" + it->second + "\" is not a whole number in " +
            std::to_string(low) + ".." + std::to_string(high) + " (--explain=" + name +
            " shows where it was set)";
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

}  // namespace

// The stream port's settings from the rules: false with `error` on a value out of range.
bool qtv_settings(const gs::ServerConfig& c, int& port, QtvConfig& q, std::string& error) {
  int pending = static_cast<int>(q.pending_s), timeout = static_cast<int>(q.stream_timeout_s);
  int say = q.say_enabled ? 1 : 0;
  if (!endpoint_int(c, "qtv_streamport", 0, 65534, port, error) ||
      !endpoint_int(c, "qtv_maxstreams", -1, 1024, q.max_streams, error) ||
      !endpoint_int(c, "qtv_pendingtimeout", 1, 60, pending, error) ||
      !endpoint_int(c, "qtv_streamtimeout", 5, 600, timeout, error) ||
      !endpoint_int(c, "qtv_sayenabled", 0, 1, say, error))
    return false;
  if (const auto it = c.rules.find("qtv_password"); it != c.rules.end()) q.password = it->second;
  q.pending_s = pending;
  q.stream_timeout_s = timeout;
  q.say_enabled = say != 0;
  return true;
}

namespace {

// A 0 or 1 endpoint rule into `on` (unset: left as it is).
bool endpoint_flag(const gs::ServerConfig& c, const char* name, bool& on, std::string& error) {
  int v = on ? 1 : 0;
  if (!endpoint_int(c, name, 0, 1, v, error)) return false;
  on = v != 0;
  return true;
}

// sv_infoflood's "N S": the most user info changes in a window of S seconds.
bool info_flood(const gs::ServerConfig& c, ServerConfig& cfg, std::string& error) {
  const auto it = c.rules.find("sv_infoflood");
  if (it == c.rules.end() || it->second.empty()) return true;
  int count = 0;
  double seconds = 0;
  char* end = nullptr;
  const char* p = it->second.c_str();
  count = static_cast<int>(std::strtol(p, &end, 10));
  const bool first = end != p;
  p = end;
  seconds = std::strtod(p, &end);
  while (*end == ' ') end++;
  if (!first || end == p || *end != '\0' || count < 0 || seconds < 0) {
    error = "sv_infoflood \"" + it->second +
            "\" is not two numbers \"N S\" of 0 or more (--explain=sv_infoflood shows where it was "
            "set)";
    return false;
  }
  cfg.info_flood_count = count;
  cfg.info_flood_s = seconds;
  return true;
}

// sv_vippassword's words and sv_vipvalues' numbers.
bool vip_words(const gs::ServerConfig& c, ServerConfig& cfg, std::string& error) {
  auto words = [](const std::string& text) {
    std::vector<std::string> out;
    for (size_t at = 0; at < text.size();) {
      const size_t end = std::min(text.find(' ', at), text.size());
      if (end > at) out.push_back(text.substr(at, end - at));
      at = end + 1;
    }
    return out;
  };
  if (const auto p = c.rules.find("sv_vippassword"); p != c.rules.end())
    cfg.vip_passwords = words(p->second);
  if (const auto p = c.rules.find("sv_redirect"); p != c.rules.end()) cfg.redirect = p->second;
  if (const auto v = c.rules.find("sv_vipvalues"); v != c.rules.end()) {
    cfg.vip_values.clear();
    for (const std::string& n : words(v->second)) {
      char* end = nullptr;
      const long level = std::strtol(n.c_str(), &end, 10);
      if (end == n.c_str() || *end != '\0' || level < 0 || level > 1000000) {
        error = "sv_vipvalues \"" + v->second + "\": \"" + n +
                "\" is not a level; give whole numbers 0 or more, spaces between "
                "(--explain=sv_vipvalues shows where it was set)";
        return false;
      }
      cfg.vip_values.push_back(static_cast<int>(level));
    }
  }
  return true;
}

}  // namespace

bool endpoint_settings(const gs::ServerConfig& c, ServerConfig& cfg, std::string& error) {
  int watervis = 0, reconnect_wait = static_cast<int>(cfg.reconnect_wait_s);
  int antilag = cfg.antilag, voice = cfg.voice ? 1 : 0;
  int safe_strafe = cfg.safe_strafe;
  int locked = cfg.locked ? 1 : 0, lan_rate = cfg.lan_rate ? 1 : 0;
  int all = cfg.allow_download ? 1 : 0, skins = cfg.allow_skins ? 1 : 0;
  int models = cfg.allow_models ? 1 : 0, sounds = cfg.allow_sounds ? 1 : 0;
  int maps = cfg.allow_maps ? 1 : 0, other = cfg.allow_other ? 1 : 0;
  if (!endpoint_int(c, "sv_maxrate", 0, 1000000, cfg.maxrate, error) ||
      !endpoint_int(c, "sv_minrate", 0, 100000, cfg.minrate, error) ||
      !endpoint_int(c, "sv_lanrate", 0, 1, lan_rate, error) ||
      !endpoint_int(c, "sv_antilag", 0, 2, antilag, error) ||
      !endpoint_flag(c, "sv_antilag_projectiles", cfg.antilag_projectiles, error) ||
      !endpoint_int(c, "sv_antilag_max", 0, 1000, cfg.antilag_max_ms, error) ||
      !endpoint_int(c, "sv_voip", 0, 1, voice, error) ||
      !endpoint_flag(c, "sv_voipecho", cfg.voice_echo, error) ||
      !endpoint_int(c, "sv_earshot", 0, 65536, cfg.earshot, error) ||
      !endpoint_flag(c, "sv_antilagexact", cfg.antilag_exact, error) ||
      !endpoint_int(c, "sv_safestrafe", 0, 100, safe_strafe, error) ||
      !endpoint_flag(c, "sv_rconcrypt", cfg.rcon_crypt, error) ||
      !endpoint_int(c, "sv_rcontime", 0, 3600, cfg.rcon_time_s, error) ||
      !gs::limit_settings(c, cfg.rcon_rate, cfg.password_rate, error) ||
      !gs::status_settings(c, cfg.status, error) ||
      !endpoint_int(c, "sv_banmode", 0, 1, cfg.ban_mode, error) ||
      !endpoint_int(c, "sv_iplimit", 0, 32, cfg.ip_limit, error) ||
      !endpoint_int(c, "sv_locked", 0, 1, locked, error) ||
      !endpoint_int(c, "sv_vipslots", 0, 32, cfg.vip_slots, error) ||
      !endpoint_int(c, "sv_idletime", 0, 86400, cfg.idle_time, error) ||
      !gs::skew_settings(c, cfg.skew, error) || !gs::name_settings(c, cfg.names, error) ||
      !endpoint_int(c, "sv_maxvipspectators", 0, 32, cfg.max_vip_spectators, error) ||
      !vip_words(c, cfg, error) || !endpoint_int(c, "maxfps", 20, 1000, cfg.maxfps, error) ||
      !endpoint_int(c, "sv_download", 0, 1, all, error) ||
      !endpoint_int(c, "sv_download_skins", 0, 1, skins, error) ||
      !endpoint_int(c, "sv_download_models", 0, 1, models, error) ||
      !endpoint_int(c, "sv_download_sounds", 0, 1, sounds, error) ||
      !endpoint_int(c, "sv_download_maps", 0, 1, maps, error) ||
      !endpoint_int(c, "sv_download_other", 0, 1, other, error) ||
      !endpoint_int(c, "sv_downloadchunks", 1, 30, cfg.chunks_per_frame, error) ||
      !endpoint_flag(c, "sv_unfake", cfg.unfake, error) ||
      !endpoint_flag(c, "sv_colorkick", cfg.colour_kick, error) ||
      !endpoint_int(c, "sv_watervis", 0, 1, watervis, error) ||
      !endpoint_int(c, "sv_fullspectate", 0, 2, cfg.full_spectate, error) ||
      !endpoint_int(c, "sv_maxping", 0, 10000, cfg.max_ping_ms, error) ||
      !endpoint_int(c, "sv_minping", 0, 1000, cfg.min_ping_ms, error) ||
      !endpoint_flag(c, "sv_minpingcmd", cfg.min_ping_cmd, error) ||
      !endpoint_int(c, "sv_reconnectwait", 0, 3600, reconnect_wait, error) ||
      !info_flood(c, cfg, error))
    return false;
  if (const auto n = c.rules.find("sv_defaultname"); n != c.rules.end() && !n->second.empty()) {
    if (sanitize_name(n->second, "") != n->second) {
      error =
          "sv_defaultname \"" + n->second +
          "\" can't be a name: it holds \\ $ # \" ; or a line break, starts or ends with a space, "
          "or is console (--explain=sv_defaultname shows where it was set)";
      return false;
    }
    cfg.default_name = n->second;
  }
  cfg.reconnect_wait_s = reconnect_wait;
  if (const auto it = c.rules.find("sv_downloadserver"); it != c.rules.end())
    cfg.download_server = it->second;
  cfg.locked = locked != 0;
  if (const auto it = c.rules.find("lrcon_password"); it != c.rules.end())
    cfg.lrcon.password = it->second;
  cfg.lan_rate = lan_rate != 0;
  cfg.max_spectators = c.max_spectators;
  cfg.antilag = antilag;
  cfg.safe_strafe = safe_strafe;
  cfg.voice = voice != 0;
  cfg.allow_download = all != 0;
  cfg.allow_skins = skins != 0;
  cfg.allow_models = models != 0;
  cfg.allow_sounds = sounds != 0;
  cfg.allow_maps = maps != 0;
  cfg.allow_other = other != 0;
  return true;
}

}  // namespace shooters_edge::net
