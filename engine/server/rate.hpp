// A CLIENT'S SEND RATE held to the server's bounds, for every game: the bytes a second a client
// asks the server to send it (its `rate`) taken as the game reads it, then held to the server's
// floor and cap, and a client out of range told in a game's words and set back.
//
// - THE GAME'S OWN READING (`RateBounds`): what no rate at all means, what a rate of 0 means, and
//   the range its network accepts; a classic game's defaults are the struct's own.
// - THE SERVER'S BOUNDS (`RateLimits`, the rules `kRateRules`): `sv_maxrate` caps a rate, 0 no
//   cap; `sv_minrate` raises one, 0 no floor, never above the cap.
// - THE VERDICT (`rate_verdict`): a rate the client asked for past the cap or under the floor is
//   reported to the player ("Your rate setting is too high for this server." and the rate it is
//   set to) and the game sets it back (a classic game puts `rate N` in the client's console).
// - THE LAN (`sv_lanrate` 1): a client from the server's own network (a private, loopback or
//   link-local address: `rate_on_lan`) is held to neither bound and never told, so a LAN party
//   plays at the rate each client asks for; 0 (the default): every client alike.
//
// ge:summary A client's send rate held to sv_minrate and sv_maxrate, and the player told
// ge:keywords rate sv_maxrate sv_minrate rate-cap rate-floor rate_in_force rate_verdict
// ge:keywords rate-rules client-rate sv_lanrate lan-rate rate_on_lan rate_limits_for
// ge:skill net
// ge:test tests/server_rate_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "net/address.hpp"
#include "server/server.hpp"

namespace ge::server {

struct RateBounds {
  int none = 99999;   // a client that names no rate
  int zero = 2500;    // a rate of 0
  int low = 500;      // the fewest the game's network sends
  int high = 300000;  // the most
};

struct RateLimits {
  int min = 0;  // sv_minrate (0: no floor)
  int max = 0;  // sv_maxrate (0: no cap)
};

inline constexpr GameRule kRateRules[] = {
    {"sv_maxrate", "The most bytes a second a client may ask for with its rate.", nullptr,
     "0: no cap (the default); 1..N: that many bytes a second"},
    {"sv_minrate",
     "The fewest bytes a second a client may ask for with its rate; a lower rate is raised to it.",
     nullptr, "0: no floor (the default); 1..N: that many bytes a second"},
    {"sv_lanrate", "Whether a client on the server's own network is held to the rate bounds.",
     nullptr, "0: every client held (the default); 1: one from a private address is not"},
};

// Whether `a` is on a local network: IPv4 10/8, 172.16/12, 192.168/16, 127/8, 169.254/16; IPv6
// loopback, fc00::/7, fe80::/10.
bool rate_on_lan(const net::Address& a);
// The bounds a client from `from` is held to: `l`, or none when `lan_rate` and it is on the LAN.
inline RateLimits rate_limits_for(const RateLimits& l, bool lan_rate, const net::Address& from) {
  return lan_rate && rate_on_lan(from) ? RateLimits{} : l;
}

// The floor in force: `min`, never above `max`.
int rate_floor(const RateLimits& l);
// The rate the server sends at for a client's `asked` (its `rate`, empty when it names none).
int rate_in_force(std::string_view asked, const RateLimits& l, const RateBounds& b = {});

struct RateVerdict {
  bool out = false;  // the asked rate is past the cap or under the floor
  int rate = 0;      // the rate it is set back to
  std::string said;  // the player's line, newlines included
};
// Whether `asked` is out of the server's bounds (no rate named never is).
RateVerdict rate_verdict(std::string_view asked, const RateLimits& l);

}  // namespace ge::server
