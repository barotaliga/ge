// How often a server answers a guess (limit.hpp states the rule).
//
// ge:summary sv_rconrate, sv_passwordrate: the value read, the bucket taken and given back
// ge:keywords sv_rconrate sv_passwordrate parse_limit limit_take limit_give
// ge:skill net
// ge:test tests/server_limit_test.cpp
#include "server/limit.hpp"

#include <cstdlib>

namespace ge::server {

namespace {

// A whole number from `s` (digits alone); false when it is not one or is past `most`.
bool whole(const std::string& s, double most, double& out) {
  if (s.empty() || s.size() > 7) return false;
  for (char ch : s)
    if (ch < '0' || ch > '9') return false;
  out = std::atof(s.c_str());
  return out <= most;
}

}  // namespace

bool parse_limit(const std::string& name, const std::string& value, TryLimit& out,
                 std::string& error) {
  std::string rest = value, period, burst;
  if (const size_t star = rest.find('*'); star != std::string::npos) {
    burst = rest.substr(star + 1);
    rest.resize(star);
  }
  if (const size_t slash = rest.find('/'); slash != std::string::npos) {
    period = rest.substr(slash + 1);
    rest.resize(slash);
  }
  TryLimit r;
  bool ok = whole(rest, 10000, r.limit);
  if (ok && !period.empty()) {
    r.period_s = period == "sec" ? 1 : period == "min" ? 60 : period == "hour" ? 3600 : 0;
    if (r.period_s == 0) ok = whole(period, 86400, r.period_s) && r.period_s >= 1;
  }
  if (ok && !burst.empty()) ok = whole(burst, 10000, r.burst) && r.burst >= 1;
  if (!ok) {
    error = name + " \"" + value +
            "\" is not \"limit[/period][*burst]\" (10, 1/min, 5/10*2; 0 for no limit) "
            "(--explain=" +
            name + " shows where it was set)";
    return false;
  }
  out = r;
  return true;
}

bool limit_settings(const ServerConfig& c, TryLimit& rcon, TryLimit& password, std::string& error) {
  TryLimit a = rcon, b = password;
  if (const auto it = c.rules.find("sv_rconrate"); it != c.rules.end() && !it->second.empty())
    if (!parse_limit("sv_rconrate", it->second, a, error)) return false;
  if (const auto it = c.rules.find("sv_passwordrate"); it != c.rules.end() && !it->second.empty())
    if (!parse_limit("sv_passwordrate", it->second, b, error)) return false;
  rcon = a;
  password = b;
  return true;
}

bool limit_take(TryBucket& b, double now, const TryLimit& rule) {
  if (rule.limit <= 0) return true;
  if (b.tokens < 0) {
    b.tokens = rule.burst;
  } else {
    b.tokens += (now - b.at) * rule.limit / rule.period_s;
    if (b.tokens > rule.burst) b.tokens = rule.burst;
  }
  b.at = now;
  if (b.tokens < 1) return false;
  b.tokens -= 1;
  return true;
}

void limit_give(TryBucket& b, const TryLimit& rule) {
  if (rule.limit <= 0 || b.tokens < 0) return;
  b.tokens += 1;
  if (b.tokens > rule.burst) b.tokens = rule.burst;
}

}  // namespace ge::server
