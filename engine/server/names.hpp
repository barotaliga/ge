// HOW OFTEN A PLAYER MAY RENAME, AND WHO HEARS OF IT (`sv_namelimit`, `sv_shownames`), the same
// rules for every game's server (docs/servers.md § The settings).
//
// - THE LIMIT (`sv_namelimit "N S"`): N renames in any S seconds; the one past them is refused,
//   the old name kept, and the player told "You can't change your name too often.". The window
//   slides, as the chat flood rule's does (guards.hpp). "0" or "0 0": no limit. "N/min",
//   "N/sec", "N/hour" and "N/S" read as "N 60", "N 1", "N 3600" and "N S".
// - THE NOTICE (`sv_shownames`): 1 tells everyone "<old> changed name to <new>." when a player in
//   the game renames, 2 when anyone does (a spectator too); 0 (the default) tells nobody. The
//   server's own log has every rename either way.
//
// The clock is the caller's (the server's own time, s). The rules are `kNameRules` (an endpoint's,
// read when it starts by `name_settings`).
//
// ge:summary sv_namelimit, sv_shownames: renames a window, the refusal, the notice to all
// ge:keywords sv_namelimit sv_shownames rename name-change name-spam name_settings
// ge:keywords rename_allowed rename_notice
// ge:skill net
// ge:test tests/server_names_test.cpp
#pragma once

#include <string>

#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kNameRules[] = {
    {"sv_namelimit", "How often a player may change name.", nullptr,
     "\"N S\": N changes in any S seconds (5 60 the default); 0: no limit"},
    {"sv_shownames", "Who is told when a player changes name.", nullptr,
     "0: nobody (the default); 1: everyone, of a player in the game; 2: everyone, of anyone"},
};

constexpr int kMostRenames = 16;  // N's largest: the ring each watch keeps

struct NameRule {
  int changes = 5;  // 0: no limit
  double window_s = 60;
  int show = 0;
};

// The two rules from `c` (unset: the defaults). False with `error` naming the value.
bool name_settings(const ServerConfig& c, NameRule& out, std::string& error);

// One seat's last renames (the ring of their times).
struct NameWatch {
  double at[kMostRenames] = {};
  int count = 0, next = 0;
};

// Whether a rename now is allowed; an allowed one is counted.
bool rename_allowed(NameWatch& w, double now, const NameRule& rule);

// Whether everyone is told of a rename by a player (`playing`: in the game, not watching).
inline bool rename_told(const NameRule& rule, bool playing) {
  return rule.show >= 2 || (rule.show == 1 && playing);
}

inline std::string rename_notice(const std::string& was, const std::string& now) {
  return was + " changed name to " + now + ".\n";
}

inline constexpr const char* kRenameRefusal = "You can't change your name too often.\n";

}  // namespace ge::server
