// A classic server's settings sheet read from its info and listings (server_sheet.hpp).
//
// ge:summary The rows of a stock QuakeWorld or Quake II server, and its listings read into them
// ge:keywords qw_sheet_from_info qw_match_listing_read q2_info_read q2_sheet_from_info
// ge:skill classic net
// ge:test tests/classic_server_sheet_test.cpp
#include "classic/server_sheet.hpp"

#include <array>
#include <cstdlib>

#include "classic/protocol/info.hpp"

namespace ge::classic {

namespace gs = ge::server;
using gs::SheetHow;
using gs::SheetRow;

namespace {

constexpr std::string_view kGroupRules = "Rules";
constexpr std::string_view kGroupVotes = "Votes";
constexpr std::string_view kGroupServer = "Server";
constexpr std::string_view kGroupMoves = "Movement";
constexpr std::string_view kGroupFlags = "Deathmatch flags";

SheetRow row(std::string_view name, std::string_view words, std::string_view group,
             std::string value, std::string_view values, SheetHow how = SheetHow::fixed,
             std::string_view line = {}, std::string_view help = {}) {
  SheetRow r;
  r.name = std::string(name);
  r.words = std::string(words);
  r.group = std::string(group);
  r.value = std::move(value);
  r.values = std::string(values);
  r.how = how;
  r.line = std::string(line);
  r.help = std::string(help);
  return r;
}

std::string or_zero(const std::string& v) { return v.empty() ? "0" : v; }

constexpr std::string_view kOnOff = "off: off; on: on";

std::string strip_high(std::string_view line) {
  std::string out;
  out.reserve(line.size());
  for (const char c : line) {
    const auto b = static_cast<unsigned char>(c & 0x7f);
    // The coloured digits sit at 18..27 (the digit's code less 30 after bit 7 is cleared).
    out += b >= 18 && b <= 27 ? static_cast<char>('0' + (b - 18)) : static_cast<char>(b);
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
    out.pop_back();
  return out;
}

std::string trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
    s.remove_suffix(1);
  return std::string(s);
}

void set_value(gs::SettingsSheet& sheet, std::string_view name, std::string value) {
  for (SheetRow& r : sheet.rows)
    if (r.name == name) {
      if (r.value != value) {
        r.value = std::move(value);
        sheet.revision++;
      }
      return;
    }
}

// The match mod's spawn models, in the order its `spawn` word cycles them.
constexpr std::array<std::string_view, 6> kSpawnModels = {
    "pre-qtest nonrandom respawns", "Normal QW respawns", "KT SpawnSafety",
    "Kombat Teams respawns",        "KTX respawns",       "KTX2 respawns"};

constexpr std::string_view kModeValues =
    "1on1: a duel; 2on2: two a side; 3on3: three a side; 4on4: four a side; 10on10: ten a side; "
    "ffa: free for all; ctf: capture the flag; hoonymode: a duel by single kills; "
    "blitz2v2: blitz, two a side; blitz4v4: blitz, four a side; 2on2on2: three teams of two; "
    "3on3on3: three teams of three; 4on4on4: three teams of four; XonX: any number a side; "
    "wipeout: wipeout; carena: clan arena; tot: tank of time";

}  // namespace

std::string qw_mode_base(std::string_view mode) {
  return std::string(mode.substr(0, std::min(mode.find('-'), mode.size())));
}

bool qw_mode_has(std::string_view mode, std::string_view suffix) {
  size_t at = mode.find('-');
  while (at != std::string_view::npos) {
    const size_t end = std::min(mode.find('-', at + 1), mode.size());
    if (mode.substr(at + 1, end - at - 1) == suffix) return true;
    at = end < mode.size() ? end : std::string_view::npos;
  }
  return false;
}

SheetSource qw_sheet_source(const std::string& serverinfo, bool ours) {
  if (ours) return SheetSource::ours;
  return protocol::info_get(serverinfo, "ktxver").empty() ? SheetSource::qw_plain
                                                          : SheetSource::qw_match;
}

void qw_sheet_from_info(gs::SettingsSheet& sheet, const std::string& info) {
  const auto get = [&info](const char* key) { return protocol::info_get(info, key); };
  const bool match = !get("ktxver").empty();
  sheet.rows.clear();
  sheet.whole = true;
  sheet.revision++;
  const auto put = [&sheet](SheetRow r) { gs::sheet_put(sheet, std::move(r)); };
  const SheetHow change = match ? SheetHow::direct : SheetHow::fixed;
  put(row("map", "Map", "", get("map"), "TEXT: the map's name",
          match ? SheetHow::vote : SheetHow::fixed, match ? "votemap %v" : "",
          "The map being played."));
  if (match) {
    const std::string mode = get("mode");
    put(row("mode", "Mode", "", qw_mode_base(mode), kModeValues, SheetHow::direct, "%v",
            "The kind of match the server is set up for."));
  }
  put(row("deathmatch", "Deathmatch mode", "", or_zero(get("deathmatch")),
          "0: no deathmatch; 1: weapons respawn, items too; 2: weapons stay; 3: weapons respawn, "
          "items stay; 4: start with every weapon; 5: as 4 with a long spawn shield",
          change, match ? "dmm%v" : "", "The deathmatch rules."));
  put(row("teamplay", "Teamplay", "", or_zero(get("teamplay")),
          match ? "1: team frags count, friendly fire hurts; 2: no friendly fire; 3: no friendly "
                  "fire, armor still hit; 4: friendly fire hurts armor and health"
                : "0: every player for himself; 1..N: that team mode",
          change, match ? "tp|" : "", "The team rules."));
  put(row("timelimit", "Time limit", "", or_zero(get("timelimit")),
          "0: no limit; 1..30: that many minutes", change, match ? "timeup1|timedown1" : "",
          "The match's length in minutes."));
  put(row("fraglimit", "Frag limit", "", or_zero(get("fraglimit")),
          match ? "0: no limit; 10: 10 frags; 20: 20 frags; 30: 30 frags; 40: 40 frags; 50: 50 "
                  "frags; 60: 60 frags; 70: 70 frags; 80: 80 frags; 90: 90 frags; 100: 100 frags"
                : "0: no limit; 1..N: that many frags",
          change, match ? "fragsup|fragsdown" : "", "The frags that end the match."));
  if (match) {
    const std::string mode = get("mode");
    const auto suffix = [&mode](std::string_view s) { return qw_mode_has(mode, s) ? "on" : "off"; };
    put(row("overtime", "Overtime", "", "",
            "0: off; 1: time based; 2: sudden death; 3: tie-break; "
            "4: golden frag",
            SheetHow::direct, "overtime|", "What follows a tied match."));
    put(row("powerups", "Powerups", "", "", kOnOff, SheetHow::direct, "powerups|",
            "Whether the quad, the pentagram, the ring and the suit are on the map."));
    put(row("midair", "Midair", kGroupRules, suffix("midair"), kOnOff, SheetHow::direct, "midair|",
            "Rockets count only on a player in the air."));
    put(row("instagib", "Instagib", kGroupRules, suffix("instagib"), kOnOff));
    put(row("arena", "Rocket arena", kGroupRules, suffix("ra"), kOnOff, SheetHow::direct,
            "arena|"));
    put(row("spawn", "Spawns", kGroupRules, "",
            "0: fixed order; 1: normal; 2: spawn safety; 3: team spawns; 4: weighted; 5: "
            "weighted, far from foes",
            SheetHow::direct, "spawn|", "How a player's spawn point is picked."));
    put(row("discharge", "Discharge", kGroupRules, "", kOnOff, SheetHow::direct, "discharge|",
            "Whether the lightning gun discharges in water."));
    put(row("fairpacks", "Fair backpacks", kGroupRules, "",
            "off: the weapon in hand; on: the best weapon; lst: the last fired", SheetHow::direct,
            "fairpacks|", "What a dead player's backpack holds."));
    put(row("dropquad", "Drop quad", kGroupRules, "", kOnOff, SheetHow::direct, "dropquad|"));
    put(row("dropring", "Drop ring", kGroupRules, "", kOnOff, SheetHow::direct, "dropring|"));
    put(row("droppack", "Drop backpacks", kGroupRules, "", kOnOff, SheetHow::direct, "droppack|"));
    put(row("lockmode", "Joining a match", kGroupRules, "",
            "team: only the teams playing; all: nobody; off: anyone", SheetHow::direct,
            "lockmode|"));
    put(row("spectalk", "Spectators heard", kGroupRules, "",
            "on: players hear spectators; off: they do not", SheetHow::direct, "silence|"));
    put(row("antilag", "Lag compensation", kGroupVotes,
            get("sv_antilag").empty() || get("sv_antilag") == "0" ? "off" : "on", kOnOff,
            SheetHow::vote, "antilag", "Shots hit where the target was on the shooter's screen."));
    put(row("teamoverlay", "Team overlay", kGroupVotes, "", kOnOff, SheetHow::vote, "teamoverlay",
            "Teammates' health, armor and place drawn on the screen."));
    put(row("nospecs", "No spectators", kGroupVotes, "", kOnOff, SheetHow::vote, "nospecs"));
    put(row("coop", "Cooperative", kGroupVotes, "", kOnOff, SheetHow::vote, "votecoop"));
  }
  put(row("maxclients", "Players", kGroupServer, get("maxclients"), "1..32: that many players",
          change, match ? "upplayers|downplayers" : ""));
  put(row("maxspectators", "Spectators", kGroupServer, get("maxspectators"),
          "0..32: that many spectators", change, match ? "upspecs|downspecs" : ""));
  put(row("maxfps", "Packets a second", kGroupServer, get("maxfps"), "N: that many"));
  put(row("watervis", "See through water", kGroupServer, or_zero(get("watervis")),
          "0: no; 1: yes"));
  put(row("hostname", "Name", kGroupServer, get("hostname"), "TEXT: the server's name"));
  put(row("*version", "Server", kGroupServer, get("*version"), "TEXT: its program"));
  if (match) put(row("ktxver", "Match mod", kGroupServer, get("ktxver"), "TEXT: its version"));
  for (const char* pm : {"pm_ktjump", "pm_bunnyspeedcap", "pm_slidefix", "pm_airstep", "pm_pground",
                         "pm_rampjump", "sv_safestrafe"})
    put(row(pm, gs::sheet_words_of(pm), kGroupMoves, or_zero(get(pm)), "0: off; N: on",
            match && std::string_view(pm) == "pm_airstep" ? SheetHow::direct : SheetHow::fixed,
            match && std::string_view(pm) == "pm_airstep" ? "airstep" : ""));
}

std::vector<std::string> qw_match_listing_asks() { return {"status1", "status2"}; }

bool qw_match_listing_read(gs::SettingsSheet& sheet, std::string_view raw) {
  const std::string line = strip_high(raw);
  // status2: the spawn model alone, then "<label>: <value>".
  for (size_t i = 0; i < kSpawnModels.size(); i++)
    if (trim(line) == kSpawnModels[i]) {
      set_value(sheet, "spawn", std::to_string(i));
      return true;
    }
  if (const size_t colon = line.find(": "); colon != std::string::npos) {
    const std::string label = trim(std::string_view(line).substr(0, colon));
    const std::string value = trim(std::string_view(line).substr(colon + 2));
    if (label == "Server locking") {
      set_value(sheet, "lockmode", value);
      return true;
    }
    if (label == "Spectalk") {
      set_value(sheet, "spectalk", value);
      return true;
    }
    if (label == "Overtime") {
      const char* v = value == "off"                                 ? "0"
                      : value.find("sudden") != std::string::npos    ? "2"
                      : value.find("tie-break") != std::string::npos ? "3"
                      : value.find("golden") != std::string::npos    ? "4"
                                                                     : "1";
      set_value(sheet, "overtime", v);
      return true;
    }
    if (label == "Server mode" || label == "Prewar" || label == "Admin election" ||
        label == "Check frametimes" || label == "Command floodprot" || label.starts_with("QiZmo") ||
        label.starts_with("CTF settings") || label.starts_with("Teaminfo"))
      return true;
  }
  // status1: labels padded to 14 columns, two pairs on a line.
  struct Label {
    std::string_view label, row;
  };
  static constexpr Label kLabels[] = {{"Maxspeed", ""},
                                      {"Deathmatch", "deathmatch"},
                                      {"Teamplay", "teamplay"},
                                      {"Timelimit", "timelimit"},
                                      {"Fraglimit", "fraglimit"},
                                      {"Powerups", "powerups"},
                                      {"Discharge", "discharge"},
                                      {"Drop Quad", "dropquad"},
                                      {"Drop Ring", "dropring"},
                                      {"Fair Backpacks", "fairpacks"},
                                      {"Drop Backpacks", "droppack"},
                                      {"spec info perm", ""},
                                      {"more spec info", ""},
                                      {"teleteam", ""},
                                      {"Berzerk", ""}};
  bool any = false;
  for (const Label& l : kLabels) {
    size_t at = line.find(l.label);
    while (at != std::string::npos && at > 0 && line[at - 1] != ' ')
      at = line.find(l.label, at + 1);
    if (at == std::string::npos) continue;
    size_t v = at + l.label.size();
    if (v >= line.size() || line[v] != ' ') continue;
    while (v < line.size() && line[v] == ' ') v++;
    const size_t end = std::min(line.find(' ', v), line.size());
    if (end == v) continue;
    any = true;
    if (l.row.empty()) continue;
    std::string value = line.substr(v, end - v);
    if (l.row == "powerups" && value != "on" && value != "off") value = "on";  // some of them
    set_value(sheet, l.row, value);
  }
  return any;
}

bool q2_info_read(std::string_view raw, std::pair<std::string, std::string>& out) {
  std::string line;
  for (const char c : raw) line += static_cast<char>(c & 0x7f);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
  // A key padded to 20 columns, a space, the value: the key is one word starting the line.
  if (line.size() < 21 || line[0] == ' ') return false;
  const size_t space = line.find(' ');
  if (space == std::string::npos || space > 20) return false;
  for (size_t i = space; i < 20; i++)
    if (line[i] != ' ') return false;
  out = {line.substr(0, space), trim(std::string_view(line).substr(20))};
  return true;
}

SheetSource q2_sheet_source(const std::vector<std::pair<std::string, std::string>>& info) {
  for (const auto& [k, v] : info)
    if (k == "gamename" && v == "OpenTDM") return SheetSource::q2_match;
  return SheetSource::q2_plain;
}

void q2_sheet_from_info(gs::SettingsSheet& sheet,
                        const std::vector<std::pair<std::string, std::string>>& info) {
  const auto get = [&info](std::string_view key) {
    for (const auto& [k, v] : info)
      if (k == key) return v;
    return std::string();
  };
  const bool match = q2_sheet_source(info) == SheetSource::q2_match;
  sheet.rows.clear();
  sheet.whole = true;
  sheet.revision++;
  const auto put = [&sheet](SheetRow r) { gs::sheet_put(sheet, std::move(r)); };
  put(row("map", "Map", "", get("mapname"), "TEXT: the map's name",
          match ? SheetHow::vote : SheetHow::fixed, match ? "vote map %v" : "",
          "The map being played."));
  if (match) {
    const std::string type = get("match_type");
    put(row("mode", "Mode", "",
            type == "TDM"    ? "tdm"
            : type == "ITDM" ? "itdm"
                             : "1v1",
            "tdm: team deathmatch; itdm: instagib team deathmatch; 1v1: a duel", SheetHow::vote,
            "vote mode %v", "The kind of match the server is set up for."));
    put(row("timelimit", "Time limit", "", "", "1..1440: that many minutes", SheetHow::vote,
            "vote timelimit %v", "The match's length in minutes."));
    put(row("overtime", "Overtime", "", "", "1..1440: that many minutes", SheetHow::vote,
            "vote overtime %v"));
    put(row("tiemode", "A tied match", kGroupRules, "",
            "none: stays a tie; ot: overtime; sd: sudden death", SheetHow::vote,
            "vote tiemode %v"));
    put(row("switchmode", "Weapon switch", kGroupRules, "",
            "normal: normal; fast: faster; instant: instant; insane: insane; extreme: extreme",
            SheetHow::vote, "vote switchmode %v"));
    put(row("telemode", "Teleporters", kGroupRules, "", "normal: normal; nofreeze: no freeze",
            SheetHow::vote, "vote telemode %v"));
    put(row("chat", "Chat in a match", kGroupRules, "", "players: players only; all: everyone",
            SheetHow::vote, "vote chat %v"));
  } else {
    put(row("deathmatch", "Deathmatch", "", get("deathmatch"), "0: off; 1: on"));
    put(row("timelimit", "Time limit", "", or_zero(get("timelimit")),
            "0: no limit; 1..N: that many minutes"));
    put(row("fraglimit", "Frag limit", "", or_zero(get("fraglimit")),
            "0: no limit; 1..N: that many frags"));
  }
  put(row("maxclients", "Players", kGroupServer, get("maxclients"), "1..N: that many players"));
  put(row("hostname", "Name", kGroupServer, get("hostname"), "TEXT: the server's name"));
  put(row("version", "Server", kGroupServer, get("version"), "TEXT: its program"));
  put(row("gamename", "Game", kGroupServer, get("gamename"), "TEXT: the game running"));
  // The deathmatch flags a row a bit, as the stock game reads them.
  struct Flag {
    int bit;
    std::string_view words;
  };
  static constexpr Flag kFlags[] = {{1, "No health"},          {2, "No powerups"},
                                    {4, "Weapons stay"},       {8, "No falling damage"},
                                    {16, "Instant powerups"},  {32, "Same map"},
                                    {64, "Teams by skin"},     {128, "Teams by model"},
                                    {256, "No friendly fire"}, {512, "Spawn farthest"},
                                    {1024, "Force respawn"},   {2048, "No armor"},
                                    {4096, "Allow exit"},      {8192, "Infinite ammo"},
                                    {16384, "Quad drop"},      {32768, "Fixed field of view"}};
  const long flags = std::strtol(get("dmflags").c_str(), nullptr, 10);
  for (const Flag& f : kFlags)
    put(row("dmflags:" + std::to_string(f.bit), f.words, kGroupFlags,
            (flags & f.bit) != 0 ? "on" : "off", kOnOff));
}

bool q2_match_listing_read(gs::SettingsSheet& sheet, std::string_view raw) {
  std::string line;
  for (const char c : raw) line += static_cast<char>(c & 0x7f);
  const size_t colon = line.find(": ");
  if (colon == std::string::npos) return false;
  const std::string label = trim(std::string_view(line).substr(0, colon + 1));
  const std::string value = trim(std::string_view(line).substr(colon + 2));
  const auto minutes = [](const std::string& v) {
    return std::to_string(std::strtol(v.c_str(), nullptr, 10));
  };
  if (label == "Game mode:") {
    set_value(sheet, "mode",
              value.find("Instagib") != std::string::npos ? "itdm"
              : value.find("Team") != std::string::npos   ? "tdm"
                                                          : "1v1");
  } else if (label == "Timelimit:") {
    set_value(sheet, "timelimit", minutes(value));
  } else if (label == "Overtime:") {
    set_value(sheet, "overtime", value == "disabled" ? "" : minutes(value));
  } else if (label == "Weapon switch:") {
    set_value(sheet, "switchmode", value == "faster" ? "fast" : value);
  } else if (label == "Teleporter mode:") {
    set_value(sheet, "telemode", value == "normal" ? "normal" : "nofreeze");
  } else if (label != "Removed weapons:" && label != "Removed powerups:" &&
             label != "Armor timer:" && label != "Weapon timer:" && label != "Gameplay bugs:" &&
             label != "Timeouts:" && label != "Timeout limits:" && !label.ends_with("skin:") &&
             !label.ends_with("Respawn mode:")) {
    return false;
  }
  return true;
}

}  // namespace ge::classic
