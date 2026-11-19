// A client's asked move time against real time (skew.hpp states the rule).
//
// ge:summary sv_speedwarn, sv_speedkick: the count, the check, the settings read
// ge:keywords sv_speedwindow sv_speedwarn sv_speedkick time-skew skew_packet skew_check
// ge:skill net
// ge:test tests/server_skew_test.cpp
#include "server/skew.hpp"

#include <algorithm>
#include <cstdlib>

namespace ge::server {

namespace {

bool number(const ServerConfig& c, const char* name, double least, double most, double& out,
            std::string& error) {
  const auto r = c.rules.find(name);
  if (r == c.rules.end() || r->second.empty()) return true;
  char* end = nullptr;
  const double v = std::strtod(r->second.c_str(), &end);
  if (end == r->second.c_str() || *end != '\0' || !(v >= least && v <= most)) {
    error = std::string(name) + " \"" + r->second + "\" is not a number from " +
            std::to_string(static_cast<int>(least)) + " to " +
            std::to_string(static_cast<int>(most)) + " (--explain=" + name +
            " shows where it was set)";
    return false;
  }
  out = v;
  return true;
}

}  // namespace

bool skew_settings(const ServerConfig& c, SkewRule& out, std::string& error) {
  SkewRule r = out;
  if (!number(c, "sv_speedwindow", 1, 600, r.window_s, error) ||
      !number(c, "sv_speedwarn", 0, 100, r.warn, error) ||
      !number(c, "sv_speedkick", 0, 100, r.kick, error))
    return false;
  out = r;
  return true;
}

void skew_packet(SkewWatch& w, double now, uint32_t sequence, const int (&msec)[3]) {
  if (w.start < 0) {  // the first packet starts the window and counts its newest command
    w = {now, static_cast<double>(msec[2]), sequence};
    return;
  }
  if (sequence <= w.last_sequence) return;  // late or repeated
  const uint32_t fresh = std::min<uint32_t>(sequence - w.last_sequence, 3);
  for (uint32_t i = 3 - fresh; i < 3; i++) w.asked_ms += msec[i];
  w.last_sequence = sequence;
}

Skew skew_check(SkewWatch& w, double now, const SkewRule& rule, double& ratio) {
  if (w.start < 0 || now - w.start < rule.window_s) return Skew::fine;
  ratio = w.asked_ms / ((now - w.start) * 1000);
  w.start = now;
  w.asked_ms = 0;
  if (rule.kick > 1 && ratio > rule.kick) return Skew::kick;
  if (rule.warn > 1 && ratio > rule.warn) return Skew::warn;
  return Skew::fine;
}

}  // namespace ge::server
