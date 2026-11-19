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
// SLOTS KEPT FOR VIPS (`sv_vipslots`, `sv_vippassword`): a client who gives one of the VIP
// passwords (or whom a game makes a VIP another way: its VIP address list, its server password)
// is asked no other password, and the last `sv_vipslots` of the players' slots are his alone: a
// newcomer who is not a VIP is refused once only those are free (`vip_slots_refuse`). Each game
// words the refusal its own way.
//
// ge:summary sv_iplimit: the most seats one IPv4 host or IPv6 /48 may hold, and the refusal
// ge:keywords sv_iplimit ip-limit connections-per-address admission connect-refusal ipv6-48
// ge:keywords admit_refusal seats-per-host sv_vipslots sv_vippassword vip-slots reserved-slots
// ge:keywords vip_slots_refuse
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
    {"sv_vipslots", "The players' slots only a VIP may take.", nullptr,
     "0: none (the default); 1..N: the last that many of the players' slots"},
    {"sv_vippassword", "The passwords, spaces between, any of which makes a client a VIP.", nullptr,
     "\"\": none (the default); TEXT: these words, and a VIP is asked no other password and may "
     "take the slots kept for VIPs"},
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
// Whether a newcomer is refused for the slots kept for VIPs: `taken` of the players' `slots` held,
// the last `kept` of them for VIPs; a VIP never is (nor anyone when no slot is free at all: the
// game's own "full").
inline bool vip_slots_refuse(int slots, int taken, int kept, bool vip) {
  return !vip && kept > 0 && taken < slots && taken >= slots - kept;
}

}  // namespace ge::server
