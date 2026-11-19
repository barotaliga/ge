// What a server tells a stranger who asks (status.hpp states the rule).
//
// ge:summary sv_status, sv_statusrate, sv_uptime: the settings read, the uptime written
// ge:keywords sv_status sv_statusrate sv_uptime status_settings uptime_text
// ge:skill net
// ge:test tests/server_status_test.cpp
#include "server/status.hpp"

#include <cstdio>
#include <cstdlib>

namespace ge::server {

namespace {

bool small(const ServerConfig& c, const char* name, int most, int& out, std::string& error) {
  const auto it = c.rules.find(name);
  if (it == c.rules.end() || it->second.empty()) return true;
  const std::string& v = it->second;
  if (v.size() != 1 || v[0] < '0' || v[0] > '0' + most) {
    error = std::string(name) + " \"" + v + "\" is not 0 to " + std::to_string(most) +
            " (--explain=" + name + " shows where it was set)";
    return false;
  }
  out = v[0] - '0';
  return true;
}

std::string unit(long n, const char* one) {
  return std::to_string(n) + " " + one + (n == 1 ? "" : "s");
}

}  // namespace

bool status_settings(const ServerConfig& c, StatusRule& out, std::string& error) {
  StatusRule r = out;
  if (!small(c, "sv_status", 2, r.show, error) || !small(c, "sv_uptime", 2, r.uptime, error))
    return false;
  if (const auto it = c.rules.find("sv_statusrate"); it != c.rules.end() && !it->second.empty())
    if (!parse_limit("sv_statusrate", it->second, r.rate, error)) return false;
  out = r;
  return true;
}

std::string uptime_text(double seconds, int form) {
  if (form <= 0) return {};
  const long t = seconds > 0 ? static_cast<long>(seconds) : 0;
  const long d = t / 86400, h = t / 3600 % 24, m = t / 60 % 60, s = t % 60;
  char b[64];
  if (form == 1) {
    if (d > 0)
      std::snprintf(b, sizeof b, "%ld+%ld:%02ld.%02ld", d, h, m, s);
    else if (h > 0)
      std::snprintf(b, sizeof b, "%ld:%02ld.%02ld", h, m, s);
    else
      std::snprintf(b, sizeof b, "%02ld.%02ld", m, s);
    return b;
  }
  std::string w;
  if (d > 0) w += unit(d, "day") + ", ";
  if (d > 0 || h > 0) w += unit(h, "hour") + ", ";
  if (d > 0 || h > 0 || m > 0) w += unit(m, "min") + ", ";
  return w + unit(s, "sec");
}

}  // namespace ge::server
