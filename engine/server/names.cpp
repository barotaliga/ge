// How often a player may rename (names.hpp states the rule).
//
// ge:summary sv_namelimit, sv_shownames: the settings read and the sliding window
// ge:keywords sv_namelimit sv_shownames rename_allowed name_settings
// ge:skill net
// ge:test tests/server_names_test.cpp
#include "server/names.hpp"

#include <cstdio>
#include <cstdlib>

namespace ge::server {

bool name_settings(const ServerConfig& c, NameRule& out, std::string& error) {
  NameRule r = out;
  if (const auto it = c.rules.find("sv_namelimit"); it != c.rules.end() && !it->second.empty()) {
    int n = 0;
    double s = 0;
    char extra = 0;
    char unit[8] = {};
    int got = std::sscanf(it->second.c_str(), "%d %lf %c", &n, &s, &extra);
    // "N/min", "N/sec", "N/hour" or "N/S": the same limit as the modern server's configs write it.
    if (std::sscanf(it->second.c_str(), "%d/%7[a-z0-9.] %c", &n, unit, &extra) == 2) {
      const std::string u = unit;
      s = u == "sec" ? 1 : u == "min" ? 60 : u == "hour" ? 3600 : std::atof(unit);
      got = 2;
    }
    const bool off = got >= 1 && n == 0 && extra == 0;
    if (!off && (got != 2 || n < 1 || n > kMostRenames || !(s >= 1 && s <= 86400))) {
      error = "sv_namelimit \"" + it->second + "\" is not \"N S\" (or \"N/min\") with N 1 to " +
              std::to_string(kMostRenames) +
              " and S 1 to 86400 seconds, or 0 (--explain=sv_namelimit shows where it was set)";
      return false;
    }
    r.changes = off ? 0 : n;
    r.window_s = off ? 0 : s;
  }
  if (const auto it = c.rules.find("sv_shownames"); it != c.rules.end() && !it->second.empty()) {
    if (it->second != "0" && it->second != "1" && it->second != "2") {
      error = "sv_shownames \"" + it->second +
              "\" is not 0, 1 or 2 (--explain=sv_shownames shows where it was set)";
      return false;
    }
    r.show = std::atoi(it->second.c_str());
  }
  out = r;
  return true;
}

bool rename_allowed(NameWatch& w, double now, const NameRule& rule) {
  if (rule.changes <= 0) return true;
  const int n = rule.changes < kMostRenames ? rule.changes : kMostRenames;
  if (w.count >= n) {  // the n-th most recent rename inside the window: refused
    const double oldest = w.at[(w.next - n + kMostRenames) % kMostRenames];
    if (now - oldest < rule.window_s) return false;
  }
  w.at[w.next] = now;
  w.next = (w.next + 1) % kMostRenames;
  if (w.count < kMostRenames) w.count++;
  return true;
}

}  // namespace ge::server
