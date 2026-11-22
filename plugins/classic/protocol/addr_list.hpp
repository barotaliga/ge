// A MODERN CLASSIC SERVER'S ADDRESS LIST: entries of an address and a mask with a comment, each
// counting its hits and the time of its last one, kept by three console words (`add…`, `del…`,
// `list…`). Quake II's server keeps two for its relay viewers (`addgtvhost`, `addgtvban`;
// porting/shooters-edge/q2-mvd-relay.md § 8) and its ban list reads the same way.
//
// ADD `<address[/mask]> [comment]`: IPv4 or IPv6 without a port, the mask a bit count (the whole
// address when left out), the comment the rest of the line; nothing printed on success.
// Refusals: "Usage: <cmd> <address[/mask]> [comment]", "Please specify a mask after '/'.", "Bad
// address: <text>", "Bad mask: <n> bits", "Entry <address>/<bits> already exists.".
// DEL `<address[/mask]|id|all>`: `all` empties it, a number is the row from 1; "Usage: <cmd>
// <address[/mask]|id|all>", "Address list is empty.", "No such index: <n>", "No such entry:
// <text>".
// LIST: "Address list is empty.", else a header, a rule and a row an entry: the id, address/bits,
// hits, the last hit's local time (day, month, hours and minutes) or "never", the comment.
//
// The lists live in memory only; a cfg run at start keeps them across runs.
//
// ge:summary A classic server's address list with masks, comments, hits and last hit
// ge:keywords address-list addgtvhost addgtvban delgtvhost listgtvhosts mask comment hits
// ge:keywords last-hit ipv6-mask addban listbans
// ge:skill classic net
// ge:test tests/classic_addr_list_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"

namespace ge::classic {

struct AddrEntry {
  net::Family family = net::Family::ipv4;
  uint8_t bytes[16] = {};
  int bits = 32;
  std::string comment;
  int64_t hits = 0;
  int64_t last_hit = 0;  // Unix seconds; 0 never
};

struct AddrList {
  std::vector<AddrEntry> entries;
};

// The first entry matching `a` (its port ignored), its hit counted at `now`; null when none.
const AddrEntry* addr_list_hit(AddrList& list, const net::Address& a, int64_t now);
// The words, `args` the line after the command's name; what to print into `out` (may be empty).
void addr_list_add(AddrList& list, std::string_view cmd, std::string_view args, std::string& out);
void addr_list_del(AddrList& list, std::string_view cmd, std::string_view args, std::string& out);
std::string addr_list_text(const AddrList& list);

}  // namespace ge::classic
