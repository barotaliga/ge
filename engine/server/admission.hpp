// HOW MANY CONNECTIONS ONE ADDRESS MAY HOLD (`sv_iplimit`), the same rule for every game's server
// (docs/servers.md § The settings): a new client is refused when its host already holds that many
// seats, so one machine (or one IPv6 network) cannot fill a public server.
//
// - THE HOST: an IPv4 address is one host; an IPv6 address counts by its /48 network (its first
//   six bytes), since one subscriber is given a whole /48; a simulated node is one host. A
//   loopback address (the server's own machine) and a link-local IPv6 address are never counted.
// - THE COUNT: each seat the host holds counts 1, a seat being freed (a dropped client's slot still
//   held, the sequel's zombie) a half. A client is refused when the count reaches the limit.
// - THE WORDS (`admit_refusal`): "Too many connections from your IP address." or, for IPv6, "Too
//   many connections from your IPv6 network."; empty when the client may come in.
//
// The rule is `kAdmitRules` (an endpoint's, read when the server starts); a game picks its own
// default (unset: the game's), 0 meaning no limit.
//
// ge:summary sv_iplimit: the most seats one IPv4 host or IPv6 /48 may hold, and the refusal
// ge:keywords sv_iplimit ip-limit connections-per-address admission connect-refusal ipv6-48
// ge:keywords admit_refusal seats-per-host
// ge:skill net
// ge:test tests/server_admission_test.cpp
#pragma once

#include <span>
#include <string>

#include "net/address.hpp"
#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kAdmitRules[] = {
    {"sv_iplimit", "The most connections one address, or one IPv6 network, may hold.", nullptr,
     "0: no limit; 1..N: that many, a slot being freed counting a half"},
};

// A seat a client holds: its address, and whether it is being freed.
struct HeldSeat {
  net::Address address;
  bool freeing = false;
};

// Whether `a` and `b` are one host by the rule above (never for loopback or link-local IPv6).
bool same_admit_host(const net::Address& a, const net::Address& b);
// The refusal for a new client from `from` when `seats` are held and the limit is `limit` (0: no
// limit); empty when it may come in.
std::string admit_refusal(int limit, const net::Address& from, std::span<const HeldSeat> seats);

}  // namespace ge::server
