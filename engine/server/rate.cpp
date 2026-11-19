// A client's send rate held to the server's bounds (rate.hpp states the rule).
//
// ge:summary The rate in force and the verdict on a rate out of the server's bounds
// ge:keywords rate_in_force rate_verdict rate_floor sv_maxrate sv_minrate sv_lanrate rate_on_lan
// ge:skill net
// ge:test tests/server_rate_test.cpp
#include "server/rate.hpp"

#include <cstdlib>

namespace ge::server {

bool rate_on_lan(const net::Address& a) {
  const uint8_t* b = a.bytes;
  if (a.family == net::Family::ipv4)
    return b[0] == 10 || b[0] == 127 || (b[0] == 172 && (b[1] & 0xf0) == 16) ||
           (b[0] == 192 && b[1] == 168) || (b[0] == 169 && b[1] == 254);
  if (a.family != net::Family::ipv6) return false;
  bool loop = b[15] == 1;
  for (int i = 0; i < 15; i++) loop = loop && b[i] == 0;
  return loop || (b[0] & 0xfe) == 0xfc || (b[0] == 0xfe && (b[1] & 0xc0) == 0x80);
}

int rate_floor(const RateLimits& l) { return l.max > 0 && l.min > l.max ? l.max : l.min; }

int rate_in_force(std::string_view asked, const RateLimits& l, const RateBounds& b) {
  int rate = asked.empty() ? b.none : std::atoi(std::string(asked).c_str());
  if (!asked.empty() && rate == 0) rate = b.zero;
  if (l.max > 0 && rate > l.max) rate = l.max;
  if (rate < rate_floor(l)) rate = rate_floor(l);
  return rate < b.low ? b.low : rate > b.high ? b.high : rate;
}

RateVerdict rate_verdict(std::string_view asked, const RateLimits& l) {
  RateVerdict v;
  if (asked.empty()) return v;  // the most the server allows
  const int rate = std::atoi(std::string(asked).c_str());
  const bool high = l.max > 0 && rate > l.max;
  const bool low = !high && rate < rate_floor(l);
  if (!high && !low) return v;
  v.out = true;
  v.rate = high ? l.max : rate_floor(l);
  v.said = std::string("\nYour rate setting is too ") + (high ? "high" : "low") +
           " for this server.\nRate set to " + std::to_string(v.rate) + "\n";
  return v;
}

}  // namespace ge::server
