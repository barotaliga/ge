// A SERVER'S ADDRESS LIST: the addresses an administrator bans (or, with the list turned round,
// the only ones let in), the same for every multiplayer game on the engine (docs/servers.md).
//
// AN ENTRY is an IPv4 pattern of up to four numbers, a missing or `*` part matching any number, so
// "10.0" and "10.0.*.*" both match the whole 10.0.x.x subnet; an optional `/<bits>` takes only the
// first bits of what is written ("10.0.4.0/22"). Each entry is `ban` or `safe` (an exact entry a
// player's vote may not ban) and may end at a time (Unix seconds; 0 never). Entries match by their
// text as written: `remove` takes the same text.
//
// THE MODE (`sv_banmode`, classic `filterban`): 1, the list refuses the addresses it matches; 0,
// only the addresses it matches are let in (a private server). An address a safe entry matches is
// never refused, whatever else matches it.
//
// THE CONSOLE WORDS (`ban_command`): `addip <pattern> [ban|safe] [+<seconds>|<unix time>]`,
// `removeip <pattern>`, `listip`, `writeip` (the list as `addip` lines, for a cfg the server runs
// at start). Entries past their time are dropped when the list is next checked.
//
// The clock is the caller's (`now`, Unix seconds), so a list never reads one itself. A simulated
// address counts as the IPv4 address net/address.hpp maps it to.
//
// ge:summary A server's ban list by address: patterns, ban or safe, times, the mode, the words
// ge:keywords ban banlist addip removeip listip writeip filterban sv_banmode ban-mode safe-entry
// ge:keywords address-filter subnet ip-ban banned ban_command ban_refuses
// ge:skill net
// ge:test tests/server_bans_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"

namespace ge::server {

struct BanEntry {
  std::string text;   // the pattern as written
  uint32_t ip = 0;    // the pattern's address, most significant byte first
  uint32_t mask = 0;  // the bits that must match
  bool safe = false;  // never refused; a vote may not ban it
  int64_t until = 0;  // Unix seconds it ends at (0: never)
};

struct BanList {
  std::vector<BanEntry> entries;
};

// `text` read as a pattern (above); false when it is not one.
bool parse_ban_pattern(std::string_view text, BanEntry& out);
// The entry that matches `a` at `now` (expired ones skipped); null when none does.
const BanEntry* ban_match(const BanList& list, const net::Address& a, int64_t now);
// Whether the server refuses `a` under `ban_mode` (1: the list bans; 0: only the list may join).
bool ban_refuses(const BanList& list, int ban_mode, const net::Address& a, int64_t now);
// A console line when it is one of the list's words: true, its answer in `out` (`writeip`'s is
// the cfg lines). Expired entries are dropped first.
bool ban_command(BanList& list, std::string_view line, int64_t now, std::string& out);

}  // namespace ge::server
